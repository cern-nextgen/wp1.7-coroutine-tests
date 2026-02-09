#ifndef COROUTINETESTS_ALIEN_SYNC_WAIT_H
#define COROUTINETESTS_ALIEN_SYNC_WAIT_H

#include <coroutine>
#include <exception>
#include <functional>
#include <latch>
#include <optional>
#include <type_traits>
#include <utility>

namespace CoroutineTests::alien {

namespace detail::sync_wait {

// deduce the result type assuming the type is an awaitable (doesn't check for
// operator co_await or await_transform)
template <typename Coro>
using result_t = decltype(std::declval<Coro>().await_resume());

namespace concepts {
template <typename T>
concept HasScheduler = requires(T t) {
    {
        t.get_scheduler()
    } -> std::convertible_to<std::function<void(std::coroutine_handle<>)>>;
};

template <typename Coro>
concept AlienCoroutine = requires(Coro coro) {
    typename Coro::promise_type;
    requires HasScheduler<typename Coro::promise_type>;
    { coro.await_resume() };
    typename result_t<Coro>;
};

}  // namespace concepts

// Helper for handling co_return in promise_type, default implementation for
// non-void ResultType
template <typename ResultType>
struct ReturnHelper {
    template <typename T>
        requires std::constructible_from<ResultType, T&&>
    void return_value(T&& value) {
        m_value.emplace(std::forward<T>(value));
    }

    std::optional<ResultType> m_value;
};

// Specialization for void return type
template <>
struct ReturnHelper<void> {
    void return_void() noexcept {}
};

// Helper coroutine type for implementing sync_wait
// Wraps another coroutine and synchronize its completion
template <typename ResultType>
class [[nodiscard]] SyncWaitTask {

    static_assert(
        std::movable<ResultType> || std::same_as<ResultType, void>,
        "SyncWaitTask<ResultType> requires ResultType to be movable or void");

    public:
    using result_type = ResultType;

    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    // Constructor from coroutine handle
    explicit SyncWaitTask(handle_type h) : m_coroutine(h) {}
    ~SyncWaitTask() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    SyncWaitTask() = default;
    SyncWaitTask(const SyncWaitTask&) = delete;
    SyncWaitTask& operator=(const SyncWaitTask&) = delete;
    SyncWaitTask(SyncWaitTask&& other) noexcept
        : m_coroutine(other.m_coroutine) {
        other.m_coroutine = {};
    }
    SyncWaitTask& operator=(SyncWaitTask&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }

    // Run the coroutine and wait for its completion, then return the result or
    // rethrow the exception
    ResultType run(std::function<void(std::coroutine_handle<>)> scheduler) {
        m_coroutine.promise().m_scheduler = std::move(scheduler);
        m_coroutine.promise().m_scheduler(m_coroutine);
        m_coroutine.promise().m_done.wait();

        if (m_coroutine.promise().m_exception) {
            std::rethrow_exception(m_coroutine.promise().m_exception);
        }

        if constexpr (std::is_void_v<ResultType>) {
            return;
        } else {
            return std::move(m_coroutine.promise().m_value).value();
        }
    }

    private:
    handle_type m_coroutine = nullptr;
};

template <typename ResultType>
struct SyncWaitTask<ResultType>::promise_type : ReturnHelper<ResultType> {
    // Storage for exceptions thrown in the coroutine body
    std::exception_ptr m_exception;
    // Handle to scheduler to resume this coroutine and propagate to children
    std::function<void(std::coroutine_handle<>)> m_scheduler;
    // Latch to signal completion of the coroutine
    std::latch m_done{1};

    // Accessor for scheduler used by child coroutines
    const auto& get_scheduler() const { return m_scheduler; }
    // Schedule resumption of this task
    void reschedule() { m_scheduler(handle_type::from_promise(*this)); }

    // Required by coroutines: create the object
    SyncWaitTask get_return_object() {
        return SyncWaitTask{handle_type::from_promise(*this)};
    }

    // Required by coroutines: suspend immediately on start (lazy execution)
    std::suspend_always initial_suspend() const noexcept { return {}; }

    // Required by coroutines: notify completion
    auto final_suspend() const noexcept {
        struct final_awaiter {
            bool await_ready() const noexcept { return false; }
            void await_suspend(handle_type h) const noexcept {
                h.promise().m_done.count_down();
            }
            void await_resume() const noexcept {}
        };
        return final_awaiter{};
    }

    // Required by coroutines: capture exceptions for later rethrowing
    void unhandled_exception() { m_exception = std::current_exception(); }
};

// Helper to create a SyncWaitTask from a coroutine, forwarding the result type
template <typename Coro>
    requires concepts::AlienCoroutine<Coro> && std::movable<Coro>
SyncWaitTask<typename Coro::result_type> make_sync_wait_task(Coro coro) {
    if constexpr (std::is_void_v<typename Coro::result_type>) {
        co_await std::move(coro);
        co_return;
    } else {
        co_return co_await std::move(coro);
    }
}

}  // namespace detail::sync_wait

template <typename Coro>
    requires detail::sync_wait::concepts::AlienCoroutine<Coro>
auto sync_wait(std::function<void(std::coroutine_handle<>)> scheduler,
               Coro coro) -> detail::sync_wait::result_t<Coro> {
    using result_type = detail::sync_wait::result_t<Coro>;
    auto task = detail::sync_wait::make_sync_wait_task<Coro>(std::move(coro));

    if constexpr (std::is_void_v<result_type>) {
        task.run(std::move(scheduler));
        return;
    } else {
        return task.run(std::move(scheduler));
    }
}

}  // namespace CoroutineTests::alien

#endif  // COROUTINETESTS_ALIEN_SYNC_WAIT_H
