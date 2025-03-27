cmake_minimum_required(VERSION 3.20)
project(CoroutineTests)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_CXX_STANDARD LESS 20)
  message(FATAL_ERROR "Unsupported C++ standard: ${CMAKE_CXX_STANDARD}. Must be at least C++20.")
endif()

add_library(CoroutineTests INTERFACE)
target_include_directories(
  CoroutineTests INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
                        $<INSTALL_INTERFACE:include/>)

option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()
