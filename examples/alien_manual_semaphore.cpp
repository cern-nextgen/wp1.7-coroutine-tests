#include <chrono>
#include <coroutine>
#include <semaphore>
#include <stdexcept>
#include <string_view>

#include "CoroutineTests/alien/manual_algorithm.hpp"
#include "CoroutineTests/alien/subtool.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"
#include "alien_timer.hpp"    // AsyncTimer // AsyncTimer
#include "logging_utils.hpp"  // log, format_name

// co_awaits a AsyncTimer
CoroutineTests::alien::subtool::Task subtool(std::string_view parent) {
    const auto self = format_name(parent, "subtool");
    log(self) << "Calling async API in subtool" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(75),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in subtool: " << status << std::endl;
    log(self) << "Finishing subtool" << std::endl;
    co_return CoroutineTests::alien::subtool::StatusCode::SUCCESS;
}

// throws
CoroutineTests::alien::subtool::Task throwing_subtool(std::string_view parent) {
    const auto self = format_name(parent, "throwing_subtool");
    log(self) << "Calling async API in throwing_subtool" << std::endl;
    // simulate immediate failure without async work
    log(self) << "About to throw exception" << std::endl;
    throw std::runtime_error("throwing_subtool simulated failure");
    co_return CoroutineTests::alien::subtool::StatusCode::FAILURE;
}

// co_awaits a AsyncTimer
CoroutineTests::alien::tool::Task tool1(std::string_view parent) {
    const auto self = format_name(parent, "tool1");
    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(100),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in tool1: " << status << std::endl;
    log(self) << "Finishing tool1" << std::endl;
    co_return CoroutineTests::alien::tool::StatusCode::FAILURE;
}

// co_awaits subtool
CoroutineTests::alien::tool::Task tool2(std::string_view parent) {
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
CoroutineTests::alien::tool::Task tool3(std::string_view parent) {
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

// Task: co_awaits AsyncTimer then tool1 then tool2 then tool3
CoroutineTests::alien::manual_algorithm::Task algorithm(
    std::string_view parent) {
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
    co_return CoroutineTests::alien::manual_algorithm::StatusCode::SUCCESS;
}

int main(int argc, char** argv) {

    log() << "main Starting" << std::endl;

    auto use_threadpool = argc > 1 && std::string(argv[1]) == "--mt";
    std::unique_ptr<CoroutineTests::Threadpool> threadpool;
    if (use_threadpool) {
        threadpool = std::make_unique<CoroutineTests::Threadpool>(2);
        log() << "main Using threadpool with 2 threads" << std::endl;
    } else {
        log() << "main Running single-threaded" << std::endl;
    }

    // Start in ready state
    std::binary_semaphore sem(1);

    bool done = false;
    CoroutineTests::alien::manual_algorithm::StatusCode result;

    // Scheduler that releases semaphore to signal readiness to resume
    auto scheduler = [&](std::coroutine_handle<>) {
        log() << "scheduler Schedule called" << std::endl;
        sem.release();
    };

    auto t = algorithm("main");
    t.set_scheduler(scheduler);

    auto resume_logic = [&]() {
        try {
            auto res = t.resume();
            if (res.has_value()) {
                result = res.value();
                done = true;
                sem.release();  // wake main to exit loop
            }
        } catch (const std::exception& e) {
            log() << "Worker caught exception: " << e.what() << "" << std::endl;
            result =
                CoroutineTests::alien::manual_algorithm::StatusCode::FAILURE;
            done = true;
            sem.release();
        }
    };

    log() << "main Scheduling Task" << std::endl;

    // Loop enqueuing resume on threadpool until coroutine is done or exception
    // was thrown
    while (!done) {
        sem.acquire();  // Wait until coroutine needs to be resumed

        log() << "main Resuming coroutine on worker thread" << std::endl;
        if (use_threadpool) {
            threadpool->enqueue_task(resume_logic);
        } else {
            resume_logic();
        }
    }

    log() << "main Task finished with result: " << result << std::endl;
    return 0;
}
