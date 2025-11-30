cmake_minimum_required(VERSION 3.25)
include(FetchContent)

# Tell the user what's happening.
message(STATUS "Building hacked.task as part of the CoroutineTests project")

# Declare where to get hacked.task from.
set(COROUTINETESTS_HACKED_TASK_SOURCE
    "URL;https://github.com/m-fila/beman_task/archive/dc720bd1ff29857776b977437713ff4a886e8517.zip;URL_MD5;84a92223cd54f8f08c9457a2d589f14b"
    CACHE STRING "Source for hacked.task, when built as part of this project")

mark_as_advanced(COROUTINETESTS_HACKED_TASK_SOURCE)
FetchContent_Declare(hacked.task SYSTEM ${COROUTINETESTS_HACKED_TASK_SOURCE})

# Turn off build tests, examples.
set(HACKED_TASK_BUILD_EXAMPLES FALSE)
set(HACKED_TASK_BUILD_TESTS FALSE)

# Get it into the current directory.
FetchContent_MakeAvailable(hacked.task)
