#include "gamepad.h"

#include <fcntl.h>
#include <linux/joystick.h>
#include <unistd.h>

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>

static int read_sysfs_hex(const std::string& path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    return 0;
  }
  int value = 0;
  file >> std::hex >> value;
  return value;
}

static std::string extract_js_name(const std::string& device_id) {
  // Extract "js0" from "/dev/input/js0"
  auto pos = device_id.rfind('/');
  if (pos == std::string::npos) {
    return device_id;
  }
  return device_id.substr(pos + 1);
}

namespace gamepad {
std::optional<GamepadInfo> get_gamepad_info(const std::string& device_id) {
  std::cout << "Listening to gamepad " << device_id << std::endl;

  int file_descriptor = open(device_id.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
  if (file_descriptor == -1) {
    std::cerr << "Could not open joystick: " << file_descriptor << std::endl;
    return std::nullopt;
  }

  char name[128]{};
  if (ioctl(file_descriptor, JSIOCGNAME(sizeof(name)), name) < 0) {
    std::cerr << "Failed to get joystick name: " << strerror(errno)
              << std::endl;
    strcpy(name, "Unknown");
  }

  name[sizeof(name) - 1] = '\0';

  std::string js_name = extract_js_name(device_id);
  std::string sysfs_base = "/sys/class/input/" + js_name + "/device/id/";
  int vendor_id = read_sysfs_hex(sysfs_base + "vendor");
  int product_id = read_sysfs_hex(sysfs_base + "product");

  return {{device_id, name, file_descriptor, vendor_id, product_id}};
}
}  // namespace gamepad
