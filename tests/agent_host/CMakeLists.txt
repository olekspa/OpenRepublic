add_executable(OpenRepublicAgentProtocolTests agent_protocol_tests.cpp)
target_link_libraries(OpenRepublicAgentProtocolTests PRIVATE OpenRepublicAgentProtocol)
target_compile_features(OpenRepublicAgentProtocolTests PRIVATE cxx_std_17)
target_include_directories(OpenRepublicAgentProtocolTests PRIVATE
    "${PROJECT_SOURCE_DIR}/third_party/WickedEngine/Editor")
add_test(NAME OpenRepublicAgentProtocolTests COMMAND OpenRepublicAgentProtocolTests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME OpenRepublicAgentExternalController
        COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/examples/agent_api/drive_logistics.py"
            "$<TARGET_FILE:OpenRepublicAgentHost>")
    set_tests_properties(OpenRepublicAgentExternalController PROPERTIES TIMEOUT 30)
endif()
