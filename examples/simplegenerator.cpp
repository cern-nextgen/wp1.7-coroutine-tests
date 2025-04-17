#include "CoroutineTests/simplegenerator.hpp"

#include <cstdint>
#include <iostream>
#include <ranges>
#include <version>
#ifdef __cpp_lib_generator
#include <generator>
#endif

CoroutineTests::SimpleGenerator<int> sequence(int start, int end) {
    // This complex counting ensures the last vaue is returned, making the corouting done.
    // (otherwise we get() twice the last value)
    int i=start++;
    int ret = i++;
    while (i < end) {
        co_yield ret;
        ret = i++;
    }
    co_return ret;
}

CoroutineTests::SimpleGenerator<int> infinite_sequence(int start) {
    while (true) {
        co_yield start++;
    }
    // Unreacheable beut needed due to promise co_returning a number
    co_return 0; 
}

int main() {
    {
        std::cout << "Simple generator example\n";
        std::cout << "Finite sequence:\n";
        auto seq = sequence(0, 10);
        while (!seq.done()) {
            auto i = seq.get();
            std::cout << i << ' ';
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
