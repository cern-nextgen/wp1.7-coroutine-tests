// AS
// usage of C++23 std::generator, a coroutine interface that
// supports input_range iterators. Compiles on GCC 14.
// Not supported on GCC 13, clang 18.


#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <generator>
#include <iostream>
#include <ranges>
#include <vector>


// const reference does not compile for generator
void print_range(auto&& R) {
   for(auto&& x : R) {
      std::cout << x << '\n';
   }
   std::cout << '\n';
}


template <typename T>
std::generator<T> sequence(T n) {
   T x{};
   while(x < n) {
      co_yield x++;
   }
}


static_assert(std::ranges::input_range<std::generator<int>>);
static_assert(not std::ranges::forward_range<std::generator<int>>);


int main() {
   print_range(sequence(10));

   std::vector x = sequence(10) | std::ranges::to<std::vector>();
   print_range(x);

   assert(std::ranges::includes(sequence(10), std::vector{1, 3, 5}));

   return EXIT_SUCCESS;
}
