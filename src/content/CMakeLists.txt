add_library(OpenRepublicContent STATIC
    glb_container.cpp
    mtl_dependencies.cpp
    mtl_syntax.cpp
    nmf_geometry.cpp
    nmf_header.cpp
    terrain_dds.cpp
    terrain_preview.cpp
)
target_include_directories(OpenRepublicContent PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(OpenRepublicContent PUBLIC cxx_std_17)

# Reuse the JSON header already pinned with Wicked Engine's Editor GLTF importer
# for the optional semantic GLB profile. Headless/content-only builds remain
# available when the Wicked Engine submodule is absent; in that case this
# optional profile and its tests are not compiled.
set(OPENREPUBLIC_WICKED_JSON_HEADER
    "${PROJECT_SOURCE_DIR}/third_party/WickedEngine/Editor/json.hpp")
if(EXISTS "${OPENREPUBLIC_WICKED_JSON_HEADER}")
    target_sources(OpenRepublicContent PRIVATE glb_semantic.cpp)
    target_include_directories(OpenRepublicContent PRIVATE
        "${PROJECT_SOURCE_DIR}/third_party/WickedEngine/Editor")
    target_compile_definitions(OpenRepublicContent PUBLIC OPENREPUBLIC_HAS_GLB_SEMANTIC_VALIDATOR=1)
    set(OPENREPUBLIC_GLB_SEMANTIC_AVAILABLE TRUE PARENT_SCOPE)
else()
    message(STATUS "Pinned Wicked Editor JSON header not found; optional GLB semantic validation is disabled")
    set(OPENREPUBLIC_GLB_SEMANTIC_AVAILABLE FALSE PARENT_SCOPE)
endif()
