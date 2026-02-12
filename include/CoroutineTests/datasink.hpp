#pragma once

#include <coroutine>
#include <exception>

namespace CoroutineTests {

// Coroutine that receives data from outside.
// Doesn't return a value, doesn't yield. Rethrows exceptions on put.
template <typename T>
class [[nodiscard]] DataSink {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    DataSink(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~DataSink() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    DataSink() = default;
    DataSink(const DataSink&) = delete;
    DataSink& operator=(const DataSink&) = delete;
    DataSink(DataSink&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    DataSink& operator=(DataSink&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }
    // copy data to the coroutine and resume it
    void put(T value) const {
        if (m_coroutine && !m_coroutine.done()) {
            m_coroutine.promise().m_current_input = value;
            m_coroutine.resume();
            if (m_coroutine.promise().m_exception) {
                std::rethrow_exception(m_coroutine.promise().m_exception);
            }
        }
    }

    private:
    handle_type m_coroutine;
};

template <typename T>
struct DataSink<T>::promise_type {
    // storage for exceptions thrown in the coroutine
    std::exception_ptr m_exception;
    // storage for the latest value received from outside
    T m_current_input{};

    // required by coroutines
    DataSink get_return_object() {
        return {DataSink::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    // resume immediately and proceed to first co_await
    std::suspend_never initial_suspend() const { return {}; }
    // called on coroutine completion
    std::suspend_always final_suspend() const noexcept { return {}; }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { m_exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return void
    void return_void() const {}
};

// Simple await that allows to push data to the coroutine.
// T received = co_await InputAwaiter<T>{};
template <typename T>
struct InputAwaiter {
    using handle_type =
        std::coroutine_handle<typename DataSink<T>::promise_type>;
    // storage for the handle of a coroutine that suspended on co_await
    handle_type m_coroutine;
    // don't resume immediately
    bool await_ready() const { return false; }
    // copy handle to the coroutine that suspended on co_await
    void await_suspend(handle_type h) { m_coroutine = h; }
    // resume the coroutine and return the value taken from promise
    T await_resume() const { return m_coroutine.promise().m_current_input; }
};

}  // namespace CoroutineTests
