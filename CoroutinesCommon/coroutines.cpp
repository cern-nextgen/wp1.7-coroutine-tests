#include <coroutine>
#include <cstdlib>
#include <exception>
#include <iostream>


// coroutine interface
template <typename PT>
class [[nodiscard]] CoTask {
public:
   // Promise type defines how to create or get the return value of the
   // coroutine, decides whether coroutines should suspend at the beginning
   // or at the end, deals with values exchanged between caller and the coroutine.
   // Created automatically when coroutine is called.
   using promise_type = PT;

   // Provides interface to resume a coroutine and in general
   // manages the state of the coroutine. Created when coroutine is called.
   using handle_type = std::coroutine_handle<promise_type>;

   explicit CoTask(const std::coroutine_handle<promise_type>& handle) : handle_{handle} {
   }

   ~CoTask() {
      if(handle_) {
         handle_.destroy();
      }
   }

   // consider supporting move semantics
   CoTask(const CoTask&) = delete;
   CoTask& operator=(const CoTask&) = delete;

   bool resume() const {
      if(!handle_ || handle_.done()) {
         return false;
      }
      handle_.resume();
      return !handle_.done();
   }

   auto get_value() const {
      return handle_.promise().x_;
   }

   auto get_result() const {
      return handle_.promise().y_;
   }

private:
   handle_type handle_;
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////
struct PromiseBase {
   // Determines whether routine starts eagerly or lazily.
   auto initial_suspend() {
      return std::suspend_always{};
   }

   // Should be suspended at the end and guarantee not to throw.
   auto final_suspend() noexcept {
      return std::suspend_always{};
   }

   // Deal with exceptions not handled locally inside coroutine.
   [[noreturn]] void unhandled_exception() {
      std::terminate();
   };
};


//////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T = void, typename U = void>
struct Promise : public PromiseBase {
   T x_;
   U y_;

   // Creates coroutine object returned to the caller of the coroutine.
   auto get_return_object() {
      return CoTask<Promise>{std::coroutine_handle<Promise>::from_promise(*this)};
   }

   auto yield_value(const T& x) {
      x_ = x;
      return std::suspend_always{};
   }

   void return_value(const U& y) {
      y_ = y;
   }
};


template <>
struct Promise<void, void> : public PromiseBase {
   auto get_return_object() {
      return CoTask<Promise>{std::coroutine_handle<Promise>::from_promise(*this)};
   }

   void return_void() {
   }
};


template <typename T>
struct Promise<T, void> : public PromiseBase {
   T x_;

   auto get_return_object() {
      return CoTask<Promise>{std::coroutine_handle<Promise>::from_promise(*this)};
   }

   auto yield_value(T x) {
      x_ = x;
      return std::suspend_always{};
   }

   void return_void() {
   }
};


template <typename U>
struct Promise<void, U> : public PromiseBase {
   U y_;

   auto get_return_object() {
      return CoTask<Promise>{std::coroutine_handle<Promise>::from_promise(*this)};
   }

   void return_value(U y) {
      y_ = y;
   }
};


using EmptyCoYield = void;
using EmptyCoReturn = void;


//////////////////////////////////////////////////////////////////////////////////////////////////////////
// examples
CoTask<Promise<EmptyCoYield, EmptyCoReturn>> co_await_int() {
   unsigned x{};

   std::cout << x++ << '\n';
   co_await std::suspend_always{};

   std::cout << x << '\n';
   co_await std::suspend_always{};
}


CoTask<Promise<unsigned, EmptyCoReturn>> co_yield_int() {
   unsigned x{};
   co_yield x++;
   co_yield x;
}


// yield unsigned, return int
CoTask<Promise<unsigned, int>> co_return_int() {
   unsigned x{};
   co_yield x++;
   co_return static_cast<int>(x);
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////
int main() {
   {
      auto cpf = co_await_int();
      while(cpf.resume()) {
      }
   }
   std::cout << '\n';


   {
      auto cpf = co_yield_int();
      while(cpf.resume()) {
         std::cout << cpf.get_value() << '\n';
      }
   }
   std::cout << '\n';


   {
      auto cpf = co_return_int();
      while(cpf.resume()) {
         std::cout << cpf.get_value() << '\n';
      }
      std::cout << cpf.get_result() << '\n';
   }


   return EXIT_SUCCESS;
}
