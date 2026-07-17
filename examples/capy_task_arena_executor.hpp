#pragma once

#include <tbb/task_arena.h>

#include <boost/capy.hpp>
#include <coroutine>
#include <memory>
#include <string_view>

#include "CoroutineTests/auditor.hpp"        // Auditor
#include "CoroutineTests/event_context.hpp"  // EventContext

class TaskArenaContext : public boost::capy::execution_context {
    public:
    explicit TaskArenaContext(tbb::task_arena& arena) : m_arena(&arena) {}

    void schedule(std::coroutine_handle<> h, const std::string& name,
                  const CoroutineTests::EventContext& ctx) const {

        m_arena->enqueue([h, name, ctx, this]() {
            for (auto& auditor : m_auditors) {
                if (auditor) {
                    auditor->start(name, ctx);
                }
            }
            h.resume();
            for (auto& auditor : m_auditors) {
                if (auditor) {
                    auditor->stop(name, ctx);
                }
            }
        });
    }
    bool operator==(const TaskArenaContext& other) const noexcept {
        return m_arena == other.m_arena;
    }

    void add_auditor(std::shared_ptr<CoroutineTests::Auditor> auditor) {
        m_auditors.push_back(auditor);
    }

    private:
    tbb::task_arena* m_arena;
    std::vector<std::shared_ptr<CoroutineTests::Auditor>> m_auditors;
};

class TaskArenaExecutor {

    public:
    explicit TaskArenaExecutor(
        TaskArenaContext& context, const std::string_view name,
        const CoroutineTests::EventContext& event_context) noexcept
        : m_context(&context), m_name(name), m_event_context(event_context) {
        static_assert(boost::capy::Executor<TaskArenaExecutor>,
                      "TaskArenaExecutor should be a valid capy Executor");
    }

    TaskArenaExecutor(TaskArenaExecutor const&) noexcept = default;

    std::coroutine_handle<> dispatch(boost::capy::continuation& c) const {
        m_context->schedule(c.h, m_name, m_event_context);
        return std::noop_coroutine();
    }
    void post(boost::capy::continuation& c) const {
        m_context->schedule(c.h, m_name, m_event_context);
    }
    TaskArenaContext& context() const noexcept { return *m_context; }
    void on_work_started() const noexcept {}
    void on_work_finished() const noexcept {}
    bool operator==(const TaskArenaExecutor& other) const noexcept {
        return m_context == other.m_context;
    }

    private:
    TaskArenaContext* m_context;
    std::string m_name;
    CoroutineTests::EventContext m_event_context;
};

static_assert(boost::capy::Executor<TaskArenaExecutor>,
              "TaskArenaExecutor should be a valid capy Executor");
