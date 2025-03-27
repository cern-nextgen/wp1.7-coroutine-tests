# CoroutineTests examples

add_executable(generator generator.cpp)
target_link_libraries(generator PRIVATE CoroutineTests)

add_executable(coroutines coroutines.cpp)
