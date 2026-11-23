// 原生回归可预先定义此守卫并提供 CreateGameInput 替身。
#ifndef GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_
#define GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_

#include <windows.h>
#include <GameInput.h>

#include <string>

// 插件不链接 GameInput.lib：缺少运行库时宿主进程仍可启动。
// 来源一：系统目录的 GameInput.dll（Windows 10 1903 起自带），优先使用，
// 保持绝大多数机器的既有行为。
// 来源二：随应用分发、位于可执行文件目录的 GameInputRedist.dll。
// 两者都以序号 1 导出 v0 GameInputCreate。
// 附带运行库仅覆盖 Microsoft 支持的 Windows 10 1903 及以上；更早系统（如
// Server 2016/2019）加载失败时与缺失同样处理：手柄不可用，宿主照常运行。
// 模块保持加载直至进程退出，与异步释放的 IGameInput 生命周期一致。
namespace gamepads_windows_gameinput {
using CreateFn = HRESULT(WINAPI*)(IGameInput**);

inline CreateFn LoadCreate(HMODULE module) {
  if (!module)
    return nullptr;
  const auto create =
      reinterpret_cast<CreateFn>(GetProcAddress(module, MAKEINTRESOURCEA(1)));
  if (!create)
    FreeLibrary(module);
  return create;
}

inline std::wstring BundledRuntimePath() {
  std::wstring path(MAX_PATH, L'\0');
  for (;;) {
    const DWORD length = GetModuleFileNameW(nullptr, path.data(),
                                            static_cast<DWORD>(path.size()));
    if (length == 0)
      return {};
    if (length < path.size()) {
      path.resize(length);
      break;
    }
    path.resize(path.size() * 2);
  }
  const auto separator = path.find_last_of(L"\\/");
  if (separator == std::wstring::npos)
    return {};
  path.resize(separator + 1);
  return path + L"GameInputRedist.dll";
}

inline CreateFn ResolveCreate() {
  if (const auto inbox = LoadCreate(LoadLibraryExW(
          L"GameInput.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)))
    return inbox;
  const auto bundled = BundledRuntimePath();
  if (bundled.empty())
    return nullptr;
  // 依赖只从运行库所在目录与系统目录解析，不受当前目录影响。
  return LoadCreate(LoadLibraryExW(
      bundled.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
}
}  // namespace gamepads_windows_gameinput

inline HRESULT CreateGameInput(IGameInput** input) {
  *input = nullptr;
  static const auto create = gamepads_windows_gameinput::ResolveCreate();
  return create ? create(input) : HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
}

#endif  // GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_
