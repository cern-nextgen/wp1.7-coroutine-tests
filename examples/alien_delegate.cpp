#include <cuda_runtime_api.h>
#include <tbb/task_arena.h>

#include <cstddef>
#include <format>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "CoroutineTests/alien/counting_scope.hpp"
#include "CoroutineTests/alien/schedule_on.hpp"
#include "CoroutineTests/alien/subtool.hpp"
#include "CoroutineTests/alien/sync_wait.hpp"
#include "CoroutineTests/alien/tool.hpp"
#include "CoroutineTests/threadpool.hpp"
#include "alien_stream_await.hpp"  // StreamAwaitable
#include "logging_utils.hpp"       // log, format_name
#include "nanospin.hpp"            // launch_nanospin

#define ERROR_CHECK_CUDA(EXP)                                              \
    do {                                                                   \
        cudaError_t errorCode = EXP;                                       \
        if (errorCode != cudaSuccess) {                                    \
            throw std::runtime_error(                                      \
                std::format("CUDA error at {}:{}: {}", __FILE__, __LINE__, \
                            cudaGetErrorString(errorCode)));               \
        }                                                                  \
    } while (false)

using namespace CoroutineTests::alien;

template <typename T>
struct DeviceBuffer {
    T* ptr = nullptr;
    std::size_t size = 0;
};

template <typename F>
tool::Task<void> delegate(F f) {
    f();
    co_return;
}

subtool::Task<DeviceBuffer<int>> clusterization(
    DeviceBuffer<int> cells, cudaStream_t stream,
    std::function<void(std::coroutine_handle<>)> delegation_scheduler,
    std::string_view parent) {

    const auto self = format_name(parent, "clusterization");
    log(self) << "Starting clusterization" << std::endl;

    const auto nCells = static_cast<int>(cells.size);

    // Copy cells back to host to count non-zero entries
    auto h_cells = std::vector<int>(nCells);

    co_await schedule_on(
        delegation_scheduler, delegate([&]() {
            log(self) << "Delegated copy of cells from device to host"
                      << std::endl;
            ERROR_CHECK_CUDA(cudaMemcpyAsync(h_cells.data(), cells.ptr,
                                             nCells * sizeof(int),
                                             cudaMemcpyDeviceToHost, stream));
        }));

    ERROR_CHECK_CUDA(co_await StreamAwaitable{stream});

    auto nClusters = 0;
    for (auto v : h_cells)
        if (v != 0)
            ++nClusters;

    log(self) << "Found " << nClusters << " clusters" << std::endl;

    // Allocate clusters of appropriate size on device
    int* d_clusters = nullptr;

    co_await schedule_on(
        delegation_scheduler, delegate([&]() {
            log(self) << "Delegated allocation of clusters on device"
                      << std::endl;
            ERROR_CHECK_CUDA(
                cudaMallocAsync(reinterpret_cast<void**>(&d_clusters),
                                nClusters * sizeof(int), stream));

            // Write some dummy data to the clusters buffer to simulate work
            ERROR_CHECK_CUDA(cudaMemsetAsync(d_clusters, 0,
                                             nClusters * sizeof(int), stream));
            ERROR_CHECK_CUDA(cudaMemsetAsync(
                d_clusters, 1, nClusters / 2 * sizeof(int), stream));
            launch_nanospin(1'000'000, stream);
        }));

    co_return DeviceBuffer<int>{d_clusters,
                                static_cast<std::size_t>(nClusters)};
}

