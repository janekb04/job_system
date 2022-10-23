#ifndef MISC_UTILITIES_H
#define MISC_UTILITIES_H

#include "defines.h"
#include <atomic>

template<typename T>
T& deatomize(std::atomic<T>& a) NOEXCEPT
{
    return *reinterpret_cast<T*>(&a);
}
template<typename T>
const T& deatomize(const std::atomic<T>& a) NOEXCEPT
{
    return *reinterpret_cast<const T*>(&a);
}

template<size_t Dummy, typename T>
decltype(auto) unpack(T&& t) NOEXCEPT
{
    return std::forward<T>(t);
}

#endif
