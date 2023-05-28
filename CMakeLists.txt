cmake_minimum_required(VERSION 3.5)
project(juro LANGUAGES CXX VERSION 0.0.1)

Include(FetchContent)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_VERBOSE_MAKEFILE ON)

set(EXECUTABLE_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/dist")
set(LIBRARY_OUTPUT_PATH "${PROJECT_SOURCE_DIR}/dist")

FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.3.2 # or a later release
)

FetchContent_MakeAvailable(Catch2)

add_library(juro SHARED src/promise.cpp src/compose/all.cpp)
add_executable(test test/src/test.cpp)
target_link_libraries(test PRIVATE Catch2::Catch2WithMain)
target_link_libraries(test PRIVATE juro)
include_directories(test/include include)
include_directories(test/include test/include)

include(Catch)
catch_discover_tests(test)