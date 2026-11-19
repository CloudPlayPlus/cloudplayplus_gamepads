#include "connection_listener.h"

#include <dirent.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <cerrno>
#include <iostream>

#include "utils.h"

namespace connection_listener {
namespace {
const std::string input_dir = "/dev/input/";
}

int open_watch() {
  int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
  if (fd == -1) {
    std::cerr << "Error initializing inotify" << std::endl;
    return -1;
  }
  if (inotify_add_watch(fd, input_dir.c_str(),
                        IN_CREATE | IN_DELETE | IN_ATTRIB | IN_ONLYDIR) == -1) {
    close(fd);
    std::cerr << "Error adding watch for " << input_dir << std::endl;
    return -1;
  }
  return fd;
}

void list_existing(const EventConsumer& event_consumer) {
  DIR* dir = opendir(input_dir.c_str());
  if (dir == nullptr) {
    return;
  }

  while (const auto* entry = readdir(dir)) {
    if (entry->d_type == DT_CHR && starts_with(entry->d_name, "js")) {
      event_consumer(
          {ConnectionEventType::CONNECTED, input_dir + entry->d_name});
    }
  }
  closedir(dir);
}

bool read_events(int fd, const EventConsumer& event_consumer) {
  alignas(inotify_event) char buffer[4096];
  const ssize_t length = read(fd, buffer, sizeof(buffer));
  if (length < 0) {
    return errno == EAGAIN || errno == EINTR;
  }
  if (length == 0) {
    return false;
  }

  for (const char* pos = buffer; pos < buffer + length;) {
    const auto* event = reinterpret_cast<const inotify_event*>(pos);
    pos += sizeof(inotify_event) + event->len;
    if (event->mask & IN_IGNORED) {
      return false;
    }
    if (event->len == 0 || (event->mask & IN_ISDIR) ||
        !starts_with(event->name, "js")) {
      continue;
    }

    if (event->mask & IN_DELETE) {
      event_consumer(
          {ConnectionEventType::DISCONNECTED, input_dir + event->name});
    } else if (event->mask & (IN_CREATE | IN_ATTRIB)) {
      event_consumer({ConnectionEventType::CONNECTED, input_dir + event->name});
    }
  }
  return true;
}
}  // namespace connection_listener
