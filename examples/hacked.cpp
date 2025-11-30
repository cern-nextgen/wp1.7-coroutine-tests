#include <beman/execution/detail/counting_scope.hpp>
#include <beman/execution/execution.hpp>
#include <beman/task/task.hpp>
#include <chrono>
#include <coroutine>
#include <exception>
#include <format>
#include <hacked/task/task.hpp>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

std::ostream& log() {
    return std::cout << std::this_thread::get_id() << "  ";
}

std::ostream& log(std::string_view self) {
    return std::cout << std::this_thread::get_id() << "  " << self << "  ";
}

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

template <typename StatusCode>
struct AsyncAPIMockup {
    StatusCode status_code;
    std::chrono::milliseconds delay;
    std::string_view parent;

    // mandatory type aliases for sender
    using sender_concept = beman::execution::sender_t;
    using completion_signatures =
        beman::execution::completion_signatures<beman::execution::set_value_t(
            StatusCode)>;

    // associated operation state
    template <beman::execution::receiver Receiver>
    struct Operation {
        Receiver receiver;
        StatusCode status_code;
        std::chrono::milliseconds delay;
        std::string_view parent;

        // mandatory type alias for operation state
        using operation_state_concept = beman::execution::operation_state_t;

        // mandatory start() method for operation state
        // mockup the async operation using a detached thread, then set value
        // with given status code
        void start() noexcept {
            std::thread([this]() {
                const auto self = std::format("   {}.AsyncAPIMockup", parent);
                log(self) << "Async operation started, will take "
                          << delay.count() << " ms" << std::endl;
                std::this_thread::sleep_for(delay);
                log(self) << "Async operation finished" << std::endl;
                beman::execution::set_value(std::move(receiver), status_code);
            }).detach();
        }
    };

    // mandatory connect() method for sender
    template <beman::execution::receiver Receiver>
    auto connect(Receiver receiver) const noexcept {
        return Operation<Receiver>{std::move(receiver), std::move(status_code),
                                   std::move(delay), parent};
    }
};

auto sched_spawn(auto&& scheduler, auto&& sender, auto&& token) {
    return beman::execution::spawn(
        beman::execution::write_env(std::forward<decltype(sender)>(sender) |
                          beman::execution::upon_error([](auto&&) noexcept { std::cout << "ERROR!\n"; }),
                      beman::execution::detail::make_env(beman::execution::get_scheduler, std::forward<decltype(scheduler)>(scheduler))),
        std::forward<decltype(token)>(token));
}

hacked::execution::task<tools::StatusCode> tool1(std::string_view parent) {
    const auto self = std::format("   {}.tool1", parent);

    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await AsyncAPIMockup{tools::StatusCode::SUCCESS,
                                          std::chrono::milliseconds(100), self};
    log(self) << "Result from async API in tool1: " << status << std::endl;

    log(self) << "Finishing tool1" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

beman::execution::task<algs::StatusCode> algorithm(std::string_view parent) {
    const auto self = std::format("   {}.algorithm", parent);

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status = co_await AsyncAPIMockup{tools::StatusCode::SUCCESS,
                                          std::chrono::milliseconds(42), self};
    log(self) << "Result from async API in algorithm: " << status << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code1 = co_await tool1(self);
    log(self) << "Result from tool1: " << code1 << std::endl;

    log(self) << "Finishing algorithm" << std::endl;
    co_return algs::StatusCode::SUCCESS;
}

int main() {
    log() << "main Starting" << std::endl;

    beman::execution::run_loop loop;
    std::jthread worker([&](std::stop_token st) {
        std::stop_callback cb{st, [&] { loop.finish(); }};
        loop.run();
    });

    // Use verbose scheduler
    beman::execution::scheduler auto scheduler = loop.get_scheduler();

    // Start executing the algorithm without blocking main
    beman::execution::counting_scope scope;

    sched_spawn(
        std::move(scheduler),
        []() -> beman::execution::task<void> {
            auto status = co_await algorithm("main");
            log() << "Final status of algorithm " << status << std::endl;
        }(),
        scope.get_token());

    // Sleep a bit to show that algorithm is already running
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    log() << "main waiting for algorithm to finish..." << std::endl;
    // Block until all work items in the scope are done
    beman::execution::sync_wait(scope.join());
    log() << "main Done" << std::endl;
    return EXIT_SUCCESS;
}
