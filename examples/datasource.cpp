#include "CoroutineTests/datasource.hpp"

#include <iostream>

CoroutineTests::DataSource<int> example() {
    co_await CoroutineTests::OutputAwaiter{42};
    co_await CoroutineTests::OutputAwaiter{1};
    co_await CoroutineTests::OutputAwaiter{-10};
    // Throw an exception when asking for too much. Another way to achieve this
    // would be to have a non void return and to call co_return instead of the
    // last co_await.
    throw std::runtime_error("Running out of data, one too many get() call!");
}

int main() {
    std::cout << "DataSource example:\n";
    auto source = example();
    std::cout << "Got: " << source.get() << '\n';
    std::cout << "Got: " << source.get() << '\n';
    std::cout << "Got: " << source.get() << '\n';
    try {
        source.get();
        std::cerr << "Failed to receive exception after one too many get()"
                  << std::endl;
        exit(EXIT_FAILURE);
    } catch (...) {
        std::cout << "Caught expected exception after one too many get()"
                  << std::endl;
    }
    return 0;
}
