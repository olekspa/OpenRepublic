add_library(OpenRepublicEconomy economy.cpp checkpoint.cpp calendar.cpp transit.cpp)
target_include_directories(OpenRepublicEconomy PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(OpenRepublicEconomy PUBLIC cxx_std_17)

