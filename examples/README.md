# Examples

Coroutines are a C++ feature that allow functions to suspend and resume execution, enabling concurrent programming with cleaner syntax.
A coroutine consists of three components:

- Return/Interface type: The return type of the coroutine (e.g. generator<T>, task<T>) that users interact with. No consensus on naming in the community.
- Promise type - Defines the behaviour of the coroutine: handles setup, suspension, and final result communication between the coroutine and its caller.
- coroutine storage/frame - An object created by the compiler to store local variables and manage suspension/resumption. Accessed with non-owning `std::coroutine_handle` (either parametrised on promise type or type-erased with `std::coroutine_handle<>`).

Coroutines interact with awaitables by awaiting them with `co_await`.

## Task

Link: [task.cpp](task.cpp)

`Task` is a very simple example coroutine that can be manually resumed but doesn't yield or return any values. It is used mostly to introduce the required boilerplate. The `Task` class defines mandatory `promise_type` and constructor accepting handle to a coroutine storage. It also implements user interface to resume the coroutine and check if it's finished.
The promise type implements mandatory functions:

- `get_return_object` that construct return type.
- `initial_suspend` which is called after construction and returns an awaitable that is then implicitly `co_awaited`. Here a built-in `std::suspend_always` is used to immediately suspend the coroutine.
- `final_suspend` which is called after coroutine reached its end and returns an awaitable that is then implicitly `co_awaited`. Here a built-in `std::suspend_always` is used to leave the coroutine suspended and return to the caller.
- `unhandled_exception` that acts as an implicit try/catch over coroutine body. Here any caught exception is propagated to the return type so that the user interface can access it.
- `return_void` - called when implicit or explicit `co_return` or `co_return void` is reached. Here empty.

## Lazy

Link: [lazy.cpp](lazy.cpp)

`Lazy` and `Eager` are two coroutines returning a value. They are implemented with a common template which parameter determines whether the coroutine starts executing immediately or is suspended. The returned value can be accessed with `get` method. The promise type defines `return_value` method that takes a value that was returned with `co_return` and gives it to the coroutine return type. The initial suspension is controlled with `initial_suspend` method which return either `std::suspend_always` or `std::suspend_never`.

## SimpleGenerator

Link: [simplegenerator.cpp](simplegenerator.cpp)

`SimpleGenerator` shows how `co_yield` can be used to output values from coroutine body to the coroutine return type. In the promise type `yield_value` method is defined to take value yielded with `co_yield value` and stash it in the promise, then return `std::suspend_always` to suspend the coroutine. In the coroutine return type, a `get` method is added to return value yielded from the coroutine, and a `next` method is added to resume the coroutine to the next `co_yield` or end.

## Generator

Link: [generator.cpp](generator.cpp)

`Generator` is a refinement of `SimpleGenerator` to fulfil the `std::input_range` concept. This is achieved by using CRTP with `std::view_interface`, adding `begin` and `end` methods, and defining a custom iterator. Iterator's `operator++` is defined to resume the coroutine, `operator*` and `operator->` are defined to return yielded value stashed in promise.

## DataSource

Link: [datasource.cpp](datasource.cpp)

`DataSource` is another variant of generator, which shows that besides `co_yield`, `co_await` also can be used to output values from coroutine frame into promise type. To achieve this a custom awaitable `OutputAwaiter` is defined. The awaiter implements mandatory methods:

- `await_ready` - possibility to decided whether to suspend or not the awaiting coroutine. This is mostly for possible optimization. Here it's not used, the coroutine will be suspended.
- `await_suspend` - called with the coroutine about to be suspended as an argument. The return type decides whether to return to caller or continue with a coroutine (the same or different). Here the value is copied to the promise.
- `await_resume` - called when awaiting coroutine is resumed. Returned value is returned from `co_await` - this is the main spotlight of the next example.

Instead of explicitly `co_await OutputAwaiter{value}` an `await_transform(T value)` method could be defined to internally construct the `OutAwaiter` and allow `co_await value`.

## DataSink

Link: [datasink.cpp](datasink.cpp)

This example shows how additional data can be injected into the coroutine frame. The main idea is to assign extra data from return type to a promise type member and use returned value from `auto value = co_await awaitable`.
To achieve this a custom `InputAwaiter` awaitable is defined, which defines `await_resume` that returns a value from promise type.

