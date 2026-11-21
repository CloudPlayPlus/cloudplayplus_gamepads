# The Flutter tooling requires that developers have a version of Visual Studio
# installed that includes CMake 3.14 or later. You should not increase this
# version, as doing so will cause the plugin to fail to compile for some
# customers of the plugin.
cmake_minimum_required(VERSION 3.14)

# Project-level configuration.
set(PROJECT_NAME "gamepads_windows")
project(${PROJECT_NAME} LANGUAGES CXX)

# This value is used when generating builds using this plugin, so it must
# not be changed
set(PLUGIN_NAME "gamepads_windows_plugin")

# Use the NuGet loader rather than the Windows SDK import library. Pin the
# headers and loader together so both use the same GameInput API definitions.
include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()
FetchContent_Declare(microsoft_gameinput
  URL https://api.nuget.org/v3-flatcontainer/microsoft.gameinput/4.1.314/microsoft.gameinput.4.1.314.nupkg
  URL_HASH SHA256=3c5d5700a210de289e9f02e2a1cb98848f32b981a5c22f7687a692f762a6cd26
  TLS_VERIFY TRUE
  TIMEOUT 60
)
FetchContent_MakeAvailable(microsoft_gameinput)
string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" gameinput_arch)
set(gameinput_lib
  "${microsoft_gameinput_SOURCE_DIR}/native/lib/${gameinput_arch}/GameInput.lib")
if(NOT EXISTS "${gameinput_lib}")
  message(FATAL_ERROR "No GameInput loader for ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}")
endif()

# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
  "gamepads_windows_plugin.cpp"
  "gamepads_windows_plugin.h"
  "gamepad.cpp"
  "gamepad.h"
  "gameinput_loader.cpp"
  "gameinput_release.h"
  "utils.cpp"
  "utils.h"
)

# Define the plugin library target. Its name must not be changed (see comment
# on PLUGIN_NAME above).
add_library(${PLUGIN_NAME} SHARED
  "include/gamepads_windows/gamepads_windows_plugin_c_api.h"
  "gamepads_windows_plugin_c_api.cpp"
  ${PLUGIN_SOURCES}
)

# Apply a standard set of build settings that are configured in the
# application-level CMakeLists.txt. This can be removed for plugins that want
# full control over build settings.
apply_standard_settings(${PLUGIN_NAME})
target_compile_options(${PLUGIN_NAME} PRIVATE /utf-8)

# Symbols are hidden by default to reduce the chance of accidental conflicts
# between plugins. This should not be removed; any symbols that should be
# exported should be explicitly exported with the FLUTTER_PLUGIN_EXPORT macro.
set_target_properties(${PLUGIN_NAME} PROPERTIES
  CXX_VISIBILITY_PRESET hidden)
target_compile_definitions(${PLUGIN_NAME} PRIVATE FLUTTER_PLUGIN_IMPL)

# Source include directories and library dependencies. Add any plugin-specific
# dependencies here.
target_include_directories(${PLUGIN_NAME} INTERFACE
  "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_include_directories(${PLUGIN_NAME} PRIVATE
  "${microsoft_gameinput_SOURCE_DIR}/native/include/v0")
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin "${gameinput_lib}")

# List of absolute paths to libraries that should be bundled with the plugin.
# This list could contain prebuilt libraries, or libraries created by an
# external build triggered from this build file.
set(gamepads_windows_bundled_libraries
  ""
  PARENT_SCOPE
)
