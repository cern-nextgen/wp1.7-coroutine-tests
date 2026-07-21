#include <cuda_runtime_api.h>
#include <tbb/task_arena.h>

#include <cstddef>
#include <exec/async_scope.hpp>
#include <exec/repeat_until.hpp>
#include <exec/single_thread_context.hpp>
#include <exec/task.hpp>
#include <iostream>
#include <stdexec/execution.hpp>
#include <string_view>

#include "CoroutineTests/event_context.hpp"  // EventContext
#include "exec_backend.hpp"                  // std exec backend selection
#include "exec_stream_await_sender.hpp"      // stream_await_sender
#include "exec_task_arena_scheduler.hpp"     // TaskArenaScheduler
#include "logging_utils.hpp"                 // log, format_name
#include "nanospin.hpp"                      // launch_nanospin
#include "nvtx_auditor.hpp"                  // NvtxAuditor
#include "nvtx_utils.hpp"                    // make_range
#include "statuscode.hpp"                    // StatusCodeImpl

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

// Poll a CUDA event for completion by repeatedly querying it.
// Each query attempt is scheduled onto the delegation scheduler.
static auto poll_event(cudaEvent_t event,
                       exec::single_thread_context& delegation_ctx,
                       std::string_view parent) {
    log(parent) << "Polling for event completion..." << std::endl;
    auto query_once =
        execution::just() | execution::then([event, parent]() -> bool {
            auto status = cudaEventQuery(event);
            if (status == cudaErrorNotReady) {
                log(parent) << "Event not ready, retrying..." << std::endl;
                return false;
            }
            ERROR_CHECK_CUDA(status);
            log(parent) << "Event completed successfully" << std::endl;
            return true;
        });

    return exec::repeat_until(execution::starts_on(
        delegation_ctx.get_scheduler(), std::move(query_once)));
}

exec::task<DeviceBuffer<int>> clusterization(
    DeviceBuffer<int> cells, cudaStream_t stream, cudaEvent_t event,
    exec::single_thread_context& delegation_ctx, std::string_view parent) {

    const auto self = format_name(parent, "clusterization");
    log(self) << "Starting clusterization" << std::endl;

    const auto nCells = static_cast<int>(cells.size);

    // Copy cells back to host to count non-zero entries
    auto h_cells = std::vector<int>(nCells);

    stdexec::sender auto copy_cells =
        stdexec::just() | stdexec::then([&]() {
            log(self) << "Delegated copy of cells from device to host"
                      << std::endl;
            ERROR_CHECK_CUDA(cudaMemcpyAsync(h_cells.data(), cells.ptr,
                                             nCells * sizeof(int),
                                             cudaMemcpyDeviceToHost, stream));
            ERROR_CHECK_CUDA(cudaEventRecord(event, stream));
        });
    co_await stdexec::on(delegation_ctx.get_scheduler(), std::move(copy_cells));

    co_await poll_event(event, delegation_ctx, self);

    auto nClusters = 0;
    for (auto v : h_cells)
        if (v != 0)
            ++nClusters;

    log(self) << "Found " << nClusters << " clusters" << std::endl;

    // Allocate clusters of appropriate size on device
    int* d_clusters = nullptr;

    stdexec::sender auto allocate_clusters =
        stdexec::just() | stdexec::then([&]() {
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
        });
    co_await stdexec::on(delegation_ctx.get_scheduler(),
                         std::move(allocate_clusters));

    co_return DeviceBuffer<int>{d_clusters,
                                static_cast<std::size_t>(nClusters)};
}

exec::task<DeviceBuffer<int>> seeding(
    DeviceBuffer<int> clusters, cudaStream_t stream, cudaEvent_t event,
    exec::single_thread_context& delegation_ctx, std::string_view parent) {

    const auto self = format_name(parent, "seeding");
    log(self) << "Starting seeding" << std::endl;

    const auto nClusters = static_cast<int>(clusters.size);

    // Copy clusters to host to count non-zero entries
    auto h_clusters = std::vector<int>(nClusters);

    stdexec::sender auto copy_clusters =
        stdexec::just() | stdexec::then([&]() {
            log(self) << "Delegated copy of clusters from device to host"
                      << std::endl;
            ERROR_CHECK_CUDA(cudaMemcpyAsync(h_clusters.data(), clusters.ptr,
                                             nClusters * sizeof(int),
                                             cudaMemcpyDeviceToHost, stream));
            ERROR_CHECK_CUDA(cudaEventRecord(event, stream));
        });
    co_await stdexec::on(delegation_ctx.get_scheduler(),
                         std::move(copy_clusters));

    co_await poll_event(event, delegation_ctx, self);

    int nSeeds = 0;
    for (auto v : h_clusters)
        if (v != 0)
            ++nSeeds;

    log(self) << "Found " << nSeeds << " seeds" << std::endl;

    // Allocate seeds of appropriate size on device
    int* d_seeds = nullptr;

    stdexec::sender auto allocate_seeds =
        stdexec::just() | stdexec::then([&]() {
            log(self) << "Delegated allocation of seeds on device" << std::endl;
            ERROR_CHECK_CUDA(cudaMallocAsync(reinterpret_cast<void**>(&d_seeds),
                                             nSeeds * sizeof(int), stream));

            // Write some dummy data to the clusters buffer to simulate work
            ERROR_CHECK_CUDA(
                cudaMemsetAsync(d_seeds, 0, nSeeds * sizeof(int), stream));
            ERROR_CHECK_CUDA(
                cudaMemsetAsync(d_seeds, 1, nSeeds / 2 * sizeof(int), stream));
            launch_nanospin(1'000'000, stream);
        });
    co_await stdexec::on(delegation_ctx.get_scheduler(),
                         std::move(allocate_seeds));

    co_return DeviceBuffer<int>{d_seeds, static_cast<std::size_t>(nSeeds)};
}