## Nestable Task

Link: [nestabletask.cpp](nestabletask.cpp)

`NestableTask` is a refinement of `Task` to allow nesting it with `co_await NestableTask` inside coroutine body. The idea is that on `co_await` the parent coroutine should be suspended and child coroutine should be resumed, then when the child coroutine is finished the parent should be resumed again.
First to allow `co_await NestableTask` an awaitable interface is added to the `NestableTask` return type. In particular the `await_suspend` takes parent coroutine as an argument and stashes it in promise, then continues with the current coroutine (this is known as the symmetric transfer idiom). Secondly the `final_suspend` method is modified to construct and return a custom awaitable. This awaitable defines the `await_suspend` to return the parent coroutine if it exists or otherwise continue with the caller.

## Event

Link: [event.cpp](event.cpp)

`SimpleEvent` is another custom awaitable. Multiple coroutines can await the same event (taken by references to the coroutine body). Then the coroutines stay suspended on event and automatically resumed when event is set. To achieve this `SimpleEvent` stashes the coroutine handles on `await_suspend` and then resumes them in `set` method.

## Async

Link: [async.cpp](async.cpp)

Until now, all the example coroutines were executing on the same thread of execution. In this example an `Async` coroutine is introduced to demonstrate executing coroutines on a thread-pool. For this, a simple, naive thread-pool resuming the coroutines is used. The `Async` is based on `NestableTask` with a change that the promise type holds a pointer to the thread-pool. In the `await_suspend` the pointer is propagated from parent coroutine to child coroutine, then the child coroutine is enqueued in the thread-pool instead of being resumed directly via symmetric transfer. Similarly, in `final_suspend` the custom awaiter now enqueues the parent coroutine into the thread-pool.

## Ping-pong

Link: [pingpong.cpp](pingpong.cpp)

Ping-pong example features two coroutines co-awaiting each other indefinitely. This demonstrates that symmetric transfer (returning coroutine handle to be resumed in `await_suspend`) doesn't build-up the call stack and avoids stack overflow as opposed to resuming a coroutine with `.resume()` in the `await_suspend`. **Compiler-bug** GCC with optimization <=01 still uses extra stack-space even with symmetric transfer. Clang works fine.

## Gaudi

Link: [gaudi.cpp](gaudi.cpp)

