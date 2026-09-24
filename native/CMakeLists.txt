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
    src/os/process.cpp
    src/cs2/schema.cpp
    src/cs2/offsets.cpp
    src/cs2/entity.cpp
    src/cs2/game.cpp
)

target_include_directories(deadlocked-native PRIVATE src)
# the rust client already ships the fonts, reuse them instead of duplicating assets
target_compile_definitions(deadlocked-native PRIVATE
    DL_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../cheat/assets")
target_link_libraries(deadlocked-native PRIVATE imgui tomlplusplus::tomlplusplus glm::glm)
target_compile_options(deadlocked-native PRIVATE -Wall -Wextra -Wpedantic)

# tests: no framework, each one is a plain executable that prints its checks
enable_testing()

add_library(deadlocked-core STATIC
    src/ui/theme.cpp
    src/config/config.cpp
    src/config/keycode.cpp
    src/os/process.cpp
    src/cs2/schema.cpp
    src/cs2/offsets.cpp
    src/cs2/entity.cpp
    src/cs2/game.cpp
)
target_include_directories(deadlocked-core PUBLIC src)
target_link_libraries(deadlocked-core PUBLIC imgui tomlplusplus::tomlplusplus glm::glm)
target_compile_options(deadlocked-core PRIVATE -Wall -Wextra -Wpedantic)

foreach(test_name config process cs2)
    add_executable(${test_name}_test tests/${test_name}_test.cpp)
    target_include_directories(${test_name}_test PRIVATE tests)
    target_link_libraries(${test_name}_test PRIVATE deadlocked-core)
    target_compile_options(${test_name}_test PRIVATE -Wall -Wextra)
    add_test(NAME ${test_name} COMMAND ${test_name}_test)
endforeach()

# the process test looks its own symbols up through the dynamic symbol table
target_link_options(process_test PRIVATE -rdynamic)

foreach(unused_guard 0)
endforeach()
