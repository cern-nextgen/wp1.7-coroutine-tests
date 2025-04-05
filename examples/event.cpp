#include "CoroutineTests/event.hpp"

#include <iostream>

#include "CoroutineTests/nestabletask.hpp"

CoroutineTests::NestableTask example_task(SimpleEvent& event) {
    std::cout << "Task resumed 0\n";
    co_await event;
    std::cout << "Task resumed 1\n";
}

CoroutineTests::NestableTask another_task(SimpleEvent& event) {
    std::cout << "Another task resumed 0\n";
    co_await event;
    std::cout << "Another task resumed 1\n";
    co_await event;
    std::cout << "Another task resumed 2\n";
}

int main() {
    std::cout << "Event example:\n";
    SimpleEvent event;
    auto task = example_task(event);
    auto another = another_task(event);
    task.resume();
    another.resume();
    std::cout << "Setting event\n";
    event.set();
    return 0;
}
