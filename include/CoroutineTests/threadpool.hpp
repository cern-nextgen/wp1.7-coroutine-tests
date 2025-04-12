#ifndef COROUTINETESTS_THREADPOOL_H
#define COROUTINETESTS_THREADPOOL_H

#include <condition_variable>
#include <coroutine>
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

    void enqueue_task(std::coroutine_handle<> coro) noexcept {
        auto lock = std::unique_lock<std::mutex>(m_mutex);
        m_tasks.push(coro);
        m_cond.notify_one();
    }

    private:
    std::vector<std::jthread> m_threads;

    std::mutex m_mutex;
    std::condition_variable m_cond;
    std::queue<std::coroutine_handle<>> m_tasks;

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
            task.resume();
        }
    }
};

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_THREADPOOL_H
