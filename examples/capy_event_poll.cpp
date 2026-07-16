#include <cuda_runtime_api.h>
#include <tbb/task_arena.h>

#include <boost/capy.hpp>
#include <boost/capy/ex/thread_pool.hpp>
#include <cstddef>
#include <iostream>
#include <latch>
#include <string_view>
#include <utility>

#include "capy_task_arena_executor.hpp"  // TaskArenaExecutor
#include "logging_utils.hpp"             // log, format_name
#include "nanospin.hpp"                  // launch_nanospin
#include "statuscode.hpp"                // StatusCodeImpl

namespace tools {
struct Tag {
    static constexpr const char* name = "tools";
};
using StatusCode = StatusCodeImpl<Tag>;
}  // namespace tools

#define ERROR_CHECK_CUDA(EXP)                                              \
    do {                                                                   \
        cudaError_t errorCode = EXP;                                       \
        if (errorCode != cudaSuccess) {                                    \
            throw std::runtime_error(                                      \
                std::format("CUDA error at {}:{}: {}", __FILE__, __LINE__, \
                            cudaGetErrorString(errorCode)));               \
        }                                                                  \
    } while (false)

template <typename T>
struct DeviceBuffer {
    T* ptr = nullptr;
    std::size_t size = 0;
};

template <typename F>
boost::capy::task<void> delegate(F&& f) {
    std::forward<F>(f)();
    co_return;
}

struct Retry {
    boost::capy::continuation continuation;
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> handle,
                       boost::capy::io_env const* env) noexcept {
        continuation.h = handle;
        env->executor.post(continuation);
    }
    void await_resume() const noexcept {}
};

boost::capy::task<void> poll(cudaEvent_t event, std::string_view parent) {
    auto status = cudaSuccess;
    log(parent) << "Polling for event completion..." << std::endl;
    while ((status = cudaEventQuery(event)) == cudaErrorNotReady) {
        log(parent) << "Event not ready, retrying..." << std::endl;
        co_await Retry{};
    }
    ERROR_CHECK_CUDA(status);
    log(parent) << "Event completed successfully" << std::endl;
}

boost::capy::task<DeviceBuffer<int>> clusterization(
    DeviceBuffer<int> cells, cudaStream_t stream, cudaEvent_t event,
    boost::capy::thread_pool& delegation_thread, std::string_view parent) {

    const auto self = format_name(parent, "clusterization");
    log(self) << "Starting clusterization" << std::endl;

    const auto nCells = static_cast<int>(cells.size);

    // Copy cells back to host to count non-zero entries
    auto h_cells = std::vector<int>(nCells);

    co_await boost::capy::run(delegation_thread.get_executor())(delegate([&]() {
        log(self) << "Delegated copy of cells from device to host" << std::endl;
        ERROR_CHECK_CUDA(cudaMemcpyAsync(h_cells.data(), cells.ptr,
                                         nCells * sizeof(int),
                                         cudaMemcpyDeviceToHost, stream));
        ERROR_CHECK_CUDA(cudaEventRecord(event, stream));
    }));

    co_await boost::capy::run(delegation_thread.get_executor())(
        poll(event, self));

    auto nClusters = 0;
    for (auto v : h_cells)
        if (v != 0)
            ++nClusters;

    log(self) << "Found " << nClusters << " clusters" << std::endl;

    // Allocate clusters of appropriate size on device
    int* d_clusters = nullptr;

    co_await boost::capy::run(delegation_thread.get_executor())(delegate([&]() {
        log(self) << "Delegated allocation of clusters on device" << std::endl;
        ERROR_CHECK_CUDA(cudaMallocAsync(reinterpret_cast<void**>(&d_clusters),
                                         nClusters * sizeof(int), stream));

        // Write some dummy data to the clusters buffer to simulate work
        ERROR_CHECK_CUDA(
            cudaMemsetAsync(d_clusters, 0, nClusters * sizeof(int), stream));
        ERROR_CHECK_CUDA(cudaMemsetAsync(d_clusters, 1,
                                         nClusters / 2 * sizeof(int), stream));
        launch_nanospin(1'000'000, stream);
    }));

    co_return DeviceBuffer<int>{d_clusters,
                                static_cast<std::size_t>(nClusters)};
}

