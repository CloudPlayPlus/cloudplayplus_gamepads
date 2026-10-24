#include <windows.h>
#include <GameInput.h>
#include <atomic>
#include <cstdlib>
#include <thread>

// 运行真实读取循环，以闸门模拟正在进行的读取和耗时的最终 Release。
static HANDLE reading_entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
static HANDLE reading_gate = CreateEventW(nullptr, TRUE, FALSE, nullptr);
static HANDLE unregister_entered = CreateEventW(nullptr, TRUE, FALSE, nullptr);
static HANDLE release_gate = CreateEventW(nullptr, TRUE, FALSE, nullptr);
static HANDLE released = CreateEventW(nullptr, TRUE, FALSE, nullptr);
static std::atomic<int> reads{0}, reading_refs{0}, input_releases{0};

struct TestReading {
  bool GetGamepadState(GameInputGamepadState*) { return false; }
  void Release() { --reading_refs; }
} reading;
struct TestDevice {
  GameInputDeviceInfo info{};
  std::atomic<int> refs{0};
  const GameInputDeviceInfo* GetDeviceInfo() { return &info; }
  void AddRef() { ++refs; }
  void Release() { --refs; }
};
struct TestInput {
  using Callback = void (*)(GameInputCallbackToken, void*, TestDevice*, uint64_t,
                            GameInputDeviceStatus, GameInputDeviceStatus);
  TestDevice device;
  Callback callback = nullptr;
  void* context = nullptr;
  HRESULT RegisterDeviceCallback(TestDevice*, GameInputKind, GameInputDeviceStatus,
      GameInputEnumerationKind, void* owner, Callback notify, GameInputCallbackToken* token) {
    context = owner; callback = notify; *token = 1;
    Notify(GameInputDeviceConnected);
    return S_OK;
  }
  void Notify(GameInputDeviceStatus status) {
    callback(1, context, &device, 0, status, GameInputDeviceNoStatus);
  }
  void UnregisterCallback(GameInputCallbackToken, uint64_t) { SetEvent(unregister_entered); }
  HRESULT GetCurrentReading(GameInputKind, TestDevice*, TestReading** result) {
    ++reads;
    SetEvent(reading_entered);
    WaitForSingleObject(reading_gate, INFINITE);
    ++reading_refs;
    *result = &reading;
    return S_OK;
  }
  void Release() {
    if (device.refs != 0 || reading_refs != 0) std::abort();
    ++input_releases;
    WaitForSingleObject(release_gate, INFINITE);
    SetEvent(released);
  }
} input;
static HRESULT TestCreate(TestInput** result) { *result = &input; return S_OK; }
#define IGameInput TestInput
#define IGameInputDevice TestDevice
#define IGameInputReading TestReading
#define GameInputCreate TestCreate
#include "../gamepad.cpp"
#undef GameInputCreate
#undef IGameInputReading
#undef IGameInputDevice
#undef IGameInput

int main() {
  Gamepads pads;
  pads.init();
  if (WaitForSingleObject(reading_entered, 2000) != WAIT_OBJECT_0) return 1;
  const auto snapshot = pads.get_gamepads();
  if (snapshot.size() != 1 || input.device.refs != 1) return 2;
  std::atomic<bool> stopped{false};
  std::thread stop([&] { pads.stop(); stopped = true; });
  if (WaitForSingleObject(unregister_entered, 2000) != WAIT_OBJECT_0) std::abort();
  if (stopped || input_releases != 0) std::abort();
  SetEvent(reading_gate);
  stop.join();
  if (!pads.get_gamepads().empty() || input.device.refs != 0 || reading_refs != 0) return 3;
  const auto until = GetTickCount64() + 2000;
  while (input_releases == 0 && GetTickCount64() < until) Sleep(1);
  if (input_releases != 1 || WaitForSingleObject(released, 0) != WAIT_TIMEOUT) return 4;
  pads.stop();  // 重复停止不能再次释放。
  SetEvent(release_gate);
  if (WaitForSingleObject(released, 2000) != WAIT_OBJECT_0 || input_releases != 1) return 5;
  if (snapshot.front().id.empty()) return 6;  // 断开后快照仍有效。

  // 设备拔出也须等待读取结束并释放设备；之后停止不会重复清理。
  ResetEvent(reading_entered);
  ResetEvent(released);
  pads.init();
  if (WaitForSingleObject(reading_entered, 2000) != WAIT_OBJECT_0) return 7;
  input.Notify(GameInputDeviceNoStatus);
  if (!pads.get_gamepads().empty() || input.device.refs != 0 || reading_refs != 0) return 8;
  pads.stop();
  if (WaitForSingleObject(released, 2000) != WAIT_OBJECT_0 || input_releases != 2) return 9;
  return 0;
}
