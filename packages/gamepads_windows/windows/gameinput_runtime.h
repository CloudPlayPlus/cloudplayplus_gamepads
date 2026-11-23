// 原生回归可预先定义此守卫并提供替身，模拟运行库缺失。
#ifndef GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_
#define GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_

#include <windows.h>

// GameInput.dll 以 /DELAYLOAD 链接：缺失时插件和宿主进程仍可启动。
// 首次调用 GameInputCreate 前必须确认系统目录中的运行库及其导出存在，
// 否则延迟加载失败会抛出结构化异常。模块保持加载，延迟加载复用同一实例。
inline bool GameInputRuntimeAvailable() {
  static const bool available = [] {
    const HMODULE module =
        LoadLibraryExW(L"GameInput.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module)
      return false;
    // v0 GameInput.dll 仅按序号 1 导出 GameInputCreate。
    if (GetProcAddress(module, MAKEINTRESOURCEA(1)))
      return true;
    FreeLibrary(module);
    return false;
  }();
  return available;
}

#endif  // GAMEPADS_WINDOWS_GAMEINPUT_RUNTIME_H_
