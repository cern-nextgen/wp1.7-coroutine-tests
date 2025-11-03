# Examples

Coroutines are a C++ feature that allow functions to suspend and resume execution, enabling concurrent programming with cleaner syntax.
A coroutine consists of three components:

- Return/Interface type: The return type of the coroutine (e.g. generator<T>, task<T>) that users interact with. No consensus on naming in the community.
- Promise type - Defines the behaviour of the coroutine: handles setup, suspension, and final result communication between the coroutine and its caller.
- coroutine storage/frame - An object created by the compiler to store local variables and manage suspension/resumption. Accessed with non-owning `std::coroutine_handle` (either parametrised on promise type or type-erased with `std::coroutine_handle<>`).

Coroutines interact with awaitables by awaiting them with `co_await`.

## Task

`Task` is a very simple example coroutine that can be manually resumed but doesn't yield or return any values. It is used mostly to introduce the required boilerplate. The `Task` class defines mandatory `promise_type` and constructor accepting handle to a coroutine storage. It also implements user interface to resume the coroutine and check if it's finished.
The promise type implements mandatory functions:

- `get_return_object` that construct return type.
- `initial_suspend` which is called after construction and returns an awaitable that is then implicitly `co_awaited`. Here a built-in `std::suspend_always` is used to immediately suspend the coroutine.
- `final_suspend` which is called after coroutine reached its end and returns an awaitable that is then implicitly `co_awaited`. Here a built-in `std::suspend_always` is used to leave the coroutine suspended and return to the caller.
- `unhandled_exception` that acts as an implicit try/catch over coroutine body. Here any caught exception is propagated to the return type so that the user interface can access it.
- `return_void` - called when implicit or explicit `co_return` or `co_return void` is reached. Here empty.

## Lazy

`Lazy` and `Eager` are two coroutines returning a value. They are implemented with a common template which parameter determines whether the coroutine starts executing immediately or is suspended. The returned value can be accessed with `get` method. The promise type defines `return_value` method that takes a value that was returned with `co_return` and gives it to the coroutine return type. The initial suspension is controlled with `initial_suspend` method which return either `std::suspend_always` or `std::suspend_never`.

## SimpleGenerator

`SimpleGenerator` shows how `co_yield` can be used to output values from coroutine body to the coroutine return type. In the promise type `yield_value` method is defined to take value yielded with `co_yield value` and stash it in the promise, then return `std::suspend_always` to suspend the coroutine. In the coroutine return type, a `get` method is added to return value yielded from the coroutine, and a `next` method is added to resume the coroutine to the next `co_yield` or end.

## Generator

`Generator` is a refinement of `SimpleGenerator` to fulfil the `std::input_range` concept. This is achieved by using CRTP with `std::view_interface`, adding `begin` and `end` methods, and defining a custom iterator. Iterator's `operator++` is defined to resume the coroutine, `operator*` and `operator->` are defined to return yielded value stashed in promise.

## DataSource

`DataSource` is another variant of generator, which shows that besides `co_yield`, `co_await` also can be used to output values from coroutine frame into promise type. To achieve this a custom awaitable `OutputAwaiter` is defined. The awaiter implements mandatory methods:

- `await_ready` - possibility to decided whether to suspend or not the awaiting coroutine. This is mostly for possible optimization. Here it's not used, the coroutine will be suspended.
- `await_suspend` - called with the coroutine about to be suspended as an argument. The return type decides whether to return to caller or continue with a coroutine (the same or different). Here the value is copied to the promise.
- `await_resume` - called when awaiting coroutine is resumed. Returned value is returned from `co_await` - this is the main spotlight of the next example.

Instead of explicitly `co_await OutputAwaiter{value}` an `await_transform(T value)` method could be defined to internally construct the `OutAwaiter` and allow `co_await value`.

## DataSink

This example shows how additional data can be injected into thecoroutine frame. The main idea is to assign extra data from return type to a promise type member and use returned value from `auto value = co_await awaitable`.
To achieve this a custom `InputAwaiter` awaitable is defined, which defines `await_resume` that returns a value from promise type.

## Nestable Task

`NestableTask` is a refinement of `Task` to allow nesting it with `co_await NestableTask` inside coroutine body. The idea is that on `co_await` the parent coroutine should be suspended and child coroutine should be resumed, then when the child coroutine is finished the parent should be resumed again.
First to allow `co_await NestableTask` an awaitable interface is added to the `NestableTask` return type. In particular the `await_suspend` takes parent coroutine as an argument and stashes it in promise, then continues with the current coroutine (this is known as the symmetric transfer idiom). Secondly the `final_suspend` method is modified to construct and return a custom awaitable. This awaitable defines the `await_suspend` to return the parent coroutine if it exists or otherwise continue with the caller.

## Event

`SimpleEvent` is another custom awaitable. Multiple coroutines can await the same event (taken by references to the coroutine body). Then the coroutines stay suspended on event and automatically resumed when event is set. To achieve this `SimpleEvent` stashes the coroutine handles on `await_suspend` and then resumes them in `set` method.

## Async

Until now, all the example coroutines were executing on the same thread of execution. In this example an `Async` coroutine is introduced to demonstrate executing coroutines on a thread-pool. For this, a simple, naive thread-pool resuming the coroutines is used. The `Async` is based on `NestableTask` with a change that the promise type holds a pointer to the thread-pool. In the `await_suspend` the pointer is propagated from parent coroutine to child coroutine, then the child coroutine is enqueued in the thread-pool instead of being resumed directly via symmetric transfer. Similarly, in `final_suspend` the custom awaiter now enqueues the parent coroutine into the thread-pool.

## Ping-pong

Ping-pong example features two coroutines co-awaiting each other indefinitely. This demonstrates that symmetric transfer (returning coroutine handle to be resumed in `await_suspend`) doesn't build-up the call stack and avoids stack overflow as opposed to resuming a coroutine with `.resume()` in the `await_suspend`. **Compiler-bug** GCC with optimization <=01 still uses extra stack-space even with symmetric transfer. Clang works fine.

## Gaudi

This example shows a hierarchy of coroutines inspired by the [Gaudi](https://gitlab.cern.ch/gaudi/Gaudi) framework, where "algorithm" is the top-level coroutine that calls nested "tools" coroutines, which in turn can also have nested "tools" coroutines. All the coroutines can yield and return status codes which are propagated through the hierarchy.

The "algorithm" coroutine can be manually resumed; each resumption continues from the innermost active coroutine. When that coroutine completes, control returns outward through the chain, and corresponding status code is returned back.

## stdexec_task

This example shows usage of `task` coroutine return type from the [stdexec](https://github.com/NVIDIA/stdexec) library. The example is similar to the `Gaudi` example, featuring a hierarchy of coroutines representing "algorithms" and "tools". The main difference is that the `stdexec::task` return type is used, which provides integration with the `stdexec` execution model. Unlike `Gaudi` example the "algorithm" coroutine doesn't have to be manually resumed; instead it is started by submitting it to a `stdexec` scheduler which handles the execution of the coroutine and its nested coroutines. A custom sender simulating call to an asynchronous API is also implemented, similar to the one in `Async` example.
