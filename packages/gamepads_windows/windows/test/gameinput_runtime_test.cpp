#include "../gameinput_runtime.h"

#include <cstdlib>

namespace {
namespace runtime = gamepads_windows_gameinput;

struct FakeInput {
  int releases = 0;
  void Release() { ++releases; }
};
FakeInput inbox_input;
FakeInput bundled_input;
int bundled_loads = 0;

using FakeCreate = HRESULT (*)(FakeInput**);
HRESULT InboxOk(FakeInput** out) {
  *out = &inbox_input;
  return S_OK;
}
HRESULT InboxFails(FakeInput** out) {
  *out = nullptr;
  return E_FAIL;
}
HRESULT InboxNull(FakeInput** out) {
  *out = nullptr;
  return S_OK;
}
HRESULT InboxPartial(FakeInput** out) {
  // 失败但仍回填对象：调用方必须释放，不能泄漏。
  *out = &inbox_input;
  return E_FAIL;
}
HRESULT BundledOk(FakeInput** out) {
  *out = &bundled_input;
  return S_OK;
}
HRESULT BundledFails(FakeInput** out) {
  *out = nullptr;
  return E_NOINTERFACE;
}

int Run(FakeCreate inbox,
        FakeCreate bundled,
        FakeInput* expected,
        runtime::Source expected_source) {
  FakeInput* input = reinterpret_cast<FakeInput*>(1);
  runtime::Source source = runtime::Source::bundled;
  const HRESULT hr = runtime::CreateWithFallback(
      &input, [&] { return inbox; },
      [&] {
        ++bundled_loads;
        return bundled;
      },
      &source);
  if (input != expected || source != expected_source)
    return 1;
  if ((expected != nullptr) != SUCCEEDED(hr))
    return 2;
  return 0;
}
}  // namespace

int main() {
  // 系统运行库可用：直接使用，不加载附带运行库。
  if (Run(InboxOk, BundledOk, &inbox_input, runtime::Source::inbox) ||
      bundled_loads != 0)
    return 1;
  // 系统 DLL 缺失：使用附带运行库。
  if (Run(nullptr, BundledOk, &bundled_input, runtime::Source::bundled))
    return 2;
  // 系统 DLL 存在但创建失败（未安装外部运行库的壳）：回退。
  if (Run(InboxFails, BundledOk, &bundled_input, runtime::Source::bundled))
    return 3;
  if (Run(InboxNull, BundledOk, &bundled_input, runtime::Source::bundled))
    return 4;
  if (Run(InboxPartial, BundledOk, &bundled_input, runtime::Source::bundled) ||
      inbox_input.releases != 1)
    return 5;
  // 都不可用：报告不可用并返回失败。
  if (Run(InboxFails, BundledFails, nullptr, runtime::Source::none) ||
      Run(nullptr, nullptr, nullptr, runtime::Source::none))
    return 6;

  // 附带运行库位于可执行文件目录，按序号 1 解析 GameInputCreate。
  const auto path = runtime::BundledRuntimePath();
  if (path.size() < 20 ||
      path.compare(path.size() - 20, 20, L"\\GameInputRedist.dll") != 0)
    return 7;
  const auto create = runtime::LoadBundled();
  if (!create)
    return 8;
  IGameInput* input = nullptr;
  if (create(&input) != S_OK ||
      input != reinterpret_cast<IGameInput*>(static_cast<ULONG_PTR>(0x5A5A)))
    return 9;
  if (runtime::LoadCreate(nullptr))
    return 10;
  return 0;
}
