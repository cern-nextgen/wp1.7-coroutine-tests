#include <chrono>
#include <cstdint>
#include <iostream>
#include <semaphore>
#include <stdexcept>
#include <thread>

#include "CoroutineTests/pingpong.hpp"

#if defined(__clang__)
#define NO_OPTIMIZE __attribute__((optnone))
#elif defined(__GNUC__)
#define NO_OPTIMIZE __attribute__((optimize("O0")))
#else
#define NO_OPTIMIZE
#endif

class LoopCounter {
    public:
    LoopCounter(const std::uint64_t max) : m_max{max} {}

    __attribute__((noinline)) NO_OPTIMIZE void count() const {
        std::uint64_t i = 0;
        while (i < m_max)
            i++;
    }

    private:
    std::uint64_t m_max;
};

CoroutineTests::Player coroutine_counter(const std::uint64_t N) {
    std::uint64_t i = 0;
    while (++i < N / 2) {
        co_await CoroutineTests::Play{};
    }
}

class SemaphoreCounter {
    public:
    SemaphoreCounter(const std::uint64_t max) : m_max{max} {}
    void setPeer(SemaphoreCounter &peer) {
        if (m_peer)
            throw std::runtime_error("peer already set");
        m_peer = &peer;
    }
    void run() {
        if (!m_peer)
            throw std::runtime_error("peer not set");
        while (true) {
            auto v = getValue();
            m_peer->setValue(v + 1);
            if (v + 1 > m_max)
                return;
        }
    }

    private:
    SemaphoreCounter *m_peer{nullptr};
    std::uint64_t m_max;
    std::uint64_t m_value{0};
    std::counting_semaphore<1> m_write_sem{1}, m_read_sem{0};
    std::uint64_t getValue() {
        m_read_sem.acquire();
        auto ret = m_value;
        m_write_sem.release();
        return ret;
    }

    public:
    void setValue(const std::uint64_t v) {
        m_write_sem.acquire();
        m_value = v;
        m_read_sem.release();
    }
};

void measure_semaphore_counter(const std::uint64_t N) {
    std::cout << "Semaphore-based counter:\n";
    auto a = SemaphoreCounter(N);
    auto b = SemaphoreCounter(N);
    a.setPeer(b);
    b.setPeer(a);
    std::thread ta{[&a]() { a.run(); }}, tb{[&b]() { b.run(); }};
    auto start = std::chrono::steady_clock::now();
    a.setValue(0);
    ta.join();
    auto stop = std::chrono::steady_clock::now();
    auto dt =
        std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
            stop - start)
            .count();
    tb.join();
    auto ns = dt / N;
    std::cout << "\ttime per iteration: " << ns << " ns\n";
}

void measure_coroutine_counter(const std::uint64_t N) {
    std::cout << "Coroutine-based counter:\n";
    auto a = coroutine_counter(N);
    auto b = coroutine_counter(N);
    a.set_peer(b);
    b.set_peer(a);
    auto start = std::chrono::steady_clock::now();
    a.start();
    auto stop = std::chrono::steady_clock::now();
    auto dt =
        std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
            stop - start)
            .count();
    auto ns = dt / N;
    std::cout << ""
              << "\ttime per iteration: " << ns << " ns\n";
}

void measure_loop_counter(const std::uint64_t N) {
    std::cout << "Simple loop counter (O0):\n";
    LoopCounter counter(N);
    auto start = std::chrono::steady_clock::now();
    counter.count();
    auto stop = std::chrono::steady_clock::now();
    auto dt =
        std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(
            stop - start)
            .count();
    auto ns = dt / N;
    std::cout << "\ttime per iteration: " << ns << " ns\n";
}

// clang-format off
const auto N_loop      = 1'000'000'000ull;
const auto N_coroutine =   100'000'000ull;
const auto N_semaphore =     1'000'000ull;
// clang-format on

int main() {
    measure_loop_counter(N_loop);
    measure_coroutine_counter(N_coroutine);
    measure_semaphore_counter(N_semaphore);
    return 0;
}
