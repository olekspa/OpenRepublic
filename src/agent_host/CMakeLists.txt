add_library(OpenRepublicAgentProtocol agent_protocol.cpp)
target_include_directories(OpenRepublicAgentProtocol
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${PROJECT_SOURCE_DIR}/third_party/WickedEngine/Editor")
target_link_libraries(OpenRepublicAgentProtocol PUBLIC OpenRepublicAgentApi)
target_compile_features(OpenRepublicAgentProtocol PUBLIC cxx_std_17)

add_executable(OpenRepublicAgentHost main.cpp)
target_link_libraries(OpenRepublicAgentHost PRIVATE OpenRepublicAgentProtocol)
target_compile_features(OpenRepublicAgentHost PRIVATE cxx_std_17)
