#pragma once

#include "exec_backend.hpp"   // std exec backend selection
#include "logging_utils.hpp"  // log, format_name

// Sender that simulates running an asynchronous API call for a given duration
// and returns a status code upon completion.
template <typename StatusCode>
struct TimerSender {
    StatusCode status_code;
    std::chrono::milliseconds delay;
    std::string_view parent;

    // mandatory type aliases for sender
    using sender_concept = execution::sender_t;
    using completion_signatures =
        execution::completion_signatures<execution::set_value_t(StatusCode)>;

    // associated operation state
    template <execution::receiver Receiver>
    struct Operation {
        Receiver receiver;
        StatusCode status_code;
        std::chrono::milliseconds delay;
        std::string_view parent;

        // mandatory type alias for operation state
        using operation_state_concept = execution::operation_state_t;

        // mandatory start() method for operation state
        // mockup the async operation using a detached thread, then set value
        // with given status code
        void start() noexcept {
            std::thread([this]() {
                const auto self = format_name(parent, "TimerSender");
                log(self) << "Async operation started, will take "
                          << delay.count() << " ms" << std::endl;
                std::this_thread::sleep_for(delay);
                log(self) << "Async operation finished" << std::endl;
                execution::set_value(std::move(receiver), status_code);
            }).detach();
        }
    };

    // mandatory connect() method for sender
    template <execution::receiver Receiver>
    auto connect(Receiver receiver) const noexcept {
        return Operation<Receiver>{std::move(receiver), std::move(status_code),
                                   std::move(delay), parent};
    }
};
