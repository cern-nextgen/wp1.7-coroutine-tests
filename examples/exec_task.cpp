#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

#include "exec_backend.hpp"   // std exec backend selection
#include "logging_utils.hpp"  // log, format_name

template <typename Tag>
class StatusCodeImpl {
    public:
    enum class Status { SUCCESS = 0, FAILURE = 1, UNDEFINED = 2 };
    inline static const StatusCodeImpl SUCCESS{Status::SUCCESS};
    inline static const StatusCodeImpl FAILURE{Status::FAILURE};
    StatusCodeImpl(Status status = Status::UNDEFINED) : m_status(status) {}
    StatusCodeImpl(const StatusCodeImpl&) = default;
    Status status() const { return m_status; }

    private:
    Status m_status;
};

template <typename Tag>
std::ostream& operator<<(std::ostream& os, const StatusCodeImpl<Tag>& sc) {
    os << Tag::name << "::StatusCode::";
    switch (sc.status()) {
        case StatusCodeImpl<Tag>::Status::SUCCESS:
            os << "SUCCESS";
            break;
        case StatusCodeImpl<Tag>::Status::FAILURE:
            os << "FAILURE";
            break;
        case StatusCodeImpl<Tag>::Status::UNDEFINED:
            os << "UNDEFINED";
            break;
    }
    return os;
}

namespace tools {
struct Tag {
    static constexpr const char* name = "tools";
};
using StatusCode = StatusCodeImpl<Tag>;
}  // namespace tools

namespace algs {
struct Tag {
    static constexpr const char* name = "algs";
};
using StatusCode = StatusCodeImpl<Tag>;
}  // namespace algs

// Sender that simulates running an asynchronous API call for a given duration
// and returns a status code upon completion.
template <typename StatusCode>
struct AsyncAPIMockup {
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
                const auto self = format_name(parent, "AsyncAPIMockup");
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

// Scheduler that wraps another scheduler and logs scheduling operations to
// standard output
template <execution::scheduler BaseScheduler>
struct VerboseScheduler {
    BaseScheduler baseSched;
    using scheduler_concept = execution::scheduler_t;

    struct env {
        BaseScheduler baseSched;

        template <typename T>
        VerboseScheduler query(
            const execution::get_completion_scheduler_t<T>&) const noexcept {
            return {baseSched};
        }
    };

    // associated sender for schedule()
    struct Sender {
        BaseScheduler baseSched;

        // mandatory type aliases for sender
        using sender_concept = execution::sender_t;
        using completion_signatures = execution::completion_signatures_of_t<
            typename execution::schedule_result_t<BaseScheduler>>;

        // mandatory connect() method for sender
        // delegate to the base scheduler and log the scheduling
        auto connect(execution::receiver auto receiver) noexcept {
            return execution::connect(
                execution::just() | execution::then([] {
                    log() << "scheduler Scheduling new work item" << std::endl;
                }) | execution::continues_on(baseSched) |
                    execution::then([] {
                        log() << "scheduler Scheduled work item to run on this "
                                 "thread "
                              << std::endl;
                    }),
                std::move(receiver));
        }

        // mandatory get_env() method for scheduler's sender
        constexpr auto get_env() const noexcept { return env{baseSched}; }
    };

    // mandatory schedule() method for scheduler
    auto schedule() const { return Sender{baseSched}; }

    // mandatory equality operator for scheduler
    bool operator==(const VerboseScheduler&) const = default;
};

execution::task<tools::StatusCode> tool1(std::string_view parent) {
    const auto self = format_name(parent, "tool1");

    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await AsyncAPIMockup{tools::StatusCode::SUCCESS,
                                          std::chrono::milliseconds(100), self};
    log(self) << "Result from async API in tool1: " << status << std::endl;

    log(self) << "Finishing tool1" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<tools::StatusCode> tool2(std::string_view parent) {
    const auto self = format_name(parent, "tool2");

    log(self) << "Calling async API in tool2" << std::endl;
    auto status1 = co_await AsyncAPIMockup{tools::StatusCode::SUCCESS,
                                           std::chrono::milliseconds(10), self};
    log(self) << "Result from async API in tool2: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code = co_await tool1(self);
    log(self) << "Result from tool1: " << code << std::endl;

    log(self) << "Calling async API in tool2" << std::endl;
    auto status2 = co_await AsyncAPIMockup{tools::StatusCode::FAILURE,
                                           std::chrono::milliseconds(10), self};
    log(self) << "Result from async API in tool2: " << status2 << std::endl;

    log(self) << "Finishing tool2" << std::endl;
    co_return tools::StatusCode::SUCCESS;
}

execution::task<tools::StatusCode> tool3(std::string_view parent) {
    const auto self = format_name(parent, "tool3");
    log(self) << "Finishing tool3" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<algs::StatusCode> algorithm(std::string_view parent) {
    const auto self = format_name(parent, "algorithm");

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status1 = co_await AsyncAPIMockup{algs::StatusCode::SUCCESS,
                                           std::chrono::milliseconds(42), self};
    log(self) << "Result from async API in algorithm: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code1 = co_await tool1(self);
    log(self) << "Result from tool1: " << code1 << std::endl;

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status2 = co_await AsyncAPIMockup{algs::StatusCode::FAILURE,
                                           std::chrono::milliseconds(17), self};
    log(self) << "Result from async API in algorithm: " << status2 << std::endl;

    log(self) << "Launching tool2" << std::endl;
    auto code2 = co_await tool2(self);
    log(self) << "Result from tool2: " << code2 << std::endl;

    log(self) << "Launching tool3" << std::endl;
    auto code3 = co_await tool3(self);
    log(self) << "Result from tool3: " << code3 << std::endl;

    log(self) << "Finishing algorithm" << std::endl;
    co_return algs::StatusCode::SUCCESS;
}

int main() {
    log() << "main Starting" << std::endl;

    execution::run_loop loop;
    std::jthread worker([&](std::stop_token st) {
        std::stop_callback cb{st, [&] { loop.finish(); }};
        loop.run();
    });

    // Use verbose scheduler
    execution::scheduler auto scheduler =
        VerboseScheduler{loop.get_scheduler()};

    // Start executing the algorithm without blocking main
    Scope scope;
    auto work = []() -> execution::task<void> {
        log() << "Starting work\n";
        auto status = co_await algorithm("main");
        log() << "Final status of algorithm " << status << std::endl;
    }();
    scope.spawn(scheduler, std::move(work));

    // Sleep a bit to show that algorithm is already running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log() << "main waiting for algorithm to finish..." << std::endl;
    // Block until all work items in the scope are done
    execution::sync_wait(scope.join());
    log() << "main Done" << std::endl;
    return EXIT_SUCCESS;
}
