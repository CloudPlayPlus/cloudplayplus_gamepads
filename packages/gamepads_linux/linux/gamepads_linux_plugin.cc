#include "rumble.h"

#include "include/gamepads_linux/gamepads_linux_plugin.h"

#include <flutter_linux/flutter_linux.h>
#include <glib-unix.h>
#include <linux/joystick.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <utility>

#include "connection_listener.h"
#include "gamepad.h"

#define GAMEPADS_LINUX_PLUGIN(obj)                                     \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), gamepads_linux_plugin_get_type(), \
                              GamepadsLinuxPlugin))

namespace {
constexpr auto input_conditions =
    static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP | G_IO_NVAL);
constexpr int input_errors = G_IO_ERR | G_IO_HUP | G_IO_NVAL;

void stop_watching(guint& source, int& fd) {
  if (source != 0) {
    g_source_remove(source);
    source = 0;
  }
  if (fd != -1) {
    close(fd);
    fd = -1;
  }
}

struct GamepadWatch {
  GamepadWatch(GamepadsLinuxPlugin* plugin, gamepad::GamepadInfo info)
      : plugin(plugin), info(std::move(info)) {}

  ~GamepadWatch() { stop_watching(source, info.file_descriptor); }

  GamepadsLinuxPlugin* plugin;
  gamepad::GamepadInfo info;
  guint source = 0;
};
}  // namespace

struct _GamepadsLinuxPlugin {
  GObject parent_instance;
  GamepadRumble* rumble;
  FlMethodChannel* channel;
  std::map<std::string, std::unique_ptr<GamepadWatch>>* gamepads;
  guint hotplug_source;
  int hotplug_fd;
};

G_DEFINE_TYPE(GamepadsLinuxPlugin, gamepads_linux_plugin, g_object_get_type())

static const char* parse_event_type(const js_event& event) {
  switch (event.type & ~JS_EVENT_INIT) {
    case JS_EVENT_BUTTON: {
      return "button";
    }
    case JS_EVENT_AXIS: {
      return "analog";
    }
    default:
      return nullptr;
  }
}

static void emit_gamepad_event(FlMethodChannel* channel,
                               const gamepad::GamepadInfo& gamepad,
                               const js_event& event) {
  const char* type = parse_event_type(event);
  if (channel && type) {
    g_autoptr(FlValue) map = fl_value_new_map();
    fl_value_set_string(map, "gamepadId",
                        fl_value_new_string(gamepad.device_id.c_str()));
    fl_value_set_string(map, "time", fl_value_new_int(event.time));
    fl_value_set_string(map, "type", fl_value_new_string(type));
    fl_value_set_string(
        map, "key", fl_value_new_string(std::to_string(event.number).c_str()));
    fl_value_set_string(map, "value", fl_value_new_float(event.value));
    fl_value_set_string(map, "vendorId", fl_value_new_int(gamepad.vendor_id));
    fl_value_set_string(map, "productId", fl_value_new_int(gamepad.product_id));
    fl_method_channel_invoke_method(channel, "onGamepadEvent", map, nullptr,
                                    nullptr, nullptr);
  }
}

static void respond_not_found(FlMethodCall* method_call) {
  g_autoptr(FlMethodResponse) response =
      FL_METHOD_RESPONSE(fl_method_not_implemented_response_new());
  fl_method_call_respond(method_call, response, nullptr);
}

static void respond(FlMethodCall* method_call, FlValue* value) {
  g_autoptr(FlMethodResponse) response =
      FL_METHOD_RESPONSE(fl_method_success_response_new(value));
  fl_method_call_respond(method_call, response, nullptr);
}

static void gamepads_linux_plugin_handle_method_call(
    GamepadsLinuxPlugin* self,
    FlMethodCall* method_call) {
  const gchar* method = fl_method_call_get_name(method_call);

  if (strcmp(method, "rumble") == 0 || strcmp(method, "hasRumble") == 0 ||
      strcmp(method, "stopRumble") == 0) {
    auto* args = fl_method_call_get_args(method_call);
    bool accepted = false;
    if (args && fl_value_get_type(args) == FL_VALUE_TYPE_MAP) {
      auto* id = fl_value_lookup_string(args, "gamepadId");
      if (id && fl_value_get_type(id) == FL_VALUE_TYPE_STRING) {
        const std::string key = fl_value_get_string(id);
        if (strcmp(method, "hasRumble") == 0)
          accepted = self->rumble->Has(key);
        else if (strcmp(method, "stopRumble") == 0)
          accepted = self->rumble->Stop(key);
        else {
          auto* low = fl_value_lookup_string(args, "lowFrequency");
          auto* high = fl_value_lookup_string(args, "highFrequency");
          auto* duration = fl_value_lookup_string(args, "durationMillis");
          if (low && high && duration &&
              fl_value_get_type(low) == FL_VALUE_TYPE_FLOAT &&
              fl_value_get_type(high) == FL_VALUE_TYPE_FLOAT &&
              fl_value_get_type(duration) == FL_VALUE_TYPE_INT &&
              fl_value_get_int(duration) >= 0 &&
              fl_value_get_int(duration) <= 30000) {
            accepted = self->rumble->Set(
                key, fl_value_get_float(low), fl_value_get_float(high),
                static_cast<int>(fl_value_get_int(duration)));
          }
        }
      }
    }
    g_autoptr(FlValue) value = fl_value_new_bool(accepted);
    respond(method_call, value);
    return;
  }
  if (strcmp(method, "listGamepads") == 0) {
    g_autoptr(FlValue) list = fl_value_new_list();
    for (const auto& [device_id, watch] : *self->gamepads) {
      if (watch->source == 0) {
        continue;
      }
      g_autoptr(FlValue) map = fl_value_new_map();
      fl_value_set(map, fl_value_new_string("id"),
                   fl_value_new_string(device_id.c_str()));
      fl_value_set(map, fl_value_new_string("name"),
                   fl_value_new_string(watch->info.name.c_str()));
      fl_value_append(list, map);
    }
    respond(method_call, list);
  } else {
    respond_not_found(method_call);
  }
}

