add_executable(OpenRepublicRoadMeshTests
    road_mesh_tests.cpp
    "${PROJECT_SOURCE_DIR}/src/app/RoadMesh.cpp"
)
target_include_directories(OpenRepublicRoadMeshTests PRIVATE "${PROJECT_SOURCE_DIR}/src/app")
target_link_libraries(OpenRepublicRoadMeshTests PRIVATE OpenRepublicContent)
target_compile_features(OpenRepublicRoadMeshTests PRIVATE cxx_std_17)
add_test(NAME OpenRepublicRoadMeshTests COMMAND OpenRepublicRoadMeshTests)
