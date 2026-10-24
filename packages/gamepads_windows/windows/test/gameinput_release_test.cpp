#include <windows.h>
#include <cstdio>

int main() {
  const auto library = LoadLibraryW(L"release_fixture.dll");
  if (!library)
    return 1;
  using Start = void (*)(HANDLE, HANDLE, LONG*);
  const auto start =
      reinterpret_cast<Start>(GetProcAddress(library, "StartRelease"));
  if (!start)
    return 2;
  const auto gate = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  const auto completed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  LONG calls = 0;
  const auto before = GetTickCount64();
  start(gate, completed, &calls);
  if (GetTickCount64() - before > 500 ||
      WaitForSingleObject(completed, 0) != WAIT_TIMEOUT)
    return 3;
  FreeLibrary(library);
  // 插件宿主已卸载；工作线程仍持有代码模块，防止释放时执行已卸载代码。
  if (!GetModuleHandleW(L"release_fixture.dll"))
    return 4;
  SetEvent(gate);
  if (WaitForSingleObject(completed, 3000) != WAIT_OBJECT_0 || calls != 1)
    return 5;
  const auto until = GetTickCount64() + 3000;
  while (GetModuleHandleW(L"release_fixture.dll") && GetTickCount64() < until)
    Sleep(10);
  if (GetModuleHandleW(L"release_fixture.dll"))
    return 6;
  CloseHandle(completed);
  CloseHandle(gate);
  std::puts(
      "slow release returned promptly; reference released once; module "
      "retained then unloaded");
  return 0;
}
