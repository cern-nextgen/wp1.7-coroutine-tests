#ifndef COROUTINETESTS_ALIEN_WHEN_ALL_H
#define COROUTINETESTS_ALIEN_WHEN_ALL_H

#include <array>
#include <atomic>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

template <typename T>
concept HasScheduler = requires(T t) {
    {
        t.get_scheduler()
    } -> std::convertible_to<std::function<void(std::coroutine_handle<>)>>;
};

namespace CoroutineTests::alien::detail {

template <typename T>
decltype(auto) get_awaiter(T&& value) {
    if constexpr (requires { std::forward<T>(value).operator co_await(); }) {
        return std::forward<T>(value).operator co_await();
    } else if constexpr (requires {
                             operator co_await(std::forward<T>(value));
                         }) {
        return operator co_await(std::forward<T>(value));
    } else {
        return std::forward<T>(value);
    }
}

template <typename T>
using awaiter_t =
    std::remove_reference_t<decltype(get_awaiter(std::declval<T>()))>;

template <typename T>
using await_result_t = decltype(std::declval<awaiter_t<T>&>().await_resume());

template <typename T>
using stored_result_t = std::conditional_t<std::is_void_v<await_result_t<T>>,
                                           std::monostate, await_result_t<T>>;

}  // namespace CoroutineTests::alien::detail

// Helper coroutine type to synchronize multiple coroutines inside
// WhenAllAwaitable
class [[nodiscard]] helper_task {
    public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    explicit helper_task(handle_type h) : m_coroutine(h) {}
    ~helper_task() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    helper_task() = default;
    helper_task(const helper_task&) = delete;
    helper_task& operator=(const helper_task&) = delete;
    helper_task(helper_task&& other) noexcept : m_coroutine(other.m_coroutine) {
        other.m_coroutine = {};
    }
    helper_task& operator=(helper_task&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }

    handle_type handle() const noexcept { return m_coroutine; }

    struct promise_type {
        std::coroutine_handle<> parent{};
        std::atomic_size_t* remaining{};
        std::exception_ptr* exception{};
        std::mutex* exception_mutex{};
        std::function<void(std::coroutine_handle<>)> scheduler;

        template <class A, class O>
        promise_type(A*, O*, std::coroutine_handle<> parent_,
                     std::atomic_size_t* remaining_,
                     std::exception_ptr* exception_,
                     std::mutex* exception_mutex_,
                     std::function<void(std::coroutine_handle<>)> scheduler_)
            : parent(parent_),
              remaining(remaining_),
              exception(exception_),
              exception_mutex(exception_mutex_),
              scheduler(std::move(scheduler_)) {}

        // Accessor for scheduler used by child coroutines
        const auto& get_scheduler() const { return scheduler; }
        // Schedule resumption of this helper
        void reschedule() { scheduler(handle_type::from_promise(*this)); }

        // Required by coroutines: create the object
        helper_task get_return_object() {
            return helper_task{handle_type::from_promise(*this)};
        }

        // Required by coroutines: suspend immediately on start (lazy execution)
        std::suspend_always initial_suspend() const noexcept { return {}; }
        // Required by coroutines: handle completion and resume parent if needed
        auto final_suspend() const noexcept {
            struct final_awaiter {
                // Don't skip final supersession
                bool await_ready() const noexcept { return false; }
                // On suspend, indicate completion and resume parent if this was
                // the last child
                void await_suspend(handle_type h) noexcept {
                    auto& promise = h.promise();
                    if (!promise.remaining) {
                        return;
                    }
                    if (promise.remaining->fetch_sub(1) == 1) {
                        promise.scheduler(promise.parent);
                    }
                }
                // Nothing special on resume
                void await_resume() const noexcept {}
            };
            return final_awaiter{};
        }
        // Required by coroutines: handle co_return void
        void return_void() noexcept {}

        void unhandled_exception() noexcept {
            if (!exception) {
                return;
            }

            if (exception_mutex) {
                std::scoped_lock lock(*exception_mutex);
                if (!*exception) {
                    *exception = std::current_exception();
                }
            } else {
                if (!*exception) {
                    *exception = std::current_exception();
                }
            }
        }
    };

