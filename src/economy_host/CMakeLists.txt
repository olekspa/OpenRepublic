add_library(OpenRepublicEconomyProtocol economy_protocol.cpp)
target_include_directories(OpenRepublicEconomyProtocol
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE "${OPENREPUBLIC_WICKED_JSON_INCLUDE_DIR}")
target_link_libraries(OpenRepublicEconomyProtocol PUBLIC OpenRepublicEconomyApi)
target_compile_features(OpenRepublicEconomyProtocol PUBLIC cxx_std_17)

add_executable(OpenRepublicEconomyHost main.cpp)
target_link_libraries(OpenRepublicEconomyHost PRIVATE OpenRepublicEconomyProtocol OpenRepublicCompat)
if(OPENREPUBLIC_LUA_AVAILABLE)
    target_link_libraries(OpenRepublicEconomyHost PRIVATE OpenRepublicScripting)
    target_compile_definitions(OpenRepublicEconomyHost PRIVATE OPENREPUBLIC_HAVE_LUA_RULES=1)
endif()
target_compile_features(OpenRepublicEconomyHost PRIVATE cxx_std_17)
