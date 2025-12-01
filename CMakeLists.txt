cmake_minimum_required(VERSION 3.25)
project(CoroutineTests)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(CMAKE_CXX_STANDARD LESS 20)
  message(FATAL_ERROR "Unsupported C++ standard: ${CMAKE_CXX_STANDARD}. Must be at least C++20.")
endif()


## dependencies options

option(USE_SYSTEM_LIBS "Use system libraries by default" FALSE)

### stdexec options

option(BUILD_STDEXEC "Build the stdexec sources included in CoroutineTests" TRUE)

if (USE_SYSTEM_LIBS)
  set(USE_SYSTEM_STDEXEC_DEFAULT ON)
else()
  set(USE_SYSTEM_STDEXEC_DEFAULT OFF)
endif()
option(USE_SYSTEM_STDEXEC
  "Pick up an existing installation of stdexec from the build environment"
  ${USE_SYSTEM_STDEXEC_DEFAULT})
unset(USE_SYSTEM_STDEXEC_DEFAULT)

if(BUILD_STDEXEC)
  if(USE_SYSTEM_STDEXEC)
    find_package(stdexec REQUIRED)
  else()
    add_subdirectory(extern/stdexec)
  endif()
endif()

### beman.task  options

option(BUILD_BEMAN_TASK "Build the beman.task sources included in CoroutineTests" TRUE)

if (USE_SYSTEM_LIBS)
  set(USE_SYSTEM_BEMAN_TASK_DEFAULT ON)
else()
  set(USE_SYSTEM_BEMAN_TASK_DEFAULT OFF)
endif()
option(USE_SYSTEM_BEMAN_TASK
  "Pick up an existing installation of beman.task from the build environment"
  ${USE_SYSTEM_BEMAN_TASK_DEFAULT})
unset(USE_SYSTEM_BEMAN_TASK_DEFAULT)

if(BUILD_BEMAN_TASK)
  if(CMAKE_CXX_STANDARD LESS 23)
    message(FATAL_ERROR "Unsupported C++ standard: ${CMAKE_CXX_STANDARD}. Must be at least C++23 for Beman.task.")
  endif()
  if(USE_SYSTEM_BEMAN_TASK)
    find_package(beman_task REQUIRED)
  else()
    add_subdirectory(extern/beman_task)
  endif()
endif()

## targets

add_library(CoroutineTests INTERFACE)
target_include_directories(
  CoroutineTests INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
                        $<INSTALL_INTERFACE:include/>)
target_compile_options(CoroutineTests INTERFACE -Werror -Wall -Wextra -Wpedantic)

option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

option(BUILD_PERF "Build performance tests" ON)
if(BUILD_PERF)
  add_subdirectory(perf)
endif()