exec::task<tools::StatusCode> reconstruct(
    cudaStream_t stream, cudaEvent_t event,
    exec::single_thread_context& delegation_ctx, std::string_view parent) {
    const auto self = format_name(parent, "reconstruction");
    log(self) << "Starting reconstruction" << std::endl;

    // Allocate some dummy input data on the device
    auto cells = DeviceBuffer<int>{nullptr, 1000};
    stdexec::sender auto allocate_cells =
        stdexec::just() | stdexec::then([&]() {
            log(self) << "Delegated allocation of input data on device"
                      << std::endl;
            ERROR_CHECK_CUDA(
                cudaMallocAsync(reinterpret_cast<void**>(&cells.ptr),
                                cells.size * sizeof(int), stream));
            ERROR_CHECK_CUDA(cudaMemsetAsync(cells.ptr, 1,
                                             cells.size * sizeof(int), stream));
        });
    co_await stdexec::on(delegation_ctx.get_scheduler(),
                         std::move(allocate_cells));

    // Run the clusterization and seeding steps
    auto clusters =
        co_await clusterization(cells, stream, event, delegation_ctx, self);
    auto seeds =
        co_await seeding(clusters, stream, event, delegation_ctx, self);

    // Cleanup
    stdexec::sender auto cleanup =
        stdexec::just() | stdexec::then([&]() {
            log(self) << "Delegated cleanup of device memory" << std::endl;
            ERROR_CHECK_CUDA(cudaFreeAsync(cells.ptr, stream));
            ERROR_CHECK_CUDA(cudaFreeAsync(clusters.ptr, stream));
            ERROR_CHECK_CUDA(cudaFreeAsync(seeds.ptr, stream));
            ERROR_CHECK_CUDA(cudaEventRecord(event, stream));
        });
    co_await stdexec::on(delegation_ctx.get_scheduler(), std::move(cleanup));

    co_await poll_event(event, delegation_ctx, self);

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
    TaskArenaContext context{task_arena, false};
    context.add_auditor(std::make_shared<NvtxAuditor>());
    exec::single_thread_context delegation_context;

    {
        cudaStream_t stream;
        ERROR_CHECK_CUDA(cudaStreamCreate(&stream));
        cudaEvent_t event;
        ERROR_CHECK_CUDA(
            cudaEventCreateWithFlags(&event, cudaEventDisableTiming));
        std::cout << "--- Single event, synchronous wait for completion ---\n";
        auto processing_range =
            CoroutineTests::nvtx_utils::make_range("Single event processing");
        log("main") << "Launching algorithm..." << std::endl;
        auto [status] =
            stdexec::sync_wait(
                stdexec::starts_on(
                    TaskArenaScheduler{context, "alg",
                                       CoroutineTests::EventContext{0, 0}},
                    reconstruct(stream, event, delegation_context, "main")))
                .value();
        log("main") << "Final status of algorithm " << status << ""
                    << std::endl;
        ERROR_CHECK_CUDA(cudaEventDestroy(event));
        ERROR_CHECK_CUDA(cudaStreamDestroy(stream));
    }

    {
        std::cout << "--- Multiple events, wait for all to complete ---\n";
        auto processing_range =
            CoroutineTests::nvtx_utils::make_range("Multiple event processing");

        auto streams = std::vector<cudaStream_t>(2);
        auto events = std::vector<cudaEvent_t>(streams.size());
        auto status = std::vector<tools::StatusCode>(streams.size());
        for (std::size_t i = 0; i < streams.size(); ++i) {
            ERROR_CHECK_CUDA(cudaStreamCreate(&streams.at(i)));
            ERROR_CHECK_CUDA(cudaEventCreateWithFlags(&events.at(i),
                                                      cudaEventDisableTiming));
        }

        auto scope = exec::async_scope{};
        log("main") << "Launching algorithms..." << std::endl;

        auto payload = [](std::vector<cudaStream_t> streams,
                          std::vector<cudaEvent_t> events,
                          std::vector<tools::StatusCode>& statuses, int i,
                          exec::single_thread_context& delegation_ctx)
            -> exec::task<void> {
            const auto name = std::format("event{}:", i);
            auto& stream = streams.at(i);
            auto& event = events.at(i);
            auto& status = statuses.at(i);
            status = co_await reconstruct(stream, event, delegation_ctx, name);
        };
        for (std::size_t i = 0; i < streams.size(); ++i) {
            scope.spawn(stdexec::starts_on(
                TaskArenaScheduler{context, "alg",
                                   CoroutineTests::EventContext{i, 0}},
                payload(streams, events, status, i, delegation_context)));
        }
        stdexec::sync_wait(scope.on_empty());

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
