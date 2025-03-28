#include "CoroutineTests/lazy.hpp"

#include <cassert>
#include <iostream>

int answer() {
    std::cout << "Calculating...\n";
    return 42;
}
CoroutineTests::Lazy<int> lazy_answer() {
    co_return answer();
}

CoroutineTests::Eager<int> eager_answer() {
    co_return answer();
}

int main() {
    std::cout << "Lazy evaluation example:\n";
    auto lazy_value = lazy_answer();
    assert(lazy_value.done() == false);
    std::cout << "Value not calculated yet.\n";
    auto result = lazy_value.get();
    assert(lazy_value.done() == true);
    std::cout << "The answer is: " << result << "\n";
    std::cout << "Checking again: " << lazy_value.get() << "\n\n";

    std::cout << "Eager evaluation example:\n";
    auto eager_value = eager_answer();
    assert(lazy_value.done() == true);
    std::cout << "Value already calculated.\n";
    result = eager_value.get();
    assert(lazy_value.done() == true);
    std::cout << "The answer is: " << result << '\n';
    std::cout << "Checking again: " << eager_value.get() << "\n";

    return 0;
}
