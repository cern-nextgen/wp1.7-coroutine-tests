#include <chrono>
#include <coroutine>
#include <iostream>
#include <stdexcept>
#include <string_view>

#include "CoroutineTests/alien/algorithm.hpp"
#include "CoroutineTests/alien/subtool.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"
#include "alien_timer.hpp"    // AsyncTimer
#include "logging_utils.hpp"  // log, format_name

using namespace CoroutineTests::alien;

// co_awaits a AsyncTimer
subtool::Task<subtool::StatusCode> subtool_execute(std::string_view parent) {
    const auto self = format_name(parent, "subtool");
    log(self) << "Calling async API in subtool" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(75),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in subtool: " << status << std::endl;
    log(self) << "Finishing subtool" << std::endl;
    co_return subtool::StatusCode::SUCCESS;
}

// throws
subtool::Task<subtool::StatusCode> throwing_subtool_execute(
    std::string_view parent) {
    const auto self = format_name(parent, "throwing_subtool");
    log(self) << "Calling async API in throwing_subtool" << std::endl;
    // simulate immediate failure without async work
    log(self) << "About to throw exception" << std::endl;
    throw std::runtime_error("throwing_subtool simulated failure");
    co_return subtool::StatusCode::FAILURE;
}

// co_awaits a AsyncTimer
tool::Task<tool::StatusCode> tool1_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool1");
    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(100),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in tool1: " << status << std::endl;
    log(self) << "Finishing tool1" << std::endl;
    co_return tool::StatusCode::FAILURE;
}

// co_awaits subtool_execute
tool::Task<tool::StatusCode> tool2_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool2");
    log(self) << "Calling async API in tool2" << std::endl;
    auto status1 = co_await AsyncTimer{std::chrono::milliseconds(10),
                                       StatusCode::SUCCESS, self};
    log(self) << "Result from async API in tool2: " << status1 << std::endl;
    log(self) << "Launching tool1" << std::endl;
    auto code = co_await tool1_execute(self);
    log(self) << "Result from tool1: " << code << std::endl;
    log(self) << "Calling async API in tool2" << std::endl;
    auto status2 = co_await AsyncTimer{std::chrono::milliseconds(10),
                                       StatusCode::FAILURE, self};
    log(self) << "Result from async API in tool2: " << status2 << std::endl;
    log(self) << "Finishing tool2" << std::endl;
    co_return tool::StatusCode::SUCCESS;
}

// tool3: co_awaits throwing_subtool_execute, catches expected exception &
// rethrows
tool::Task<tool::StatusCode> tool3_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool3");
    try {
        auto code = co_await throwing_subtool_execute(self);
        log(self) << "throwing_subtool returned (unexpected): " << code
                  << std::endl;
        log(self) << "Finishing tool3" << std::endl;
        co_return tool::StatusCode::FAILURE;
    } catch (const std::runtime_error& e) {
        log(self) << "Caught exception (expected): " << e.what()
                  << "; rethrowing" << std::endl;
        throw e;
    }
}

// tool4: co_awaits a AsyncTimer, but return void result
tool::Task<void> tool4_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool4");
    log(self) << "Calling async API in tool4" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(50),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in tool4: " << status << std::endl;
    if (status.status() != StatusCode::SUCCESS) {
        log(self) << "Async operation in tool4 failed, throwing exception"
                  << std::endl;
        throw std::runtime_error("tool4_async operation failed");
    }
    log(self) << "Finishing tool4" << std::endl;
    co_return;
}

// Task: co_awaits AsyncTimer then tool1_execute then tool2_execute then
// tool3_execute
algorithm::Task<algorithm::StatusCode> algorithm_execute(
    std::string_view parent) {
    const auto self = format_name(parent, "algorithm");
    log(self) << "Starting algorithm" << std::endl;
    log(self) << "Calling async API in algorithm" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(42),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in algorithm: " << status << std::endl;
    log(self) << "Launching tool1" << std::endl;
    auto code1 = co_await tool1_execute(self);
    log(self) << "Result from tool1: " << code1 << std::endl;
    log(self) << "Launching tool2" << std::endl;
    auto code2 = co_await tool2_execute(self);
    log(self) << "Result from tool2: " << code2 << std::endl;
    log(self) << "Launching tool3" << std::endl;
    try {
        co_await tool3_execute(self);
        log(self) << "Unreachable code" << std::endl;
    } catch (const std::runtime_error& e) {
        log(self) << "Caught exception  (expected) from tool3: " << e.what()
                  << std::endl;
    }
    log(self) << "Launching tool4" << std::endl;
    co_await tool4_execute(self);
    log(self) << "tool4 done" << std::endl;
    log(self) << "Finishing algorithm" << std::endl;
    co_return algorithm::StatusCode::SUCCESS;
}

int main() {
    log("main") << "Starting" << std::endl;
    CoroutineTests::Threadpool threadpool(1);
    auto scheduler = [&threadpool](std::coroutine_handle<> handle) {
        log("Scheduler") << "Reschedule called, enqueuing  resumption"
                         << std::endl;
        threadpool.enqueue_task(handle);
    };
    log("main") << "Launching algorithm..." << std::endl;
    auto t = algorithm_execute("main");
    auto future = t.schedule_on(scheduler);
    log("main") << "Waiting for algorithm completion..." << std::endl;
    auto status = future.get();
    log("main") << "Final status of algorithm " << status << "" << std::endl;
    return 0;
}
