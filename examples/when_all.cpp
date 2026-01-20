#include "CoroutineTests/alien/when_all.hpp"

#include <chrono>
#include <coroutine>
#include <exception>
#include <format>
#include <iostream>
#include <string_view>
#include <thread>

#include "CoroutineTests/alien/algorithm.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"

// ------------------------------------------------------------
// Logging helpers
// ------------------------------------------------------------

std::ostream& log() {
    return std::cout << std::this_thread::get_id() << "  ";
}

std::ostream& log(std::string_view self) {
    return std::cout << std::this_thread::get_id() << "  " << self << "  ";
}

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
        os << "StatusCode::";
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
    std::string_view parent;

    bool await_ready() const noexcept { return false; }

    template <typename T>
    void await_suspend(std::coroutine_handle<T> handle) noexcept {
        std::thread([this, handle]() {
            const auto self = std::format("   {}.AsyncAPIMockup", parent);
            log(self) << "Async operation started, will take " << delay.count()
                      << " ms" << std::endl;
            std::this_thread::sleep_for(delay);
            log(self) << "Async operation finished" << std::endl;
            handle.promise().reschedule();
        }).detach();
    }
    StatusCode await_resume() const noexcept { return status_code; }
};

// co_await a MockupAwaiter
CoroutineTests::alien::tool::Tool toolA(std::string_view parent) {
    auto self = std::format("   {}.toolA", parent);
    log(self) << "Calling async API in toolA" << std::endl;
    auto status = co_await MockupAwaiter{std::chrono::milliseconds(80),
                                         StatusCode::SUCCESS, self};
    log(self) << "Result from async API in toolA: " << status << std::endl;
    log(self) << "Finishing toolA" << std::endl;
    co_return CoroutineTests::alien::tool::StatusCode::SUCCESS;
}

// co_await a MockupAwaiter
CoroutineTests::alien::tool::Tool toolB(std::string_view parent) {
    auto self = std::format("   {}.toolB", parent);
    log(self) << "Calling async API in toolB" << std::endl;
    auto status = co_await MockupAwaiter{std::chrono::milliseconds(40),
                                         StatusCode::SUCCESS, self};
    log(self) << "Result from async API in toolB: " << status << std::endl;
    log(self) << "Finishing toolB" << std::endl;
    co_return CoroutineTests::alien::tool::StatusCode::FAILURE;
}

// co_await toolA and toolB in parallel via when_all
CoroutineTests::alien::algorithm::Algorithm algorithm(std::string_view parent) {
    auto self = std::format("   {}.algorithm", parent);
    log(self) << "Starting algorithm\n";
    log(self) << "Launching toolA and toolB in parallel\n";
    try {
        auto [codeA, codeB] = co_await when_all(toolA(self), toolB(self));
        log(self) << "Result from toolA: " << codeA
                  << ", Result from toolB: " << codeB << '\n';

    } catch (const std::exception& e) {
        log(self) << "when_all threw: " << e.what() << '\n';
        co_return CoroutineTests::alien::algorithm::StatusCode::FAILURE;
    }
    log(self) << "Finishing algorithm" << std::endl;
    co_return CoroutineTests::alien::algorithm::StatusCode::SUCCESS;
}

int main() {
    log() << "main Starting\n";
    CoroutineTests::Threadpool threadpool(1);
    auto scheduler = [&threadpool](std::coroutine_handle<> handle) {
        log() << "scheduler Reschedule called, enqueuing  resumption\n";
        threadpool.enqueue_task(handle);
    };
    log() << "main Launching algorithm...\n";
    auto t = algorithm("main");
    auto future = t.schedule_on(scheduler);
    log() << "main Waiting for algorithm completion...\n";
    auto status = future.get();
    log() << "main Final status of algorithm " << status << "\n";
    return 0;
}
