#include <chrono>
#include <coroutine>
#include <exception>
#include <string_view>

#include "CoroutineTests/alien/algorithm.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/alien/when_all.hpp"
#include "CoroutineTests/threadpool.hpp"
#include "alien_timer.hpp"    // AsyncTimer
#include "logging_utils.hpp"  // log, format_name

// co_await a AsyncTimer
CoroutineTests::alien::tool::Tool toolA(std::string_view parent) {
    const auto self = format_name(parent, "toolA");
    log(self) << "Starting toolA" << std::endl;
    log(self) << "Calling async API in toolA" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(80),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in toolA: " << status << std::endl;
    log(self) << "Finishing toolA" << std::endl;
    co_return CoroutineTests::alien::tool::StatusCode::SUCCESS;
}

// co_await a AsyncTimer
CoroutineTests::alien::tool::Tool toolB(std::string_view parent) {
    const auto self = format_name(parent, "toolB");
    log(self) << "Starting toolB" << std::endl;
    log(self) << "Calling async API in toolB" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(40),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from async API in toolB: " << status << std::endl;
    log(self) << "Finishing toolB" << std::endl;
    co_return CoroutineTests::alien::tool::StatusCode::FAILURE;
}

// co_await toolA and toolB in parallel via when_all
CoroutineTests::alien::algorithm::Algorithm algorithm(std::string_view parent) {
    const auto self = format_name(parent, "algorithm");
    log(self) << "Starting algorithm" << std::endl;
    log(self) << "Launching toolA, toolB and AsyncTimer in parallel"
              << std::endl;
    try {
        auto [codeA, codeB, codeC] = co_await CoroutineTests::alien::when_all(
            toolA(self), toolB(self),
            AsyncTimer{std::chrono::milliseconds(20), StatusCode::SUCCESS,
                       self});
        log(self) << "Result from toolA: " << codeA
                  << ", result from toolB: " << codeB
                  << ", result from AsyncTimer: " << codeC << std::endl;

    } catch (const std::exception& e) {
        log(self) << "when_all threw: " << e.what() << std::endl;
        co_return CoroutineTests::alien::algorithm::StatusCode::FAILURE;
    }
    log(self) << "Finishing algorithm" << std::endl;
    co_return CoroutineTests::alien::algorithm::StatusCode::SUCCESS;
}

int main() {
    log() << "main Starting" << std::endl;
    CoroutineTests::Threadpool threadpool(1);
    auto scheduler = [&threadpool](std::coroutine_handle<> handle) {
        log() << "scheduler Schedule called, enqueuing  execution" << std::endl;
        threadpool.enqueue_task(handle);
    };
    log() << "main Launching algorithm..." << std::endl;
    auto t = algorithm("main");
    auto future = t.schedule_on(scheduler);
    log() << "main Waiting for algorithm completion..." << std::endl;
    auto status = future.get();
    log() << "main Final status of algorithm " << status << "" << std::endl;
    return 0;
}
