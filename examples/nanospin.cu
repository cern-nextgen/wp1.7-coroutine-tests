#include "nanospin.hpp"

static __global__ void nanospin(std::uint64_t ns) {
    long long start = clock64();
    auto cps = 1'400'000'000LL;  // Assuming 1.4 GHz clock rate
    auto delta = (long long)((ns * cps) / 1'000'000'000ULL);
    auto end = start + delta;
    while (clock64() < end) {
    }
}

void launch_nanospin(std::uint64_t ns, cudaStream_t stream) {
    nanospin<<<1, 32, 0, stream>>>(ns);
}
