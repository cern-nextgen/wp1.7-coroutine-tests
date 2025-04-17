#ifndef COROUTINETESTS_SIMPLEGENERATOR_H
#define COROUTINETESTS_SIMPLEGENERATOR_H

#include <coroutine>
#include <exception>

namespace CoroutineTests {

// Simple coroutine that can yield values.
// Doesn't return a value. Rethrows exceptions on get.
template <typename T>
class [[nodiscard]] SimpleGenerator {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    SimpleGenerator(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~SimpleGenerator() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    SimpleGenerator() = default;
    SimpleGenerator(const SimpleGenerator&) = delete;
    SimpleGenerator& operator=(const SimpleGenerator&) = delete;
    SimpleGenerator(SimpleGenerator&& other) noexcept
        : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    SimpleGenerator& operator=(SimpleGenerator&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }
    // resume the coroutine from outside and get yielded value
    T get() const {
        if (!m_coroutine.done()) {
            m_coroutine.resume();
        }
        if (m_coroutine.promise().m_exception) {
            std::rethrow_exception(m_coroutine.promise().m_exception);
        }
        return m_coroutine.promise().m_value;
    }
    // check if finished from outside
    inline bool done() const { return m_coroutine.done(); }

    private:
    handle_type m_coroutine;
};

template <typename T>
struct SimpleGenerator<T>::promise_type {
    // storage for exceptions thrown in the coroutine
    std::exception_ptr m_exception;
    // yielded value
    T m_value{};
    // required by coroutines
    SimpleGenerator get_return_object() {
        return {SimpleGenerator::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    std::suspend_always initial_suspend() const { return {}; }
    // called on coroutine completion
    std::suspend_always final_suspend() const noexcept { return {}; }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { m_exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return_void
    void return_value(T value) {
        m_value = std::move(value);
    }
    std::suspend_always yield_value(T value) {
        m_value = std::move(value);
        return {};
    }
};

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_SIMPLEGENERATOR_H
