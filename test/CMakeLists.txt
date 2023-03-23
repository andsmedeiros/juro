cmake_minimum_required(VERSION 3.5)

project(juro LANGUAGES CXX VERSION 0.0.1)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_VERBOSE_MAKEFILE ON)

find_package(Catch2 3 REQUIRED)
add_executable(test ../test/src/test.cpp)
target_link_libraries(test PRIVATE Catch2::Catch2WithMain)
include_directories(include ../include)
include_directories(include ../test/include)

include(Catch)
catch_discover_tests(test)