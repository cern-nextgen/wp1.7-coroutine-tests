#include <cuda_runtime_api.h>
#include <tbb/task_arena.h>

#include "exec_backend.hpp"               // std exec backend selection
#include "exec_stream_await_sender.hpp"   // stream_await_sender
#include "exec_task_arena_scheduler.hpp"  // TaskArenaScheduler
#include "logging_utils.hpp"              // log, format_name
#include "nanospin.hpp"                   // launch_nanospin
#include "statuscode.hpp"                 // StatusCodeImpl

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

execution::task<DeviceBuffer<int>> clusterization(DeviceBuffer<int> cells,
                                                  cudaStream_t stream,
                                                  std::string_view parent) {

    const auto self = format_name(parent, "clusterization");
    log(self) << "Starting clusterization" << std::endl;

    const auto nCells = static_cast<int>(cells.size);

    // Copy cells back to host to count non-zero entries
    auto h_cells = std::vector<int>(nCells);

    ERROR_CHECK_CUDA(cudaMemcpyAsync(h_cells.data(), cells.ptr,
                                     nCells * sizeof(int),
                                     cudaMemcpyDeviceToHost, stream));

    ERROR_CHECK_CUDA(co_await stream_await_sender{stream});

    auto nClusters = 0;
    for (auto v : h_cells)
        if (v != 0)
            ++nClusters;

    log(self) << "Found " << nClusters << " clusters" << std::endl;

    // Allocate clusters of appropriate size on device
    int* d_clusters = nullptr;
    ERROR_CHECK_CUDA(cudaMallocAsync(reinterpret_cast<void**>(&d_clusters),
                                     nClusters * sizeof(int), stream));

    // Write some dummy data to the clusters buffer to simulate work
    ERROR_CHECK_CUDA(
        cudaMemsetAsync(d_clusters, 0, nClusters * sizeof(int), stream));
    ERROR_CHECK_CUDA(
        cudaMemsetAsync(d_clusters, 1, nClusters / 2 * sizeof(int), stream));
    launch_nanospin(1'000'000, stream);

    co_return DeviceBuffer<int>{d_clusters,
                                static_cast<std::size_t>(nClusters)};
}

execution::task<DeviceBuffer<int>> seeding(DeviceBuffer<int> clusters,
                                           cudaStream_t stream,
                                           std::string_view parent) {

    const auto self = format_name(parent, "seeding");
    log(self) << "Starting seeding" << std::endl;

    const auto nClusters = static_cast<int>(clusters.size);

    // Copy clusters to host to count non-zero entries
    auto h_clusters = std::vector<int>(nClusters);

    ERROR_CHECK_CUDA(cudaMemcpyAsync(h_clusters.data(), clusters.ptr,
                                     nClusters * sizeof(int),
                                     cudaMemcpyDeviceToHost, stream));

    ERROR_CHECK_CUDA(co_await stream_await_sender{stream});

    int nSeeds = 0;
    for (auto v : h_clusters)
        if (v != 0)
            ++nSeeds;

    log(self) << "Found " << nSeeds << " seeds" << std::endl;

    // Allocate seeds of appropriate size on device
    int* d_seeds = nullptr;
    ERROR_CHECK_CUDA(cudaMallocAsync(reinterpret_cast<void**>(&d_seeds),
                                     nSeeds * sizeof(int), stream));

    // Write some dummy data to the seeds buffer to simulate work
    ERROR_CHECK_CUDA(cudaMemsetAsync(d_seeds, 0, nSeeds * sizeof(int), stream));
    ERROR_CHECK_CUDA(
        cudaMemsetAsync(d_seeds, 1, nSeeds / 2 * sizeof(int), stream));
    launch_nanospin(1'000'000, stream);

    co_return DeviceBuffer<int>{d_seeds, static_cast<std::size_t>(nSeeds)};
}

execution::task<tools::StatusCode> reconstruct(cudaStream_t stream,
                                               std::string_view parent) {
    const auto self = format_name(parent, "reconstruction");
    log(self) << "Starting reconstruction" << std::endl;

    // Allocate some dummy input data on the device
    auto cells = DeviceBuffer<int>{nullptr, 1000};
    ERROR_CHECK_CUDA(cudaMallocAsync(reinterpret_cast<void**>(&cells.ptr),
                                     cells.size * sizeof(int), stream));
    ERROR_CHECK_CUDA(
        cudaMemsetAsync(cells.ptr, 1, cells.size * sizeof(int), stream));

    // Run the clusterization and seeding steps
    auto clusters = co_await clusterization(cells, stream, self);
    auto seeds = co_await seeding(clusters, stream, self);

    // Cleanup
    ERROR_CHECK_CUDA(cudaFreeAsync(cells.ptr, stream));
    ERROR_CHECK_CUDA(cudaFreeAsync(clusters.ptr, stream));
    ERROR_CHECK_CUDA(cudaFreeAsync(seeds.ptr, stream));

    ERROR_CHECK_CUDA(co_await stream_await_sender{stream});

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

    tbb::task_arena task_arena{2, 0};
    execution::scheduler auto scheduler = get_scheduler(task_arena, false);

    {
        cudaStream_t stream;
        ERROR_CHECK_CUDA(cudaStreamCreate(&stream));
        std::cout << "--- Single event, synchronous wait for completion ---\n";
        log("main") << "Launching algorithm..." << std::endl;
        auto [status] =
            execution::sync_wait(
                execution::starts_on(scheduler, reconstruct(stream, "main")))
                .value();
        log("main") << "Final status of algorithm " << status << ""
                    << std::endl;
        ERROR_CHECK_CUDA(cudaStreamDestroy(stream));
    }

    {
        std::cout << "--- Multiple events, wait for all to complete ---\n";

        auto streams = std::vector<cudaStream_t>(2);
        auto status = std::vector<tools::StatusCode>(streams.size());
        for (auto& stream : streams) {
            ERROR_CHECK_CUDA(cudaStreamCreate(&stream));
        }

        auto scope = Scope{};
        log("main") << "Launching algorithms..." << std::endl;

        auto payload = [](std::vector<cudaStream_t> streams,
                          std::vector<tools::StatusCode>& statuses,
                          int i) -> execution::task<void> {
            const auto name = std::format("event{}:", i);
            auto& stream = streams.at(i);
            auto& status = statuses.at(i);
            status = co_await reconstruct(stream, name);
        };
        for (std::size_t i = 0; i < streams.size(); ++i) {
            scope.spawn(scheduler, payload(streams, status, i));
        }
        execution::sync_wait(scope.join());

        for (auto& stream : streams) {
            ERROR_CHECK_CUDA(cudaStreamDestroy(stream));
        }

        log("main") << "All algorithms completed. Final statuses: ";
        for (auto s : status) {
            std::cout << s << ' ';
        }
        std::cout << std::endl;
    }
    return 0;
}
