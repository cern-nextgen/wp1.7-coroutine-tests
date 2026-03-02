cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building capy as part of the CoroutineTests project")

# Declare where to get stdexec from.
set(COROUTINETESTS_CAPY_SOURCE
    "URL;https://github.com/cppalliance/capy/archive/df92ffbb0670c06ebf0f201da73f06d1385d5d9b.zip;URL_MD5;d6b6e1e25684986d17ae92c13f992cfc"
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
