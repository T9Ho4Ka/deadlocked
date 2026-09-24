cmake_minimum_required(VERSION 3.24)
project(deadlocked_native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

include(cmake/dependencies.cmake)

find_package(OpenSSL REQUIRED)

# what this build calls itself, and whose releases it compares against. the rust client
# hardcodes the upstream repository; a fork publishes no releases, so pointing this at one
# would answer every check with a 404.
set(DL_VERSION "1.3.0" CACHE STRING "version this build reports")
set(DL_UPDATE_REPO "avitran0/deadlocked" CACHE STRING "repository the update check asks about")

add_executable(deadlocked-native
    src/main.cpp
    src/app.cpp
    src/ui/sidebar.cpp
    src/ui/tabs.cpp
    src/ui/widgets.cpp
)

target_include_directories(deadlocked-native PRIVATE src)
# everything shared comes through the library, so the two cannot be built with
# different flags, which is exactly how the overlay lost its x11 handling once
target_link_libraries(deadlocked-native PRIVATE deadlocked-core)
target_compile_options(deadlocked-native PRIVATE -Wall -Wextra -Wpedantic)

# tests: no framework, each one is a plain executable that prints its checks
enable_testing()

add_library(deadlocked-core STATIC
    src/ui/theme.cpp
    src/config/config.cpp
    src/config/keycode.cpp
    src/os/process.cpp
    src/os/mouse.cpp
    src/cs2/schema.cpp
    src/cs2/offsets.cpp
    src/cs2/entity.cpp
    src/cs2/game.cpp
    src/cs2/snapshot.cpp
    src/cs2/input.cpp
    src/cs2/features.cpp
    src/cs2/physics.cpp
    src/geometry/bvh.cpp
    src/overlay/window.cpp
    src/overlay/esp.cpp
    src/overlay/trails.cpp
    src/overlay/sounds.cpp
    src/overlay/gl.cpp
    src/overlay/model.cpp
    src/game_math.cpp
    src/net/radar_frame.cpp
    src/net/radar.cpp
    src/net/update.cpp
    src/net/json.cpp
)
target_include_directories(deadlocked-core PUBLIC src)
target_compile_definitions(deadlocked-core PUBLIC
    DL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../cheat/assets"
    DL_VERSION="${DL_VERSION}"
    DL_UPDATE_REPO="${DL_UPDATE_REPO}")
target_link_libraries(deadlocked-core PUBLIC imgui tomlplusplus::tomlplusplus glm::glm cgltf
    OpenSSL::SSL OpenSSL::Crypto)
if(X11_FOUND)
    target_link_libraries(deadlocked-core PUBLIC PkgConfig::X11)
    target_compile_definitions(deadlocked-core PUBLIC DL_HAVE_X11)
endif()
target_compile_options(deadlocked-core PRIVATE -Wall -Wextra -Wpedantic)

foreach(test_name config process cs2 math bvh postcard relay)
    add_executable(${test_name}_test tests/${test_name}_test.cpp)
    if(test_name STREQUAL "relay")
        target_sources(${test_name}_test PRIVATE src/relay/state.cpp src/relay/websocket.cpp)
    endif()
    target_include_directories(${test_name}_test PRIVATE tests)
    target_link_libraries(${test_name}_test PRIVATE deadlocked-core)
    target_compile_options(${test_name}_test PRIVATE -Wall -Wextra)
    add_test(NAME ${test_name} COMMAND ${test_name}_test)
endforeach()

# the process test looks its own symbols up through the dynamic symbol table
target_link_options(process_test PRIVATE -rdynamic)

foreach(unused_guard 0)
endforeach()

# diagnostic harness: attaches to a running cs2 and reports what the port can see.
# read only, and the only way to exercise the cs2 layer at all.
add_executable(deadlocked-probe tools/probe.cpp)
target_link_libraries(deadlocked-probe PRIVATE deadlocked-core)
target_compile_options(deadlocked-probe PRIVATE -Wall -Wextra)

# the relay: a standalone server, sharing the wire format and the snapshot with the client
add_executable(deadlocked-relay
    src/relay/main.cpp
    src/relay/server.cpp
    src/relay/state.cpp
    src/relay/websocket.cpp
)
target_link_libraries(deadlocked-relay PRIVATE deadlocked-core)
target_compile_options(deadlocked-relay PRIVATE -Wall -Wextra -Wpedantic)
