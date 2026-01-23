#ifndef COROUTINETESTS_ALIEN_SUBTOOL_H
#define COROUTINETESTS_ALIEN_SUBTOOL_H

#include <coroutine>
#include <exception>
#include <functional>
#include <iostream>

namespace CoroutineTests::alien::subtool {

class StatusCode {
    public:
    /// StatusCode values
    enum Status { SUCCESS = 0, FAILURE = 1, UNDEFINED = 2 };
    /// Constructor
    StatusCode(Status status = UNDEFINED) : m_status(status) {}

    /// Get the status of the statuscode
    Status status() const { return m_status; }

    /// Friend function to output the status code
    friend std::ostream& operator<<(std::ostream& os,
                                    const StatusCode& status) {
        os << "subtool::StatusCode::";
        switch (status.status()) {
            case StatusCode::SUCCESS:
                os << "SUCCESS";
                break;
            case StatusCode::FAILURE:
                os << "FAILURE";
                break;
            case StatusCode::UNDEFINED:
                os << "UNDEFINED";
                break;
        }
        return os;
    }

    private:
    Status m_status;
};

template <typename T>
concept HasScheduler = requires(T t) {
    {
        t.get_scheduler()
    } -> std::convertible_to<std::function<void(std::coroutine_handle<>)>>;
};

// Nestable coroutine, can be co_awaited by other coroutines.
// Returns a StatusCode value via co_return, doesn't co_yield.
class [[nodiscard]] Task {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    // Constructor from coroutine handle
    Task(handle_type coroutine_handle) : m_coroutine(coroutine_handle) {}
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

    // Awaitable interface: always suspend to allow async execution
    bool await_ready() const noexcept { return false; }
    // Awaitable interface: setup parent/scheduler relationship and transfer
    // control to this coroutine
    template <HasScheduler T>
    inline handle_type await_suspend(std::coroutine_handle<T> handle) noexcept;
    // Awaitable interface: return result or rethrow exception on resume
    StatusCode await_resume() const;

    private:
    handle_type m_coroutine = nullptr;
};

struct Task::promise_type {
    // Storage for the co_return result value
    StatusCode m_value;
    // Storage for exceptions thrown in the coroutine body
    std::exception_ptr m_exception;
    // Handle to the parent coroutine that co_awaited this task
    std::coroutine_handle<> m_parent;
    // Handle to scheduler to resume this coroutine and propagate to children
    std::function<void(std::coroutine_handle<>)> m_scheduler;

    // Accessor for scheduler used by child coroutines
    const auto& get_scheduler() const { return m_scheduler; }
    // Schedule resumption of this task
    void reschedule() { m_scheduler(handle_type::from_promise(*this)); }

    // Required by coroutines: create the object
    Task get_return_object() { return {handle_type::from_promise(*this)}; }
    // Required by coroutines: suspend immediately on start (lazy execution)
    std::suspend_always initial_suspend() const { return {}; }
    // Required by coroutines: handle completion and resume parent
    auto final_suspend() const noexcept {
        struct final_awaiter {
            // Don't skip final supersession
            bool await_ready() const noexcept { return false; }
            // Resume parent coroutine with symmetric transfer or return to
            // caller
            std::coroutine_handle<> await_suspend(handle_type handle) noexcept {
                auto parent = handle.promise().m_parent;
                if (parent) {
                    return parent;
                }
                return std::noop_coroutine();
            }
            // No action needed on resume
            void await_resume() const noexcept {}
        };
        return final_awaiter{};
    }
    // Required by coroutines: capture exceptions for later rethrowing
    void unhandled_exception() { m_exception = std::current_exception(); }
    // Required by coroutines: store the co_return value
    void return_value(StatusCode value) { m_value = value; }
};

template <HasScheduler T>
inline Task::handle_type Task::await_suspend(
    std::coroutine_handle<T> handle) noexcept {
    m_coroutine.promise().m_parent = handle;
    m_coroutine.promise().m_scheduler = handle.promise().get_scheduler();
    return m_coroutine;
}

inline StatusCode Task::await_resume() const {
    if (m_coroutine.promise().m_exception) {
        std::rethrow_exception(m_coroutine.promise().m_exception);
    }
    return m_coroutine.promise().m_value;
}
}  // namespace CoroutineTests::alien::subtool
#endif  // COROUTINETESTS_ALIEN_SUBTOOL_H
