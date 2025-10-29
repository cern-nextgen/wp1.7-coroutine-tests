# Coroutine perf tests

add_executable(counters_benchmark counters_benchmark.cpp)
target_link_libraries(counters_benchmark PRIVATE CoroutineTests)
