#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>

// #include "ban_heap.h"
#include "compat.h"
#include "job.h"
#include "set_affinity.h"
#include "value_or_error.h"

template <int I> static job<int> test_coro() {
  if constexpr (I == 0 || I == 1) {
    co_return 1;
  } else {
    auto [r0, r1] = co_await when_all(test_coro<I - 1>(), test_coro<I - 2>());
    co_return r0 + r1;
  }
}

template <int I> [[gnu::noinline]] int test_normal() {
  if constexpr (I == 0 || I == 1) {
    return 1;
  } else {
    int result = test_normal<I - 1>() + test_normal<I - 2>();
    return result;
  }
}

// vectorize the scheduler loop
//    not posssible: cannot atomically decrement counters in different places in
//    memory

// store the count of notifiables in the linked list node
//    not worth it: small performance improvement on iteration, but requires an
//    additional atomic store for each call to add notifiable
// store begin and end in scheduler as 32 bit values and load them atomically in
// empty

template <typename Func> auto benchmark(Func &&func) {
  auto begin = std::chrono::high_resolution_clock::now();
  func();
  return std::chrono::high_resolution_clock::now() - begin;
}

template <int Iters> void compare() {
  std::cout << "Comparing " << Iters << ": ";
  auto coro_time = benchmark([]() {
    const auto j = test_coro<Iters>();
    scheduler.run();
    int x = j.get_future().get();
    std::cout << x << ' ';
  });
  auto normal_time = benchmark([]() {
    int x = test_normal<Iters>();
    std::cout << x << ' ';
  });
  std::cout << coro_time.count() / 1e6 << "ms " << normal_time.count() / 1e6
            << "ms " << coro_time.count() / (float)normal_time.count() << '\n';
}

int main() {
  set_this_process_priority_high();
  for (int i = 0; i < 10000; ++i) {
    compare<23>();
    std::cout << '\n';
  }
}

#define EVEN_P(d, index, _) BOOST_PP_NOT(BOOST_PP_MOD_D(d, index, 2))
#define EVEN(list) BOOST_PP_LIST_FILTER(EVEN_P, nil, list)

#define NAMES_L(...) EVEN(BOOST_PP_VARIADIC_TO_LIST(__VA_ARGS__))
#define NAMES_T(...) BOOST_PP_LIST_TO_TUPLE(NAMES_L(__VA_ARGS__))

#define ODD_P(d, index, _) BOOST_PP_MOD_D(d, index, 2)
#define ODD(list) BOOST_PP_LIST_FILTER(ODD_P, nil, list)

#define INITIALIZERS_L(...) ODD(BOOST_PP_VARIADIC_TO_LIST(__VA_ARGS__))
#define INITIALIZERS_T(...) BOOST_PP_LIST_TO_TUPLE(INITIALIZERS_L(__VA_ARGS__))

#define FUTURE_DECLARATION(r, _, index, element)                               \
  auto element = BOOST_PP_LIST_AT(INITIALIZERS_L(__VA_ARGS__), index)

#define CO_INIT(...)                                                           \
  auto [NAMES_T(__VA_ARGS__)] = co_await [&]() {                               \
    BOOST_PP_FOR_EACH_I(FUTURE_DECLARATION, _, NAMES_L(__VA_ARGS__));          \
    co_return co_await when_all(NAMES_T(__VA_ARGS__));                         \
  }();
