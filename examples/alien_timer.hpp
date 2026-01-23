#ifndef COROUTINETESTS_EXAMPLES_ALIEN_TIMER_HPP
#define COROUTINETESTS_EXAMPLES_ALIEN_TIMER_HPP

#include <chrono>
#include <coroutine>
#include <thread>

#include "logging_utils.hpp"

class StatusCode {
    public:
    /// StatusCode values
    enum Status { SUCCESS = 0, FAILURE = 1, UNDEFINED = 2 };
    /// Constructor
    StatusCode(Status status = UNDEFINED) : m_status(status) {}

    /// Get the status of the statuscode
    Status status() const { return m_status; }
    /// Friend function to output the status code
    friend std::ostream& operator<<(std::ostream& os,
                                    const StatusCode& status) {
        os << "Algorithm::StatusCode::";
        switch (status.status()) {
            case StatusCode::SUCCESS:
                os << "SUCCESS";
                break;
            case StatusCode::FAILURE:
                os << "FAILURE";
                break;
            case StatusCode::UNDEFINED:
                os << "UNDEFINED";
                break;
        }
        return os;
    }

    private:
    Status m_status;
};

struct AsyncTimer {
    std::chrono::milliseconds delay;
    StatusCode status_code;
    std::string_view parent;

    bool await_ready() const noexcept { return false; }

    template <typename T>
    void await_suspend(std::coroutine_handle<T> handle) noexcept {
        std::thread([this, handle]() {
            const auto self = format_name(parent, "AsyncTimer");
            log(self) << "Async operation started, will take " << delay.count()
                      << " ms" << std::endl;
            std::this_thread::sleep_for(delay);
            log(self) << "Async operation finished" << std::endl;
            handle.promise().reschedule();
        }).detach();
    }
    StatusCode await_resume() const noexcept { return status_code; }
};

#endif  // COROUTINETESTS_EXAMPLES_ALIEN_TIMER_HPP
