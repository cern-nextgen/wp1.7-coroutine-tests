cmake_minimum_required(VERSION 3.25)
project(CoroutineTests)

set(CMAKE_CXX_STANDARD 20 CACHE STRING "")
set(CMAKE_CXX_EXTENSIONS FALSE CACHE BOOL "Disable C++ extensions" )
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

### beman.execution  options

option(BUILD_BEMAN_EXECUTION "Build the beman.execution sources included in CoroutineTests" TRUE)

if (USE_SYSTEM_LIBS)
  set(USE_SYSTEM_BEMAN_EXECUTION_DEFAULT ON)
else()
  set(USE_SYSTEM_BEMAN_EXECUTION_DEFAULT OFF)
endif()
option(USE_SYSTEM_BEMAN_EXECUTION
  "Pick up an existing installation of beman.execution from the build environment"
  ${USE_SYSTEM_BEMAN_EXECUTION_DEFAULT})
unset(USE_SYSTEM_BEMAN_EXECUTION_DEFAULT)

if(BUILD_BEMAN_EXECUTION)
  if(CMAKE_CXX_STANDARD LESS 23)
    message(FATAL_ERROR "Unsupported C++ standard: ${CMAKE_CXX_STANDARD}. Must be at least C++23 for Beman.execution.")
  endif()
  if(USE_SYSTEM_BEMAN_EXECUTION)
    find_package(beman_execution REQUIRED)
  else()
    add_subdirectory(extern/beman_execution)
  endif()
endif()

### tbb options

option(BUILD_TBB "Build the tbb sources included in CoroutineTests" TRUE)

if (USE_SYSTEM_LIBS)
  set(USE_SYSTEM_TBB_DEFAULT ON)
else()
  set(USE_SYSTEM_TBB_DEFAULT OFF)
endif()
option(USE_SYSTEM_TBB
  "Pick up an existing installation of tbb from the build environment"
  ${USE_SYSTEM_TBB_DEFAULT})
unset(USE_SYSTEM_TBB_DEFAULT)

if(BUILD_TBB)
  if(USE_SYSTEM_TBB)
    find_package(tbb REQUIRED)
  else()
    add_subdirectory(extern/tbb)
  endif()
endif()

### capy options

option(BUILD_CAPY "Build the capy sources included in CoroutineTests" TRUE)

if (USE_SYSTEM_LIBS)
  set(USE_SYSTEM_CAPY_DEFAULT ON)
else()
  set(USE_SYSTEM_CAPY_DEFAULT OFF)
endif()
option(USE_SYSTEM_CAPY
  "Pick up an existing installation of capy from the build environment"
  ${USE_SYSTEM_CAPY_DEFAULT})
unset(USE_SYSTEM_CAPY_DEFAULT)

if(BUILD_CAPY)
  if(USE_SYSTEM_CAPY)
    find_package(capy REQUIRED)
  else()
    add_subdirectory(extern/capy)
  endif()
endif()

### CUDA options

option(BUILD_CUDA "Build with CUDA support" OFF)
if(BUILD_CUDA)
  find_package(CUDAToolkit REQUIRED)
  enable_language(CUDA)
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
