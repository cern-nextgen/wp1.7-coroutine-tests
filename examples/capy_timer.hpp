#pragma once

#include <boost/capy.hpp>
#include <boost/capy/continuation.hpp>
#include <chrono>
#include <coroutine>
#include <thread>

#include "logging_utils.hpp"  // log, format_name
#include "statuscode.hpp"     // StatusCodeImpl

namespace timer {
struct Tag {
    static constexpr const char* name = "timer";
};
using StatusCode = StatusCodeImpl<Tag>;
}  // namespace timer

class TimerIoAwaitable {
    public:
    TimerIoAwaitable(std::chrono::milliseconds delay,
                     timer::StatusCode status_code, std::string_view parent)
        : m_delay(delay), m_status_code(status_code), m_parent(parent) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle,
                       boost::capy::io_env const* env) noexcept {
        m_continuation.h = handle;
        std::thread([this, env]() {
            const auto self = format_name(m_parent, "TimerIoAwaitable");
            log(self) << "Async operation started, will take "
                      << m_delay.count() << " ms" << std::endl;
            std::this_thread::sleep_for(m_delay);
            log(self) << "Async operation finished" << std::endl;
            env->executor.post(m_continuation);
        }).detach();
    }
    timer::StatusCode await_resume() const noexcept { return m_status_code; }

    private:
    std::chrono::milliseconds m_delay;
    timer::StatusCode m_status_code;
    std::string_view m_parent;
    boost::capy::continuation m_continuation;
};
