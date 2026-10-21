add_executable(OpenRepublicTerrainReaderTests
    terrain_reader_tests.cpp
)
target_link_libraries(OpenRepublicTerrainReaderTests PRIVATE OpenRepublicContent)
target_compile_features(OpenRepublicTerrainReaderTests PRIVATE cxx_std_17)
add_test(NAME OpenRepublicTerrainReaderTests COMMAND OpenRepublicTerrainReaderTests)

add_executable(OpenRepublicMtlSyntaxTests
    mtl_syntax_tests.cpp
)
target_link_libraries(OpenRepublicMtlSyntaxTests PRIVATE OpenRepublicContent)
target_compile_features(OpenRepublicMtlSyntaxTests PRIVATE cxx_std_17)
add_test(NAME OpenRepublicMtlSyntaxTests COMMAND OpenRepublicMtlSyntaxTests)

add_executable(OpenRepublicGlbContainerTests
    glb_container_tests.cpp
)
target_link_libraries(OpenRepublicGlbContainerTests PRIVATE OpenRepublicContent)
target_compile_features(OpenRepublicGlbContainerTests PRIVATE cxx_std_17)
add_test(NAME OpenRepublicGlbContainerTests COMMAND OpenRepublicGlbContainerTests)

if(OPENREPUBLIC_GLB_SEMANTIC_AVAILABLE)
    add_executable(OpenRepublicGlbSemanticTests
        glb_semantic_tests.cpp
    )
    target_link_libraries(OpenRepublicGlbSemanticTests PRIVATE OpenRepublicContent)
    target_compile_features(OpenRepublicGlbSemanticTests PRIVATE cxx_std_17)
    add_test(NAME OpenRepublicGlbSemanticTests COMMAND OpenRepublicGlbSemanticTests)
endif()
