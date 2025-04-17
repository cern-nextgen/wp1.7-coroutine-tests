# Coroutine perf tests

add_executable(simplecounter simplecounter.cpp Timer.cpp)
target_link_libraries(simplecounter PRIVATE CoroutineTests)

add_executable(silentpaircounter silentpaircounter.cpp Timer.cpp)
target_link_libraries(simplecounter PRIVATE CoroutineTests)

add_executable(semaphorecounter semaphorecounter.cpp Timer.cpp)
target_link_libraries(semaphorecounter PRIVATE CoroutineTests)