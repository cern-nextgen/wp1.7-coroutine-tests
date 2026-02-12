#pragma once

#include <coroutine>

// This file contains the skeletons for coroutine return type/interface,
// coroutine promise type and awaitables, listing their customizations points.
// The skeletons are not intended to be used as is, but rather as a starting
// point for implementing coroutines and awaitables.

// Skeleton for a coroutine return type/interface
// `[[nodiscard]]` is used to avoid mistaking coroutine for a function and
// dropping the ReturnType.
class [[nodiscard]] ReturnType {
    public:
    // REQUIRED typedef or implementing `std::coroutine_traits`
    // `typedef` to a coroutine promise type
    struct promise_type;
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful
    // REQUIRED
    // construct `ReturnType` from a coroutine handle
    ReturnType(handle_type coroutine_handle) : m_coroutine(coroutine_handle) {}
    // coroutine handle isn't owning and destroy has to be called manually
    ~ReturnType() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    // remaining constructors and assignment operators:
    // - use invalid coroutine handle by default
    // - delete copy constructor and assignment operator as `ReturnType` is
    // assumed to own the handle
    // - move constructor and assignment operator to transfer ownership of the
    // handle
    ReturnType() = default;
    ReturnType(const ReturnType&) = delete;
    ReturnType& operator=(const ReturnType&) = delete;
    ReturnType(ReturnType&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    ReturnType& operator=(ReturnType&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }

    private:
    // non-owning handle to the coroutine storage/frame
    handle_type m_coroutine;
};

// coroutine promise type used by the `ReturnType`
struct ReturnType::promise_type {
    // REQUIRED
    // create `ReturnType`
    ReturnType get_return_object() {
        return {ReturnType::handle_type::from_promise(*this)};
    }
    // REQUIRED
    // called on coroutine start, then implicitly `co_await` the returned value
    /*awaitable*/ initial_suspend();
    // REQUIRED
    // called on coroutine completion, then implicitly `co_await` the returned
    // value
    /*awaitable*/ final_suspend();
    // REQUIRED
    // implicitly acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception();

    // REQUIRED either return_void or return_value
    // called on (implicit or explicit) `co_return` or `co_return void`.
    // Conflicts with `return_value` declaration
    void return_void();
    // called on `co_return value` if `value` isn't `void`. Conflicts with
    // `return_void` declaration
    /*value type*/ return_value();

    // called on `co_yield value`, then `co_await` the returned value
    /*awaitable*/ yield_value(/*value type*/);

    // called on `co_await value`, transform `value` to awaitable then
    // `co_await` it. Not used for implicit `co_await` in `initial_suspend`,
    // `final_suspend`, `co_yield`
    // alternatively an awaitable can implement `operator co_await`
    /*awaitable*/ await_transform(/*value type*/);
};

// Skeleton of awaitable type
// coroutine return type/interface can also implement the awaitable interface to
// act as an awaitable
struct Awaitable {
    // REQUIRED
    // optimization point to skip suspending the coroutine
    bool await_ready() const { return false; }
    // REQUIRED
    // gets handle to coroutine that is about to be suspended then decides where
    // to continue the handle can be either type erased
    // `std::coroutine_handle<>` or specialized returned type/value are used to
    // control the continuation:
    // - `void` - suspend the coroutine and return control to the caller
    // - `true` - suspend the coroutine and return control to the caller (same
    // as `void`)
    // - `false` - resume the coroutine that was to be suspended
    // - `std::coroutine_handle` - suspend the coroutine and resume returned
    // coroutine
    // - `std::noop_coroutine_handle` - suspend the coroutine and return control
    // to the caller (same as `void`)
    /* void or bool or std::coroutine_handle*/ await_suspend(handle_type h);
    // REQUIRED
    // resume the suspended coroutine and return the value, e.g.
    // `auto value = co_await Awaitable{};`
    /*value type*/ await_resume();
};
