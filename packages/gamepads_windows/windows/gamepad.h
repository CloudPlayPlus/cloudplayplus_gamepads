#pragma once
#include <wtypes.h>

#include <windows.h>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <GameInput.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

struct GamepadData {
  std::string id;
  std::string name;
  int num_buttons;
  int vendor_id;
  int product_id;
};

struct Event {
  int time;
  std::string type;
  std::string key;
  double value;
};

class Gamepads {
 private:
  struct Reader {
    GamepadData data;
    IGameInputDevice* device = nullptr;
    std::atomic<bool> stopping{false};
    std::thread thread;
    ~Reader() {
      stopping = true;
      if (thread.joinable()) thread.join();
      if (device) device->Release();
    }
  };
  std::list<std::unique_ptr<Reader>> readers_;
  std::mutex mutex_;
  bool stopping_ = false;
  IGameInput* input_ = nullptr;

  GameInputCallbackToken deviceCallbackToken{};
  void read_gamepad(Reader* reader);

  void on_gamepad_connected(IGameInputDevice* device);
  void on_gamepad_disconnected(IGameInputDevice* device);

 public:
  std::optional<std::function<void(GamepadData* gamepad, const Event& event)>>
      event_emitter;
  void init();
  void stop();
  std::list<GamepadData> get_gamepads();
};

extern Gamepads gamepads;