boost::capy::task<DeviceBuffer<int>> seeding(
    DeviceBuffer<int> clusters, cudaStream_t stream, cudaEvent_t event,
    boost::capy::thread_pool& delegation_thread, std::string_view parent) {

    const auto self = format_name(parent, "seeding");
    log(self) << "Starting seeding" << std::endl;

    const auto nClusters = static_cast<int>(clusters.size);

    // Copy clusters to host to count non-zero entries
    auto h_clusters = std::vector<int>(nClusters);

    co_await boost::capy::run(delegation_thread.get_executor())(delegate([&]() {
        log(self) << "Delegated copy of clusters to host" << std::endl;
        ERROR_CHECK_CUDA(cudaMemcpyAsync(h_clusters.data(), clusters.ptr,
                                         nClusters * sizeof(int),
                                         cudaMemcpyDeviceToHost, stream));
        ERROR_CHECK_CUDA(cudaEventRecord(event, stream));
    }));

    co_await boost::capy::run(delegation_thread.get_executor())(
        poll(event, self));

    int nSeeds = 0;
    for (auto v : h_clusters)
        if (v != 0)
            ++nSeeds;

    log(self) << "Found " << nSeeds << " seeds" << std::endl;

    // Allocate seeds of appropriate size on device
    int* d_seeds = nullptr;

    co_await boost::capy::run(delegation_thread.get_executor())(delegate([&]() {
        log(self) << "Delegated allocation of seeds on device" << std::endl;
        ERROR_CHECK_CUDA(cudaMallocAsync(reinterpret_cast<void**>(&d_seeds),
                                         nSeeds * sizeof(int), stream));

        // Write some dummy data to the seeds buffer to simulate work
        ERROR_CHECK_CUDA(
            cudaMemsetAsync(d_seeds, 0, nSeeds * sizeof(int), stream));
        ERROR_CHECK_CUDA(
            cudaMemsetAsync(d_seeds, 1, nSeeds / 2 * sizeof(int), stream));
        launch_nanospin(1'000'000, stream);
    }));

    co_return DeviceBuffer<int>{d_seeds, static_cast<std::size_t>(nSeeds)};
}

boost::capy::task<tools::StatusCode> reconstruct(
    cudaStream_t stream, cudaEvent_t event,
    boost::capy::thread_pool& delegation_thread, std::string parent) {
    const auto self = format_name(parent, "reconstruction");
    log(self) << "Starting reconstruction" << std::endl;

    // Allocate some dummy input data on the device
    auto cells = DeviceBuffer<int>{nullptr, 1000};
    co_await boost::capy::run(delegation_thread.get_executor())(delegate([&]() {
        log(self) << "Delegated allocation of input data on device"
                  << std::endl;
        ERROR_CHECK_CUDA(cudaMallocAsync(reinterpret_cast<void**>(&cells.ptr),
                                         cells.size * sizeof(int), stream));
        ERROR_CHECK_CUDA(
            cudaMemsetAsync(cells.ptr, 1, cells.size * sizeof(int), stream));
    }));

    // Run the clusterization and seeding steps
    auto clusters =
        co_await clusterization(cells, stream, event, delegation_thread, self);
    auto seeds =
        co_await seeding(clusters, stream, event, delegation_thread, self);

    // Cleanup
    co_await boost::capy::run(delegation_thread.get_executor())(delegate([&]() {
        log(self) << "Delegated cleanup of device memory" << std::endl;
        ERROR_CHECK_CUDA(cudaFreeAsync(cells.ptr, stream));
        ERROR_CHECK_CUDA(cudaFreeAsync(clusters.ptr, stream));
        ERROR_CHECK_CUDA(cudaFreeAsync(seeds.ptr, stream));
        ERROR_CHECK_CUDA(cudaEventRecord(event, stream));
    }));

    co_await boost::capy::run(delegation_thread.get_executor())(
        poll(event, self));

    log(self) << "Finishing reconstruction" << std::endl;
    co_return tools::StatusCode::SUCCESS;
}

