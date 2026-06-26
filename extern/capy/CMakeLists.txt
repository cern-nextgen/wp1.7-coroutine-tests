cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building capy as part of the CoroutineTests project")

# Declare where to get stdexec from.
set(COROUTINETESTS_CAPY_SOURCE
    "URL;https://github.com/cppalliance/capy/archive/9144290189fa149b27617c7d9a476c8fbffb7b8c.zip;URL_MD5;3f537c7e80ea1954a1c322cb3d2ce5fd"
    CACHE STRING "Source for capy, when built as part of this project")

mark_as_advanced(COROUTINETESTS_CAPY_SOURCE)
FetchContent_Declare(capy SYSTEM ${COROUTINETESTS_CAPY_SOURCE})

# Turn off build tests, examples, and docs.
set(BOOST_CAPY_BUILD_TESTS FALSE)
set(BOOST_CAPY_BUILD_EXAMPLES FALSE)
set(BOOST_CAPY_BUILD_BENCH FALSE)
set(BOOST_CAPY_MRDOCS_BUILD FALSE)

# Get it into the current directory.
FetchContent_MakeAvailable(capy)
