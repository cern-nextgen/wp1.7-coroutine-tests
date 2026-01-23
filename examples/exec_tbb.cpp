#include <tbb/task_arena.h>

#include <chrono>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <utility>

#include "exec_backend.hpp"     // std exec backend selection
#include "exec_statuscode.hpp"  // exec StatusCodeImpl
#include "exec_timer.hpp"       // TimerSender
#include "logging_utils.hpp"    // log, format_name

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

// Scheduler enqueuing work into a TBB task arena
struct TaskArenaScheduler {
    tbb::task_arena* arena = nullptr;
    using scheduler_concept = execution::scheduler_t;

    struct Env {
        tbb::task_arena* arena;
        template <typename T>
        auto query(
            const execution::get_completion_scheduler_t<T>&) const noexcept {
            return TaskArenaScheduler{arena};
        }
    };

    template <execution::receiver Receiver>
    struct Operation {
        std::remove_cvref_t<Receiver> receiver;
        tbb::task_arena* arena;
        using operation_state_concept = execution::operation_state_t;

        void start() & noexcept {
            log() << "Submitting work to task arena" << std::endl;
            arena->enqueue([this]() {
                log() << "Running work item in task arena" << std::endl;
                execution::set_value(std::move(receiver));
            });
        }
    };

    struct Sender {
        tbb::task_arena* arena;
        using sender_concept = execution::sender_t;
        using completion_signatures =
            execution::completion_signatures<execution::set_value_t()>;

        template <execution::receiver Receiver>
        auto connect(Receiver&& receiver) {
            return Operation<Receiver>(std::forward<Receiver>(receiver), arena);
        }

        auto get_env() const noexcept { return Env{arena}; }
    };

    auto schedule() const noexcept { return Sender{arena}; }
    bool operator==(const TaskArenaScheduler& other) const = default;
};

static_assert(execution::scheduler<TaskArenaScheduler>,
              "TaskArenaScheduler should model scheduler");

TaskArenaScheduler get_scheduler(tbb::task_arena& arena) {
    return TaskArenaScheduler{&arena};
}

execution::task<tools::StatusCode> tool1(std::string_view parent) {
    const auto self = format_name(parent, "tool1");

    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await TimerSender{tools::StatusCode::SUCCESS,
                                       std::chrono::milliseconds(100), self};
    log(self) << "Result from async API in tool1: " << status << std::endl;

    log(self) << "Finishing tool1" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<tools::StatusCode> tool2(std::string_view parent) {
    const auto self = format_name(parent, "tool2");

    log(self) << "Calling async API in tool2" << std::endl;
    auto status1 = co_await TimerSender{tools::StatusCode::SUCCESS,
                                        std::chrono::milliseconds(10), self};
    log(self) << "Result from async API in tool2: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code = co_await tool1(self);
    log(self) << "Result from tool1: " << code << std::endl;

    log(self) << "Calling async API in tool2" << std::endl;
    auto status2 = co_await TimerSender{tools::StatusCode::FAILURE,
                                        std::chrono::milliseconds(10), self};
    log(self) << "Result from async API in tool2: " << status2 << std::endl;

    log(self) << "Finishing tool2" << std::endl;
    co_return tools::StatusCode::SUCCESS;
}

execution::task<tools::StatusCode> tool3(std::string_view parent) {
    const auto self = format_name(parent, "tool3");
    log(self) << "Finishing tool3" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<algs::StatusCode> algorithm(std::string_view parent) {
    const auto self = format_name(parent, "algorithm");

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status1 = co_await TimerSender{algs::StatusCode::SUCCESS,
                                        std::chrono::milliseconds(42), self};
    log(self) << "Result from async API in algorithm: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code1 = co_await tool1(self);
    log(self) << "Result from tool1: " << code1 << std::endl;

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status2 = co_await TimerSender{algs::StatusCode::FAILURE,
                                        std::chrono::milliseconds(17), self};
    log(self) << "Result from async API in algorithm: " << status2 << std::endl;

    log(self) << "Launching tool2" << std::endl;
    auto code2 = co_await tool2(self);
    log(self) << "Result from tool2: " << code2 << std::endl;

    log(self) << "Launching tool3" << std::endl;
    auto code3 = co_await tool3(self);
    log(self) << "Result from tool3: " << code3 << std::endl;

    log(self) << "Finishing algorithm" << std::endl;
    co_return algs::StatusCode::SUCCESS;
}

int main() {
    log() << "main Starting" << std::endl;

    tbb::task_arena arena{2};
    execution::scheduler auto scheduler = get_scheduler(arena);

    // Start executing the algorithm without blocking main
    Scope scope;
    auto work = []() -> execution::task<void> {
        log() << "Starting work" << std::endl;
        auto status = co_await algorithm("main");
        log() << "Final status of algorithm " << status << std::endl;
    }();
    scope.spawn(scheduler, std::move(work));

    // Sleep a bit to show that algorithm is already running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log() << "main waiting for algorithm to finish..." << std::endl;
    // Block until all work items in the scope are done
    execution::sync_wait(scope.join());
    log() << "main Done" << std::endl;
    return EXIT_SUCCESS;
}