This example shows a hierarchy of coroutines inspired by the [Gaudi](https://gitlab.cern.ch/gaudi/Gaudi) framework, where "algorithm" is the top-level coroutine that calls nested "tools" coroutines, which in turn can also have nested "tools" coroutines. All the coroutines can yield and return status codes which are propagated through the hierarchy.

The "algorithm" coroutine can be manually resumed; each resumption continues from the innermost active coroutine. When that coroutine completes, control returns outward through the chain, and corresponding status code is returned back.

## Exec task

Link: [exec_task.cpp](exec_task.cpp)

This example shows usage of `task` coroutine return type from future C++26 standard. The example can be compiled with either [stdexec](https://github.com/NVIDIA/stdexec) or [beman.task](https://github.com/bemanproject/execution) library. The example is similar to the "Gaudi" example, featuring a hierarchy of coroutines representing "algorithms" and "tools". The main difference is that the `task` return type is used, which provides integration with the "senders/receivers" execution model. Unlike "Gaudi" example the "algorithm" coroutine doesn't have to be manually resumed; instead it is started by submitting it to a "scheduler" which handles the execution of the coroutine and its nested coroutines. A custom sender simulating call to an asynchronous API is also implemented, similar to the one in "Async" example.

## Exec TBB

Link: [exec_tbb.cpp](exec_tbb.cpp)

This is a variant of the ["Exec task" example](#exec-task), but using a custom C++ senders/receivers scheduler to execute task coroutines on Intel TBB task arena.

## Alien

Link: [alien.cpp](alien.cpp)

This example demonstrates how multiple coroutine types that know nothing about each other can interoperate in a hierarchy, similar to the ["Gaudi" example](#gaudi). The `algorithm::Task` coroutine can be directly scheduled, while the other coroutine types can only by awaited by their parent coroutine. The hierarchy requires that:

- each coroutine implements `get_scheduler` method to provide access to its scheduler for its child coroutines
- `co_await` a child coroutine transfers control to the child coroutine
- `final_suspend` method in each coroutine resumes its parent coroutine

## Alien manual

Link: [alien_manual.cpp](alien_manual.cpp)

This is an alternative implementation of the ["Alien" example](#alien), where the `algorithm::Task` coroutine is supposed to be manually resumed from outside instead of automatically continuing once scheduled. The "scheduler" handle passed between coroutines is supposed to notify the caller when the `algorithm::Task` coroutine is ready to be resumed and internally inform the `algorithm::Task` coroutine about next child coroutine to resume. The coroutines in the hierarchy still doesn't know each other types and relay only on the common interface with `get_scheduler` and semantic of `co_await` and `final_suspend` as in the ["Alien" example](#alien).

## Alien manual semaphore

Link: [alien_manual_semaphore.cpp](alien_manual_semaphore.cpp)

This example is a alternative of the ["Alien manual" example](#alien-manual), where instead of a custom scheduler, a binary semaphore is used to notify the caller when the `algorithm::Task` coroutine is ready to be resumed. The example be default run in a single-threaded mode, but can also be run in multi-threaded mode with `--mt` in which case the resumption will be enqueued into a thread-pool.

## Alien when_all

Link: [alien_when_all.cpp](alien_when_all.cpp)

This example demonstrates a `when_all` algorithm compatible with coroutine semantics as in ["Alien" example](#alien). The `when_all` algorithm takes multiple awaitables, awaits them all concurrently, and returns a tuple of their results once all are completed. This implementation returns results in a tuple, where `void` results are represented by `std::monostate`. In case of exceptions thrown by nested awaitables, the first encountered exception is stored, the others tasks continue to completion, and then the exception is rethrown.

## Alien sync_wait

Link: [alien_sync_wait.cpp](alien_sync_wait.cpp)

This example demonstrates a `sync_wait` algorithm compatible with coroutine semantics as in ["Alien" example](#alien). The algorithm takes a scheduler and coroutine, waits until its completion, and return the results or rethrows an exception.

## Alien counting_scope

Link: [alien_counting_scope.cpp](alien_counting_scope.cpp)

This example demonstrates dynamic work submitting with `counting_scope` compatible with coroutine semantic as in ["Alien" example](#alien). `spawn` schedules execution of a coroutine that returns `void`. `join()` blocks the current thread until all submitted coroutines are finished (and rethrows the first exception captured from submitted work, if any).

## Capy task

Link: [capy_task.cpp](capy_task.cpp)

This is a variant of ["Exec task" example](#exec-task) using Boost.Capy and IoAwaitables protocol ([p4003](https://www.open-std.org/JTC1/SC22/WG21/docs/papers/2026/p4003r0.pdf)) instead of C++26 execution.

## Capy TBB

Link: [capy_tbb.cpp](capy_tbb.cpp)

This is a variant of ["Exec tbb" example](#exec-tbb) using Boost.Capy and IoAwaitables protocol ([p4003](https://www.open-std.org/JTC1/SC22/WG21/docs/papers/2026/p4003r0.pdf)) instead of C++26 execution. This example shows implementation and usage of custom executor scheduling tasks on TBB task arena.

## Reconstruction

Link: [alien_reco.cpp](alien_reco.cpp) and [exec_reco.cpp](exec_reco.cpp)

These examples show a setup and coroutine chain loosely inspired by track reconstruction on GPU in high energy physics experiments. The `reconstruct` coroutine prepares a mockup input data on a CUDA device, then `co_await`s the `clustering` coroutine, then receives output from it and `co_await`s the `seeding` coroutine. Both `clustering` and `seeding` coroutines receive a device buffer, copy it asynchronously back to host, suspend until the copy is done, then count non-zero elements and allocate new buffer of that size for their results. In `main` the `reconstruct` coroutines are executed in a TBB task arena either synchronously waiting for the result from the submitting or dynamically starting a few `reconstruct` coroutines and waiting until all the work is finished. In the `alien_reco` the application is implemented with "alien" coroutines (as in [alien examples](#alien)) , while in `exec_reco_stdexec` the application is implemented with C++26 execution.
