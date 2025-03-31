#include "CoroutineTests/task.hpp"

#include <iostream>

CoroutineTests::Task example() {
    std::cout << "Task resumed 0\n";
    co_await std::suspend_always{};
    std::cout << "Task resumed 1\n";
    co_await std::suspend_always{};
    std::cout << "Task resumed 2\n";
    co_await std::suspend_always{};
    std::cout << "Task resumed 3\n";
}

int main() {
    std::cout << "Task example:\n";
    auto task = example();
    while (!task.done()) {
        task.resume();
    }
    std::cout << "Task completed\n";
    return 0;
}
