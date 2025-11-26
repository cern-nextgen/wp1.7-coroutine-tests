
// System include(s).
#include <cassert>
#include <coroutine>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <memory>
#include <string_view>
#include <type_traits>

std::ostream &log(std::string_view self) {
    return std::cout << self << "  ";
}

std::ostream &log() {
    return std::cout;
    ;
}

namespace Gaudi {
/// Very simple StatusCode substitute
class StatusCode {
    public:
    /// StatusCode values
    enum Status { SUCCESS = 0, FAILURE = 1, UNDEFINED = 2 };
    /// Constructor
    StatusCode(Status status = UNDEFINED) : m_status(status) {}

    /// Get the status of the statuscode
    Status status() const { return m_status; }

    /// Friend function to output the status code
    friend std::ostream &operator<<(std::ostream &os,
                                    const StatusCode &status) {
        os << "Gaudi::StatusCode::";
        switch (status.status()) {
            case StatusCode::SUCCESS:
                os << "SUCCESS";
                break;
            case StatusCode::FAILURE:
                os << "FAILURE";
                break;
            case StatusCode::UNDEFINED:
                os << "UNDEFINED";
                break;
        }
        return os;
    }

    private:
    Status m_status;
};

namespace details {
/// Helper class to destroy a coroutine handle
///
/// @tparam T Type of the coroutine promise
///
template <typename T>
struct CoroutinePromiseDestroyerT {
    /// Type of the pointer to delete / destroy
    using pointer_type = std::add_pointer_t<std::coroutine_handle<T>>;

    /// Operator destroying the coroutine (handle)
    void operator()(pointer_type handle_ptr) const {
        // The pointer must always be valid here.
        assert(handle_ptr != nullptr);
        // If the pointed-to object is a valid handle, destroy the coroutine
        // that it points to.
        if (*handle_ptr) {
            handle_ptr->destroy();
        }
        // Delete the handle itself.
        delete handle_ptr;
    }

};  // struct CoroutinePromiseDestroyerT

}  // namespace details

/// Return type for coroutines
///
/// @tparam T The return / yield type of the coroutine
///
template <typename T>
class [[nodiscard("Coroutine results must be used")]] CoroutineT {

    public:
    /// Promise object for the coroutine
    struct promise;

    /// @name Types required by coroutines
    /// @{

    /// Type returned by the coroutine
    using value_type = T;
    /// Type of the promise object
    using promise_type = promise;

    /// @}

    /// @name Types used just by this class
    /// @{

    /// The type of the internal handle for the promise
    using handle_type = std::coroutine_handle<promise_type>;

    /// @}

    /// Default constructor
    CoroutineT() = default;
    /// Constructor from a coroutine handle
    explicit CoroutineT(handle_type handle)
        : m_handle{new handle_type{handle}} {}

    /// @name Functions required by coroutines
    /// @{

    /// On a @c co_await we always suspend the coroutine
    ///
    /// @return @c false
    ///
    constexpr bool await_ready() const noexcept {
        assert(m_handle);
        return m_handle->done();
    }

    /// When resuming, we resume the coroutine that the object represents
    constexpr value_type await_resume() const noexcept {
        assert(m_handle);
        return m_handle->promise().m_value;
    }

    /// Remember the handle of the parent coroutine
    constexpr void await_suspend(handle_type handle) {
        // Connect the two coroutines
        assert(m_handle);
        assert(!m_handle->promise().m_child_handle);
        handle.promise().m_child_handle = *m_handle;
        // Propagate our return value to the parent coroutine.
        handle.promise().m_value = m_handle->promise().m_value;
    }

    /// @}

    /// Get the returned value out of the coroutine
    ///
    /// @return The value returned by the coroutine
    ///
    auto value() const {
        assert(m_handle);
        return m_handle->promise().m_value;
    }

    /// Check if the coroutine is done
    ///
    /// @return @c true if the coroutine is done, @c false otherwise
    ///
    bool done() const {
        assert(m_handle);
        return m_handle->done();
    }

    /// Resume the coroutine
    ///
    /// This function will throw if the coroutine has an unhandled exception
    /// and the coroutine is not done.
    ///
    /// @return @c true if the coroutine was resumed, @c false otherwise
    ///
    void resume() {
        /// Lambda resuming a specific coroutine handle
        auto resume_coroutine = [](handle_type handle) {
            handle.resume();
            if (handle.promise().m_exception) {
                std::rethrow_exception(handle.promise().m_exception);
            }
        };
        /// Lambda finding the "innermost" child's (and its parent's) coroutine
        auto find_coroutines =
            [](handle_type handle) -> std::pair<handle_type, handle_type> {
            handle_type parent_handle = handle;
            handle_type child_handle = handle.promise().m_child_handle;
            while (child_handle && child_handle.promise().m_child_handle) {
                parent_handle = child_handle;
                child_handle = child_handle.promise().m_child_handle;
            }
            return std::make_pair(parent_handle, child_handle);
        };

        assert(m_handle);
        // Find the coroutine handle(s).
        auto handles = find_coroutines(*m_handle);

        // Handle the child coroutines, if there are any.
        while (handles.second) {
            resume_coroutine(handles.second);
            if (handles.second.done()) {
                // If the innermost couroutine is done, go on to run its parent
                // right away.
                handles.first.promise().m_child_handle = nullptr;
                handles = find_coroutines(*m_handle);
            } else {
                // If the innermost coroutine is not done yet, just grab its
                // yielded value, and return.
                m_handle->promise().m_value = handles.second.promise().m_value;
                return;
            }
        }

        // If we reached this point, then there is no "in-flight" child
        // coroutine anymore. So we need to resume the main/parent coroutine.
        assert(handles.first == *m_handle);
        resume_coroutine(*m_handle);
    }

