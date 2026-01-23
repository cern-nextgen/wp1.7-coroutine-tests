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

// co_awaits a AsyncTimer
CoroutineTests::alien::subtool::SubTool subtool(std::string_view parent) {
    const auto self = format_name(parent, "subtool");
    log(self) << "Calling async API in subtool" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(75),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in subtool: " << status << std::endl;
    log(self) << "Finishing subtool" << std::endl;
    co_return CoroutineTests::alien::subtool::StatusCode::SUCCESS;
}

// throws
CoroutineTests::alien::subtool::SubTool throwing_subtool(
    std::string_view parent) {
    const auto self = format_name(parent, "throwing_subtool");
    log(self) << "Calling async API in throwing_subtool" << std::endl;
    // simulate immediate failure without async work
    log(self) << "About to throw exception" << std::endl;
    throw std::runtime_error("throwing_subtool simulated failure");
    co_return CoroutineTests::alien::subtool::StatusCode::FAILURE;
}

// co_awaits a AsyncTimer
CoroutineTests::alien::tool::Tool tool1(std::string_view parent) {
    const auto self = format_name(parent, "tool1");
    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(100),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in tool1: " << status << std::endl;
    log(self) << "Finishing tool1" << std::endl;
    co_return CoroutineTests::alien::tool::StatusCode::FAILURE;
}

// co_awaits subtool
CoroutineTests::alien::tool::Tool tool2(std::string_view parent) {
    const auto self = format_name(parent, "tool2");
    log(self) << "Calling async API in tool2" << std::endl;
    auto status1 = co_await AsyncTimer{std::chrono::milliseconds(10),
                                       StatusCode::SUCCESS, self};
    log(self) << "Result from async API in tool2: " << status1 << std::endl;
    log(self) << "Launching tool1" << std::endl;
    auto code = co_await tool1(self);
    log(self) << "Result from tool1: " << code << std::endl;
    log(self) << "Calling async API in tool2" << std::endl;
    auto status2 = co_await AsyncTimer{std::chrono::milliseconds(10),
                                       StatusCode::FAILURE, self};
    log(self) << "Result from async API in tool2: " << status2 << std::endl;
    log(self) << "Finishing tool2" << std::endl;
    co_return CoroutineTests::alien::tool::StatusCode::SUCCESS;
}

// tool3: co_awaits throwing_subtool, catches expected exception & rethrows
CoroutineTests::alien::tool::Tool tool3(std::string_view parent) {
    const auto self = format_name(parent, "tool3");
    try {
        auto code = co_await throwing_subtool(self);
        log(self) << "throwing_subtool returned (unexpected): " << code
                  << std::endl;
        log(self) << "Finishing tool3" << std::endl;
        co_return CoroutineTests::alien::tool::StatusCode::FAILURE;
    } catch (const std::runtime_error& e) {
        log(self) << "Caught exception (expected): " << e.what()
                  << "; rethrowing" << std::endl;
        throw e;
    }
}

// Algorithm: co_awaits AsyncTimer then tool1 then tool2 then tool3
CoroutineTests::alien::algorithm::Algorithm algorithm(std::string_view parent) {
    const auto self = format_name(parent, "algorithm");
    log(self) << "Starting algorithm" << std::endl;
    log(self) << "Calling async API in algorithm" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(42),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in algorithm: " << status << std::endl;
    log(self) << "Launching tool1" << std::endl;
    auto code1 = co_await tool1(self);
    log(self) << "Result from tool1: " << code1 << std::endl;
    log(self) << "Launching tool2" << std::endl;
    auto code2 = co_await tool2(self);
    log(self) << "Result from tool2: " << code2 << std::endl;
    log(self) << "Launching tool3" << std::endl;
    try {
        co_await tool3(self);
        log(self) << "Unreachable code" << std::endl;
    } catch (const std::runtime_error& e) {
        log(self) << "Caught exception  (expected) from tool3: " << e.what()
                  << std::endl;
    }
    log(self) << "Finishing algorithm" << std::endl;
    co_return CoroutineTests::alien::algorithm::StatusCode::SUCCESS;
}

int main() {
    log() << "main Starting\n";
    CoroutineTests::Threadpool threadpool(1);
    auto scheduler = [&threadpool](std::coroutine_handle<> handle) {
        log() << "scheduler Reschedule called, enqueuing  resumption\n";
        threadpool.enqueue_task(handle);
    };
    log() << "main Launching algorithm...\n";
    auto t = algorithm("main");
    auto future = t.schedule_on(scheduler);
    log() << "main Waiting for algorithm completion...\n";
    auto status = future.get();
    log() << "main Final status of algorithm " << status << "\n";
    return 0;
}
