cmake_minimum_required(VERSION 3.10)
project(simple_crawler VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer a vendored toml++ in thirdparty/ if present, otherwise fetch it via FetchContent
if(EXISTS ${CMAKE_SOURCE_DIR}/thirdparty/tomlplusplus/CMakeLists.txt)
	message(STATUS "Using vendored tomlplusplus from thirdparty/tomlplusplus")
	add_subdirectory(${CMAKE_SOURCE_DIR}/thirdparty/tomlplusplus)
else()
	include(FetchContent)
	FetchContent_Declare(
		tomlplusplus
		GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
		GIT_TAG v3.4.0
	)
	FetchContent_MakeAvailable(tomlplusplus)
endif()

# If a top-level plugins/ directory exists, build plugins (example plugin)
if(EXISTS ${CMAKE_SOURCE_DIR}/plugins)
	file(GLOB PLUGIN_DIRS RELATIVE ${CMAKE_SOURCE_DIR}/plugins ${CMAKE_SOURCE_DIR}/plugins/*)
	foreach(d ${PLUGIN_DIRS})
		if(IS_DIRECTORY ${CMAKE_SOURCE_DIR}/plugins/${d} AND EXISTS ${CMAKE_SOURCE_DIR}/plugins/${d}/CMakeLists.txt)
			add_subdirectory(${CMAKE_SOURCE_DIR}/plugins/${d})
		endif()
	endforeach()
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(LibXml2 REQUIRED)

file(GLOB ALL_SRC src/*.cpp)
set(LIB_SOURCES ${ALL_SRC})
list(REMOVE_ITEM LIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)

add_executable(simple_crawler src/main.cpp ${LIB_SOURCES})
target_include_directories(simple_crawler PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(simple_crawler PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(simple_crawler PRIVATE ${LIBXML2_INCLUDE_DIR})
target_link_libraries(simple_crawler PRIVATE ${LIBXML2_LIBRARIES})
target_link_libraries(simple_crawler PRIVATE tomlplusplus::tomlplusplus)

# Example target (uses library sources, but different main)
add_executable(example_fetch examples/example.cpp ${LIB_SOURCES})
target_include_directories(example_fetch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(example_fetch PRIVATE Threads::Threads OpenSSL::SSL OpenSSL::Crypto)
target_include_directories(example_fetch PRIVATE ${LIBXML2_INCLUDE_DIR})
target_link_libraries(example_fetch PRIVATE ${LIBXML2_LIBRARIES})
target_link_libraries(example_fetch PRIVATE tomlplusplus::tomlplusplus)
