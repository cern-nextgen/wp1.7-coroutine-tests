#ifndef COROUTINETESTS_DATASOURCE_H
#define COROUTINETESTS_DATASOURCE_H

#include <coroutine>
#include <exception>
#include <stdexcept>

namespace CoroutineTests {

template <typename T>
class [[nodiscard]] DataSource {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    DataSource(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~DataSource() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    DataSource() = default;
    DataSource(const DataSource&) = delete;
    DataSource& operator=(const DataSource&) = delete;
    DataSource(DataSource&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    DataSource& operator=(DataSource&& other) noexcept {
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
        if (m_coroutine) {
            if (!m_coroutine.done()) {
                m_coroutine.resume();
                if (m_coroutine.promise().exception) {
                    std::rethrow_exception(m_coroutine.promise().exception);
                }
            }
            return m_coroutine.promise().current_output;
        }
        throw std::logic_error("get() called on an invalid coroutine handle");
    }

    private:
    handle_type m_coroutine;
};

template <typename T>
struct DataSource<T>::promise_type {
    std::exception_ptr exception;
    T current_output{};

    // required by coroutines
    DataSource get_return_object() {
        return {DataSource::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    std::suspend_always initial_suspend() { return {}; }
    // called on coroutine completion
    std::suspend_always final_suspend() noexcept { return {}; }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return_void
    void return_void() {}
};

template <typename T>
struct OutputAwaiter {
    OutputAwaiter(T value) : value(value) {}
    T value;
    bool await_ready() { return false; }
    void await_suspend(
        std::coroutine_handle<typename DataSource<T>::promise_type> h) {
        h.promise().current_output = value;
    }
    void await_resume() {}
};

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_DATASOURCE_H