static void method_call_cb([[maybe_unused]] FlMethodChannel* flutter_channel,
                           FlMethodCall* method_call,
                           gpointer user_data) {
  GamepadsLinuxPlugin* plugin = GAMEPADS_LINUX_PLUGIN(user_data);
  gamepads_linux_plugin_handle_method_call(plugin, method_call);
}

static bool read_gamepad_events(GamepadWatch* watch, int fd) {
  // Bound each dispatch so a busy joystick leaves time for Flutter and GTK.
  for (int i = 0; i < 32; ++i) {
    js_event event;
    const ssize_t size = read(fd, &event, sizeof(event));
    if (size != sizeof(event)) {
      return size < 0 && (errno == EAGAIN || errno == EINTR);
    }
    emit_gamepad_event(watch->plugin->channel, watch->info, event);
  }
  return true;
}

static gboolean on_gamepad_ready(gint fd,
                                 GIOCondition conditions,
                                 gpointer user_data) {
  auto* watch = static_cast<GamepadWatch*>(user_data);
  if (!(conditions & input_errors) && read_gamepad_events(watch, fd)) {
    return G_SOURCE_CONTINUE;
  }
  watch->source = 0;
  stop_watching(watch->source, watch->info.file_descriptor);
  return G_SOURCE_REMOVE;
}

static void on_connection_event(
    GamepadsLinuxPlugin* self,
    const connection_listener::ConnectionEvent& event) {
  const auto& key = event.device_id;
  if (event.type == connection_listener::ConnectionEventType::DISCONNECTED) {
    if (self->gamepads->erase(key) != 0) {
      std::cout << "Gamepad disconnected " << key << std::endl;
    }
    return;
  }

  auto existing = self->gamepads->find(key);
  if (existing != self->gamepads->end() && existing->second->source != 0) {
    return;
  }
  auto info = gamepad::get_gamepad_info(key);
  if (!info) {
    return;
  }

  auto watch = std::make_unique<GamepadWatch>(self, std::move(*info));
  watch->source = g_unix_fd_add(watch->info.file_descriptor, input_conditions,
                                on_gamepad_ready, watch.get());
  std::cout << "Gamepad connected " << key << " - " << watch->info.name
            << std::endl;
  self->gamepads->insert_or_assign(key, std::move(watch));
}

static gboolean on_hotplug_ready(gint fd,
                                 GIOCondition conditions,
                                 gpointer user_data) {
  auto* self = GAMEPADS_LINUX_PLUGIN(user_data);
  if (!(conditions & input_errors) &&
      connection_listener::read_events(fd, [self](const auto& event) {
        on_connection_event(self, event);
      })) {
    return G_SOURCE_CONTINUE;
  }

  self->hotplug_source = 0;
  stop_watching(self->hotplug_source, self->hotplug_fd);
  return G_SOURCE_REMOVE;
}

static void gamepads_linux_plugin_dispose(GObject* object) {
  auto* self = GAMEPADS_LINUX_PLUGIN(object);
  stop_watching(self->hotplug_source, self->hotplug_fd);
  delete self->gamepads;
  self->gamepads = nullptr;
  delete self->rumble;
  self->rumble = nullptr;
  if (self->channel != nullptr) {
    g_object_remove_weak_pointer(G_OBJECT(self->channel),
                                 reinterpret_cast<gpointer*>(&self->channel));
    self->channel = nullptr;
  }
  G_OBJECT_CLASS(gamepads_linux_plugin_parent_class)->dispose(object);
}

static void gamepads_linux_plugin_class_init(GamepadsLinuxPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = gamepads_linux_plugin_dispose;
}

static void gamepads_linux_plugin_init(GamepadsLinuxPlugin* self) {
  self->rumble = new GamepadRumble();
  self->gamepads = new std::map<std::string, std::unique_ptr<GamepadWatch>>();
  self->hotplug_fd = -1;
}

void gamepads_linux_plugin_register_with_registrar(
    FlPluginRegistrar* registrar) {
  auto* plugin = GAMEPADS_LINUX_PLUGIN(
      g_object_new(gamepads_linux_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMethodCodec) codec = fl_standard_method_codec_new();
  g_autoptr(FlMethodChannel) channel =
      fl_method_channel_new(fl_plugin_registrar_get_messenger(registrar),
                            "xyz.luan/gamepads", FL_METHOD_CODEC(codec));
  plugin->channel = channel;
  g_object_add_weak_pointer(G_OBJECT(channel),
                            reinterpret_cast<gpointer*>(&plugin->channel));
  fl_method_channel_set_method_call_handler(
      channel, method_call_cb, g_object_ref(plugin), g_object_unref);

  // Keep hotplug, input, and method calls on the same GTK main loop.
  plugin->hotplug_fd = connection_listener::open_watch();
  if (plugin->hotplug_fd != -1) {
    plugin->hotplug_source = g_unix_fd_add(plugin->hotplug_fd, input_conditions,
                                           on_hotplug_ready, plugin);
  }
  connection_listener::list_existing(
      [plugin](const auto& event) { on_connection_event(plugin, event); });
  g_object_unref(plugin);
}
