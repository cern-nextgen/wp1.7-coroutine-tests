#ifndef COROUTINETESTS_LAZY_H
#define COROUTINETESTS_LAZY_H

#include <coroutine>
#include <exception>

namespace CoroutineTests {

namespace detail {

// Simple coroutine wrapper that returns a value lazily or eagerly.
// Co await and co yield are disabled, only co return is allowed.
// No custom allocator is used. Exceptions are caught and rethrown.
template <typename T, bool is_lazy>
class [[nodiscard]] MaybeLazy {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    MaybeLazy(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~MaybeLazy() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    MaybeLazy() = default;
    MaybeLazy(const MaybeLazy&) = delete;
    MaybeLazy& operator=(const MaybeLazy&) = delete;
    MaybeLazy(MaybeLazy&& other) noexcept
        : m_coroutine{other.m_coroutine} {
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

    T get() {
        if (!m_coroutine.done()) {
            m_coroutine.resume();
        }
        if (m_coroutine.promise().exception) {
            std::rethrow_exception(m_coroutine.promise().exception);
        }
        return m_coroutine.promise().current_value;
    }
    bool done() const { return m_coroutine.done(); }

    private:
    handle_type m_coroutine;
};

template <typename T, bool is_lazy>
struct MaybeLazy<T, is_lazy>::promise_type {
    T current_value;
    std::exception_ptr exception;
    // required by coroutines
    MaybeLazy<T, is_lazy> get_return_object() {
        return {MaybeLazy<T, is_lazy>::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    auto initial_suspend() {
        if constexpr (is_lazy) {
            return std::suspend_always{};  // suspend execution
        } else {
            return std::suspend_never{};  // start execution immediately
        }
    }
    // called on coroutine completion
    std::suspend_always final_suspend() noexcept { return {}; }
    // called on co_yield
    std::suspend_always yield_value(T value) = delete;  // no co yield allowed
    // called on (implicit or explicit) co_return or co_return void. Conflicts
    // return_void.
    void return_value(T value) { current_value = std::move(value); }
    // disable co_await inside coroutine
    void await_transform() = delete;
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { exception = std::current_exception(); }
};
}  // namespace detail

template <typename T>
using Lazy = detail::MaybeLazy<T, true>;
template <typename T>
using Eager = detail::MaybeLazy<T, false>;

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_LAZY_H
