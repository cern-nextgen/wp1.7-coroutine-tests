#ifndef COROUTINETESTS_ALIEN_subtool_H
#define COROUTINETESTS_ALIEN_subtool_H

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
        os << "SubTool::StatusCode::";
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

// Nestable coroutine that can be scheduled on a threadpool.
// Does co_return value, doesn't co_yield.
class [[nodiscard]] SubTool {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    SubTool(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~SubTool() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    SubTool() = default;
    SubTool(const SubTool&) = delete;
    SubTool& operator=(const SubTool&) = delete;
    SubTool(SubTool&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    SubTool& operator=(SubTool&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }

    // awaitable interface
    // don't skip suspensions
    bool await_ready() const noexcept { return false; }
    // when awaited, store the parent coroutine handle and reschedule this
    // coroutine
    template <HasScheduler T>
    inline void await_suspend(std::coroutine_handle<T> handle) noexcept;
    // nothing special on resume, doesn't produce a value
    StatusCode await_resume() const noexcept;

    private:
    handle_type m_coroutine = nullptr;
};

struct SubTool::promise_type {
    // storage for the result value
    StatusCode m_value;
    // storage for exceptions thrown in the coroutine
    std::exception_ptr m_exception;
    // handle to the parent coroutine if the coroutine has one
    std::coroutine_handle<> m_parent;
    // handle to the threadpool
    std::function<void(std::coroutine_handle<>)> m_scheduler;

    // enqueue the coroutine on the threadpool
    void reschedule() { m_scheduler(handle_type::from_promise(*this)); }
    // required by coroutines
    SubTool get_return_object() { return {handle_type::from_promise(*this)}; }
    // called on coroutine start
    std::suspend_always initial_suspend() const { return {}; }
    // called on coroutine completion
    // on final_suspend reschedule the parent coroutine if it has one
    auto final_suspend() const noexcept {
        struct final_awaiter {
            // don't skip suspensions
            bool await_ready() const noexcept { return false; }
            // reschedule the parent coroutine if it has one or return control
            // to the caller
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
    void unhandled_exception() { m_exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return void
    void return_value(StatusCode value) { m_value = value; }
};

template <HasScheduler T>
inline void SubTool::await_suspend(std::coroutine_handle<T> handle) noexcept {
    m_coroutine.promise().m_parent = handle;
    m_coroutine.promise().m_scheduler = handle.promise().get_scheduler();
    m_coroutine.promise().reschedule();
}

inline StatusCode SubTool::await_resume() const noexcept {
    return m_coroutine.promise().m_value;
}
}  // namespace CoroutineTests::alien::subtool
#endif  // COROUTINETESTS_ALIEN_subtool_H
