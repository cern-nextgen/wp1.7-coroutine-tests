#pragma once

#include <tbb/task_arena.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "CoroutineTests/auditor.hpp"        // Auditor
#include "CoroutineTests/event_context.hpp"  // EventContext
#include "exec_backend.hpp"                  // std exec backend selection
#include "logging_utils.hpp"                 // log

// Wrapper class holding common objects. Should be kept alive for the lifetime
// of the scheduler and all work items scheduled through it. This isn't required
// by the the scheduler interface.
class TaskArenaContext {

    public:
    explicit TaskArenaContext(tbb::task_arena& arena,
                              bool log_scheduling = true)
        : m_arena(&arena), m_log_scheduling(log_scheduling) {}

    tbb::task_arena& arena() const noexcept { return *m_arena; }

    void add_auditor(std::shared_ptr<CoroutineTests::Auditor> auditor) {
        m_auditors.push_back(std::move(auditor));
    }

    template <execution::receiver Receiver>
    void enqueue(Receiver&& receiver, const std::string& name,
                 const CoroutineTests::EventContext& ctx) const {
        if (m_log_scheduling) {
            log() << "Scheduling operation" << std::endl;
        }
        m_arena->enqueue(
            std::function([receiver = std::forward<Receiver>(receiver), name,
                           ctx, this]() mutable {
                for (auto& auditor : m_auditors) {
                    if (auditor) {
                        auditor->start(name, ctx);
                    }
                }
                if (m_log_scheduling) {
                    log() << "Resuming operation" << std::endl;
                }
                execution::set_value(std::move(receiver));
                for (auto& auditor : m_auditors) {
                    if (auditor) {
                        auditor->stop(name, ctx);
                    }
                }
            }));
    }

    private:
    tbb::task_arena* m_arena;
    std::vector<std::shared_ptr<CoroutineTests::Auditor> > m_auditors;
    bool m_log_scheduling = true;
};

// Scheduler delegating work to a TaskArenaContext
class TaskArenaScheduler {

    private:
    struct SchedulerData {
        std::string name;
        CoroutineTests::EventContext event_context;
    };
    TaskArenaScheduler(TaskArenaContext* context,
                       std::shared_ptr<SchedulerData> scheduler_data)
        : m_context(context), m_scheduler_data(std::move(scheduler_data)) {}

    TaskArenaContext* m_context;

    std::shared_ptr<SchedulerData> m_scheduler_data;

    // workaround for beman.task accepting only 32 bit types for scheduler data

    public:
    using scheduler_concept = execution::scheduler_t;

    TaskArenaScheduler(TaskArenaContext& context, std::string_view name = {},
                       CoroutineTests::EventContext event_context = {})
        : m_context(&context),
          m_scheduler_data(std::make_shared<SchedulerData>(
              SchedulerData{std::string(name), event_context})) {}

    struct Env {

        TaskArenaContext* context;
        std::shared_ptr<SchedulerData> scheduler_data;

        template <typename T>
        auto query(
            const execution::get_completion_scheduler_t<T>&) const noexcept {
            return TaskArenaScheduler{context, scheduler_data};
        }
    };

    template <execution::receiver Receiver>
    struct Operation {

        std::remove_cvref_t<Receiver> receiver;

        TaskArenaContext* context;

        std::shared_ptr<TaskArenaScheduler::SchedulerData> scheduler_data;

        using operation_state_concept = execution::operation_state_t;

        void start() & noexcept {
            context->enqueue(std::move(receiver), scheduler_data->name,
                             scheduler_data->event_context);
        }
    };
    struct Sender;

    auto schedule() const noexcept;

    bool operator==(const TaskArenaScheduler& other) const noexcept {
        return m_context == other.m_context;
    }
};

struct TaskArenaScheduler::Sender {

    TaskArenaScheduler scheduler;

    using sender_concept = execution::sender_t;

    using completion_signatures =
        execution::completion_signatures<execution::set_value_t()>;

    template <execution::receiver Receiver>
    auto connect(Receiver&& receiver) {
        return Operation<Receiver>{std::forward<Receiver>(receiver),
                                   scheduler.m_context,
                                   scheduler.m_scheduler_data};
    }

    auto get_env() const noexcept {
        return Env{scheduler.m_context, scheduler.m_scheduler_data};
    }
};

inline auto TaskArenaScheduler::schedule() const noexcept {
    return Sender{*this};
}

static_assert(execution::scheduler<TaskArenaScheduler>,
              "TaskArenaScheduler should model scheduler");
