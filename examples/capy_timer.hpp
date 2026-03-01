#pragma once

#include <boost/capy.hpp>
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

struct TimerIoAwaitable {
    std::chrono::milliseconds delay;
    timer::StatusCode status_code;
    std::string_view parent;

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle,
                       boost::capy::io_env const* env) noexcept {
        std::thread([this, handle, env]() {
            const auto self = format_name(parent, "TimerIoAwaitable");
            log(self) << "Async operation started, will take " << delay.count()
                      << " ms" << std::endl;
            std::this_thread::sleep_for(delay);
            log(self) << "Async operation finished" << std::endl;
            env->executor.post(handle);
        }).detach();
    }
    timer::StatusCode await_resume() const noexcept { return status_code; }
};