    private:
    handle_type m_coroutine = nullptr;
};

// helper task to co_await forwarded awaitable and store result
template <typename Awaitable>
helper_task make_helper_task(
    Awaitable* awaitable,
    std::optional<CoroutineTests::alien::detail::stored_result_t<Awaitable>>*
        out,
    std::coroutine_handle<> parent, std::atomic_size_t* remaining,
    std::exception_ptr* exception, std::mutex* exception_mutex,
    std::function<void(std::coroutine_handle<>)> scheduler) {
    (void)parent;
    (void)remaining;
    (void)exception;
    (void)exception_mutex;
    (void)scheduler;

    if constexpr (std::is_void_v<CoroutineTests::alien::detail::await_result_t<
                      Awaitable>>) {
        co_await *awaitable;
        if (out) {
            out->emplace(std::monostate{});
        }
    } else {
        auto result = co_await *awaitable;
        if (out) {
            out->emplace(std::move(result));
        }
    }
}

template <typename... Awaitables>
class WhenAllAwaitable {
    public:
    explicit WhenAllAwaitable(Awaitables&&... awaitables)
        : m_awaitables(std::forward<Awaitables>(awaitables)...),
          m_remaining(sizeof...(Awaitables)) {}

    // skip suspension if no awaitables
    bool await_ready() const noexcept { return sizeof...(Awaitables) == 0; }

    // on suspend, store handle to parent and start helper tasks for each
    // awaitable
    template <HasScheduler T>
    void await_suspend(std::coroutine_handle<T> parent) {
        m_parent = parent;
        m_scheduler = parent.promise().get_scheduler();
        start_all(std::make_index_sequence<sizeof...(Awaitables)>{});
    }
    // on resume, return tuple of results or rethrow first exception
    auto await_resume() {
        if (m_exception) {
            std::rethrow_exception(m_exception);
        }
        return take_results(std::make_index_sequence<sizeof...(Awaitables)>{});
    }

    private:
    // helper to start all awaitables
    template <std::size_t... I>
    void start_all(std::index_sequence<I...>) {
        (start_one<I>(), ...);
    }

    // helper to create and start helper task awaiting the I-th awaitable
    template <std::size_t I>
    void start_one() {
        using awaitable_t = std::tuple_element_t<I, std::tuple<Awaitables...>>;
        auto& awaitable = std::get<I>(m_awaitables);
        auto& out = std::get<I>(m_results);

        m_tasks[I] = make_helper_task<awaitable_t>(
            std::addressof(awaitable), std::addressof(out), m_parent,
            &m_remaining, &m_exception, &m_exception_mutex, m_scheduler);

        m_scheduler(m_tasks[I].handle());
    }

    // helper to put the results into a tuple
    template <std::size_t... I>
    auto take_results(std::index_sequence<I...>) {
        return std::tuple<
            CoroutineTests::alien::detail::stored_result_t<Awaitables>...>{
            std::move(*std::get<I>(m_results))...};
    }

    private:
    // forwarded awaitables
    std::tuple<Awaitables...> m_awaitables;
    // parent coroutine handle
    std::coroutine_handle<> m_parent{};
    // scheduler to resume coroutines
    std::function<void(std::coroutine_handle<>)> m_scheduler;
    // helper tasks awaiting each awaitable
    std::array<helper_task, sizeof...(Awaitables)> m_tasks{};
    // storage for results of each awaitable
    std::tuple<std::optional<
        CoroutineTests::alien::detail::stored_result_t<Awaitables>>...>
        m_results{};
    // counter for remaining unfinished awaitables
    std::atomic_size_t m_remaining{0};
    // mutex protecting exception storage
    std::mutex m_exception_mutex;
    // first exception thrown by any awaitable
    std::exception_ptr m_exception{};
};

// factory function to create WhenAllAwaitable
template <typename... Awaitables>
auto when_all(Awaitables&&... awaitables) {
    return WhenAllAwaitable<Awaitables...>(
        std::forward<Awaitables>(awaitables)...);
}

#endif  // COROUTINETESTS_ALIEN_WHEN_ALL_H
