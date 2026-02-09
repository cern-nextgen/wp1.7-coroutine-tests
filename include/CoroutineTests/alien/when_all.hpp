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
#include <variant>

namespace CoroutineTests::alien {

namespace detail::when_all {
namespace concepts {
template <typename T>
concept HasScheduler = requires(T t) {
    {
        t.get_scheduler()
    } -> std::convertible_to<std::function<void(std::coroutine_handle<>)>>;
};
}  // namespace concepts

// get awaiter from an awaitable, try out different ways according to the
// standard (await_transform not included)
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

// type of the awaiter obtained from an awaitable
template <typename T>
using awaiter_t =
    std::remove_reference_t<decltype(get_awaiter(std::declval<T>()))>;

// type returned by await_resume of the awaiter
template <typename T>
using await_result_t = decltype(std::declval<awaiter_t<T>&>().await_resume());

// type to store the result of awaited awaitable
// if void, use monostate, else use the actual result type
template <typename T>
using stored_result_t = std::conditional_t<std::is_void_v<await_result_t<T>>,
                                           std::monostate, await_result_t<T>>;

// Helper coroutine type to synchronize multiple coroutines inside
// WhenAllAwaitable
class [[nodiscard]] HelperTask {
    public:
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    explicit HelperTask(handle_type h) : m_coroutine(h) {}
    ~HelperTask() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    HelperTask() = default;
    HelperTask(const HelperTask&) = delete;
    HelperTask& operator=(const HelperTask&) = delete;
    HelperTask(HelperTask&& other) noexcept : m_coroutine(other.m_coroutine) {
        other.m_coroutine = {};
    }
    HelperTask& operator=(HelperTask&& other) noexcept {
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
        std::coroutine_handle<> m_parent{};
        std::atomic_size_t& m_remaining;
        std::exception_ptr m_exception;
        std::mutex& m_exception_mutex;
        std::function<void(std::coroutine_handle<>)>& m_scheduler;

        // Non-default constructor to pass context from WhenAllAwaitable
        // The constructor will be used if coroutine function has the same
        // signature Unused parameters are only to match the signature
        template <class Awaitable, class Output>
        promise_type(Awaitable&, Output&, std::coroutine_handle<> parent,
                     std::atomic_size_t& remaining,
                     std::exception_ptr exception, std::mutex& exception_mutex,
                     std::function<void(std::coroutine_handle<>)>& scheduler)
            : m_parent(parent),
              m_remaining(remaining),
              m_exception(exception),
              m_exception_mutex(exception_mutex),
              m_scheduler(scheduler) {}

        // Accessor for scheduler used by child coroutines
        const auto& get_scheduler() const { return m_scheduler; }
        // Schedule resumption of this helper
        void reschedule() { m_scheduler(handle_type::from_promise(*this)); }

        // Required by coroutines: create the object
        HelperTask get_return_object() {
            return HelperTask{handle_type::from_promise(*this)};
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
                    if (promise.m_remaining.fetch_sub(1) == 1) {
                        promise.m_scheduler(promise.m_parent);
                    }
                }
                // Nothing special on resume
                void await_resume() const noexcept {}
            };
            return final_awaiter{};
        }
        // Required by coroutines: handle co_return void
        void return_void() noexcept {}

        // Required by coroutines: store first exception into shared storage
        void unhandled_exception() noexcept {
            std::scoped_lock lock(m_exception_mutex);
            if (!m_exception) {
                m_exception = std::current_exception();
            }
        }
    };

    private:
    handle_type m_coroutine = nullptr;
};

// helper task to co_await forwarded awaitable and store result
// The unused arguments are are used implicitly for by HelperTask promise type
// construction
template <typename Awaitable>
HelperTask make_helper_task(
    Awaitable& awaitable, std::optional<stored_result_t<Awaitable>>& out,
    std::coroutine_handle<> /*parent*/, std::atomic_size_t& /*remaining*/,
    std::exception_ptr /*exception*/, std::mutex& /*exception_mutex*/,
    std::function<void(std::coroutine_handle<>)> /*scheduler*/) {

    if constexpr (std::is_void_v<await_result_t<Awaitable>>) {
        co_await awaitable;
        out.emplace(std::monostate{});
    } else {
        auto result = co_await awaitable;
        out.emplace(std::move(result));
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
    template <concepts::HasScheduler T>
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
            awaitable, out, m_parent, m_remaining, m_exception,
            m_exception_mutex, m_scheduler);

        m_scheduler(m_tasks[I].handle());
    }

    // helper to put the results into a tuple
    // the void in results is represented by monostate
    template <std::size_t... I>
    auto take_results(std::index_sequence<I...>) {
        return std::tuple<detail::when_all::stored_result_t<Awaitables>...>{
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
    std::array<detail::when_all::HelperTask, sizeof...(Awaitables)> m_tasks{};
    // storage for results of each awaitable
    std::tuple<std::optional<detail::when_all::stored_result_t<Awaitables>>...>
        m_results{};
    // counter for remaining unfinished awaitables
    std::atomic_size_t m_remaining{0};
    // mutex protecting exception storage
    std::mutex m_exception_mutex;
    // first exception thrown by any awaitable
    std::exception_ptr m_exception{};
};

}  // namespace detail::when_all

// factory function to create WhenAllAwaitable
template <typename... Awaitables>
auto when_all(Awaitables&&... awaitables) {
    return detail::when_all::WhenAllAwaitable<Awaitables...>(
        std::forward<Awaitables>(awaitables)...);
}

}  // namespace CoroutineTests::alien
#endif  // COROUTINETESTS_ALIEN_WHEN_ALL_H
