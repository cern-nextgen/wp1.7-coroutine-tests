
// System include(s).
#include <cassert>
#include <coroutine>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <memory>
#include <type_traits>
#include <string_view>
#include <format>

namespace Gaudi
{
   namespace details
   {
      /// Helper class to destroy a coroutine handle
      ///
      /// @tparam T Type of the coroutine promise
      ///
      template <typename T>
      struct CoroutinePromiseDestroyerT
      {
         /// Type of the pointer to delete / destroy
         using pointer_type = std::add_pointer_t<std::coroutine_handle<T>>;

         /// Operator destroying the coroutine (handle)
         void operator()(pointer_type handle_ptr) const
         {
            // The pointer must always be valid here.
            assert(handle_ptr != nullptr);
            // If the pointed-to object is a valid handle, destroy the coroutine that it points to.
            if (*handle_ptr)
            {
               handle_ptr->destroy();
            }
            // Delete the handle itself.
            delete handle_ptr;
         }

      }; // struct CoroutinePromiseDestroyerT

   } // namespace details

   /// Return type for coroutines
   ///
   /// @tparam T The return / yield type of the coroutine
   ///
   template <typename T>
   class [[nodiscard("Coroutine results must be used")]] CoroutineT
   {

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
      explicit CoroutineT(handle_type handle) : m_handle{new handle_type{handle}} {}

      /// @name Functions required by coroutines
      /// @{

      /// On a @c co_await we always suspend the coroutine
      ///
      /// @return @c false
      ///
      constexpr bool await_ready() const noexcept { return false; }

      /// When resuming, we resume the coroutine that the object represents
      constexpr void await_resume() const noexcept {}

      /// Remember the handle of the parent coroutine
      constexpr void await_suspend(handle_type handle)
      {
         // Connect the two coroutines
         assert(m_handle);
         handle.promise().m_child_handle = *m_handle;
         // Propagate our return value to the parent coroutine.
         handle.promise().yield_value(m_handle->promise().m_value);
      }

      /// @}

      /// Get the returned value out of the coroutine
      ///
      /// @return The value returned by the coroutine
      ///
      auto value() const
      {
         assert(m_handle);
         return m_handle->promise().m_value;
      }

      /// Check if the coroutine is done
      ///
      /// @return @c true if the coroutine is done, @c false otherwise
      ///
      bool done() const
      {
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
      void resume()
      {
         /// Lambda resuming a specific coroutine handle
         auto resume_coroutine = [](handle_type handle)
         {
            handle.resume();
            if (handle.promise().m_exception)
            {
               std::rethrow_exception(handle.promise().m_exception);
            }
         };
         /// Lambda finding the "innermost" child's (and its parent's) coroutine
         auto find_coroutines = [](handle_type handle) -> std::pair<handle_type, handle_type>
         {
            handle_type parent_handle = handle;
            handle_type child_handle = handle.promise().m_child_handle;
            while (child_handle && child_handle.promise().m_child_handle)
            {
               parent_handle = child_handle;
               child_handle = child_handle.promise().m_child_handle;
            }
            return std::make_pair(parent_handle, child_handle);
         };

         assert(m_handle);
         // Find the coroutine handle(s).
         auto handles = find_coroutines(*m_handle);
         // Check if there's a child coroutine.
         if (handles.second)
         {
            // If the child coroutine is not done, resume it
            if (!handles.second.done())
            {
               resume_coroutine(handles.second);
               m_handle->promise().m_value = handles.second.promise().m_value;
            }
            // If the child coroutine is done, we need to resume its parent.
            else
            {
               handles.first.promise().m_child_handle = nullptr;
               resume_coroutine(handles.first);
               m_handle->promise().m_value = handles.first.promise().m_value;
            }
         }
         else
         {
            // If there is no child coroutine, just resume the current one
            resume_coroutine(*m_handle);
         }
      }

   private:
      /// Handle to the coroutine
      std::unique_ptr<handle_type, details::CoroutinePromiseDestroyerT<promise_type>> m_handle;
   };

   /// Promise type used by @c Gaudi::CoroutineT
   template <typename T>
   struct CoroutineT<T>::promise
   {
      /// Possible exception thrown in the coroutine
      std::exception_ptr m_exception;
      /// The yielded/returned value
      value_type m_value{};
      /// Handle to a possible child coroutine
      handle_type m_child_handle;

      /// @name Functions required by coroutines
      /// @{

      /// Construct a @c CoroutineT object from this promise
      CoroutineT get_return_object() { return CoroutineT{CoroutineT::handle_type::from_promise(*this)}; }

      /// Start running the coroutine right away
      std::suspend_never initial_suspend() const { return {}; }
      /// When the coroutine is finished, suspend it
      std::suspend_always final_suspend() const noexcept { return {}; }

      /// Propagate unhandled exceptions to the caller
      void unhandled_exception() { m_exception = std::current_exception(); }

      /// Yield a value back to the caller
      template <std::convertible_to<T> U>
      std::suspend_always yield_value(U &&value)
      {
         m_value = std::forward<U>(value);
         return {};
      }
      /// Return a value to the caller
      template <std::convertible_to<T> U>
      void return_value(U &&value)
      {
         m_value = std::forward<U>(value);
      }

      /// @}

   }; // struct promise

} // namespace Gaudi

Gaudi::CoroutineT<int> tool1(std::string_view parent)
{
   std::cout << std::format("Starting {}.tool1", parent) << std::endl;
   co_yield 1;
   std::cout << std::format("Continuing {}.tool1", parent) << std::endl;
   co_yield 2;
   std::cout << std::format("Finishing {}.tool1", parent) << std::endl;
   co_return 3;
}

Gaudi::CoroutineT<int> tool2(std::string_view parent)
{
   std::cout << std::format("Starting {}.tool2", parent) << std::endl;
   co_yield 11;

   std::cout << std::format("Launching tool1 from {}.tool2", parent) << std::endl;
   co_await tool1(std::format("{}.tool2", parent));

   std::cout << std::format("Continuing {}.tool2", parent) << std::endl;
   co_yield 12;
   std::cout << std::format("Finishing {}.tool2", parent) << std::endl;
   co_return 13;
}

Gaudi::CoroutineT<int> algorithm()
{
   std::cout << "Starting algorithm" << std::endl;
   co_yield 42;

   std::cout << "Launching tool1" << std::endl;
   co_await tool1("algorithm");

   std::cout << "Continuing algorithm" << std::endl;
   co_yield 84;

   std::cout << "Launching tool2" << std::endl;
   co_await tool2("algorithm");

   std::cout << "Finishing algorithm" << std::endl;
   co_return 126;
}

int main()
{
   std::cout << "Starting main" << std::endl;
   auto alg = algorithm();
   while (!alg.done())
   {
      std::cout << "Main: " << alg.value() << std::endl;
      alg.resume();
   }
   std::cout << "Main: " << alg.value() << std::endl;
   std::cout << "Main finished" << std::endl;
   return EXIT_SUCCESS;
}
