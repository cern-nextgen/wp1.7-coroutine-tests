#pragma once

#include <cuda_runtime_api.h>

#include "exec_backend.hpp"  // std exec backend selection
/// Wrapper sender suspending execution until all operations on a CUDA
/// stream are complete.

class stream_await_sender {
    public:
    struct env {};

    // associated operation state
    template <execution::receiver Receiver>
    class stream_await_operation;

    using sender_concept = execution::sender_t;
    using completion_signatures =
        execution::completion_signatures<execution::set_value_t(cudaError_t)>;

    explicit stream_await_sender(const cudaStream_t stream)
        : m_stream(stream) {}
    env get_env() const noexcept { return {}; }

    template <execution::receiver Receiver>
    auto connect(Receiver&& receiver) const {
        return stream_await_operation<std::remove_cvref_t<Receiver>>(
            std::forward<Receiver>(receiver), m_stream);
    }

    private:
    cudaStream_t m_stream;
};

/// Operation state associated with @c stream_await_sender
///
template <execution::receiver Receiver>
class stream_await_sender::stream_await_operation {
    public:
    using operation_state_concept = execution::operation_state_t;

    stream_await_operation(Receiver&& recv, const cudaStream_t stream)
        : m_receiver(std::forward<Receiver>(recv)), m_stream(stream) {}

    void start() & noexcept {

        auto err = cudaLaunchHostFunc(m_stream, callback, &m_receiver);
        // If setting up the callback failed, we need to propage the error
        // immediately
        if (err != cudaSuccess) {
            execution::set_value(std::move(m_receiver), err);
        }
    }

    private:
    std::remove_cvref_t<Receiver> m_receiver;
    cudaStream_t m_stream;

    static void CUDART_CB callback(void* userData) {
        auto& recv = *static_cast<Receiver*>(userData);
        execution::set_value(std::move(recv), cudaSuccess);
    }
};

static_assert(execution::sender<stream_await_sender>);
