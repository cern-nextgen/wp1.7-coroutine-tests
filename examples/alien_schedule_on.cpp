#include <coroutine>
#include <functional>
#include <string_view>

#include "CoroutineTests/alien/schedule_on.hpp"
#include "CoroutineTests/alien/sync_wait.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"
#include "alien_timer.hpp"    // AsyncTimer
#include "logging_utils.hpp"  // log, format_name

using namespace CoroutineTests::alien;

tool::Task<void> inner_most(std::string_view parent) {
    const auto self = format_name(parent, "inner_most");
    log(self) << "Starting inner_most" << std::endl;
    log(self) << "Calling async API in inner_most" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(75),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in inner_most: " << status << std::endl;
    log(self) << "Finishing inner_most" << std::endl;
    co_return;
}

tool::Task<tool::StatusCode> inner(std::string_view parent) {
    const auto self = format_name(parent, "inner");
    log(self) << "Starting inner" << std::endl;
    co_await inner_most(self);
    log(self) << "Finishing inner" << std::endl;
    co_return tool::StatusCode::SUCCESS;
}

tool::Task<void> outer(
    std::function<void(std::coroutine_handle<>)> inner_scheduler,
    std::string_view parent) {
    const auto self = format_name(parent, "outer");
    log(self) << "Starting outer" << std::endl;
    auto status = co_await schedule_on(inner_scheduler, inner(self));
    log(self) << "Received status " << status << " from inner" << std::endl;
    log(self) << "Finishing outer" << std::endl;
    co_return;
}

tool::Task<tool::StatusCode> outer_most(
    std::function<void(std::coroutine_handle<>)> inner_scheduler,
    std::string_view parent) {
    const auto self = format_name(parent, "outer_most");
    log(self) << "Starting outer_most" << std::endl;
    co_await outer(inner_scheduler, self);
    log(self) << "Finishing outer_most" << std::endl;
    co_return tool::StatusCode::SUCCESS;
}

int main() {
    log() << "main Starting" << std::endl;
    CoroutineTests::Threadpool threadpool(1);
    auto scheduler = [&threadpool](std::coroutine_handle<> handle) {
        log() << "scheduler called, enqueuing work" << std::endl;
        threadpool.enqueue_task(handle);
    };

    CoroutineTests::Threadpool inner_threadpool(1);
    auto inner_scheduler = [&inner_threadpool](std::coroutine_handle<> handle) {
        log() << "inner_scheduler called, enqueuing work" << std::endl;
        inner_threadpool.enqueue_task(handle);
    };

    log() << "main Launching outer_most and waiting for completion..."
          << std::endl;
    auto status = CoroutineTests::alien::sync_wait(
        scheduler, outer_most(inner_scheduler, "main"));
    log() << "main Final status of outer_most " << status << "" << std::endl;
    return 0;
}
