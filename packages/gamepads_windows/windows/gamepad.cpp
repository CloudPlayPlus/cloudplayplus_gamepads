#include <algorithm>
#include <ppl.h>
#include <vector>
#include <concrt.h>
#include <winerror.h>

#include "gamepad.h"
#include "gameinput_release.h"
#include "utils.h"
#include <GameInput.h>
#include <iomanip>
#include <sstream>
#pragma comment(lib, "GameInput.lib")

Gamepads gamepads;

std::string get_button_name(uint32_t button) {
  switch (button) {
    case GameInputGamepadMenu:
      return "menu";
    case GameInputGamepadView:
      return "view";
    case GameInputGamepadA:
      return "a";
    case GameInputGamepadB:
      return "b";
    case GameInputGamepadX:
      return "x";
    case GameInputGamepadY:
      return "y";
    case GameInputGamepadDPadUp:
      return "dpadUp";
    case GameInputGamepadDPadDown:
      return "dpadDown";
    case GameInputGamepadDPadLeft:
      return "dpadLeft";
    case GameInputGamepadDPadRight:
      return "dpadRight";
    case GameInputGamepadLeftShoulder:
      return "leftShoulder";
    case GameInputGamepadRightShoulder:
      return "rightShoulder";
    case GameInputGamepadLeftThumbstick:
      return "leftThumbstick";
    case GameInputGamepadRightThumbstick:
      return "rightThumbstick";
  }
  return "button-" + std::to_string(button);
}

std::string AppLocalDeviceIdToString(const APP_LOCAL_DEVICE_ID& id) {
  std::ostringstream oss;
  oss << std::hex << std::setfill('0');
  for (size_t i = 0; i < APP_LOCAL_DEVICE_ID_SIZE; ++i) {
    oss << std::setw(2) << static_cast<int>(id.value[i]);
  }
  return oss.str();
}

std::list<Event> diff_states(const GameInputGamepadState& old,
                             const GameInputGamepadState& current) {
  std::time_t now = std::time(nullptr);
  int time = static_cast<int>(now);

  std::list<Event> events;
  if (old.leftThumbstickX != current.leftThumbstickX) {
    events.push_back(
        {time, "analog", "leftThumbstickX", current.leftThumbstickX});
  }
  if (old.leftThumbstickY != current.leftThumbstickY) {
    events.push_back(
        {time, "analog", "leftThumbstickY", current.leftThumbstickY});
  }
  if (old.rightThumbstickX != current.rightThumbstickX) {
    events.push_back(
        {time, "analog", "rightThumbstickX", current.rightThumbstickX});
  }
  if (old.rightThumbstickY != current.rightThumbstickY) {
    events.push_back(
        {time, "analog", "rightThumbstickY", current.rightThumbstickY});
  }
  if (old.leftTrigger != current.leftTrigger) {
    events.push_back({time, "analog", "leftTrigger", current.leftTrigger});
  }
  if (old.rightTrigger != current.rightTrigger) {
    events.push_back({time, "analog", "rightTrigger", current.rightTrigger});
  }
  if (old.buttons != current.buttons) {
    // While GameInputDeviceInfo.controllerButtonCount often gives 14,
    // if you install GameInput v3 redistributable, the reported
    // button count drops to zero. Button input is still reported.
    for (uint32_t i = 0; i < 14; ++i) {
      bool was_pressed = old.buttons & (1 << i);
      bool is_pressed = current.buttons & (1 << i);
      if (was_pressed != is_pressed) {
        double value = is_pressed ? 1.0 : 0.0;
        auto key = get_button_name(1 << i);
        events.push_back({time, "button", key, value});
      }
    }
  }
  return events;
}

bool are_states_different(const GameInputGamepadState& a,
                          const GameInputGamepadState& b) {
  return a.leftThumbstickX != b.leftThumbstickX ||
         a.leftThumbstickY != b.leftThumbstickY ||
         a.leftTrigger != b.leftTrigger ||
         a.rightThumbstickX != b.rightThumbstickX ||
         a.rightThumbstickY != b.rightThumbstickY ||
         a.rightTrigger != b.rightTrigger || a.buttons != b.buttons;
}

