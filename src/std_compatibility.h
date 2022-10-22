#ifndef STD_COMPATIBILITY_H
#define STD_COMPATIBILITY_H

#if __has_include(<coroutine>)
#    include <coroutine>
#elif __has_include(<experimental/coroutine>)
#    include <experimental/coroutine>
namespace std
{
    using namespace experimental;
}
#else
#    error "No coroutine header found"
#endif

#include "defines.h"
#include <type_traits>
#include <utility>

namespace std
{
#ifndef __cpp_lib_forward_like
    // sample implementation from cppreference.com
    template<class T, class U>
    ALWAYS_INLINE [[nodiscard]] constexpr auto&& forward_like(U&& x) noexcept
    {
        constexpr bool is_adding_const = std::is_const_v<std::remove_reference_t<T>>;
        if constexpr (std::is_lvalue_reference_v<T&&>)
        {
            if constexpr (is_adding_const)
            {
                return std::as_const(x);
            }
            else
            {
                return static_cast<U&>(x);
            }
        }
        else
        {
            if constexpr (is_adding_const)
            {
                return std::move(std::as_const(x));
            }
            else
            {
                return std::move(x);
            }
        }
    }
#endif
#ifndef __cpp_lib_hardware_interference_size
#    if defined(__arm__) && __APPLE__ // M-series ARM CPU
    constexpr inline size_t hardware_constructive_interference_size = 128;
    constexpr inline size_t hardware_destructive_interference_size  = 128;
#    else // other ARM CPUs and x86
    constexpr inline size_t hardware_constructive_interference_size = 64;
    constexpr inline size_t hardware_destructive_interference_size  = 64;
#    endif
#endif
} // namespace std

#endif
