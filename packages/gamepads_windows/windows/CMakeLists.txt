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

# Any new source files that you add to the plugin should be added here.
list(APPEND PLUGIN_SOURCES
  "gamepads_windows_plugin.cpp"
  "gamepads_windows_plugin.h"
  "gamepad.cpp"
  "gamepad.h"
  "gameinput_release.h"
  "gameinput_runtime.h"
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
target_link_libraries(${PLUGIN_NAME} PRIVATE flutter flutter_wrapper_plugin)

# 不链接 GameInput.lib：运行时按序号解析系统 GameInput.dll，缺失时回退到随应用
# 分发的 GameInputRedist.dll（见 gameinput_runtime.h）。后者取自固定版本、固定哈希的
# Microsoft.GameInput NuGet 包；包与 DLL 哈希均校验，任一不符即配置失败。
set(GAMEINPUT_VERSION "4.1.314")
set(GAMEINPUT_NUPKG_SHA256
  "3c5d5700a210de289e9f02e2a1cb98848f32b981a5c22f7687a692f762a6cd26")
# Visual Studio 生成器以 CMAKE_VS_PLATFORM_NAME 表示实际目标平台（-A 可带附加字段）。
if(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64" OR
   (NOT CMAKE_VS_PLATFORM_NAME AND
    CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$"))
  set(GAMEINPUT_ARCH "arm64")
  set(GAMEINPUT_DLL_SHA256
    "9021ce3bc0f6c61ce7800f6940d82cfb826d27008f7710a894c69ac97085a8c4")
else()
  set(GAMEINPUT_ARCH "x64")
  set(GAMEINPUT_DLL_SHA256
    "15fcfb28879dd0bead760652405c82d39e5d6bc784ed9345621ae1fed0dc35b4")
endif()
set(GAMEINPUT_CACHE "${CMAKE_BINARY_DIR}/gameinput-${GAMEINPUT_VERSION}")
set(GAMEINPUT_NUPKG "${GAMEINPUT_CACHE}/microsoft.gameinput.${GAMEINPUT_VERSION}.nupkg")
set(GAMEINPUT_REDIST "${GAMEINPUT_CACHE}/${GAMEINPUT_ARCH}/GameInputRedist.dll")
if(NOT EXISTS "${GAMEINPUT_REDIST}")
  file(DOWNLOAD
    "https://api.nuget.org/v3-flatcontainer/microsoft.gameinput/${GAMEINPUT_VERSION}/microsoft.gameinput.${GAMEINPUT_VERSION}.nupkg"
    "${GAMEINPUT_NUPKG}"
    EXPECTED_HASH SHA256=${GAMEINPUT_NUPKG_SHA256}
    TLS_VERIFY ON
    STATUS GAMEINPUT_DOWNLOAD)
  list(GET GAMEINPUT_DOWNLOAD 0 GAMEINPUT_DOWNLOAD_CODE)
  if(NOT GAMEINPUT_DOWNLOAD_CODE EQUAL 0)
    message(FATAL_ERROR "Microsoft.GameInput ${GAMEINPUT_VERSION} download failed: ${GAMEINPUT_DOWNLOAD}")
  endif()
  set(GAMEINPUT_EXTRACT "${GAMEINPUT_CACHE}/extract")
  file(REMOVE_RECURSE "${GAMEINPUT_EXTRACT}")
  file(MAKE_DIRECTORY "${GAMEINPUT_EXTRACT}")
  # cmake -E tar 在 3.14 即支持 zip（nupkg）；file(ARCHIVE_EXTRACT) 需 3.18。
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar xf "${GAMEINPUT_NUPKG}"
      "native/bin/${GAMEINPUT_ARCH}/GameInputRedist.dll"
    WORKING_DIRECTORY "${GAMEINPUT_EXTRACT}"
    RESULT_VARIABLE GAMEINPUT_EXTRACT_RESULT)
  if(NOT GAMEINPUT_EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "Microsoft.GameInput ${GAMEINPUT_VERSION} extraction failed")
  endif()
  file(COPY "${GAMEINPUT_EXTRACT}/native/bin/${GAMEINPUT_ARCH}/GameInputRedist.dll"
    DESTINATION "${GAMEINPUT_CACHE}/${GAMEINPUT_ARCH}")
  file(REMOVE_RECURSE "${GAMEINPUT_EXTRACT}")
endif()
file(SHA256 "${GAMEINPUT_REDIST}" GAMEINPUT_ACTUAL_SHA256)
if(NOT GAMEINPUT_ACTUAL_SHA256 STREQUAL GAMEINPUT_DLL_SHA256)
  file(REMOVE "${GAMEINPUT_REDIST}")
  message(FATAL_ERROR "GameInputRedist.dll hash mismatch: ${GAMEINPUT_ACTUAL_SHA256}")
endif()

# List of absolute paths to libraries that should be bundled with the plugin.
# This list could contain prebuilt libraries, or libraries created by an
# external build triggered from this build file.
set(gamepads_windows_bundled_libraries
  "${GAMEINPUT_REDIST}"
  PARENT_SCOPE
)
