#pragma once

#include <windows.h>
#include <new>

// 仅在回调、读取线程及设备引用均已清理后转移最后一个引用。
// 工作线程只拥有输入对象和模块，不得访问插件或 Flutter 状态。
template <typename Input>
struct GameInputReleaseWork {
  Input* input;
  HMODULE module;

  static DWORD WINAPI Run(void* context) {
    auto* work = static_cast<GameInputReleaseWork*>(context);
    const auto module = work->module;
    work->input->Release();
    delete work;
    // 原子卸载并退出，避免返回到已卸载的插件代码。
    FreeLibraryAndExitThread(module, 0);
  }
};

template <typename Input>
void ReleaseGameInputOffThread(Input* input) {
  if (!input) return;
  using Work = GameInputReleaseWork<Input>;
  HMODULE module = nullptr;
  if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                        reinterpret_cast<LPCWSTR>(&Work::Run), &module)) {
    auto* work = new (std::nothrow) Work{input, module};
    if (work) {
      const auto thread = CreateThread(nullptr, 0, &Work::Run, work, 0, nullptr);
      if (thread) {
        CloseHandle(thread);
        return;
      }
      delete work;
    }
    FreeLibrary(module);
  }
  // 系统资源不足时保留正确的引用释放，不能泄漏或遗留悬空调用。
  input->Release();
}
