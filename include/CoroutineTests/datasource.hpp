#ifndef COROUTINETESTS_DATASOURCE_H
#define COROUTINETESTS_DATASOURCE_H

#include <coroutine>
#include <exception>
#include <stdexcept>

namespace CoroutineTests {

// Coroutine that pushes data to outside.
// Doesn't return a value, doesn't yield. Rethrows exceptions on get.
// Effectively for producing data from the coroutine co_yield could be used, but
// here a more generic approach using only co_await is exercised.
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
    // resume coroutine and get the value
    T get() const {
        if (m_coroutine) {
            if (!m_coroutine.done()) {
                m_coroutine.resume();
                if (m_coroutine.promise().m_exception) {
                    std::rethrow_exception(m_coroutine.promise().m_exception);
                }
                return m_coroutine.promise().m_current_output;
            }
            throw std::runtime_error("Calling get() on a done coroutine.");
        }
        throw std::logic_error("get() called on an invalid coroutine handle");
    }

    private:
    handle_type m_coroutine;
};

template <typename T>
struct DataSource<T>::promise_type {
    // storage for exceptions thrown in the coroutine
    std::exception_ptr m_exception;
    // storage for the latest value to be pushed to outside
    T m_current_output{};

    // required by coroutines
    DataSource get_return_object() {
        return {DataSource::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    std::suspend_always initial_suspend() const { return {}; }
    // called on coroutine completion
    std::suspend_always final_suspend() const noexcept { return {}; }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { m_exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return void
    void return_void() const {}
};

// Simple awaiter that allows to receive data from the coroutine.
// co_await OutputAwaiter{some_value};
// This could be potentially replaced by just co_yield
template <typename T>
struct OutputAwaiter {
    OutputAwaiter(T value) : m_value(value) {}
    T m_value;
    // don't resume immediately
    bool await_ready() const { 
        return false; 
    }
    // copy data from awaiter to the promise of coroutine that suspended
    void await_suspend(
        std::coroutine_handle<typename DataSource<T>::promise_type> h) {
        h.promise().m_current_output = m_value;
    }
    // nothing special on resume
    void await_resume() const {}
};

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_DATASOURCE_H
