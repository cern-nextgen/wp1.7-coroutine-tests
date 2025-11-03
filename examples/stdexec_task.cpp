#include <cstdlib>
#include <exec/async_scope.hpp>
#include <exec/static_thread_pool.hpp>
#include <exec/task.hpp>
#include <format>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

template <typename Tag>
class StatusCodeImpl {
    public:
    enum class Status { SUCCESS = 0, FAILURE = 1, UNDEFINED = 2 };
    inline static const StatusCodeImpl SUCCESS{Status::SUCCESS};
    inline static const StatusCodeImpl FAILURE{Status::FAILURE};
    StatusCodeImpl(Status status = Status::UNDEFINED) : m_status(status) {}
    StatusCodeImpl(const StatusCodeImpl& sc) = default;
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
    using sender_concept = stdexec::sender_t;
    using completion_signatures =
        stdexec::completion_signatures<stdexec::set_value_t(StatusCode)>;

    template <stdexec::receiver Receiver>

    // associated operation state
    struct Operation {
        Receiver receiver;
        StatusCode status_code;
        std::chrono::milliseconds delay;
        std::string_view parent;

        // mandatory type alias for operation state
        using operation_state_concept = stdexec::operation_state_t;

        // mandatory start() method for operation state
        // mockup the async operation using a detached thread, then set value
        // with given status code
        void start() noexcept {
            std::thread([this]() {
                const auto self = std::format("   {}.AsyncAPIMockup", parent);
                std::cout << std::this_thread::get_id() << "  " << self
                          << "  Async operation started, will take "
                          << delay.count() << " ms" << std::endl;
                std::this_thread::sleep_for(delay);
                std::cout << std::this_thread::get_id() << "  " << self
                          << "  Async operation finished" << std::endl;
                stdexec::set_value(std::move(receiver), status_code);
            }).detach();
        }
    };

    // mandatory connect() method for sender
    template <stdexec::receiver Receiver>
    auto connect(Receiver receiver) const noexcept {
        return Operation<Receiver>{std::move(receiver), std::move(status_code),
                                   std::move(delay), parent};
    }
};

// Scheduler that wraps another scheduler and logs scheduling operations to
// standard output
template <stdexec::scheduler BaseScheduler>
struct VerboseScheduler {
    BaseScheduler baseSched;

    // associated sender for schedule()
    struct Sender {
        BaseScheduler baseSched;

        // mandatory type aliases for sender
        using sender_concept = stdexec::sender_t;
        using completion_signatures = stdexec::completion_signatures_of_t<
            typename stdexec::schedule_result_t<BaseScheduler>>;

        // mandatory connect() method for sender
        // delegate to the base scheduler and log the scheduling
        auto connect(stdexec::receiver auto receiver) const noexcept {
            return stdexec::connect(
                stdexec::just() | stdexec::then([] {
                    std::cout << std::this_thread::get_id()
                              << "  scheduler Scheduling new work item"
                              << std::endl;
                }) | stdexec::continues_on(baseSched) |
                    stdexec::then([] {
                        std::cout << std::this_thread::get_id()
                                  << "  scheduler Scheduled work item to run "
                                     "on this thread "
                                  << std::endl;
                    }),
                std::move(receiver));
        }
        // mandatory get_env() method for scheduler's sender
        // delegate to the base scheduler
        constexpr auto get_env() const noexcept {
            return stdexec::env{
                stdexec::prop{
                    stdexec::get_completion_scheduler<stdexec::set_value_t>,
                    VerboseScheduler{baseSched}},
                stdexec::get_env(stdexec::schedule(baseSched))};
        }
    };
    // mandatory schedule() method for scheduler
    auto schedule() const { return Sender{baseSched}; }
    // mandatory equality operator for scheduler
    bool operator==(const VerboseScheduler& other) const = default;
    // mandatory get_env() method for scheduler
    auto query(
        stdexec::get_forward_progress_guarantee_t guarantee) const noexcept {
        return guarantee(baseSched);
    }
};

