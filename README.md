# WP 1.7 Coroutine Tests

These are some (largely) independent tests with
[coroutines](https://en.cppreference.com/w/cpp/language/coroutines)
for the task scheduling studies in Work Package 1.7.

This project isn't intended as a coroutine support library.  If you're looking for a fully-fledged coroutine library, consider using [cppcoro](https://github.com/lewissbaker/cppcoro) or [concurrencpp](https://github.com/David-Haim/concurrencpp).

## Getting started

The project requires a C++20 compiler with coroutine support.

To build the project run:

```sh
cmake -S . -B build
cmake --build build
```

Building the examples using `std::generator` require C++23 standard:

```sh
cmake -S . -B build -DCMAKE_CXX_STANDARD=23
cmake --build build
```

Then run the examples, for instance:

```sh
./build/examples/generator
```

