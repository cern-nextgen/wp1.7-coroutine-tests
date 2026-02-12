#pragma once

#include <coroutine>
#include <deque>

#include "CoroutineTests/nestabletask.hpp"

// Synchronization event. Can be awaited by multiple coroutines. Can be set once
// to resume all the coroutines suspended on it. Coroutines awaiting event that
// was already set will resume immediately.
class SimpleEvent {
    public:
    void set() {
        if (!m_awaiter.m_ready) {
            m_awaiter.m_ready = true;
            for (auto& handle : m_awaiter.m_handles) {
                if (handle && !handle.done()) {
                    handle.resume();
                    if (handle.promise().m_exception) {
                        std::rethrow_exception(handle.promise().m_exception);
                    }
                }
                m_awaiter.m_handles.clear();
            }
        }
    }

    auto& operator co_await() { return m_awaiter; }

    private:
    // If not ready stashes awaiting coroutines and return control to the
    // caller. If ready immediately resume awaiting coroutine.
    struct Awaiter {
        bool await_ready() const noexcept { return false; }
        std::coroutine_handle<> await_suspend(
            std::coroutine_handle<CoroutineTests::NestableTask::promise_type>
                handle) noexcept {
            if (m_ready) {
                return handle;
            }
            m_handles.push_back(handle);
            return std::noop_coroutine();
        }
        void await_resume() const noexcept {}

        std::deque<
            std::coroutine_handle<CoroutineTests::NestableTask::promise_type>>
            m_handles;
        bool m_ready{false};
    };
    Awaiter m_awaiter;
};
