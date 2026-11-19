#pragma once

#include <optional>
#include <string>

namespace gamepad {
struct GamepadInfo {
  std::string device_id;
  std::string name;
  int file_descriptor;
  int vendor_id;
  int product_id;
};

std::optional<GamepadInfo> get_gamepad_info(const std::string& device_id);
}  // namespace gamepad
