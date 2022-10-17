#ifndef COMPAT_H
#define COMPAT_H

#if __has_include(<coroutine>)
#include <coroutine>
#elif __has_include(<experimental/coroutine>)
#include <experimental/coroutine>
namespace std {
using namespace experimental;
}
#else
#error "No coroutine header found"
#endif

#if __clang_major__ == 14
#error "Clang 14 is not supported due to HALO (coroutine heap elision) bug"
#endif

#include "assume.h"
#include <type_traits>
#include <utility>

#ifndef __cpp_lib_forward_like
namespace std {
template <class T, class U>
ALWAYS_INLINE [[nodiscard]] constexpr auto &&forward_like(U &&x) noexcept {
  constexpr bool is_adding_const = std::is_const_v<std::remove_reference_t<T>>;
  if constexpr (std::is_lvalue_reference_v<T &&>) {
    if constexpr (is_adding_const) {
      return std::as_const(x);
    } else {
      return static_cast<U &>(x);
    }
  } else {
    if constexpr (is_adding_const) {
      return std::move(std::as_const(x));
    } else {
      return std::move(x);
    }
  }
}
constexpr inline size_t hardware_destructive_interference_size = 64;
} // namespace std
#endif

#endif
