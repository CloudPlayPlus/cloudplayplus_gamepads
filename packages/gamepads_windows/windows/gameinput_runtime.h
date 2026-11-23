// 原生回归可预先定义此守卫并提供 CreateGameInput 替身。
#ifndef GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_
#define GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_

#include <windows.h>
#include <GameInput.h>

#include <atomic>
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

inline CreateFn LoadInbox() {
  return LoadCreate(
      LoadLibraryExW(L"GameInput.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32));
}

inline CreateFn LoadBundled() {
  const auto bundled = BundledRuntimePath();
  if (bundled.empty())
    return nullptr;
  // 依赖只从运行库所在目录与系统目录解析，不受当前目录影响。
  return LoadCreate(LoadLibraryExW(
      bundled.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
}

enum class Source : int { none = 0, inbox = 1, bundled = 2 };

// 先用系统运行库；系统 DLL 不存在或创建失败时改用附带运行库。
// Windows 10 2004 至 Windows 11 23H2 自带的 GameInput.dll 只是校验签名后加载
// 外部运行库的壳，系统未安装该运行库时创建会失败。
// 附带运行库仅在需要时加载，系统运行库可用的机器不加载它。
template <typename Input, typename Inbox, typename Bundled>
HRESULT CreateWithFallback(Input** input,
                           Inbox inbox,
                           Bundled bundled,
                           Source* source) {
  *input = nullptr;
  *source = Source::none;
  HRESULT hr = HRESULT_FROM_WIN32(ERROR_MOD_NOT_FOUND);
  const auto attempt = [&](auto create, Source from) {
    if (!create)
      return false;
    hr = create(input);
    if (SUCCEEDED(hr) && *input) {
      *source = from;
      return true;
    }
    if (*input) {
      (*input)->Release();
      *input = nullptr;
    }
    if (SUCCEEDED(hr))
      hr = E_POINTER;
    return false;
  };
  if (!attempt(inbox(), Source::inbox))
    attempt(bundled(), Source::bundled);
  return hr;
}

inline std::atomic<int> current_source{static_cast<int>(Source::none)};
}  // namespace gamepads_windows_gameinput

inline HRESULT CreateGameInput(IGameInput** input) {
  namespace runtime = gamepads_windows_gameinput;
  runtime::Source source;
  const HRESULT hr = runtime::CreateWithFallback(
      input,
      [] {
        static const auto create = runtime::LoadInbox();
        return create;
      },
      [] {
        static const auto create = runtime::LoadBundled();
        return create;
      },
      &source);
  runtime::current_source.store(static_cast<int>(source));
  return hr;
}

// 最近一次创建使用的运行库："system"、"bundled" 或 "unavailable"。
inline const char* GameInputRuntimeSource() {
  switch (static_cast<gamepads_windows_gameinput::Source>(
      gamepads_windows_gameinput::current_source.load())) {
    case gamepads_windows_gameinput::Source::inbox:
      return "system";
    case gamepads_windows_gameinput::Source::bundled:
      return "bundled";
    default:
      return "unavailable";
  }
}

#endif  // GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_
