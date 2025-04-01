#include "CoroutineTests/datasink.hpp"

#include <iostream>

CoroutineTests::DataSink<int> example() {
    while (true) {
        auto x = co_await CoroutineTests::InputAwaiter<int>{};
        std::cout << "Coroutine received: " << x << '\n';
    }
}

int main() {
    std::cout << "DataSink example:\n";
    auto sink = example();
    sink.put(42);
    sink.put(1);
    sink.put(-10);
    return 0;
}
