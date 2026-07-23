#pragma once

// Backend selection: stdexec (default) or beman::execution

#ifdef USE_BEMAN
#include <beman/execution/execution.hpp>
#include <iostream>
namespace execution = beman::execution;
#else
#include <exec/async_scope.hpp>
#include <exec/task.hpp>
#include <stdexec/execution.hpp>
namespace execution {
using namespace stdexec;

using sender_tag = stdexec::sender_t;
using receiver_tag = stdexec::receiver_t;
using scheduler_tag = stdexec::scheduler_t;
using operation_state_tag = stdexec::operation_state_t;

template <typename T>
using task = exec::task<T>;
using run_loop = execution::run_loop;
}  // namespace execution
#endif

#include <thread>

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
                    std::cerr << "Error" << std::endl;
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

class single_thread_context {
    execution::run_loop loop_;
    std::thread thread_;

    public:
    single_thread_context() : loop_(), thread_([this] { loop_.run(); }) {}

    ~single_thread_context() {
        loop_.finish();
        thread_.join();
    }

    auto get_scheduler() noexcept { return loop_.get_scheduler(); }

    [[nodiscard]] auto get_thread_id() const noexcept -> std::thread::id {
        return thread_.get_id();
    }
};
