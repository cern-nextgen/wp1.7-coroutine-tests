#pragma once
#include <cuda_runtime_api.h>

#include <coroutine>

/// Awaitable that resumes a coroutine when a CUDA stream reaches a certain
/// point. Internally cudaLaunchHostFunc is used to set up a resumption callback
/// on the stream.
class StreamAwaitable {
    public:
    explicit StreamAwaitable(cudaStream_t stream) : m_stream(stream) {}

    bool await_ready() const noexcept { return false; }
    template <typename Promise>
    void await_suspend(std::coroutine_handle<Promise> handle) {
        m_error = cudaLaunchHostFunc(m_stream, resumption_callback<Promise>,
                                     handle.address());
        // If the callback couldn't be registered, we need to reschedule the
        // coroutine immediately to avoid deadlock.
        if (m_error != cudaSuccess) {
            handle.promise().reschedule();
        }
    }
    cudaError_t await_resume() const noexcept { return m_error; }

    private:
    cudaStream_t m_stream;
    cudaError_t m_error = cudaSuccess;

    template <typename Promise>
    static void resumption_callback(void* userData) {
        auto handle = std::coroutine_handle<Promise>::from_address(userData);
        handle.promise().reschedule();
    }
};
