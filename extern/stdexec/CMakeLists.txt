cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building stdexec as part of the CoroutineTests project")

# Declare where to get stdexec from.
set(COROUTINETESTS_STDEXEC_SOURCE
    "URL;https://github.com/NVIDIA/stdexec/archive/refs/tags/nvhpc-26.05.tar.gz;URL_MD5;14d830d378be7cf687df97b1c0312c39"
    CACHE STRING "Source for stdexec, when built as part of this project")

mark_as_advanced(COROUTINETESTS_STDEXEC_SOURCE)
FetchContent_Declare(stdexec SYSTEM ${COROUTINETESTS_STDEXEC_SOURCE})

# Turn off build tests, examples, and docs.
set(STDEXEC_BUILD_EXAMPLES FALSE)
set(STDEXEC_BUILD_TESTS FALSE)
set(STDEXEC_BUILD_DOCS FALSE)
set(STDEXEC_ENABLE_TBB FALSE)
set(STDEXEC_ENABLE_TASKFLOW FALSE)

# Get it into the current directory.
FetchContent_MakeAvailable(stdexec)
