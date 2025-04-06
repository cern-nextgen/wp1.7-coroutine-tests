#include "CoroutineTests/simplegenerator.hpp"

#include <cstdint>
#include <iostream>
#include <ranges>
#include <version>
#ifdef __cpp_lib_generator
#include <generator>
#endif

CoroutineTests::SimpleGenerator<int> sequence(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

CoroutineTests::SimpleGenerator<int> infinite_sequence(int start) {
    while (true) {
        co_yield start++;
    }
}

int main() {
    {
        std::cout << "Simple generator example\n";
        std::cout << "Finite sequence:\n";
        auto seq = sequence(0, 10);
        while (!seq.done()) {
            std::cout << seq.get() << ' ';
        }
        std::cout << '\n';
        std::cout << "Infinite sequence:\n";
        auto inf_seq = infinite_sequence(0);
        for (int i = 0; i < 10; ++i) {
            std::cout << inf_seq.get() << ' ';
        }
        std::cout << '\n';
    }
    return 0;
}
