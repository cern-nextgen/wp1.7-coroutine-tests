#pragma once

#include <concepts>
#include <coroutine>
#include <exception>
#include <functional>
#include <optional>

namespace CoroutineTests::alien {

namespace detail::schedule_on {
namespace concepts {
template <typename T>
concept HasScheduler = requires(T t) {
    {
        t.get_scheduler()
    } -> std::convertible_to<std::function<void(std::coroutine_handle<>)>>;
};
}  // namespace concepts

// Helper coroutine type allowing for having different scheduler than parent
// coroutine.
template <typename ResultType>
class [[nodiscard]] Task {

    static_assert(std::movable<ResultType> || std::same_as<ResultType, void>,
                  "Task<ResultType> requires ResultType to be movable or void");

    public:
    using result_type = ResultType;

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
    // Awaitable interface: setup parent relationship, suspend parent and
    // schedule this coroutine on its scheduler
    template <detail::schedule_on::concepts::HasScheduler T>
    inline void await_suspend(std::coroutine_handle<T> handle) noexcept;
    // Awaitable interface: return result or rethrow exception on resume
    result_type await_resume() const;

    private:
    handle_type m_coroutine = nullptr;
};

// Helper for handling co_return in promise_type, default implementation for
// non-void ResultType
template <typename ResultType>
struct ReturnHelper {
    // Storage for the co_return result value
    std::optional<ResultType> m_value;

    // Required by coroutines, mutually exclusive with return_void
    // Store the co_return value
    template <typename T>
        requires std::constructible_from<ResultType, T&&>
    void return_value(T&& value) {
        m_value.emplace(std::forward<T>(value));
    }
    // Overload to resolve ambiguity
    void return_value(ResultType value) { m_value.emplace(std::move(value)); }
};

// Specialization for void return type
template <>
struct ReturnHelper<void> {
    // Required by coroutines, mutually exclusive with return_value
    // Handle co_return without value
    void return_void() {}
};

template <typename ResultType>
struct Task<ResultType>::promise_type
    : public ReturnHelper<typename Task<ResultType>::result_type> {
    // Storage for exceptions thrown in the coroutine body
    std::exception_ptr m_exception;
    // Handle to the parent coroutine that co_awaited this task
    std::coroutine_handle<> m_parent;
    // Handle to scheduler to resume this coroutine and propagate to children
    std::function<void(std::coroutine_handle<>)> m_scheduler;
    // Handle to the scheduler to resume parent coroutine
    std::function<void(std::coroutine_handle<>)> m_parent_scheduler;

    // Non-default constructor to pass the scheduler. The constructor will be
    // used if coroutine function has the same signature. The unused parameters
    // are here only to match the signature.
    template <typename Coro>
    promise_type(std::function<void(std::coroutine_handle<>)> scheduler,
                 Coro&&) noexcept
        : m_scheduler(scheduler) {}

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
            // Don't skip final suspension
            bool await_ready() const noexcept { return false; }
            // Resume parent coroutine on its own scheduler
            void await_suspend(handle_type handle) noexcept {
                auto parent = handle.promise().m_parent;
                auto parent_scheduler = handle.promise().m_parent_scheduler;
                if (parent && parent_scheduler) {
                    parent_scheduler(parent);
                }
            }
            // No action needed on resume
            void await_resume() const noexcept {}
        };
        return final_awaiter{};
    }
    // Required by coroutines: capture exceptions for later rethrowing
    void unhandled_exception() { m_exception = std::current_exception(); }
};

template <typename ResultType>
template <detail::schedule_on::concepts::HasScheduler T>
inline void Task<ResultType>::await_suspend(
    std::coroutine_handle<T> handle) noexcept {
    m_coroutine.promise().m_parent = handle;
    m_coroutine.promise().m_parent_scheduler = handle.promise().get_scheduler();
    m_coroutine.promise().reschedule();
}

template <typename ResultType>
inline typename Task<ResultType>::result_type Task<ResultType>::await_resume()
    const {
    if (m_coroutine.promise().m_exception) {
        std::rethrow_exception(m_coroutine.promise().m_exception);
    }
    if constexpr (std::same_as<result_type, void>) {
        return;
    } else {
        return std::move(m_coroutine.promise().m_value).value();
    }
}
}  // namespace detail::schedule_on

template <typename Coro>
auto schedule_on(std::function<void(std::coroutine_handle<>)>, Coro coro)
    -> detail::schedule_on::Task<typename Coro::result_type> {
    co_return co_await coro;
}

}  // namespace CoroutineTests::alien
