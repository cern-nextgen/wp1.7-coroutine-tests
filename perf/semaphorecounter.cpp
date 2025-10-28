#include <cstdint>
#include <iostream>
#include <semaphore>
#include <stdexcept>
#include <thread>

#include "Timer.hpp"

#pragma GCC push_options
#pragma GCC optimize("O0")

auto N = 1000ull * 1000 * 10;

class CollaborativeCounter {
    public:
    void setPeer(CollaborativeCounter &peer) {
        if (m_peer)
            throw std::runtime_error("peerl already set");
        m_peer = &peer;
    }
    void run() {
        if (!m_peer)
            throw std::runtime_error("peer not set");
        while (true) {
            auto v = getValue();
            m_peer->setValue(v + 1);
            if (v + 1 > N)
                return;
        }
    }

    private:
    CollaborativeCounter *m_peer{nullptr};
    std::uint64_t m_value;
    std::counting_semaphore<1> m_write_sem{1}, m_read_sem{0};
    std::uint64_t getValue() {
        m_read_sem.acquire();
        auto ret = m_value;
        m_write_sem.release();
        return ret;
    }

    public:
    void setValue(std::uint64_t v) {
        m_write_sem.acquire();
        m_value = v;
        m_read_sem.release();
    }
};

int main() {
    CollaborativeCounter a, b;
    a.setPeer(b), b.setPeer(a);
    std::thread ta{[&a]() { a.run(); }}, tb{[&b]() { b.run(); }};
    Timer t;
    a.setValue(0);
    ta.join();
    auto dt = t.secs();
    tb.join();
    auto ns = dt / N * 1000ull * 1000 * 1000;
    std::cout << "count time=" << ns << " ns" << std::endl;
    return 0;
}

#pragma GCC pop_options
