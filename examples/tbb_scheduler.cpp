#include <tbb/task_arena.h>

#include <chrono>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

// Backend selection: stdexec (default) or beman::execution

#ifdef USE_BEMAN
#include <beman/execution/execution.hpp>
#include <beman/task/task.hpp>
namespace execution = beman::execution;
#else
#include <exec/async_scope.hpp>
#include <exec/task.hpp>
#include <stdexec/execution.hpp>
namespace execution {
using namespace stdexec;
template <typename T>
using task = exec::task<T>;
using run_loop = execution::run_loop;
}  // namespace execution
#endif

// Asynchronous/Counting scope helper
struct Scope {

#ifdef USE_BEMAN
    beman::execution::counting_scope scope;
    // helper for spawning with counting scope
    static auto sched_spawn(auto scheduler, auto&& sender, auto&& token) {
        auto work = execution::starts_on(
            scheduler, std::forward<decltype(sender)>(sender));
        return execution::spawn(
            execution::write_env(
                std::move(work) | execution::upon_error([](auto&&) noexcept {
                    std::cout << "ERROR!\n";
                }),
                execution::detail::make_env(execution::get_scheduler,
                                            scheduler)),
            std::forward<decltype(token)>(token));
    }
#else
    exec::async_scope scope;
#endif  // USE_BEMAN

    void spawn(execution::scheduler auto&& scheduler,
               execution::sender auto&& sender) {
#ifdef USE_BEMAN
        sched_spawn(std::forward<decltype(scheduler)>(scheduler),
                    std::forward<decltype(sender)>(sender), scope.get_token());
#else
        scope.spawn(
            execution::starts_on(std::forward<decltype(scheduler)>(scheduler),
                                 std::forward<decltype(sender)>(sender)));
#endif  // USE_BEMAN
    }

    auto join() {
#ifdef USE_BEMAN
        return scope.join();
#else
        return scope.on_empty();
#endif  // USE_BEMAN
    }
};

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
                const auto self = std::format("   {}.AsyncAPIMockup", parent);
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

// Scheduler enqueuing work into a TBB task arena
struct TaskArenaScheduler {
    tbb::task_arena* arena = nullptr;
    using scheduler_concept = execution::scheduler_t;

    struct Env {
        tbb::task_arena* arena;
        template <typename T>
        auto query(
            const execution::get_completion_scheduler_t<T>&) const noexcept {
            return TaskArenaScheduler{arena};
        }
    };

    template <execution::receiver Receiver>
    struct Operation {
        std::remove_cvref_t<Receiver> receiver;
        tbb::task_arena* arena;
        using operation_state_concept = execution::operation_state_t;

        void start() & noexcept {
            log() << "Submitting work to task arena" << std::endl;
            arena->enqueue([this]() {
                log() << "Running work item in task arena" << std::endl;
                execution::set_value(std::move(receiver));
            });
        }
    };

    struct Sender {
        tbb::task_arena* arena;
        using sender_concept = execution::sender_t;
        using completion_signatures =
            execution::completion_signatures<execution::set_value_t()>;

        template <execution::receiver Receiver>
        auto connect(Receiver&& receiver) {
            return Operation<Receiver>(std::forward<Receiver>(receiver), arena);
        }

        auto get_env() const noexcept { return Env{arena}; }
    };

    auto schedule() const noexcept { return Sender{arena}; }
    bool operator==(const TaskArenaScheduler& other) const = default;
};

static_assert(execution::scheduler<TaskArenaScheduler>,
              "TaskArenaScheduler should model scheduler");

TaskArenaScheduler get_scheduler(tbb::task_arena& arena) {
    return TaskArenaScheduler{&arena};
}

execution::task<tools::StatusCode> tool1(std::string_view parent) {
    const auto self = std::format("   {}.tool1", parent);

    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await AsyncAPIMockup{tools::StatusCode::SUCCESS,
                                          std::chrono::milliseconds(100), self};
    log(self) << "Result from async API in tool1: " << status << std::endl;

    log(self) << "Finishing tool1" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<tools::StatusCode> tool2(std::string_view parent) {
    const auto self = std::format("   {}.tool2", parent);

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
    const auto self = std::format("   {}.tool3", parent);
    log(self) << "Finishing tool3" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

execution::task<algs::StatusCode> algorithm(std::string_view parent) {
    const auto self = std::format("   {}.algorithm", parent);

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

    tbb::task_arena arena{2};
    execution::scheduler auto scheduler = get_scheduler(arena);

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
