#include <boost/capy.hpp>
#include <chrono>
#include <coroutine>
#include <cstdlib>
#include <latch>
#include <string_view>

#include "capy_timer.hpp"     // TimerIoAwaitable
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

template <boost::capy::Executor Ex>
class VerboseExecutor {

    public:
    VerboseExecutor(Ex& ex) : m_executor(&ex) {
        static_assert(boost::capy::Executor<VerboseExecutor<Ex>>,
                      "VerboseExecutor should be a valid capy Executor");
    }

    auto post(std::coroutine_handle<> h) const {
        log() << "executor posting new work item" << std::endl;
        m_executor->post(h);
    }
    auto dispatch(std::coroutine_handle<> h) const {
        log() << "executor dispatching new work item" << std::endl;
        return m_executor->dispatch(h);
    }
    auto& context() const noexcept { return m_executor->context(); }
    auto on_work_started() const noexcept {
        log() << "executor work started" << std::endl;
        m_executor->on_work_started();
    }
    auto on_work_finished() const noexcept {
        log() << "executor work finished" << std::endl;
        m_executor->on_work_finished();
    }
    bool operator==(const VerboseExecutor& other) const noexcept {
        return (*m_executor) == (*other.m_executor);
    }

    private:
    Ex* m_executor;
};

boost::capy::task<tools::StatusCode> tool1_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool1");

    log(self) << "Calling async API in tool1" << std::endl;
    auto status = co_await TimerIoAwaitable{std::chrono::milliseconds(100),
                                            timer::StatusCode::SUCCESS, self};
    log(self) << "Result from async API in tool1: " << status << std::endl;

    log(self) << "Finishing tool1" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

boost::capy::task<tools::StatusCode> tool2_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool2");

    log(self) << "Calling async API in tool2" << std::endl;
    auto status1 = co_await TimerIoAwaitable{std::chrono::milliseconds(10),
                                             timer::StatusCode::SUCCESS, self};
    log(self) << "Result from async API in tool2: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code = co_await tool1_execute(self);
    log(self) << "Result from tool1: " << code << std::endl;

    log(self) << "Calling async API in tool2" << std::endl;
    auto status2 = co_await TimerIoAwaitable{std::chrono::milliseconds(10),
                                             timer::StatusCode::FAILURE, self};
    log(self) << "Result from async API in tool2: " << status2 << std::endl;

    log(self) << "Finishing tool2" << std::endl;
    co_return tools::StatusCode::SUCCESS;
}

boost::capy::task<tools::StatusCode> tool3_execute(std::string_view parent) {
    const auto self = format_name(parent, "tool3");
    log(self) << "Finishing tool3" << std::endl;
    co_return tools::StatusCode::FAILURE;
}

boost::capy::task<algs::StatusCode> algorithm_execute(std::string_view parent) {
    const auto self = format_name(parent, "algorithm");

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status1 = co_await TimerIoAwaitable{std::chrono::milliseconds(42),
                                             timer::StatusCode::SUCCESS, self};
    log(self) << "Result from async API in algorithm: " << status1 << std::endl;

    log(self) << "Launching tool1" << std::endl;
    auto code1 = co_await tool1_execute(self);
    log(self) << "Result from tool1: " << code1 << std::endl;

    log(self) << "Calling async API in algorithm" << std::endl;
    auto status2 = co_await TimerIoAwaitable{std::chrono::milliseconds(17),
                                             timer::StatusCode::FAILURE, self};
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
    log() << "main Starting" << std::endl;

    auto pool = boost::capy::thread_pool(4);
    auto pool_executor = pool.get_executor();
    auto verbose_executor = VerboseExecutor(pool_executor);

    auto final_result = algs::StatusCode{};
    auto done = std::latch{1};
    auto result_handler = [&done, &final_result](algs::StatusCode code) {
        final_result = code;
        done.count_down();
    };

    log() << "main launching algorithm" << std::endl;
    boost::capy::run_async(verbose_executor,
                           result_handler)(algorithm_execute("main"));

    log() << "main waiting for algorithm to finish..." << std::endl;
    done.wait();
    log() << "Final status of algorithm " << final_result << std::endl;

    log() << "main Done" << std::endl;
    return EXIT_SUCCESS;
}
