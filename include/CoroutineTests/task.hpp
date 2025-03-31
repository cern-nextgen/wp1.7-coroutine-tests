#ifndef COROUTINETESTS_TASK_H
#define COROUTINETESTS_TASK_H

#include <coroutine>
#include <exception>

namespace CoroutineTests {

class [[nodiscard]] Task {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    Task(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~Task() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    Task() = default;
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    Task(Task&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }
    void resume();
    bool done() const { return m_coroutine.done(); }

    private:
    handle_type m_coroutine;
};


struct Task::promise_type {
    std::exception_ptr exception;
    // required by coroutines
    Task get_return_object() {
        return {Task::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    std::suspend_always initial_suspend() { return {}; }
    // called on coroutine completion
    std::suspend_always final_suspend() noexcept { return {}; }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return_void
    void return_void() {}
};

inline void Task::resume() {
    if (!m_coroutine.done()) {
        m_coroutine.resume();
    }
    if (m_coroutine.promise().exception) {
        std::rethrow_exception(m_coroutine.promise().exception);
    }
}

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_TASK_H
