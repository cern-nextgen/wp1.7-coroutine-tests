#include <cuda_runtime_api.h>

#include <cstdint>

/// Launch a GPU spin for the specified duration in ns on the given CUDA stream.
void launch_nanospin(std::uint64_t ns, cudaStream_t stream);
