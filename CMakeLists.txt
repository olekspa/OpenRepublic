cmake_minimum_required(VERSION 3.19)

project(OpenRepublic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OPENREPUBLIC_BUILD_CLIENT "Build the Wicked Engine desktop client" ${WIN32})
option(OPENREPUBLIC_BUILD_TESTS "Build OpenRepublic simulation verification targets" ON)

# The authoritative simulation has no graphics or platform dependency.
add_subdirectory(src/sim)
add_subdirectory(src/agent_api)
add_subdirectory(src/content)

if(OPENREPUBLIC_BUILD_CLIENT)
    if(NOT WIN32)
        message(FATAL_ERROR "The Wicked Engine desktop client currently targets Windows; configure with -DOPENREPUBLIC_BUILD_CLIENT=OFF to build the headless simulation.")
    endif()

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

    # Lua-backed rules are optional and use Wicked Engine's bundled Lua target.
    if(TARGET LUA)
        add_subdirectory(src/scripting)
    endif()

    add_subdirectory(src/app)
endif()

if(OPENREPUBLIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/sim)
    add_subdirectory(tests/agent_api)
    add_subdirectory(tests/content)

    if(OPENREPUBLIC_BUILD_CLIENT AND TARGET LUA)
        add_subdirectory(tests/scripting)
    endif()
endif()
