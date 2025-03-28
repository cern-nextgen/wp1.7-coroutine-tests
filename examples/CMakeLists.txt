# CoroutineTests examples

add_executable(generator generator.cpp)
target_link_libraries(generator PRIVATE CoroutineTests)
add_executable(lazy lazy.cpp)
target_link_libraries(lazy PRIVATE CoroutineTests)

add_executable(coroutines coroutines.cpp)
