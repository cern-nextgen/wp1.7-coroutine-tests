#pragma once

#include <coroutine>
#include <exception>

#include "CoroutineTests/threadpool.hpp"

namespace CoroutineTests {

// Nestable coroutine that can be scheduled on a threadpool.
// Doesn't return a value, doesn't yield.
class [[nodiscard]] Async {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    explicit Async(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~Async() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    Async() = default;
    Async(const Async&) = delete;
    Async& operator=(const Async&) = delete;
    Async(Async&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    Async& operator=(Async&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }
    // start the coroutine on the threadpool
    inline void schedule_on(Threadpool& threadpool);

    // awaitable interface
    // don't skip suspensions
    bool await_ready() const noexcept { return false; }
    // when awaited, store the parent coroutine handle and reshedule this
    // coroutine
    inline void await_suspend(handle_type handle) noexcept;
    // nothing special on resume, doesn't produce a value
    void await_resume() const noexcept {}

    private:
    handle_type m_coroutine;
    bool m_started = false;
};

struct Async::promise_type {
    // storage for exceptions thrown in the coroutine
    std::exception_ptr m_exception;
    // handle to the parent coroutine if the coroutine has one
    handle_type m_parent;
    // handle to the threadpool
    Threadpool* m_scheduler = nullptr;

    // enqueue the coroutine on the threadpool
    void reschedule() {
        if (m_scheduler) {
            m_scheduler->enqueue_task(handle_type::from_promise(*this));
        }
    }
    // required by coroutines
    Async get_return_object() {
        return Async{Async::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    std::suspend_always initial_suspend() const { return {}; }
    // called on coroutine completion
    // on final_suspend reschedule the parent coroutine if it has one
    auto final_suspend() const noexcept {
        struct final_awaiter {
            // don't skip suspensions
            bool await_ready() const noexcept { return false; }
            // reschedule the parent coroutine if it has one or return control
            // to the caller
            void await_suspend(handle_type handle) noexcept {
                auto parent = handle.promise().m_parent;
                if (parent) {
                    return parent.promise().reschedule();
                }
            }
            // nothing special on resume, doesn't produce a value
            void await_resume() const noexcept {}
        };
        return final_awaiter{};
    }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { m_exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return void
    void return_void() const {}
};

inline void Async::await_suspend(handle_type handle) noexcept {
    m_coroutine.promise().m_parent = handle;
    m_coroutine.promise().m_scheduler = handle.promise().m_scheduler;
    m_coroutine.promise().reschedule();
}

void Async::schedule_on(Threadpool& threadpool) {
    if (m_coroutine && !m_started) {
        m_coroutine.promise().m_scheduler = &threadpool;
        threadpool.enqueue_task(m_coroutine);
    }
    m_started = true;
}
}  // namespace CoroutineTests
