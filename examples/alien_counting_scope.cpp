#include <chrono>
#include <coroutine>
#include <string_view>

#include "CoroutineTests/alien/counting_scope.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"
#include "alien_timer.hpp"    // AsyncTimer
#include "logging_utils.hpp"  // log, format_name

using namespace CoroutineTests::alien;

tool::Task<void> child(std::string_view parent,
                       std::chrono::milliseconds delay) {
    const auto self =
        format_name(parent, std::format("child({}ms)", delay.count()));
    log(self) << "Starting child" << std::endl;
    co_await AsyncTimer{delay, StatusCode::SUCCESS, self};
    log(self) << "Finishing child" << std::endl;
    co_return;
}

int main() {
    log() << "main Starting" << std::endl;
    CoroutineTests::Threadpool threadpool(2);
    auto scheduler = [&threadpool](std::coroutine_handle<> handle) {
        log() << "scheduler Schedule called, enqueuing execution" << std::endl;
        threadpool.enqueue_task(handle);
    };

    CoroutineTests::alien::counting_scope scope;
    log() << "main Spawning work" << std::endl;
    scope.spawn(scheduler, child("main", std::chrono::milliseconds(20)));
    scope.spawn(scheduler, child("main", std::chrono::milliseconds(50)));
    scope.spawn(scheduler, child("main", std::chrono::milliseconds(10)));

    log() << "main Waiting for scope to become empty" << std::endl;
    scope.join();
    log() << "main Done" << std::endl;
    return 0;
}