void Gamepads::init() {
  stopping_ = false;
  if (FAILED(GameInputCreate(&input_)) || !input_) return;
  const auto hr = input_->RegisterDeviceCallback(
      nullptr, GameInputKindGamepad, GameInputDeviceConnected,
      GameInputAsyncEnumeration, this,
      [](GameInputCallbackToken, void* context, IGameInputDevice* device,
         uint64_t, GameInputDeviceStatus status, GameInputDeviceStatus) {
        auto* self = static_cast<Gamepads*>(context);
        if (status & GameInputDeviceConnected)
          self->on_gamepad_connected(device);
        else
          self->on_gamepad_disconnected(device);
      }, &deviceCallbackToken);
  if (FAILED(hr)) stop();
}

void Gamepads::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
    for (auto& reader : readers_) reader->stopping = true;
  }
  // 回调可能仍在执行；必须等待结束，才能销毁 this 及读取线程。
  if (input_ && deviceCallbackToken) {
    input_->UnregisterCallback(deviceCallbackToken, UINT64_MAX);
    deviceCallbackToken = 0;
  }
  std::list<std::unique_ptr<Reader>> readers;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    readers.swap(readers_);
  }
  // Reader 析构先 join 再释放设备，不能在读取仍进行时释放输入对象。
  readers.clear();
  event_emitter.reset();
  auto* input = input_;
  input_ = nullptr;
  ReleaseGameInputOffThread(input);
}

std::list<GamepadData> Gamepads::get_gamepads() {
  std::lock_guard<std::mutex> lock(mutex_);
  std::list<GamepadData> result;
  for (const auto& reader : readers_) result.push_back(reader->data);
  return result;
}

void Gamepads::on_gamepad_connected(IGameInputDevice* device) {
  const auto* info = device->GetDeviceInfo();
  if (!info) return;
  std::lock_guard<std::mutex> lock(mutex_);
  if (stopping_) return;
  const auto id = AppLocalDeviceIdToString(info->deviceId);
  for (const auto& reader : readers_)
    if (reader->data.id == id) return;
  auto reader = std::make_unique<Reader>();
  reader->data = {id,
                 info->displayName && info->displayName->data ? info->displayName->data : "",
                 static_cast<int>(info->controllerButtonCount),
                 static_cast<int>(info->vendorId),
                 static_cast<int>(info->productId)};
  device->AddRef();
  reader->device = device;
  auto* raw = reader.get();
  // 先登记所有权；回调返回后仍持有设备引用，直到读取线程结束。
  readers_.push_back(std::move(reader));
  try {
    raw->thread = std::thread([this, raw] { read_gamepad(raw); });
  } catch (...) {
    readers_.pop_back();
  }
}

void Gamepads::on_gamepad_disconnected(IGameInputDevice* device) {
  const auto* info = device->GetDeviceInfo();
  if (!info) return;
  const auto id = AppLocalDeviceIdToString(info->deviceId);
  std::unique_ptr<Reader> removed;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = readers_.begin(); it != readers_.end(); ++it) {
      if ((*it)->data.id == id) {
        removed = std::move(*it);
        readers_.erase(it);
        break;
      }
    }
  }
  // 不持列表锁等待线程，查询手柄只返回自有值快照。
}

void Gamepads::read_gamepad(Reader* reader) {
  GameInputGamepadState previous_state{};
  while (!reader->stopping) {
    IGameInputReading* reading = nullptr;
    GameInputGamepadState state{};
    input_->GetCurrentReading(GameInputKindGamepad, reader->device, &reading);
    if (reading) {
      if (reading->GetGamepadState(&state) && !reader->stopping) {
        if (are_states_different(previous_state, state)) {
          for (const auto& event : diff_states(previous_state, state)) {
            if (event_emitter) (*event_emitter)(&reader->data, event);
          }
        }
        previous_state = state;
      }
      reading->Release();
    }
    Sleep(8);
  }
}
