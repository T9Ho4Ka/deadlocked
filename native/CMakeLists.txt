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

add_executable(deadlocked-native
    src/main.cpp
    src/app.cpp
    src/ui/theme.cpp
    src/ui/sidebar.cpp
    src/ui/tabs.cpp
    src/ui/widgets.cpp
    src/config/config.cpp
    src/config/keycode.cpp
)

target_include_directories(deadlocked-native PRIVATE src)
# the rust client already ships the fonts, reuse them instead of duplicating assets
target_compile_definitions(deadlocked-native PRIVATE
    DL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../cheat/assets")
target_link_libraries(deadlocked-native PRIVATE imgui tomlplusplus::tomlplusplus)
target_compile_options(deadlocked-native PRIVATE -Wall -Wextra -Wpedantic)
