cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building beman.task as part of the CoroutineTests project")

# Declare where to get beman.task from.
set(COROUTINETESTS_BEMAN_TASK_SOURCE
    "URL;https://github.com/bemanproject/task/archive/a34fe8edbae1509f30c036166e7468c6f3c3693e.zip;URL_MD5;ab5d2f3f28c8346a7f2d0b2ff9276354"
    CACHE STRING "Source for beman.task, when built as part of this project")

mark_as_advanced(COROUTINETESTS_BEMAN_TASK_SOURCE)
FetchContent_Declare(beman.task SYSTEM ${COROUTINETESTS_BEMAN_TASK_SOURCE})

# Turn off build tests, examples.
set(BEMAN_TASK_BUILD_EXAMPLES FALSE)
set(BEMAN_TASK_BUILD_TESTS FALSE)

# Get it into the current directory.
FetchContent_MakeAvailable(beman.task)
