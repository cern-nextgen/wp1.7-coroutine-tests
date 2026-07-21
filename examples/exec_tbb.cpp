#include <tbb/task_arena.h>

#include <chrono>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <utility>

#include "exec_backend.hpp"               // std exec backend selection
#include "exec_task_arena_scheduler.hpp"  // TaskArenaScheduler/// TaskArenaSchduler
#include "exec_timer.hpp"                 // TimerSender
#include "logging_utils.hpp"              // log, format_name
#include "statuscode.hpp"                 // StatusCodeImpl

namespace tools {
struct Tag {
    static constexpr const char* name = "tools";
};
using StatusCode = StatusCodeImpl<Tag>;
}  // namespace tools

namespace algs {
struct Tag {
    static constexpr const char* name = "algs";
};
using StatusCode = StatusCodeImpl<Tag>;
}  // namespace algs

execution::task<tools::StatusCode> tool1_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool1");

    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await TimerSender{tools::StatusCode::SUCCESS,
                                       std::chrono::milliseconds(100), self};
    log(self) << "Result from async API in tool1: " << status << std::endl;

    log(self) << "Finishing tool1" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<tools::StatusCode> tool2_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool2");

    log(self) << "Calling async API in tool2" << std::endl;
    auto status1 = co_await TimerSender{tools::StatusCode::SUCCESS,
                                        std::chrono::milliseconds(10), self};
    log(self) << "Result from async API in tool2: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code = co_await tool1_execute(self);
    log(self) << "Result from tool1: " << code << std::endl;

    log(self) << "Calling async API in tool2" << std::endl;
    auto status2 = co_await TimerSender{tools::StatusCode::FAILURE,
                                        std::chrono::milliseconds(10), self};
    log(self) << "Result from async API in tool2: " << status2 << std::endl;

    log(self) << "Finishing tool2" << std::endl;
    co_return tools::StatusCode::SUCCESS;
}

execution::task<tools::StatusCode> tool3_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool3");
    log(self) << "Finishing tool3" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<algs::StatusCode> algorithm_execute(std::string_view parent) {
    const auto self = format_name(parent, "algorithm");

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status1 = co_await TimerSender{algs::StatusCode::SUCCESS,
                                        std::chrono::milliseconds(42), self};
    log(self) << "Result from async API in algorithm: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code1 = co_await tool1_execute(self);
    log(self) << "Result from tool1: " << code1 << std::endl;

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status2 = co_await TimerSender{algs::StatusCode::FAILURE,
                                        std::chrono::milliseconds(17), self};
    log(self) << "Result from async API in algorithm: " << status2 << std::endl;

    log(self) << "Launching tool2" << std::endl;
    auto code2 = co_await tool2_execute(self);
    log(self) << "Result from tool2: " << code2 << std::endl;

    log(self) << "Launching tool3" << std::endl;
    auto code3 = co_await tool3_execute(self);
    log(self) << "Result from tool3: " << code3 << std::endl;

    log(self) << "Finishing algorithm" << std::endl;
    co_return algs::StatusCode::SUCCESS;
}

int main() {
    log("main") << "Starting" << std::endl;

    tbb::task_arena arena{2};
    TaskArenaContext context{arena};

    // Start executing the algorithm without blocking main
    Scope scope;
    auto work = []() -> execution::task<void> {
        log() << "Starting work" << std::endl;
        auto status = co_await algorithm_execute("main");
        log() << "Final status of algorithm " << status << std::endl;
    }();
    scope.spawn(TaskArenaScheduler(context, "algorithm",
                                   CoroutineTests::EventContext{0, 0}),
                std::move(work));

    // Sleep a bit to show that algorithm is already running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log("main") << "Waiting for algorithm to finish..." << std::endl;
    // Block until all work items in the scope are done
    execution::sync_wait(scope.join());
    log("main") << "Done" << std::endl;
    return EXIT_SUCCESS;
}
