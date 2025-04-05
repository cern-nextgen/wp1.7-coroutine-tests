#ifndef COROUTINETESTS_NESTABLETASK_H
#define COROUTINETESTS_NESTABLETASK_H

#include <coroutine>
#include <exception>

namespace CoroutineTests {

// Coroutine that can be nested and co_awaited on inside another coroutine.
// Doesn't return a value, doesn't yield. Rethrows exceptions on resume.
// The outer coroutine should be resumed from outside.
class [[nodiscard]] NestableTask {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    NestableTask(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~NestableTask() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    NestableTask() = default;
    NestableTask(const NestableTask&) = delete;
    NestableTask& operator=(const NestableTask&) = delete;
    NestableTask(NestableTask&& other) noexcept
        : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    NestableTask& operator=(NestableTask&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }
    // resume the coroutine from outside
    inline void resume() const;
    // check if finished from outside
    inline bool done() const { return m_coroutine.done(); }

    // awaitable interface
    // don't skip suspensions
    bool await_ready() const noexcept { return false; }
    // when awaited, store the parent coroutine handle and resume this coroutine
    inline auto await_suspend(handle_type handle) noexcept;
    // nothing special on resume, doesn't produce a value
    void await_resume() const noexcept {}

    private:
    handle_type m_coroutine;
};

struct NestableTask::promise_type {
    // storage for exceptions thrown in the coroutine
    std::exception_ptr exception;
    // handle to the parent coroutine if the coroutine has one
    handle_type m_parent;
    // required by coroutines
    NestableTask get_return_object() {
        return {NestableTask::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    std::suspend_always initial_suspend() const { return {}; }
    // called on coroutine completion
    // on final_suspend return control to the parent coroutine
    auto final_suspend() const noexcept {
        struct final_awaiter {
            // don't skip suspensions
            bool await_ready() const noexcept { return false; }
            // resume the parent of the suspended coroutine if it has one
            // or if not then continue control to the caller
            std::coroutine_handle<> await_suspend(handle_type handle) noexcept {
                auto parent = handle.promise().m_parent;
                if (parent) {
                    return parent;
                }
                return std::noop_coroutine();
            }
            // nothing special on resume, doesn't produce a value
            void await_resume() const noexcept {}
        };
        return final_awaiter{};
    }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return_void
    void return_void() const {}
};

inline void NestableTask::resume() const {
    if (!m_coroutine.done()) {
        m_coroutine.resume();
    }
    if (m_coroutine.promise().exception) {
        std::rethrow_exception(m_coroutine.promise().exception);
    }
}

inline auto NestableTask::await_suspend(handle_type handle) noexcept {
    m_coroutine.promise().m_parent = handle;
    return m_coroutine;
}

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_NESTABLETASK_H
