add_library(OpenRepublicContent STATIC
    glb_container.cpp
    terrain_dds.cpp
)
target_include_directories(OpenRepublicContent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(OpenRepublicContent PUBLIC cxx_std_17)
