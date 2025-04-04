#include "CoroutineTests/nestabletask.hpp"

#include <chrono>
#include <iostream>
#include <thread>

struct AsyncMockup {
    std::chrono::milliseconds delay;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle) const noexcept {
        std::thread([delay = delay, handle]() {
            std::this_thread::sleep_for(delay);
            std::cout << "AsyncMockup done\n";
            handle.resume();
        }).detach();
    }
    void await_resume() const noexcept {}
};

CoroutineTests::NestableTask inner_task() {
    std::cout << "Inner task resumed 0\n";
    co_await AsyncMockup{std::chrono::milliseconds(2500)};
    std::cout << "Inner task resumed 1\n";
    co_await AsyncMockup{std::chrono::milliseconds(1500)};
    std::cout << "Inner task resumed 2\n";
    co_return;
}

CoroutineTests::NestableTask middle_task() {
    std::cout << "Middle task resumed 0\n";
    co_await inner_task();
    std::cout << "Middle task resumed 1\n";
}

CoroutineTests::NestableTask outer_task() {
    std::cout << "Outer task resumed 0\n";
    co_await middle_task();
    std::cout << "Outer task resumed 1\n";
}

int main() {
    std::cout << "Nested tasks example example:\n";
    auto outer = outer_task();
    outer.resume();
    while(!outer.done()) {
        std::cout << "."<<std::flush;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    std::cout << "Outer task done!\n";
    return 0;
}
