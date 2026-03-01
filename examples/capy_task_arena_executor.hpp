#include <tbb/task_arena.h>

#include <boost/capy.hpp>
#include <coroutine>

class TaskArenaContext : public boost::capy::execution_context {
    public:
    TaskArenaContext(tbb::task_arena& arena) : m_arena(&arena) {}

    void schedule(std::coroutine_handle<> h) const {
        m_arena->enqueue([h]() { h.resume(); });
    }
    bool operator==(const TaskArenaContext& other) const noexcept {
        return m_arena == other.m_arena;
    }

    private:
    tbb::task_arena* m_arena;
};

class TaskArenaExecutor {

    public:
    TaskArenaExecutor(TaskArenaContext& context) noexcept
        : m_context(&context) {
        static_assert(boost::capy::Executor<TaskArenaExecutor>,
                      "TaskArenaExecutor should be a valid capy Executor");
    }

    TaskArenaExecutor(TaskArenaExecutor const&) noexcept = default;

    std::coroutine_handle<> dispatch(std::coroutine_handle<> h) const {
        m_context->schedule(h);
        return std::noop_coroutine();
    }
    void post(std::coroutine_handle<> h) const { m_context->schedule(h); }
    TaskArenaContext& context() const noexcept { return *m_context; }
    void on_work_started() const noexcept {}
    void on_work_finished() const noexcept {}
    bool operator==(const TaskArenaExecutor& other) const noexcept {
        return m_context == other.m_context;
    }

    private:
    TaskArenaContext* m_context;
};
