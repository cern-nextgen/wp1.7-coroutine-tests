#pragma once

// Backend selection: stdexec (default) or beman::execution

#ifdef USE_BEMAN
#include <beman/execution/execution.hpp>
#include <beman/task/task.hpp>
#include <iostream>
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
