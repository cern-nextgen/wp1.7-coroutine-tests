#include <format>
#include <string_view>

#include "CoroutineTests/alien/this_task.hpp"
#include "CoroutineTests/alien/schedule_on.hpp"
#include "CoroutineTests/alien/sync_wait.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"
#include "logging_utils.hpp"

using namespace CoroutineTests::alien;

using scheduler_t = std::function<void(std::coroutine_handle<>)>;
tool::Task<void> innermost_task(std::string_view parent) {
    const auto self = format_name(parent, "innermost_task");
    log(self) << "Running innermost task" << std::endl;
    co_return;
}

tool::Task<void> inner_task(scheduler_t innermost_scheduler,
                            std::string_view parent) {
    const auto self = format_name(parent, "inner_task");
    log(self) << "Running inner task, scheduling innermost task on innermost "
                 "scheduler"
              << std::endl;
    co_await schedule_on(innermost_scheduler, innermost_task(self));
    log(self) << "Innermost task completed, resuming inner task" << std::endl;
    co_return;
}

tool::Task<void> outer_task(scheduler_t inner_scheduler,
                            std::string_view parent) {
    const auto self = format_name(parent, "outer_task");
    log(self) << "Running outer task" << std::endl;
    auto current_scheduler = co_await this_task::scheduler;
    log(self) << "Got current scheduler in outer task, scheduling inner task "
                 "on inner scheduler"
              << std::endl;
    co_await schedule_on(inner_scheduler, inner_task(current_scheduler, self));
    log(self) << "Inner task completed, resuming outer task" << std::endl;
    co_return;
}

int main() {
    log() << "Starting main, launching task..." << std::endl;
    auto threadpool_A = CoroutineTests::Threadpool(1);
    auto scheduler_A = [&](std::coroutine_handle<> h) {
        threadpool_A.enqueue_task(h);
    };

    auto threadpool_B = CoroutineTests::Threadpool(1);
    auto scheduler_B = [&](std::coroutine_handle<> h) {
        threadpool_B.enqueue_task(h);
    };

    sync_wait(scheduler_A, outer_task(scheduler_B, "main"));
    log() << "Finished waiting for task" << std::endl;
    return 0;
}