    private:
    /// Handle to the coroutine
    std::unique_ptr<handle_type,
                    details::CoroutinePromiseDestroyerT<promise_type>>
        m_handle;
};

/// Promise type used by @c Gaudi::CoroutineT
template <typename T>
struct CoroutineT<T>::promise {
    /// Possible exception thrown in the coroutine
    std::exception_ptr m_exception;
    /// The yielded/returned value
    value_type m_value{};
    /// Handle to a possible child coroutine
    handle_type m_child_handle;

    /// @name Functions required by coroutines
    /// @{

    /// Construct a @c CoroutineT object from this promise
    CoroutineT get_return_object() {
        return CoroutineT{CoroutineT::handle_type::from_promise(*this)};
    }

    /// Start running the coroutine right away
    std::suspend_never initial_suspend() const { return {}; }
    /// When the coroutine is finished, suspend it
    std::suspend_always final_suspend() const noexcept { return {}; }

    /// Propagate unhandled exceptions to the caller
    void unhandled_exception() { m_exception = std::current_exception(); }

    /// Yield a value back to the caller
    template <std::convertible_to<T> U>
    std::suspend_always yield_value(U &&value) {
        m_value = std::forward<U>(value);
        return {};
    }
    /// Return a value to the caller
    template <std::convertible_to<T> U>
    void return_value(U &&value) {
        m_value = std::forward<U>(value);
    }

    /// @}

};  // struct promise

}  // namespace Gaudi

Gaudi::CoroutineT<Gaudi::StatusCode> tool1(std::string_view parent) {
    const std::string self = std::format("   {}.tool1", parent);
    log(self) << "Yielding from tool1" << std::endl;
    co_yield Gaudi::StatusCode::SUCCESS;
    log(self) << "Yielding from tool1" << std::endl;
    co_yield Gaudi::StatusCode::FAILURE;
    log(self) << "Finishing tool1" << std::endl;
    co_return Gaudi::StatusCode::FAILURE;
}

Gaudi::CoroutineT<Gaudi::StatusCode> tool2(std::string_view parent) {
    const std::string self = std::format("   {}.tool2", parent);
    log(self) << "Yielding from tool2" << std::endl;
    co_yield Gaudi::StatusCode::SUCCESS;

    log(self) << "Launching tool1" << std::endl;
    Gaudi::StatusCode code = co_await tool1(self);
    log(self) << "Result from tool1: " << code << std::endl;

    log(self) << "Yielding from tool2" << std::endl;
    co_yield Gaudi::StatusCode::FAILURE;
    log(self) << "Finishing tool2" << std::endl;
    co_return Gaudi::StatusCode::SUCCESS;
}

Gaudi::CoroutineT<Gaudi::StatusCode> tool3(std::string_view parent) {
    const std::string self = std::format("   {}.tool3", parent);
    log(self) << "Finishing tool3" << std::endl;
    co_return Gaudi::StatusCode::FAILURE;
}

Gaudi::CoroutineT<Gaudi::StatusCode> algorithm(std::string_view parent) {
    const std::string self = std::format("   {}.algorithm", parent);
    log(self) << "Yielding from algorithm" << std::endl;
    co_yield Gaudi::StatusCode::SUCCESS;

    log(self) << "Launching tool1" << std::endl;
    Gaudi::StatusCode code1 = co_await tool1(self);
    log(self) << "Result from tool1: " << code1 << std::endl;

    log(self) << "Yielding from algorithm" << std::endl;
    co_yield Gaudi::StatusCode::SUCCESS;

    log(self) << "Launching tool2" << std::endl;
    Gaudi::StatusCode code2 = co_await tool2(self);
    log(self) << "Result from tool2: " << code2 << std::endl;

    log(self) << "Launching tool3" << std::endl;
    Gaudi::StatusCode code3 = co_await tool3(self);
    log(self) << "Result from tool3: " << code3 << std::endl;

    log(self) << "Finishing algorithm" << std::endl;
    co_return Gaudi::StatusCode::SUCCESS;
}

int main() {
    log() << "Starting main" << std::endl;
    auto alg = algorithm("main");
    while (!alg.done()) {
        log() << "Value in main: " << alg.value() << std::endl;
        alg.resume();
    }
    log() << "Final value in main: " << alg.value() << std::endl;
    return EXIT_SUCCESS;
}
