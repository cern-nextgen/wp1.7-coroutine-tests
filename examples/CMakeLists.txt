# CoroutineTests examples

add_executable(generator generator.cpp)
target_link_libraries(generator PRIVATE CoroutineTests)
add_executable(lazy lazy.cpp)
target_link_libraries(lazy PRIVATE CoroutineTests)
add_executable(task task.cpp)
target_link_libraries(task PRIVATE CoroutineTests)
add_executable(datasource datasource.cpp)
target_link_libraries(datasource PRIVATE CoroutineTests)
add_executable(datasink datasink.cpp)
target_link_libraries(datasink PRIVATE CoroutineTests)
add_executable(nestabletask nestabletask.cpp)
target_link_libraries(nestabletask PRIVATE CoroutineTests)

add_executable(coroutines coroutines.cpp)
