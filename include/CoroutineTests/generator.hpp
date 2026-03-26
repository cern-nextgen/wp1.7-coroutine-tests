#pragma once

#include <coroutine>
#include <exception>
#include <ranges>

namespace CoroutineTests {

// Simple generator that yields values of type T.
// The generator is a range and can be used in range-based for loops or standard
// algorithms.
// Uses default memory allocation.
template <typename T>
class [[nodiscard]] Generator
    : public std::ranges::view_interface<Generator<T>> {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    explicit Generator(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~Generator() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    Generator() = default;
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    Generator(Generator&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }

    // begin and end required by view_interface
    class Iter;
    Iter begin() const {
        if (m_coroutine) {
            m_coroutine.resume();
        }
        return Iter{m_coroutine};
    }
    std::default_sentinel_t end() const { return {}; }

    private:
    handle_type m_coroutine;
};

template <typename T>
struct Generator<T>::promise_type {
    T m_current_value;
    std::exception_ptr m_exception;
    // required by coroutines
    Generator get_return_object() {
        return Generator{Generator<T>::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    std::suspend_always initial_suspend() const { return {}; }
    // called on coroutine completion
    std::suspend_always final_suspend() const noexcept { return {}; }
    // called on co_yield
    std::suspend_always yield_value(T value) {
        m_current_value = std::move(value);
        return {};
    }
    // called on (implicit or explicit) co_return or co_return void
    void return_void() {}
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { m_exception = std::current_exception(); }
};

template <typename T>
class Generator<T>::Iter {
    public:
    using iterator_category = std::input_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using reference = const T&;
    using pointer = const T*;

    Iter() = default;
    explicit Iter(const Generator<T>::handle_type coroutine)
        : m_coroutine{coroutine} {}

    // Resume the coroutine and check if exception was thrown
    Iter& operator++() {
        m_coroutine.resume();
        if (m_coroutine.promise().m_exception) {
            std::rethrow_exception(m_coroutine.promise().m_exception);
        }
        return *this;
    }

    Iter operator++(int) {
        Iter temp = *this;
        ++(*this);
        return temp;
    }
    // Return the current value of the coroutine
    reference operator*() const {
        return m_coroutine.promise().m_current_value;
    }
    pointer operator->() const {
        return &(m_coroutine.promise().m_current_value);
    }

    bool operator==(std::default_sentinel_t) const {
        return !m_coroutine || m_coroutine.done();
    }

    private:
    Generator<T>::handle_type m_coroutine = nullptr;
};

}  // namespace CoroutineTests
