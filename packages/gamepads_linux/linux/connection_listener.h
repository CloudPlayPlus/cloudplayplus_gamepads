#pragma once

#include <functional>
#include <string>

namespace connection_listener {
enum class ConnectionEventType {
  CONNECTED,
  DISCONNECTED,
};

struct ConnectionEvent {
  ConnectionEventType type;
  std::string device_id;
};

using EventConsumer = std::function<void(const ConnectionEvent&)>;

int open_watch();
void list_existing(const EventConsumer& event_consumer);
bool read_events(int fd, const EventConsumer& event_consumer);
}  // namespace connection_listener
