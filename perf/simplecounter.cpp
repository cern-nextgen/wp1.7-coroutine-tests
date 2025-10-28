#include <cstdint>
#include <iostream>

#include "Timer.hpp"

#pragma GCC push_options
#pragma GCC optimize("O0")

class SimpleCounter {
    public:
    SimpleCounter(std::uint64_t max) : m_max{max} {}
    void count() {
        uint64_t i = 0;
        while (i < m_max)
            i++;
    }

    private:
    uint64_t m_max;
};

int main() {
    auto count = 1000ull * 1000 * 1000;
    SimpleCounter sc(count);
    Timer t;
    sc.count();
    auto dt = t.secs();
    std::cout << "count time=" << dt / count * 1000ull * 1000 * 1000 << " ns"
              << std::endl;
}

#pragma GCC pop_options
