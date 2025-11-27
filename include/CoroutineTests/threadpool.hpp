#ifndef COROUTINETESTS_THREADPOOL_H
#define COROUTINETESTS_THREADPOOL_H

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
namespace CoroutineTests {
// Simple and naive thread pool implementation.
// Coroutines are pushed to a common queue and picked-up by the threads to
// execute.

class Threadpool {
    public:
    explicit Threadpool(const std::size_t threadCount) {
        for (std::size_t i = 0; i < threadCount; ++i) {
            m_threads.emplace_back([this]() { thread_loop(); });
        }
    }

    ~Threadpool() {
        m_stop = true;
        m_cond.notify_all();
    }

    void enqueue_task(std::function<void()> task) noexcept {
        auto lock = std::unique_lock<std::mutex>(m_mutex);
        m_tasks.push(task);
        m_cond.notify_one();
    }

    private:
    std::vector<std::jthread> m_threads;

    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::queue<std::function<void()>> m_tasks;

    std::atomic<bool> m_stop = false;

    void thread_loop() {
        while (!m_stop) {
            auto lock = std::unique_lock<std::mutex>(m_mutex);
            while (!m_stop && m_tasks.empty()) {
                m_cond.wait(lock);
            }
            if (m_stop) {
                break;
            }
            auto task = m_tasks.front();
            m_tasks.pop();
            lock.unlock();
            task();
        }
    }
};

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_THREADPOOL_H
