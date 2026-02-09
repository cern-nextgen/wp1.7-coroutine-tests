#include <chrono>
#include <coroutine>
#include <iostream>
#include <string_view>

#include "CoroutineTests/alien/sync_wait.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"
#include "alien_timer.hpp"    // AsyncTimer
#include "logging_utils.hpp"  // log, format_name

using namespace CoroutineTests::alien;

tool::Task<tool::StatusCode> execute(std::string_view parent) {
    const auto self = format_name(parent, "execute");
    log(self) << "Starting execute" << std::endl;
    log(self) << "Calling AsyncTimer" << std::endl;
    auto status = co_await AsyncTimer{std::chrono::milliseconds(100),
                                      StatusCode::SUCCESS, self};
    log(self) << "Result from AsyncTimer: " << status << std::endl;
    log(self) << "Finishing execute" << std::endl;
    co_return tool::StatusCode::SUCCESS;
}

tool::Task<void> void_execute(std::string_view parent) {
    const auto self = format_name(parent, "void_execute");
    log(self) << "Starting void_execute" << std::endl;
    log(self) << "Calling execute " << std::endl;
    auto status = co_await execute(self);
    log(self) << "Result from execute: " << status << std::endl;
    log(self) << "Finishing void_execute" << std::endl;
    co_return;
}

int main() {
    log() << "main Starting" << std::endl;
    CoroutineTests::Threadpool threadpool(1);
    auto scheduler = [&threadpool](std::coroutine_handle<> handle) {
        log() << "scheduler Reschedule called, enqueuing  resumption"
              << std::endl;
        threadpool.enqueue_task(handle);
    };
    log() << "main Launching execute and waiting for completion..."
          << std::endl;
    auto status = CoroutineTests::alien::sync_wait(scheduler, execute("main"));
    log() << "main Final status of execute " << status << "" << std::endl;
    std::cout << std::endl;
    log() << "main Launching void_execute and waiting for completion..."
          << std::endl;
    CoroutineTests::alien::sync_wait(scheduler, void_execute("main"));
    log() << "main Finished void_execute" << std::endl;
    return 0;
}
