cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building beman.task as part of the CoroutineTests project")

# Declare where to get beman.task from.
set(COROUTINETESTS_BEMAN_TASK_SOURCE
    "URL;https://github.com/bemanproject/task/archive/74aa21016d2fffe7acaebadeb0957e8b736a793d.zip;URL_MD5;400fac9112c2d69550bed9baa68cc3b6"
    CACHE STRING "Source for beman.task, when built as part of this project")

mark_as_advanced(COROUTINETESTS_BEMAN_TASK_SOURCE)
FetchContent_Declare(beman.task SYSTEM ${COROUTINETESTS_BEMAN_TASK_SOURCE})

# Turn off build tests, examples.
set(BEMAN_TASK_BUILD_EXAMPLES FALSE)
set(BEMAN_TASK_BUILD_TESTS FALSE)

# Get it into the current directory.
FetchContent_MakeAvailable(beman.task)
