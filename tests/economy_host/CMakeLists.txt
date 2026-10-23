add_executable(OpenRepublicEconomyHostTests economy_host_tests.cpp)
target_link_libraries(OpenRepublicEconomyHostTests PRIVATE OpenRepublicEconomyProtocol)
target_compile_features(OpenRepublicEconomyHostTests PRIVATE cxx_std_17)
target_include_directories(OpenRepublicEconomyHostTests PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
add_test(NAME OpenRepublicEconomyHostTests COMMAND OpenRepublicEconomyHostTests)

add_executable(OpenRepublicHaulRetargetProtocolTests haul_retarget_protocol_tests.cpp)
target_link_libraries(OpenRepublicHaulRetargetProtocolTests PRIVATE OpenRepublicEconomyProtocol)
target_include_directories(OpenRepublicHaulRetargetProtocolTests PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
add_test(NAME EconomyHaulRetargetProtocol COMMAND OpenRepublicHaulRetargetProtocolTests)

add_executable(OpenRepublicHaulAdviserTests haul_adviser_tests.cpp)
target_link_libraries(OpenRepublicHaulAdviserTests PRIVATE OpenRepublicEconomyProtocol)
target_include_directories(OpenRepublicHaulAdviserTests PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
add_test(NAME EconomyHaulAdviser COMMAND OpenRepublicHaulAdviserTests)

add_executable(OpenRepublicCheckpointProtocolTests checkpoint_protocol_tests.cpp)
target_link_libraries(OpenRepublicCheckpointProtocolTests PRIVATE OpenRepublicEconomyProtocol)
target_include_directories(OpenRepublicCheckpointProtocolTests PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
add_test(NAME EconomyCheckpointProtocol COMMAND OpenRepublicCheckpointProtocolTests)

add_executable(OpenRepublicCommandHistoryProtocolTests command_history_protocol_tests.cpp)
target_link_libraries(OpenRepublicCommandHistoryProtocolTests PRIVATE OpenRepublicEconomyProtocol)
target_include_directories(OpenRepublicCommandHistoryProtocolTests PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
add_test(NAME EconomyCommandHistoryProtocol COMMAND OpenRepublicCommandHistoryProtocolTests)

add_executable(OpenRepublicUtilityProtocolTests utility_protocol_tests.cpp)
target_link_libraries(OpenRepublicUtilityProtocolTests PRIVATE OpenRepublicEconomyProtocol)
target_compile_features(OpenRepublicUtilityProtocolTests PRIVATE cxx_std_17)
target_include_directories(OpenRepublicUtilityProtocolTests PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
add_test(NAME EconomyUtilityProtocol COMMAND OpenRepublicUtilityProtocolTests)

add_executable(OpenRepublicUtilityDiagnosticsProtocolTests utility_diagnostics_protocol_tests.cpp)
target_link_libraries(OpenRepublicUtilityDiagnosticsProtocolTests PRIVATE OpenRepublicEconomyProtocol)
target_compile_features(OpenRepublicUtilityDiagnosticsProtocolTests PRIVATE cxx_std_17)
target_include_directories(OpenRepublicUtilityDiagnosticsProtocolTests PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
add_test(NAME EconomyUtilityDiagnosticsProtocol COMMAND OpenRepublicUtilityDiagnosticsProtocolTests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME EconomyStorageProfileExternalController
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/storage_profile_controller.py"
            "$<TARGET_FILE:OpenRepublicEconomyHost>")
    set_tests_properties(EconomyStorageProfileExternalController PROPERTIES TIMEOUT 30)
    add_test(NAME OpenRepublicEconomyHostExternalController
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/economy_controller.py"
            "$<TARGET_FILE:OpenRepublicEconomyHost>")
    set_tests_properties(OpenRepublicEconomyHostExternalController PROPERTIES TIMEOUT 30)
    add_test(NAME OpenRepublicAutopilotTests
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_republic_autopilot.py")
    add_test(NAME OpenRepublicReplanningExternalController
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/replanning_controller.py"
            "$<TARGET_FILE:OpenRepublicEconomyHost>")
    set_tests_properties(OpenRepublicReplanningExternalController PROPERTIES TIMEOUT 30)
    add_test(NAME EconomyCheckpointClient
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_economy_checkpoint.py")
    add_test(NAME EconomyCheckpointExternalController
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/checkpoint_controller.py"
            "$<TARGET_FILE:OpenRepublicEconomyHost>")
    set_tests_properties(EconomyCheckpointExternalController PROPERTIES TIMEOUT 30)
    add_test(NAME EconomyCommandHistoryExternalController
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/command_history_controller.py"
            "$<TARGET_FILE:OpenRepublicEconomyHost>")
    set_tests_properties(EconomyCommandHistoryExternalController PROPERTIES TIMEOUT 90)
    add_test(NAME EconomyUtilityExternalController
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/utility_controller.py"
            "$<TARGET_FILE:OpenRepublicEconomyHost>")
    set_tests_properties(EconomyUtilityExternalController PROPERTIES TIMEOUT 30)
    if(TARGET OpenRepublic)
        add_test(NAME EconomyDesktopCheckpointArguments
            COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/desktop_checkpoint_arguments.py"
                "$<TARGET_FILE:OpenRepublic>")
        set_tests_properties(EconomyDesktopCheckpointArguments PROPERTIES TIMEOUT 60)
    endif()
    add_test(NAME OpenRepublicScenarioScore
        COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/test_scenario_score.py"
            "$<TARGET_FILE:OpenRepublicEconomyHost>")
    set_tests_properties(OpenRepublicScenarioScore PROPERTIES TIMEOUT 30)
endif()
