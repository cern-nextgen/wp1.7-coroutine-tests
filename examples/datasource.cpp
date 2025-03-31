#include "CoroutineTests/datasource.hpp"

#include <iostream>

CoroutineTests::DataSource<int> example() {
    auto i = 0;
    while (true) {
        co_await CoroutineTests::OutputAwaiter{i++};
    }
}


int main() {
    std::cout << "DataSource example:\n";
    auto source = example();
    std::cout << "Got: " << source.get() << '\n';
    std::cout << "Got: " << source.get() << '\n';
    std::cout << "Got: " << source.get() << '\n';
    return 0;
}
