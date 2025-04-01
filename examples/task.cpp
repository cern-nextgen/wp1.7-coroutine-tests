#include "CoroutineTests/task.hpp"

#include <iostream>
#include <stdexcept>

CoroutineTests::Task example() {
    std::cout << "Task resumed 0\n";
    co_await std::suspend_always{};
    std::cout << "Task resumed 1\n";
    co_await std::suspend_always{};
    std::cout << "Task resumed 2\n";
    co_await std::suspend_always{};
    std::cout << "Task resumed 3\n";
}

CoroutineTests::Task throwing_example() {
    std::cout << "Throwing task resumed 0\n";
    co_await std::suspend_always{};
    std::cout << "Throwing task resumed 1\n";
    co_await std::suspend_always{};
    std::cout << "Throwing task resumed 2\n";
    throw std::runtime_error("Exception!");
    co_await std::suspend_always{};
    std::cout << "Throwing task resumed 3\n";
}

int main() {
    std::cout << "Task example:\n";
    auto task = example();
    while (!task.done()) {
        task.resume();
    }
    std::cout << "Task completed\n";
    auto another_task = throwing_example();
    while (!another_task.done()) {
        try {
            another_task.resume();
        } catch (const std::exception& e) {
            std::cout << "Caught exception: " << e.what() << '\n';
        }
    }
    std::cout << "Throwing task completed\n";
    return 0;
}
