cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building tbb as part of the CoroutineTests project")

# Declare where to get tbb from.
set(COROUTINETESTS_TBB_SOURCE
    "URL;https://github.com/uxlfoundation/oneTBB/archive/refs/tags/v2022.3.0.zip;URL_MD5;68dbd75b30096398fe72816085ca2031"
    CACHE STRING "Source for tbb, when built as part of this project")

mark_as_advanced(COROUTINETESTS_TBB_SOURCE)
FetchContent_Declare(tbb SYSTEM ${COROUTINETESTS_TBB_SOURCE})

# Options used in the build of TBB.
set( TBB_TEST FALSE CACHE BOOL "Turn off the TBB tests" )
set( TBB_STRICT FALSE CACHE BOOL "Do not throw errors on compiler warnings" )

# Make TBB work without warnings with modern CMake versions.
set( CMAKE_POLICY_VERSION_MINIMUM "3.10" )

# Get it into the current directory.
FetchContent_MakeAvailable(tbb)
