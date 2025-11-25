#include <chrono>
#include <coroutine>
#include <iostream>
#include <thread>

#include "CoroutineTests/alien/algorithm.hpp"
#include "CoroutineTests/alien/subtool.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"

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

struct MockupAwaiter {
    std::chrono::milliseconds delay;
    StatusCode status_code;

    MockupAwaiter(std::chrono::milliseconds d, StatusCode sc)
        : delay(d), status_code(sc) {}

    bool await_ready() const noexcept { return false; }

    template <typename T>
    void await_suspend(std::coroutine_handle<T> handle) noexcept {
        std::thread([this, handle]() {
            std::this_thread::sleep_for(delay);
            std::cout << std::this_thread::get_id() << "  MockupAwaiter done\n";
            handle.promise().reschedule();
        }).detach();
    }
    StatusCode await_resume() const noexcept { return status_code; }
};

CoroutineTests::alien::algorithm::Algorithm trivial_algorithm() {
    std::cout << std::this_thread::get_id() << "  Starting trivial algorithm"
              << std::endl;
    co_return CoroutineTests::alien::algorithm::StatusCode::SUCCESS;
}

CoroutineTests::alien::algorithm::Algorithm algorithm_with_awaiter() {

    std::cout << std::this_thread::get_id()
              << "  Starting algorithm with awaiter" << std::endl;
    auto status = co_await MockupAwaiter(std::chrono::milliseconds(200),
                                         StatusCode::SUCCESS);
    std::cout << std::this_thread::get_id()
              << "  Awaiter returned status: " << status << std::endl;
    if (status.status() != StatusCode::SUCCESS) {
        co_return CoroutineTests::alien::algorithm::StatusCode::FAILURE;
    }
    co_return CoroutineTests::alien::algorithm::StatusCode::SUCCESS;
}

CoroutineTests::alien::tool::Tool success_tool() {
    co_return CoroutineTests::alien::tool::StatusCode::SUCCESS;
}

CoroutineTests::alien::tool::Tool failure_tool() {
    co_return CoroutineTests::alien::tool::StatusCode::FAILURE;
}

CoroutineTests::alien::algorithm::Algorithm algorithm_w_tools() {
    auto code1 = co_await success_tool();
    std::cout << std::this_thread::get_id()
              << "  Tool1 returned status: " << code1 << std::endl;
    if (code1.status() != CoroutineTests::alien::tool::StatusCode::SUCCESS) {
        co_return CoroutineTests::alien::algorithm::StatusCode::FAILURE;
    }
    auto code2 = co_await failure_tool();
    std::cout << std::this_thread::get_id()
              << "  Tool2 returned status: " << code2 << std::endl;
    if (code2.status() != CoroutineTests::alien::tool::StatusCode::SUCCESS) {
        co_return CoroutineTests::alien::algorithm::StatusCode::FAILURE;
    }

    co_return CoroutineTests::alien::algorithm::StatusCode::SUCCESS;
}

CoroutineTests::alien::subtool::SubTool success_subtool() {
    auto code = co_await MockupAwaiter(std::chrono::milliseconds(100),
                                       StatusCode::SUCCESS);
    std::cout << std::this_thread::get_id()
              << "  SubTool awaiter returned status: " << code << std::endl;
    if (code.status() != StatusCode::SUCCESS) {
        co_return CoroutineTests::alien::subtool::StatusCode::FAILURE;
    }
    co_return CoroutineTests::alien::subtool::StatusCode::SUCCESS;
}

CoroutineTests::alien::tool::Tool tool_with_subtool() {
    auto subtool_code = co_await success_subtool();
    std::cout << std::this_thread::get_id()
              << "  Tool's SubTool returned status: " << subtool_code
              << std::endl;
    if (subtool_code.status() !=
        CoroutineTests::alien::subtool::StatusCode::SUCCESS) {
        co_return CoroutineTests::alien::tool::StatusCode::FAILURE;
    }
    co_return CoroutineTests::alien::tool::StatusCode::SUCCESS;
}

CoroutineTests::alien::algorithm::Algorithm algorithm_w_subtools() {
    auto tool_code1 = co_await tool_with_subtool();
    std::cout << std::this_thread::get_id()
              << "  Algorithm's Tool1 returned status: " << tool_code1
              << std::endl;
    if (tool_code1.status() !=
        CoroutineTests::alien::tool::StatusCode::SUCCESS) {
        co_return CoroutineTests::alien::algorithm::StatusCode::FAILURE;
    }
    auto tool_code2 = co_await tool_with_subtool();
    std::cout << std::this_thread::get_id()
              << "  Algorithm's Tool2 returned status: " << tool_code2
              << std::endl;
    if (tool_code2.status() !=
        CoroutineTests::alien::tool::StatusCode::SUCCESS) {
        co_return CoroutineTests::alien::algorithm::StatusCode::FAILURE;
    }
    co_return CoroutineTests::alien::algorithm::StatusCode::SUCCESS;
}

int main() {
    std::cout << std::this_thread::get_id() << "  Starting main\n";
    CoroutineTests::Threadpool threadpool(1);
    {
        auto alg = trivial_algorithm();
        auto future = alg.schedule_on(threadpool);
        std::cout << std::this_thread::get_id()
                  << "  Algorithm scheduled, waiting for completion...\n";
        auto status = future.get();
        std::cout << std::this_thread::get_id()
                  << "  Algorithm completed with status: " << status
                  << "\n---\n";
    }
    {
        std::cout << std::this_thread::get_id()
                  << "  Starting algorithm with awaiter...\n";
        auto alg = algorithm_with_awaiter();
        auto future = alg.schedule_on(threadpool);
        std::cout << std::this_thread::get_id()
                  << "  Algorithm scheduled, waiting for completion...\n";
        auto status = future.get();
        std::cout << std::this_thread::get_id()
                  << "  Algorithm completed with status: " << status
                  << "\n---\n";
    }
    {
        std::cout << std::this_thread::get_id()
                  << "  Starting algorithm with tool...\n";
        auto t = algorithm_w_tools();
        auto future = t.schedule_on(threadpool);
        std::cout << std::this_thread::get_id()
                  << "  Tool scheduled, waiting for completion...\n";
        auto status = future.get();
        std::cout << std::this_thread::get_id()
                  << "  Tool completed with status: " << status << "\n---\n";
    }
    {
        std::cout << std::this_thread::get_id()
                  << "  Starting algorithm with tools and subtools...\n";
        auto t = algorithm_w_subtools();
        auto future = t.schedule_on(threadpool);
        std::cout << std::this_thread::get_id()
                  << "  Algorithm scheduled, waiting for completion...\n";
        auto status = future.get();
        std::cout << std::this_thread::get_id()
                  << "  Algorithm completed with status: " << status << "\n";
    }
    return 0;
}
