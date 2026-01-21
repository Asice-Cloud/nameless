cmake_minimum_required(VERSION 3.10)
project(jsonl_storage_plugin)

add_library(jsonl_storage_plugin SHARED jsonl_storage_plugin.cpp)
set_target_properties(jsonl_storage_plugin PROPERTIES PREFIX "" )
target_include_directories(jsonl_storage_plugin PRIVATE ${CMAKE_SOURCE_DIR}/include)
# place shared library in the build directory so runtime can load from build/
set_target_properties(jsonl_storage_plugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