int main() {
    int deviceCount = 0;
    auto error_id = cudaGetDeviceCount(&deviceCount);

    if (error_id != cudaSuccess) {
        std::cout << "cudaGetDeviceCount returned "
                  << static_cast<int>(error_id) << "\n"
                  << cudaGetErrorString(error_id) << "\n";
        return EXIT_FAILURE;
    }

    if (deviceCount == 0) {
        std::cout << "No CUDA devices found.\n";
        return EXIT_FAILURE;
    }

    log("main") << "Starting" << std::endl;

    auto task_arena = tbb::task_arena{2, 0};
    auto context = TaskArenaContext(task_arena);
    auto executor = TaskArenaExecutor(context);
    auto delegation_thread = boost::capy::thread_pool(1);

    {
        std::cout << "--- Single event, synchronous wait for completion ---\n";
        cudaStream_t stream;
        ERROR_CHECK_CUDA(cudaStreamCreate(&stream));
        cudaEvent_t event;
        ERROR_CHECK_CUDA(
            cudaEventCreateWithFlags(&event, cudaEventDisableTiming));
        auto final_result = tools::StatusCode{};
        auto done = std::latch{1};
        auto result_handler = [&done, &final_result](tools::StatusCode code) {
            final_result = code;
            done.count_down();
        };

        log("main") << "Launching algorithm..." << std::endl;
        boost::capy::run_async(executor, result_handler)(
            reconstruct(stream, event, delegation_thread, "main"));
        done.wait();
        log("main") << "Final status of algorithm " << final_result << ""
                    << std::endl;
        ERROR_CHECK_CUDA(cudaEventDestroy(event));
        ERROR_CHECK_CUDA(cudaStreamDestroy(stream));
    }

    {
        std::cout << "--- Multiple events, wait for all to complete ---\n ";

        auto streams = std::vector<cudaStream_t>(2);
        auto events = std::vector<cudaEvent_t>(streams.size());
        auto status = std::vector<tools::StatusCode>(streams.size());
        for (std::size_t i = 0; i < streams.size(); ++i) {
            ERROR_CHECK_CUDA(cudaStreamCreate(&streams.at(i)));
            ERROR_CHECK_CUDA(cudaEventCreateWithFlags(&events.at(i),
                                                      cudaEventDisableTiming));
        }

        auto done = std::latch{static_cast<std::ptrdiff_t>(streams.size())};
        log("main") << "Launching algorithms..." << std::endl;

        for (std::size_t i = 0; i < streams.size(); ++i) {
            auto result_handler = [&done, &status, i](tools::StatusCode code) {
                status.at(i) = code;
                done.count_down();
            };

// silence false-positive warning about uninitialized variables in Capy
// allocator, affected GCC 15, 16
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

            boost::capy::run_async(executor, result_handler)(
                reconstruct(streams.at(i), events.at(i), delegation_thread,
                            "event" + std::to_string(i)));
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
        }
        done.wait();
        for (std::size_t i = 0; i < streams.size(); ++i) {
            ERROR_CHECK_CUDA(cudaEventDestroy(events.at(i)));
            ERROR_CHECK_CUDA(cudaStreamDestroy(streams.at(i)));
        }

        log("main") << "All algorithms completed. Final statuses: ";
        for (auto s : status) {
            std::cout << s << ' ';
        }
        std::cout << std::endl;
    }
    return 0;
}
