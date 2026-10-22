add_library(OpenRepublicSceneControl STATIC scene_control.cpp)
target_include_directories(OpenRepublicSceneControl
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
target_compile_features(OpenRepublicSceneControl PUBLIC cxx_std_17)
