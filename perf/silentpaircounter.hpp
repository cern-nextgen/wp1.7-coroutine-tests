#ifndef COROUTINEPERF_SILENTCOUNTER_H
#define COROUTINEPERF_SILENTCOUNTER_H

#include <coroutine>
#include <exception>
#include <stdexcept>

namespace CoroutineTests {

// Test for lateral yielding of coroutines.

// Coroutine intended to work in a pair with a peer.
// Each coroutine yields to the peer for a fixed number of iterations.
// This will test the implementation of lateral movement.
class [[nodiscard]] SilentPairCounter {
public:
  struct promise_type; // typedef required by coroutines
  using handle_type =
      std::coroutine_handle<promise_type>; // not required but useful

  SilentPairCounter(handle_type coroutine_handle)
      : m_coroutine(coroutine_handle) {} // required by coroutines
  ~SilentPairCounter() {
    if (m_coroutine) {
      m_coroutine.destroy();
    }
  }
  SilentPairCounter() = default;
  SilentPairCounter(const SilentPairCounter &) = delete;
  SilentPairCounter &operator=(const SilentPairCounter &) = delete;
  SilentPairCounter(SilentPairCounter &&other) noexcept
      : m_coroutine{other.m_coroutine} {
    other.m_coroutine = {};
  }
  SilentPairCounter &operator=(SilentPairCounter &&other) noexcept {
    if (this != &other) {
      if (m_coroutine) {
        m_coroutine.destroy();
      }
      m_coroutine = other.m_coroutine;
      other.m_coroutine = {};
    }
    return *this;
  }
  // resume coroutine and get the value
  void setPeer(SilentPairCounter &peer);
  inline void resume() const;
  inline bool done() const { return m_coroutine.done(); }

private:
  handle_type m_coroutine;
};

struct SilentPairCounter::promise_type {
  // storage for exceptions thrown in the coroutine
  std::exception_ptr m_exception;
  // Handle of the peer
  SilentPairCounter::handle_type m_peer;
  // required by coroutines
  SilentPairCounter get_return_object() {
    return {SilentPairCounter::handle_type::from_promise(*this)};
  }
  // called on coroutine start
  std::suspend_always initial_suspend() const { return {}; }
  // called on coroutine completion
  std::suspend_always final_suspend() const noexcept { return {}; }
  // acts as a catch block for exceptions thrown in the coroutine
  void unhandled_exception() { m_exception = std::current_exception(); }
  // called on (implicit or explicit) co_return or co_return void
  void return_void() const {}

  // awaitable interface
  // don't skip suspensions
  bool await_ready() const noexcept { return false; }
  // when awaited, store the parent coroutine handle and resume this coroutine
  inline auto await_suspend(handle_type handle) noexcept;
  // nothing special on resume, doesn't produce a value
  void await_resume() const noexcept {}
};

inline void SilentPairCounter::resume() const {
  if (!m_coroutine.done()) {
    m_coroutine.resume();
  }
  if (m_coroutine.promise().m_exception) {
    std::rethrow_exception(m_coroutine.promise().m_exception);
  }
}

void SilentPairCounter::setPeer(SilentPairCounter &peer) {
  auto peerHandle = peer.m_coroutine;
  if (!m_coroutine || m_coroutine.done())
    throw std::runtime_error("Coroutine anavailble");
  auto &ourPeer = m_coroutine.promise().m_peer;
  if (ourPeer)
    throw std::runtime_error("Peer already set");
  if (!peerHandle)
    throw std::runtime_error("EMpty peer handle");
  ourPeer = peerHandle;
  m_coroutine.resume();
  auto &e = m_coroutine.promise().m_exception;
  if (e)
    std::rethrow_exception(e);
}

auto SilentPairCounter::promise_type::await_suspend(
    handle_type handle) noexcept {
  return m_peer;
}

// Simple awaiter that allows to receive data from the coroutine.
// co_await OutputAwaiter{some_value};
// This could be potentially replaced by just co_yield
struct PeerAwaiter {
  using handle_type =
      std::coroutine_handle<typename SilentPairCounter::promise_type>;
  // storage for the handle of a coroutine that suspended on co_await
  handle_type m_coroutine;
  // don't resume immediately
  bool await_ready() const { return false; }
  // copy handle to the coroutine that suspended on co_await
  void await_suspend(handle_type h) { m_coroutine = h; }
  // resume the coroutine and return the value taken from promise
  auto await_resume() const { return m_coroutine.promise().m_peer; }
};

} // namespace CoroutineTests
#endif // COROUTINEPERF_SILENTCOUNTER_H
