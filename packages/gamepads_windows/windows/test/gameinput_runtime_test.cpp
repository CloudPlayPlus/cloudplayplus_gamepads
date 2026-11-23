#include "../gameinput_runtime.h"

#include <cstdlib>

// 随应用分发的运行库：路径位于可执行文件目录，按序号 1 解析 GameInputCreate。
// 系统 GameInput.dll 是否存在取决于主机，优先级由代码顺序保证，此处不依赖它。
int main() {
  namespace runtime = gamepads_windows_gameinput;
  const auto path = runtime::BundledRuntimePath();
  if (path.size() < 20 ||
      path.compare(path.size() - 20, 20, L"\\GameInputRedist.dll") != 0)
    return 1;
  const auto create = runtime::LoadCreate(LoadLibraryExW(
      path.c_str(), nullptr,
      LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32));
  if (!create)
    return 2;
  IGameInput* input = nullptr;
  if (create(&input) != S_OK ||
      input != reinterpret_cast<IGameInput*>(static_cast<ULONG_PTR>(0x5A5A)))
    return 3;
  if (runtime::LoadCreate(nullptr))
    return 4;
  return 0;
}
