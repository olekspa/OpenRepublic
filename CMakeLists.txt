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

# Explicit read-only inspection target; not a default build or a CTest corpus run.
add_executable(OpenRepublicConstructionProfileProbe EXCLUDE_FROM_ALL tools/construction_profile_probe.cpp)
target_link_libraries(OpenRepublicConstructionProfileProbe PRIVATE OpenRepublicContent)
target_compile_features(OpenRepublicConstructionProfileProbe PRIVATE cxx_std_17)

add_subdirectory(src/economy)
add_subdirectory(src/procgeo)
add_subdirectory(src/runtime)
add_subdirectory(src/compat)
add_subdirectory(src/economy_api)

# The stdio Agent API adapter reuses the same MIT-licensed JSON header pinned
# with Wicked Engine's Editor importer. The authoritative API/simulation remain
# buildable without this optional transport dependency.
set(OPENREPUBLIC_AGENT_HOST_AVAILABLE FALSE)
if(NOT DEFINED OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR)
    set(_OPENREPUBLIC_DEFAULT_JSON_INCLUDE_DIR
        "${PROJECT_SOURCE_DIR}/third_party/WickedEngine/Editor")
    set(OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR "${_OPENREPUBLIC_DEFAULT_JSON_INCLUDE_DIR}" CACHE PATH
        "Directory containing the pinned Wicked Engine Editor/json.hpp header")
endif()
set(OPENREPUBLIC_WICKED_JSON_HEADER
    "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}/json.hpp")
if(EXISTS "${OPENREPUBLIC_WICKED_JSON_HEADER}")
    add_subdirectory(src/scenectl)
    set(OPENREPUBLIC_SCENECTL_AVAILABLE TRUE)
else()
    set(OPENREPUBLIC_SCENECTL_AVAILABLE FALSE)
    message(STATUS "OpenRepublicSceneControl disabled: nlohmann/json.hpp is unavailable")
endif()
set(OPENREPUBLIC_AGENT_JSON_HEADER
    "${PROJECT_SOURCE_DIR}/third_party/WickedEngine/Editor/json.hpp")
if(EXISTS "${OPENREPUBLIC_AGENT_JSON_HEADER}")
    add_subdirectory(src/agent_host)
    set(OPENREPUBLIC_AGENT_HOST_AVAILABLE TRUE)
else()
    message(STATUS "OpenRepublicAgentHost disabled: pinned nlohmann/json.hpp is unavailable")
endif()

# Lua rules only need the plain C Lua library; the headless build compiles it without the engine.
set(WICKED_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/WickedEngine")
if(NOT OPENREPUBLIC_BUILD_CLIENT AND EXISTS "${WICKED_ROOT_DIR}/WickedEngine/LUA/CMakeLists.txt")
    enable_language(C)
    set(WICKED_PIC OFF)
    add_subdirectory("${WICKED_ROOT_DIR}/WickedEngine/LUA" lua_headless EXCLUDE_FROM_ALL)
    add_subdirectory(src/scripting)
endif()
if(TARGET LUA OR EXISTS "${WICKED_ROOT_DIR}/WickedEngine/LUA/CMakeLists.txt")
    set(OPENREPUBLIC_LUA_AVAILABLE TRUE)
endif()

set(OPENREPUBLIC_ECONOMY_HOST_AVAILABLE FALSE)
if(EXISTS "${OPENREPUBLIC_WICKED_JSON_HEADER}")
    add_subdirectory(src/economy_host)
    set(OPENREPUBLIC_ECONOMY_HOST_AVAILABLE TRUE)
else()
    message(STATUS "OpenRepublicEconomyHost disabled: pinned nlohmann/json.hpp is unavailable")
endif()

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

    add_subdirectory(src/scripting)
    add_subdirectory(src/app)
endif()

if(OPENREPUBLIC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/sim)
    add_subdirectory(tests/agent_api)
    add_subdirectory(tests/content)
    add_subdirectory(tests/economy)
    add_subdirectory(tests/procgeo)
    add_subdirectory(tests/runtime)
    add_subdirectory(tests/compat)
    add_subdirectory(tests/app_geometry)
    add_subdirectory(tests/economy_api)

    if(OPENREPUBLIC_SCENECTL_AVAILABLE)
        add_subdirectory(tests/scenectl)
    endif()

    if(OPENREPUBLIC_AGENT_HOST_AVAILABLE)
        add_subdirectory(tests/agent_host)
    endif()

    if(OPENREPUBLIC_ECONOMY_HOST_AVAILABLE)
        add_subdirectory(tests/economy_host)
    endif()

    if(TARGET OpenRepublicScripting)
        add_subdirectory(tests/scripting)
    endif()
endif()
