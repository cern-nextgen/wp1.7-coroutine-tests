cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building beman.execution as part of the CoroutineTests project")

# Declare where to get beman.execution from.
set(COROUTINETESTS_BEMAN_EXECUTION_SOURCE
    "GIT_REPOSITORY;https://github.com/bemanproject/execution.git;GIT_TAG;894b080a088e0dc65893b4b6462aad615f8d0baf;GIT_SHALLOW;TRUE"
    CACHE STRING "Source for beman.execution, when built as part of this project")

mark_as_advanced(COROUTINETESTS_BEMAN_EXECUTION_SOURCE)
FetchContent_Declare(beman.execution SYSTEM ${COROUTINETESTS_BEMAN_EXECUTION_SOURCE})

# Turn off build tests, examples.
set(BEMAN_USE_MODULES FALSE)
set(BEMAN_EXECUTION_BUILD_EXAMPLES FALSE)
set(BEMAN_EXECUTION_BUILD_TESTS FALSE)

# Get it into the current directory.
FetchContent_MakeAvailable(beman.execution)
