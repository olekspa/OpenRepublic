add_executable(OpenRepublic WIN32
    main.cpp
    ContentPreview.cpp
    EconomyOverlay.cpp
    SyntheticRegionFixture.cpp
    EconomyOverlay.h
    ContentPreview.h
    LocalEconomyPipeEndpoint.cpp
    LocalEconomyPipeEndpoint.h
    MapShowcaseCapture.h
    NmfPreviewCapture.h
    OpenRepublicApplication.cpp
    OpenRepublicApplication.h
    "${WICKED_ROOT_DIR}/Editor/ModelImporter_GLTF.cpp"
)

target_include_directories(OpenRepublic PRIVATE
    "${WICKED_ROOT_DIR}"
    "${WICKED_ROOT_DIR}/Editor"
)

target_compile_definitions(OpenRepublic PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(OpenRepublic PRIVATE WickedEngine OpenRepublicSimulation OpenRepublicContent OpenRepublicAgentApi OpenRepublicRuntime OpenRepublicCompat OpenRepublicEconomyApi OpenRepublicEconomyProtocol shell32 winhttp)

if(TARGET OpenRepublicScripting)
    target_link_libraries(OpenRepublic PRIVATE OpenRepublicScripting)
    target_compile_definitions(OpenRepublic PRIVATE OPENREPUBLIC_WITH_LUA_RULES=1)
endif()

set_target_properties(OpenRepublic PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

add_dependencies(OpenRepublic Content)

add_custom_command(TARGET OpenRepublic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WICKED_ROOT_DIR}/WickedEngine/dxcompiler.dll"
        "$<TARGET_FILE_DIR:OpenRepublic>/dxcompiler.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${WICKED_ROOT_DIR}/Editor/startup.lua"
        "$<TARGET_FILE_DIR:OpenRepublic>/startup.lua"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:OpenRepublic>/data/rules"
    VERBATIM
)

if(TARGET OpenRepublicScripting)
    add_custom_command(TARGET OpenRepublic POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/data/rules/demo_rules.lua"
            "$<TARGET_FILE_DIR:OpenRepublic>/data/rules/demo_rules.lua"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${CMAKE_BINARY_DIR}/data/rules"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_SOURCE_DIR}/data/rules/demo_rules.lua"
            "${CMAKE_BINARY_DIR}/data/rules/demo_rules.lua"
        VERBATIM
    )
endif()