exec::task<tools::StatusCode> tool1(std::string_view parent) {
    const auto self = std::format("   {}.tool1", parent);

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Calling async API in tool1" << std::endl;
    auto status = co_await AsyncAPIMockup{tools::StatusCode::SUCCESS,
                                          std::chrono::milliseconds(100), self};
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from async API in tool1: " << status << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Finishing tool1" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

exec::task<tools::StatusCode> tool2(std::string_view parent) {
    const auto self = std::format("   {}.tool2", parent);

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Calling async API in tool2" << std::endl;
    auto status1 = co_await AsyncAPIMockup{tools::StatusCode::SUCCESS,
                                           std::chrono::milliseconds(10), self};
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from async API in tool2: " << status1 << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Launching tool1" << std::endl;
    auto code = co_await tool1(self);
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from tool1: " << code << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Calling async API in tool2" << std::endl;
    auto status2 = co_await AsyncAPIMockup{tools::StatusCode::FAILURE,
                                           std::chrono::milliseconds(10), self};
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from async API in tool2: " << status2 << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Finishing tool2" << std::endl;
    co_return tools::StatusCode::SUCCESS;
}

exec::task<tools::StatusCode> tool3(std::string_view parent) {
    const auto self = std::format("   {}.tool3", parent);
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Finishing tool3" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

exec::task<algs::StatusCode> algorithm(std::string_view parent) {
    const auto self = std::format("   {}.algorithm", parent);

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Calling async API in algorithm" << std::endl;
    auto status1 = co_await AsyncAPIMockup{algs::StatusCode::SUCCESS,
                                           std::chrono::milliseconds(42), self};
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from async API in algorithm: " << status1
              << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Launching tool1" << std::endl;
    auto code1 = co_await tool1(self);
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from tool1: " << code1 << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Calling async API in algorithm" << std::endl;
    auto status2 = co_await AsyncAPIMockup{algs::StatusCode::FAILURE,
                                           std::chrono::milliseconds(17), self};
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from async API in algorithm: " << status2
              << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Launching tool2" << std::endl;
    auto code2 = co_await tool2(self);
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from tool2: " << code2 << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Launching tool3" << std::endl;
    auto code3 = co_await tool3(self);
    std::cout << std::this_thread::get_id() << "  " << self
              << "  Result from tool3: " << code3 << std::endl;

    std::cout << std::this_thread::get_id() << "  " << self
              << "  Finishing algorithm" << std::endl;
    co_return algs::StatusCode::SUCCESS;
}

int main() {
    std::cout << std::this_thread::get_id() << "  main Starting" << std::endl;
    exec::static_thread_pool pool{2};
    stdexec::scheduler auto scheduler = VerboseScheduler{pool.get_scheduler()};
    // Alternatively use the base scheduler directly
    // stdexec::scheduler auto scheduler = pool.get_scheduler();

    exec::async_scope scope;
    // Start executing the algorithm without blocking main
    scope.spawn(
        stdexec::starts_on(std::move(scheduler), []() -> exec::task<void> {
            auto status = co_await algorithm("main");
            std::cout << std::this_thread::get_id() << "  "
                      << "Final status of algorithm " << status << std::endl;
        }()));
    // Alternatively block main and wait for the algorithm to finish
    // auto [final_status] =
    // stdexec::sync_wait(stdexec::starts_on(std::move(scheduler),
    //                                       algorithm("main"))).value();
    // std::cout << std::this_thread::get_id()
    //           << "  main Final status of algorithm "
    //           << final_status << std::endl;

    // Sleep a bit to show that algorithm is already running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << std::this_thread::get_id()
              << "  main waiting for algorithm to finish..." << std::endl;
    // Block until all work items in the scope are done
    stdexec::sync_wait(scope.on_empty());
    std::cout << std::this_thread::get_id() << "  main Done" << std::endl;
    return EXIT_SUCCESS;
}
