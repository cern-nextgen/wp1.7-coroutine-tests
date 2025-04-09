#ifndef COROUTINETESTS_PINGPONG_H
#define COROUTINETESTS_PINGPONG_H

#include <coroutine>
#include <exception>

namespace CoroutineTests {

class [[nodiscard]] Player {
    public:
    struct promise_type;  // typedef required by coroutines
    using handle_type =
        std::coroutine_handle<promise_type>;  // not required but useful

    Player(handle_type coroutine_handle)
        : m_coroutine(coroutine_handle) {}  // required by coroutines
    ~Player() {
        if (m_coroutine) {
            m_coroutine.destroy();
        }
    }
    Player() = default;
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;
    Player(Player&& other) noexcept : m_coroutine{other.m_coroutine} {
        other.m_coroutine = {};
    }
    Player& operator=(Player&& other) noexcept {
        if (this != &other) {
            if (m_coroutine) {
                m_coroutine.destroy();
            }
            m_coroutine = other.m_coroutine;
            other.m_coroutine = {};
        }
        return *this;
    }
    void start() {
        if (m_coroutine && !m_coroutine.done()) {
            m_coroutine.resume();
        }
    }
    // awaitable interface
    // don't skip suspensions
    bool await_ready() const noexcept { return false; }

    inline handle_type await_suspend(handle_type handle) noexcept;
    // nothing special on resume, doesn't produce a value
    void await_resume() const noexcept {}

    private:
    handle_type m_coroutine;
};

struct Player::promise_type {
    // storage for exceptions thrown in the coroutine
    std::exception_ptr m_exception;
    // non-owning handle to the peer coroutine
    handle_type m_peer;
    // required by coroutines
    Player get_return_object() {
        return {Player::handle_type::from_promise(*this)};
    }
    // called on coroutine start
    std::suspend_always initial_suspend() const { return {}; }
    // called on coroutine completion
    std::suspend_always final_suspend() const noexcept {
        return {};
    }
    // acts as a catch block for exceptions thrown in the coroutine
    void unhandled_exception() { m_exception = std::current_exception(); }
    // called on (implicit or explicit) co_return or co_return void
    void return_void() const {}
};

inline Player::handle_type Player::await_suspend(handle_type handle) noexcept {
    m_coroutine.promise().m_peer = handle;
    handle.promise().m_peer= m_coroutine;
    return m_coroutine;    
}

struct Play{
    bool await_ready() const noexcept { return false; }
    Player::handle_type await_suspend(Player::handle_type handle) noexcept {
        return handle.promise().m_peer;
    }
    void await_resume() const noexcept {}
};

}  // namespace CoroutineTests
#endif  // COROUTINETESTS_PINGPONG_H
