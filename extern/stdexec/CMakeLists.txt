cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building stdexec as part of the CoroutineTests project")

# Declare where to get stdexec from.
set(COROUTINETESTS_STDEXEC_SOURCE
    "URL;https://github.com/NVIDIA/stdexec/archive/refs/tags/nvhpc-25.09.tar.gz;URL_MD5;e385824bb560a3ac36ce859a2e3ded1f"
    CACHE STRING "Source for stdexec, when built as part of this project")

mark_as_advanced(COROUTINETESTS_STDEXEC_SOURCE)
FetchContent_Declare(stdexec SYSTEM ${COROUTINETESTS_STDEXEC_SOURCE})

# Turn off build tests, examples, and docs.
set(STDEXEC_BUILD_EXAMPLES FALSE)
set(STDEXEC_BUILD_TESTS FALSE)
set(STDEXEC_BUILD_DOCS FALSE)

# Get it into the current directory.
FetchContent_MakeAvailable(stdexec)
