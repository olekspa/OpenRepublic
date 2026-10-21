add_executable(OpenRepublic WIN32
    main.cpp
    OpenRepublicApplication.cpp
    OpenRepublicApplication.h
)

target_compile_definitions(OpenRepublic PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(OpenRepublic PRIVATE WickedEngine)

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
    VERBATIM
)
