#include <tbb/task_arena.h>

#include <string_view>

#include "exec_backend.hpp"               // std exec backend selection
#include "exec_task_arena_scheduler.hpp"  // TaskArenaScheduler
#include "exec_timer.hpp"                 // TimerSender
#include "logging_utils.hpp"              // log, format_name

execution::task<void> inner(std::string_view parent) {
    const auto self = format_name(parent, "Inner");
    log(self) << "Starting Inner" << std::endl;
    log(self) << "Calling async API in Inner" << std::endl;
    auto result =
        co_await TimerSender{-1, std::chrono::milliseconds(100), self};
    log(self) << "Result from async API in Inner: " << result << std::endl;
    log(self) << "Finishing Inner" << std::endl;
    co_return;
}

execution::task<void> Middle(std::string_view parent) {
    const auto self = format_name(parent, "Middle");
    log(self) << "Starting Middle" << std::endl;
    co_await inner(self);
    log(self) << "Finishing Middle" << std::endl;
}

execution::task<void> Outer(std::string_view parent) {
    const auto self = format_name(parent, "Outer");
    log(self) << "Starting Outer" << std::endl;
    co_await Middle(self);
    log(self) << "Finishing Outer" << std::endl;
}

int main() {
    log("main") << "Starting" << std::endl;
    tbb::task_arena arena{1};
    TaskArenaContext context{arena};

    std::cout << "----------------------------------------" << std::endl;
    log("main") << "Coroutine execution" << std::endl;
    execution::sync_wait(execution::starts_on(
        TaskArenaScheduler{context, "coroutine", {0, 0}}, Outer("main")));

    std::cout << "----------------------------------------" << std::endl;
    log("main") << "Sender execution" << std::endl;

    auto scheduler = TaskArenaScheduler{context, "sender", {0, 0}};
    auto work =
        execution::just() | execution::then([]() {
            log("Left") << "Starting and Finishing" << std::endl;
        }) |
        execution::then([]() { log("Middle") << "Starting" << std::endl; }) |
        execution::let_value([]() {
            return TimerSender{-1, std::chrono::milliseconds(100), "Middle"};
        }) |
        execution::continues_on(scheduler) | execution::then([](auto&&) {
            log("Middle") << "Finishing" << std::endl;
        }) |
        execution::then(
            []() { log("Right") << "Starting and Finishing" << std::endl; });
    execution::sync_wait(execution::starts_on(scheduler, std::move(work)));

    log("main") << "Done" << std::endl;
    return 0;
}
