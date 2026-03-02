#pragma once

#include <tbb/task_arena.h>

#include "exec_backend.hpp"   // std exec backend selection
#include "logging_utils.hpp"  // log, format_name

// Scheduler enqueuing work into a TBB task arena
struct TaskArenaScheduler {
    tbb::task_arena* arena = nullptr;
    bool log_scheduling = true;
    using scheduler_concept = execution::scheduler_t;

    struct Env {
        tbb::task_arena* arena;
        bool log_scheduling;
        template <typename T>
        auto query(
            const execution::get_completion_scheduler_t<T>&) const noexcept {
            return TaskArenaScheduler{arena, log_scheduling};
        }
    };

    template <execution::receiver Receiver>
    struct Operation {
        std::remove_cvref_t<Receiver> receiver;
        tbb::task_arena* arena;
        bool log_scheduling;
        using operation_state_concept = execution::operation_state_t;

        void start() & noexcept {
            if (log_scheduling) {
                log() << "Submitting work to task arena" << std::endl;
            }
            arena->enqueue([this]() {
                if (log_scheduling) {
                    log() << "Running work item in task arena" << std::endl;
                }
                execution::set_value(std::move(receiver));
            });
        }
    };

    struct Sender {
        tbb::task_arena* arena;
        bool log_scheduling;
        using sender_concept = execution::sender_t;
        using completion_signatures =
            execution::completion_signatures<execution::set_value_t()>;

        template <execution::receiver Receiver>
        auto connect(Receiver&& receiver) {
            return Operation<Receiver>(std::forward<Receiver>(receiver), arena,
                                       log_scheduling);
        }

        auto get_env() const noexcept { return Env{arena, log_scheduling}; }
    };

    auto schedule() const noexcept { return Sender{arena, log_scheduling}; }
    bool operator==(const TaskArenaScheduler& other) const = default;
};

static_assert(execution::scheduler<TaskArenaScheduler>,
              "TaskArenaScheduler should model scheduler");

inline TaskArenaScheduler get_scheduler(tbb::task_arena& arena,
                                        bool log = true) {
    return TaskArenaScheduler{&arena, log};
}
