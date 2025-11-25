#ifndef COROUTINETESTS_ALIEN_ALGORITHM_H
#define COROUTINETESTS_ALIEN_ALGORITHM_H

#include <coroutine>
#include <exception>
#include <functional>
#include <future>
#include <iostream>

namespace CoroutineTests::alien::algorithm {

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
        os << "Algorithm::StatusCode::";
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

// Nestable coroutine that can be scheduled on a threadpool.
// Does co_return value, doesn't co_yield.
class [[nodiscard]] Algorithm {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    using scheduler_type = std::function<void(std::coroutine_handle<>)>;

    Algorithm(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~Algorithm() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    Algorithm() = default;
    Algorithm(const Algorithm&) = delete;
    Algorithm& operator=(const Algorithm&) = delete;
    Algorithm(Algorithm&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    Algorithm& operator=(Algorithm&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }
    // start the coroutine on the threadpool
    inline std::future<StatusCode> schedule_on(const scheduler_type& scheduler);
    inline StatusCode get() const;

    private:
    handle_type m_coroutine = nullptr;
    bool m_started = false;
};

struct Algorithm::promise_type {
    // storage for exceptions thrown in the coroutine
    std::exception_ptr m_exception;
    // handle to the parent coroutine if the coroutine has one
    handle_type m_parent;
    // handle to scheduler
    scheduler_type m_scheduler;
    std::promise<StatusCode> m_promise;

    void reschedule() { m_scheduler(handle_type::from_promise(*this)); }

    const auto& get_scheduler() const { return m_scheduler; }
    // required by coroutines
    Algorithm get_return_object() { return {handle_type::from_promise(*this)}; }
    // called on coroutine start
    std::suspend_always initial_suspend() const noexcept { return {}; }
    // called on coroutine completion
    // on final_suspend reschedule the parent coroutine if it has one
    std::suspend_always final_suspend() const noexcept { return {}; }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() {
        m_exception = std::current_exception();
        m_promise.set_exception(m_exception);
    }
    // called on (implicit or explicit) co_return or co_return void
    void return_value(StatusCode value) { m_promise.set_value(value); }
};

std::future<StatusCode> Algorithm::schedule_on(
    const scheduler_type& scheduler) {
    if (m_coroutine && !m_started) {
        m_coroutine.promise().m_scheduler = scheduler;
        m_coroutine.promise().reschedule();
        m_started = true;
        return m_coroutine.promise().m_promise.get_future();
    }
    throw std::runtime_error("Algorithm already started or invalid");
}

}  // namespace CoroutineTests::alien::algorithm
#endif  // COROUTINETESTS_ALIEN_ALGORITHM_H
