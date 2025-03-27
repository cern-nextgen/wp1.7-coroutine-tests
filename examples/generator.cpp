#include "CoroutineTests/generator.hpp"
#include <cstdint>
#include <iostream>
#include <ranges>
#include <version>
#ifdef __cpp_lib_generator
#include <generator>
#endif

// Bounded ranges

CoroutineTests::Generator<int> sequence(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}

#ifdef __cpp_lib_generator
std::generator<int> sequence_std(int start, int end) {
    for (int i = start; i < end; ++i) {
        co_yield i;
    }
}
#endif

// Unbounded ranges

uint32_t xorshift32(uint32_t x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

CoroutineTests::Generator<std::uint32_t> xorshift_generator(
    std::uint32_t seed) {
    while (true) {
        seed = xorshift32(seed);
        co_yield seed;
    }
}

#ifdef __cpp_lib_generator
std::generator<std::uint32_t> xorshift_generator_std(std::uint32_t seed) {
    while (true) {
        seed = xorshift32(seed);
        co_yield seed;
    }
}
#endif

// throwing exceptions

CoroutineTests::Generator<int> throwing_generator() {
    auto i = 0;
    while (true) {
        if (i == 5) {
            throw std::runtime_error("Exception thrown from coroutine");
        }
        co_yield i++;
    }
}

#ifdef __cpp_lib_generator
std::generator<int> throwing_generator_std() {
    auto i = 0;
    while (true) {
        if (i == 5) {
            throw std::runtime_error("Exception thrown from coroutine");
        }
        co_yield i++;
    }
}
#endif

int main() {
    {
        std::cout << "Using CoroutineTests::generator\n";
        auto range = sequence(0, 10);
        for (auto i : range) {
            std::cout << i << ' ';
        }
        std::cout << '\n';
        std::cout << "PRNG:\n";
        for (auto i : xorshift_generator(1) | std::views::take(5)) {
            std::cout << i << ' ';
        }
        std::cout << '\n';
        std::cout << "Throwing exceptions:\n";
        try {
            for (auto i : throwing_generator()) {
                std::cout << i << ' ';
            }
        } catch (const std::exception& e) {
            std::cout << e.what() << '\n';
        }
        std::cout << '\n';
    }
#ifdef __cpp_lib_generator
    {
        std::cout << "Using std::generator\n";
        std::cout << "Range:\n";
        auto range = sequence_std(0, 10);
        for (auto i : range) {
            std::cout << i << ' ';
        }
        std::cout << '\n';
        std::cout << "PRNG:\n";
        for (auto i : xorshift_generator_std(1) | std::views::take(5)) {
            std::cout << i << ' ';
        }
        std::cout << '\n';
        std::cout << "Throwing exceptions:\n";
        try {
            for (auto i : throwing_generator()) {
                std::cout << i << ' ';
            }
        } catch (const std::exception& e) {
            std::cout << e.what() << '\n';
        }
        std::cout << '\n';
    }
#endif
    return 0;
}
