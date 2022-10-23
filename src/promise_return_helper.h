#ifndef PROMISE_RETURN_HELPER_H
#define PROMISE_RETURN_HELPER_H

#include "std_compatibility.h"

template<typename Promise, typename T>
struct promise_return_helper
{
    template<typename U>
    ALWAYS_INLINE constexpr void return_value(U&& val) noexcept(noexcept(
        static_cast<Promise*>(this)->return_value_storage.set_value(std::move(val))))
    {
        static_cast<Promise*>(this)->return_value_storage.set_value(std::move(val));
    }
};
template<typename Promise>
struct promise_return_helper<Promise, void>
{
    ALWAYS_INLINE constexpr void return_void() const NOEXCEPT
    {
    }
};

#endif
