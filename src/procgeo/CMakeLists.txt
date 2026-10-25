add_library(OpenRepublicProcGeo STATIC mesh.cpp vehicles.cpp props.cpp buildings.cpp)
target_include_directories(OpenRepublicProcGeo PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(OpenRepublicProcGeo PUBLIC cxx_std_17)