subtool::Task<DeviceBuffer<int>> seeding(
    DeviceBuffer<int> clusters, cudaStream_t stream,
    std::function<void(std::coroutine_handle<>)> delegation_scheduler,
    std::string_view parent) {

    const auto self = format_name(parent, "seeding");
    log(self) << "Starting seeding" << std::endl;

    const auto nClusters = static_cast<int>(clusters.size);

    // Copy clusters to host to count non-zero entries
    auto h_clusters = std::vector<int>(nClusters);

    co_await schedule_on(
        delegation_scheduler, delegate([&]() {
            log(self) << "Delegated copy of clusters to host" << std::endl;
            ERROR_CHECK_CUDA(cudaMemcpyAsync(h_clusters.data(), clusters.ptr,
                                             nClusters * sizeof(int),
                                             cudaMemcpyDeviceToHost, stream));
        }));

    ERROR_CHECK_CUDA(co_await StreamAwaitable{stream});

    int nSeeds = 0;
    for (auto v : h_clusters)
        if (v != 0)
            ++nSeeds;

    log(self) << "Found " << nSeeds << " seeds" << std::endl;

    // Allocate seeds of appropriate size on device
    int* d_seeds = nullptr;

    co_await schedule_on(
        delegation_scheduler, delegate([&]() {
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

tool::Task<tool::StatusCode> reconstruct(
    cudaStream_t stream,
    std::function<void(std::coroutine_handle<>)> delegation_scheduler,
    std::string_view parent) {
    const auto self = format_name(parent, "reconstruction");
    log(self) << "Starting reconstruction" << std::endl;

    // Allocate some dummy input data on the device
    auto cells = DeviceBuffer<int>{nullptr, 1000};
    co_await schedule_on(
        delegation_scheduler, delegate([&]() {
            log(self) << "Delegated allocation of input data on device"
                      << std::endl;
            ERROR_CHECK_CUDA(
                cudaMallocAsync(reinterpret_cast<void**>(&cells.ptr),
                                cells.size * sizeof(int), stream));
            ERROR_CHECK_CUDA(cudaMemsetAsync(cells.ptr, 1,
                                             cells.size * sizeof(int), stream));
        }));

    // Run the clusterization and seeding steps
    auto clusters =
        co_await clusterization(cells, stream, delegation_scheduler, self);
    auto seeds = co_await seeding(clusters, stream, delegation_scheduler, self);

    // Cleanup
    co_await schedule_on(
        delegation_scheduler, delegate([&]() {
            log(self) << "Delegated cleanup of device memory" << std::endl;
            ERROR_CHECK_CUDA(cudaFreeAsync(cells.ptr, stream));
            ERROR_CHECK_CUDA(cudaFreeAsync(clusters.ptr, stream));
            ERROR_CHECK_CUDA(cudaFreeAsync(seeds.ptr, stream));
        }));

    ERROR_CHECK_CUDA(co_await StreamAwaitable{stream});

    log(self) << "Finishing reconstruction" << std::endl;
    co_return tool::StatusCode::SUCCESS;
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

    log() << "main Starting" << std::endl;

    tbb::task_arena task_arena{2, 0};
    auto scheduler = [&task_arena](std::coroutine_handle<> handle) {
        task_arena.enqueue([handle]() { handle.resume(); });
    };

    CoroutineTests::Threadpool delegation_threadpool(1);
    auto delegation_scheduler =
        [&delegation_threadpool](std::coroutine_handle<> handle) {
            delegation_threadpool.enqueue_task(handle);
        };

    {
        cudaStream_t stream;
        ERROR_CHECK_CUDA(cudaStreamCreate(&stream));
        std::cout << "--- Single event, synchronous wait for completion ---\n";
        log() << "main Launching algorithm..." << std::endl;
        auto status = sync_wait(
            scheduler, reconstruct(stream, delegation_scheduler, "main"));
        log() << "main Final status of algorithm " << status << "" << std::endl;
        ERROR_CHECK_CUDA(cudaStreamDestroy(stream));
    }

    {
        std::cout << "--- Multiple events, wait for all to complete ---\n";

        auto streams = std::vector<cudaStream_t>(2);
        auto status = std::vector<tool::StatusCode>(streams.size());
        for (auto& stream : streams) {
            ERROR_CHECK_CUDA(cudaStreamCreate(&stream));
        }

        auto scope = counting_scope{};
        log() << "main Launching algorithms..." << std::endl;

        auto payload = [](std::vector<cudaStream_t> streams,
                          std::vector<tool::StatusCode>& statuses,
                          std::function<void(std::coroutine_handle<>)>
                              delegation_scheduler,
                          int i) -> tool::Task<void> {
            const auto name = std::format("event{}:main", i);
            auto& stream = streams.at(i);
            auto& status = statuses.at(i);
            status = co_await reconstruct(
                stream, std::move(delegation_scheduler), name);
            co_return;
        };

        for (std::size_t i = 0; i < streams.size(); ++i) {
            scope.spawn(scheduler,
                        payload(streams, status, delegation_scheduler,
                                static_cast<int>(i)));
        }
        scope.join();

        for (auto& stream : streams) {
            ERROR_CHECK_CUDA(cudaStreamDestroy(stream));
        }

        log() << "main All algorithms completed. Final statuses: ";
        for (auto s : status) {
            std::cout << s << ' ';
        }
        std::cout << std::endl;
    }
    return 0;
}
