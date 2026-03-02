#pragma once
#include <cuda_runtime_api.h>

#include <boost/capy.hpp>
#include <coroutine>

class StreamIoAwaitable {
    public:
    StreamIoAwaitable(cudaStream_t stream) : m_stream(stream) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> handle,
                       boost::capy::io_env const* env) noexcept {
        m_context.handle = handle;
        m_context.env = env;
        m_error = cudaLaunchHostFunc(m_stream, resumption_callback, &m_context);
        // If the callback couldn't be registered, we need to reschedule the
        // coroutine immediately to avoid deadlock.
        if (m_error != cudaSuccess) {
            resumption_callback(&m_context);
        }
    }
    cudaError_t await_resume() const noexcept { return m_error; }

    private:
    struct context {
        std::coroutine_handle<> handle;
        boost::capy::io_env const* env;
    };
    cudaStream_t m_stream;
    cudaError_t m_error = cudaSuccess;
    context m_context;

    static void resumption_callback(void* userData) {
        auto* ctx = static_cast<context*>(userData);
        ctx->env->executor.post(ctx->handle);
    }
};
