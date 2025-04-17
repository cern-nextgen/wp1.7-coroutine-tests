#include "silentpaircounter.hpp"
#include "Timer.hpp"
#include <cstdint>
#include <iostream>

#pragma GCC push_options
#pragma GCC optimize("O0")

auto N = 1000ull * 1000 * 1000;

CoroutineTests::SilentPairCounter spc() {
  // TODO: peer could be hidden in the promise.
  auto peer = co_await CoroutineTests::PeerAwaiter();
  std::uint64_t i = 0;
  while (++i < N / 2) {
    co_await peer.promise();
    i = i;
  }
}

int main() {
  auto a = spc();
  auto b = spc();
  a.setPeer(b);
  b.setPeer(a);
  Timer t;
  a.resume();
  auto dt = t.secs();
  auto ns = dt / N * 1000ull * 1000 * 1000;
  std::cout << "a/b: " << (a.done() ? "done" : "not done") << "/"
            << (b.done() ? "done" : "not done") << " time per iteration: " << ns
            << " ns" << std::endl;
  return 0;
}

#pragma GCC pop_options