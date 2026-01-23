#ifndef COROUTINETESTS_ALIEN_MANUAL_ALGORITHM_H
#define COROUTINETESTS_ALIEN_MANUAL_ALGORITHM_H

#include <coroutine>
#include <functional>
#include <iostream>
#include <optional>
namespace CoroutineTests::alien::manual_algorithm {

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
        os << "algorithm::StatusCode::";
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

// Top-level coroutine that can be scheduled from outside.
// Returns a StatusCode value via co_return, doesn't co_yield.
class [[nodiscard]] Task {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    using scheduler_type = std::function<void(std::coroutine_handle<>)>;

    // Required by coroutines
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

    // Resume this coroutine or its currently running child coroutine
    // Returns status code when the task is finished, otherwise an empty
    // optional
    inline std::optional<StatusCode> resume();
    // Set user provided scheduler for this task and its child coroutines
    // Provided scheduler will be wrapped, so that coroutine calling it will be
    // saved to m_current
    inline void set_scheduler(scheduler_type scheduler);

    private:
    // handle to the coroutine associated with this task (owning)
    handle_type m_coroutine = nullptr;
    // handle to the currently running coroutine (non-owning)
    std::coroutine_handle<> m_current = m_coroutine;
};

struct Task::promise_type {
    // Handle to scheduler
    scheduler_type m_scheduler;
    // Storage for the co_return result value
    std::optional<StatusCode> m_value;

    // Schedule resumption of this task
    void reschedule() { m_scheduler(handle_type::from_promise(*this)); }
    // Accessor for scheduler used by child coroutines
    const auto& get_scheduler() const { return m_scheduler; }

    // Required by coroutines: create the object
    Task get_return_object() { return {handle_type::from_promise(*this)}; }
    // Required by coroutines: suspend immediately on start (lazy execution)
    std::suspend_always initial_suspend() const noexcept { return {}; }
    // Required by coroutines: suspend on completion
    std::suspend_always final_suspend() const noexcept { return {}; }
    // Required by coroutines: handle exceptions thrown in the coroutine body
    void unhandled_exception() { throw std::current_exception(); }
    // Required by coroutines: handle co_return <value>
    void return_value(StatusCode value) { m_value = value; }
};

std::optional<StatusCode> Task::resume() {
    if (m_current && !m_current.done()) {
        m_current.resume();
    }
    return m_coroutine.promise().m_value;
}

void Task::set_scheduler(scheduler_type scheduler) {
    if (m_coroutine) {
        m_coroutine.promise().m_scheduler =
            [this, scheduler](std::coroutine_handle<> handle) {
                m_current = handle;
                scheduler(handle);
            };
    }
}
}  // namespace CoroutineTests::alien::manual_algorithm
#endif  // COROUTINETESTS_ALIEN_MANUAL_ALGORITHM_H
