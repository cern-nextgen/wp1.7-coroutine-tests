#include <chrono>
#include <cstdlib>
#include <string_view>
#include <thread>
#include <utility>

#include "exec_backend.hpp"   // std exec backend selection
#include "exec_timer.hpp"     // TimerSender
#include "logging_utils.hpp"  // log, format_name
#include "statuscode.hpp"     // StatusCodeImpl

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

// Scheduler that wraps another scheduler and logs scheduling operations to
// standard output
template <execution::scheduler BaseScheduler>
struct VerboseScheduler {
    BaseScheduler baseSched;
    using scheduler_concept = execution::scheduler_tag;

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

        // mandatory type alias for sender
        using sender_concept = execution::sender_tag;

        // Build the actual sender pipeline once, share it between
        // connect() and completion-signature computation.
        auto make_sender() noexcept {
            return execution::just() | execution::then([] {
                       log("Scheduler")
                           << "Scheduling new work item" << std::endl;
                   }) |
                   execution::continues_on(baseSched) | execution::then([] {
                       log("Scheduler") << "Scheduled work item to run on this "
                                           "thread "
                                        << std::endl;
                   });
        }

        // mandatory get_completion_signatures() method for sender
        template <class Env>
        auto get_completion_signatures(Env&&) noexcept
            -> execution::completion_signatures_of_t<decltype(make_sender()),
                                                     Env> {
            return {};
        }

        // mandatory connect() method for sender
        auto connect(execution::receiver auto receiver) noexcept {
            return execution::connect(make_sender(), std::move(receiver));
        }

        // mandatory get_env() method for sender
        constexpr auto get_env() const noexcept { return env{baseSched}; }
    };

    // mandatory schedule() method for scheduler
    auto schedule() const { return Sender{baseSched}; }

    // mandatory equality operator for scheduler
    bool operator==(const VerboseScheduler&) const = default;
};

execution::task<tools::StatusCode> tool1_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool1");

    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await TimerSender{tools::StatusCode::SUCCESS,
                                       std::chrono::milliseconds(100), self};
    log(self) << "Result from async API in tool1: " << status << std::endl;

    log(self) << "Finishing tool1" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<tools::StatusCode> tool2_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool2");

    log(self) << "Calling async API in tool2" << std::endl;
    auto status1 = co_await TimerSender{tools::StatusCode::SUCCESS,
                                        std::chrono::milliseconds(10), self};
    log(self) << "Result from async API in tool2: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code = co_await tool1_execute(self);
    log(self) << "Result from tool1: " << code << std::endl;

    log(self) << "Calling async API in tool2" << std::endl;
    auto status2 = co_await TimerSender{tools::StatusCode::FAILURE,
                                        std::chrono::milliseconds(10), self};
    log(self) << "Result from async API in tool2: " << status2 << std::endl;

    log(self) << "Finishing tool2" << std::endl;
    co_return tools::StatusCode::SUCCESS;
}

execution::task<tools::StatusCode> tool3_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool3");
    log(self) << "Finishing tool3" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<algs::StatusCode> algorithm_execute(std::string_view parent) {
    const auto self = format_name(parent, "algorithm");

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status1 = co_await TimerSender{algs::StatusCode::SUCCESS,
                                        std::chrono::milliseconds(42), self};
    log(self) << "Result from async API in algorithm: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code1 = co_await tool1_execute(self);
    log(self) << "Result from tool1: " << code1 << std::endl;

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status2 = co_await TimerSender{algs::StatusCode::FAILURE,
                                        std::chrono::milliseconds(17), self};
    log(self) << "Result from async API in algorithm: " << status2 << std::endl;

    log(self) << "Launching tool2" << std::endl;
    auto code2 = co_await tool2_execute(self);
    log(self) << "Result from tool2: " << code2 << std::endl;

    log(self) << "Launching tool3" << std::endl;
    auto code3 = co_await tool3_execute(self);
    log(self) << "Result from tool3: " << code3 << std::endl;

    log(self) << "Finishing algorithm" << std::endl;
    co_return algs::StatusCode::SUCCESS;
}

int main() {
    log("main") << "Starting" << std::endl;

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
        log() << "Starting work" << std::endl;
        auto status = co_await algorithm_execute("main");
        log() << "Final status of algorithm " << status << std::endl;
    }();
    scope.spawn(scheduler, std::move(work));

    // Sleep a bit to show that algorithm is already running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log("main") << "Waiting for algorithm to finish..." << std::endl;
    // Block until all work items in the scope are done
    execution::sync_wait(scope.join());
    log("main") << "Done" << std::endl;
    return EXIT_SUCCESS;
}
