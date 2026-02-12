#pragma once

#include <atomic>
#include <condition_variable>
#include <coroutine>
#include <exception>
#include <functional>
#include <mutex>
#include <type_traits>
#include <utility>

namespace CoroutineTests::alien {

namespace detail::counting_scope {

// Deduce the result type assuming the type is an awaitable (doesn't check for
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
}  // namespace detail::counting_scope

// A minimal counting scope compatible with alien-style coroutines.
// Allows to eagerly start multiple tasks (returning void) and wait for their
// completion.
class counting_scope {
    public:
    counting_scope() = default;
    counting_scope(const counting_scope&) = delete;
    counting_scope& operator=(const counting_scope&) = delete;
    counting_scope(counting_scope&&) = delete;
    counting_scope& operator=(counting_scope&&) = delete;
    ~counting_scope() = default;

    // Eagerly start a new task in this scope using the provided scheduler and
    // coroutine. The counting_scope keeps track of the number of unfinished
    // tasks.
    template <typename Coro>
        requires detail::counting_scope::concepts::AlienCoroutine<Coro> &&
                 std::is_same_v<detail::counting_scope::result_t<Coro>, void>
    void spawn(std::function<void(std::coroutine_handle<>)> scheduler,
               Coro coro) {
        m_count += 1;
        auto task =
            make_detached_task(std::move(coro), *this, std::move(scheduler));
        task.start_detached();
    }
    // Wait for all tasks in this scope to complete and rethrow the first
    // exception if any task threw.
    void join() {
        if (m_count != 0) {
            std::unique_lock lock(m_join_mutex);
            m_join_cv.wait(lock, [&] { return m_count == 0; });
        }
        if (auto exception = take_exception()) {
            std::rethrow_exception(exception);
        }
    }

    private:
    // Store exception unless something is already stored in which case
    // keep the original one and ignore the new one
    void store_exception(std::exception_ptr exception) noexcept {
        if (!exception) {
            return;
        }
        std::scoped_lock lock(m_exception_mutex);
        if (!m_exception) {
            m_exception = std::move(exception);
        }
    }

    // Take stored exception for rethrowing and clear the storage
    std::exception_ptr take_exception() noexcept {
        std::scoped_lock lock(m_exception_mutex);
        return std::exchange(m_exception, nullptr);
    }

    // Called by helper tasks when they complete.
    // If this was the last unfinished task then notify to stop the wait.
    void on_task_complete() noexcept {
        if (m_count.fetch_sub(1) != 1) {
            return;
        }
        std::scoped_lock lock(m_join_mutex);
        m_join_cv.notify_all();
    }

    // Helper coroutine type for wrapping other coroutines and notifying the
    // counting_scope on completion.
    class [[nodiscard]] DetachedTask {
        public:
        struct promise_type {
            counting_scope& m_scope;
            std::function<void(std::coroutine_handle<>)> m_scheduler;

            // Non-default constructor to pass counting_scope reference and
            // scheduler The constructor will be used if coroutine function has
            // the same signature The unused parameters are here only to match
            // the signature
            template <class Coro>
            promise_type(
                Coro&& /*coro*/, counting_scope& scope,
                std::function<void(std::coroutine_handle<>)> scheduler) noexcept
                : m_scope(scope), m_scheduler(std::move(scheduler)) {}

            // Provide scheduler propagation for alien-style awaitables.
            const auto& get_scheduler() const { return m_scheduler; }
            // Schedule resumption of this helper
            void reschedule() { m_scheduler(handle_type::from_promise(*this)); }

            // Required by coroutines: create the object
            DetachedTask get_return_object() {
                return DetachedTask{handle_type::from_promise(*this)};
            }

            // Required by coroutines: suspend immediately on start
            std::suspend_always initial_suspend() const noexcept { return {}; }

            // Required by coroutines: handle completion and resume parent if
            // needed
            auto final_suspend() const noexcept {
                struct final_awaiter {
                    // Don't skip final suspension
                    bool await_ready() const noexcept { return false; }
                    // On suspend, indicate completion and destroy the coroutine
                    // handle
                    void await_suspend(handle_type h) const noexcept {
                        auto& promise = h.promise();
                        promise.m_scope.on_task_complete();
                        h.destroy();
                    }
                    // Nothing special on resume since the task is already
                    // complete
                    void await_resume() const noexcept {}
                };
                return final_awaiter{};
            }

            void return_void() noexcept {}

            void unhandled_exception() noexcept {
                m_scope.store_exception(std::current_exception());
            }
        };

        using handle_type = std::coroutine_handle<promise_type>;

        explicit DetachedTask(handle_type h) : m_coroutine(h) {}
        DetachedTask() = default;
        DetachedTask(const DetachedTask&) = delete;
        DetachedTask& operator=(const DetachedTask&) = delete;
        DetachedTask(DetachedTask&& other) noexcept
            : m_coroutine(std::exchange(other.m_coroutine, {})) {}
        DetachedTask& operator=(DetachedTask&& other) noexcept {
            if (this != &other) {
                if (m_coroutine) {
                    m_coroutine.destroy();
                }
                m_coroutine = std::exchange(other.m_coroutine, {});
            }
            return *this;
        }
        ~DetachedTask() {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
        }

        // Release the handle and start executing on the scheduler
        void start_detached() noexcept {
            auto handle = std::exchange(m_coroutine, {});
            handle.promise().reschedule();
        }

        private:
        handle_type m_coroutine{};
    };

    // Create a DetachedTask that wraps the provided coroutine and notifies the
    // counting_scope on completion.
    template <typename Coro>
        requires detail::counting_scope::concepts::AlienCoroutine<Coro> &&
                 std::is_same_v<detail::counting_scope::result_t<Coro>, void>
    static DetachedTask make_detached_task(
        Coro awaitable, counting_scope& /*scope*/,
        std::function<void(std::coroutine_handle<>)> /*scheduler*/) {
        co_await std::move(awaitable);
        co_return;
    }

    private:
    std::atomic_size_t m_count{0};  // number of unfinished tasks in this scope

    std::mutex m_join_mutex;  // coordinates join waiting and task completion
    std::condition_variable m_join_cv;  // notified when m_count reaches 0

    std::mutex m_exception_mutex;  // protects m_exception
    std::exception_ptr
        m_exception;  // first exception thrown by any task in this scope
};

}  // namespace CoroutineTests::alien
