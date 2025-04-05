#include "CoroutineTests/async.hpp"

#include <iostream>
#include <stdexcept>
#include <thread>

#include "CoroutineTests/threadpool.hpp"

struct AsyncAPIMockup {
    int id;
    std::chrono::milliseconds delay;
    bool await_ready() const noexcept { return false; }
    void await_suspend(
        std::coroutine_handle<CoroutineTests::Async::promise_type> handle)
        const noexcept {
        std::thread([this, handle]() {
            std::this_thread::sleep_for(delay);
            std::cout << "AsyncMockup " << id << " done\n";
            handle.promise().reschedule();
        }).detach();
    }
    void await_resume() const noexcept {}
};

CoroutineTests::Async inner_task(int id) {
    std::cout << "Inner task " << id
              << " resumed 0, thread: " << std::this_thread::get_id() << '\n';
    co_await AsyncAPIMockup{id, std::chrono::milliseconds(2500)};
    std::cout << "Inner task " << id
              << " resumed 1, thread: " << std::this_thread::get_id() << '\n';
    co_await AsyncAPIMockup{id, std::chrono::milliseconds(1500)};
    std::cout << "Inner task " << id
              << " resumed 2, thread: " << std::this_thread::get_id() << '\n';
    co_return;
}

CoroutineTests::Async middle_task(int id) {
    std::cout << "Middle task " << id
              << " resumed 0, thread: " << std::this_thread::get_id() << '\n';
    co_await inner_task(id);
    std::cout << "Middle task " << id
              << " resumed 1, thread: " << std::this_thread::get_id() << '\n';
}

CoroutineTests::Async outer_task(int id) {
    std::cout << "Outer task " << id
              << " resumed 0, thread: " << std::this_thread::get_id() << '\n';
    co_await middle_task(id);
    std::cout << "Outer task " << id
              << " resumed 1, thread: " << std::this_thread::get_id() << '\n';
}

int main() {
    std::cout << "Async example:\n";
    auto threadpool = CoroutineTests::Threadpool(2);
    auto outer1 = outer_task(1);
    auto outer2 = outer_task(2);
    outer1.schedule_on(threadpool);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    outer2.schedule_on(threadpool);
    std::this_thread::sleep_for(std::chrono::milliseconds(5000));

    return 0;
}
