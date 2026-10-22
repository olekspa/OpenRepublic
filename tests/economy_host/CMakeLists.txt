add_executable(OpenRepublicEconomyHostTests economy_host_tests.cpp)
target_link_libraries(OpenRepublicEconomyHostTests PRIVATE OpenRepublicEconomyProtocol)
target_compile_features(OpenRepublicEconomyHostTests PRIVATE cxx_std_17)
target_include_directories(OpenRepublicEconomyHostTests PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
add_test(NAME OpenRepublicEconomyHostTests COMMAND OpenRepublicEconomyHostTests)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME OpenRepublicEconomyHostExternalController
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/economy_controller.py"
            "$<TARGET_FILE:OpenRepublicEconomyHost>")
    set_tests_properties(OpenRepublicEconomyHostExternalController PROPERTIES TIMEOUT 30)
endif()
