cmake_minimum_required(VERSION 3.19)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(OpenRepublicSimulation LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Also works when included by the main project after it has already added sim/.
if(NOT TARGET OpenRepublicSimulation)
    add_subdirectory(../../src/sim simulation)
endif()

add_executable(OpenRepublicSimulationTests simulation_tests.cpp)
target_link_libraries(OpenRepublicSimulationTests PRIVATE OpenRepublicSimulation)
enable_testing()
add_test(NAME OpenRepublicSimulationTests COMMAND OpenRepublicSimulationTests)
