# WP 1.7 Coroutine Tests

These are some (largely) independent tests with
[coroutines](https://en.cppreference.com/w/cpp/language/coroutines)
for the task scheduling studies in Work Package 1.7.

This project isn't intended as a coroutine support library.  If you're looking for a fully-fledged coroutine library, consider using [cppcoro](https://github.com/lewissbaker/cppcoro), [libcoro](https://github.com/jbaldwin/libcoro) or  [concurrencpp](https://github.com/David-Haim/concurrencpp). For composing asynchronous operations across libraries, consider using either [std::execution](https://en.cppreference.com/w/cpp/experimental/execution.html) (senders/receivers, with reference implementation [Nvidia/stdexec](https://github.com/NVIDIA/stdexec) or [bemanproject/execution](https://github.com/bemanproject/execution)) or IoAwaitables protocol ([p4003](https://www.open-std.org/JTC1/SC22/WG21/docs/papers/2026/p4003r0.pdf), with reference implementation [Boost.Capy](https://github.com/cppalliance/capy/)).

## Getting started

The project requires a C++20 compiler with coroutine support.

To build the project run:

```sh
cmake --workflow --preset 20
```

Building the examples using `std::generator` or `beman.task` requires C++23 standard:

```sh
cmake --workflow --preset 23
```

The `20-cuda` and `23-cuda` presets can be used to enable building examples using CUDA (version of CUDA used must support given C++ standard).

Then run the examples, for instance:

```sh
./build/examples/generator
```

## Development

To make experimentation easier, a [skeleton coroutine](./coroutine_skeleton.hpp) is provided that includes most of the boilerplate code and can be copied and completed with your own implementation.
