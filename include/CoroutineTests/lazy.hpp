#pragma once

#include <coroutine>
#include <exception>

namespace CoroutineTests {

namespace detail {

// Simple coroutine wrapper that returns a value lazily or eagerly.
// co_yield is disabled, only co_return is allowed.
// No custom allocator is used. Exceptions are caught and rethrown.
template <typename T, bool is_lazy>
class [[nodiscard]] MaybeLazy {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    explicit MaybeLazy(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~MaybeLazy() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    MaybeLazy() = default;
    MaybeLazy(const MaybeLazy&) = delete;
    MaybeLazy& operator=(const MaybeLazy&) = delete;
    MaybeLazy(MaybeLazy&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    MaybeLazy& operator=(MaybeLazy&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }

    T get() const {
        if (!m_coroutine.done()) {
            m_coroutine.resume();
        }
        if (m_coroutine.promise().m_exception) {
            std::rethrow_exception(m_coroutine.promise().m_exception);
        }
        return m_coroutine.promise().m_current_value;
    }
    bool done() const { return m_coroutine.done(); }

    private:
    handle_type m_coroutine;
};

template <typename T, bool is_lazy>
struct MaybeLazy<T, is_lazy>::promise_type {
    T m_current_value;
    std::exception_ptr m_exception;
    // required by coroutines
    MaybeLazy get_return_object() {
        return MaybeLazy{MaybeLazy::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    auto initial_suspend() const {
        if constexpr (is_lazy) {
            return std::suspend_always{};  // suspend execution
        } else {
            return std::suspend_never{};  // start execution immediately
        }
    }
    // called on coroutine completion
    std::suspend_always final_suspend() const noexcept { return {}; }
    // called on co_yield
    std::suspend_always yield_value(T value) = delete;  // no co yield allowed
    // called on (implicit or explicit) co_return or co_return void. Conflicts
    // return_void.
    void return_value(T value) { m_current_value = std::move(value); }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { m_exception = std::current_exception(); }
};
}  // namespace detail

template <typename T>
using Lazy = detail::MaybeLazy<T, true>;
template <typename T>
using Eager = detail::MaybeLazy<T, false>;

}  // namespace CoroutineTests
