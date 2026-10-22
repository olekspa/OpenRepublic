add_library(OpenRepublicCompat STATIC
    catalog_import.cpp
)
target_include_directories(OpenRepublicCompat PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(OpenRepublicCompat PUBLIC OpenRepublicContent OpenRepublicEconomy)
target_compile_features(OpenRepublicCompat PUBLIC cxx_std_17)
