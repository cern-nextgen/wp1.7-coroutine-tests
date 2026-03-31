#include <coroutine>
#include <functional>

namespace CoroutineTests::alien {

class GetScheduler {
    public:
    // Don't skip suspension, we need to get the scheduler from the promise.
    bool await_ready() const noexcept { return false; }

    // On suspend, capture the handle to the current coroutine and get the
    // scheduler from the promise. Then resume the coroutine immediately.
    template <typename PromiseType>
    std::coroutine_handle<> await_suspend(
        std::coroutine_handle<PromiseType> h) noexcept {
        m_scheduler = h.promise().get_scheduler();
        return h;
    }
    // On resume, return the captured scheduler.
    auto await_resume() const noexcept { return m_scheduler; }

    private:
    // Storage for the captured scheduler.
    std::function<void(std::coroutine_handle<>)> m_scheduler;
};

}  // namespace CoroutineTests::alien
