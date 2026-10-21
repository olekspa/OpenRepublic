cmake_minimum_required(VERSION 3.19)

project(OpenRepublic LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The initial OpenRepublic application prototype currently targets Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(WICKED_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/WickedEngine")
if(NOT EXISTS "${WICKED_ROOT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "Wicked Engine submodule is missing. Clone with --recurse-submodules or run git submodule update --init --recursive.")
endif()

# Build only the engine library needed by OpenRepublic. The upstream CMake
# project defines and builds the engine target without changing vendor files.
set(WICKED_EDITOR OFF CACHE BOOL "" FORCE)
set(WICKED_TESTS OFF CACHE BOOL "" FORCE)
set(WICKED_IMGUI_EXAMPLE OFF CACHE BOOL "" FORCE)
set(WICKED_WINDOWS_TEMPLATE OFF CACHE BOOL "" FORCE)
set(WICKED_ENABLE_SYMLINKS OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/WickedEngine EXCLUDE_FROM_ALL)
add_subdirectory(src/app)
