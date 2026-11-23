#include <windows.h>

// 替身运行库：与 GameInputRedist.dll 一样以序号 1 导出 GameInputCreate。
extern "C" HRESULT WINAPI FakeGameInputCreate(void** input) {
  *input = reinterpret_cast<void*>(static_cast<ULONG_PTR>(0x5A5A));
  return S_OK;
}
