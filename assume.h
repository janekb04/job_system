#ifndef ASSUME_H
#define ASSUME_H

#include <atomic>

#define ASSUME_UNREACHABLE
#define ALWAYS_INLINE [[gnu::always_inline]]

#ifndef ATOMIC_DONT_CARE
constexpr inline auto acquire = std::memory_order_acquire;
constexpr inline auto release = std::memory_order_release;
constexpr inline auto acq_rel = std::memory_order_acq_rel;
#else
constexpr inline auto acquire = std::memory_order_relaxed;
constexpr inline auto release = std::memory_order_relaxed;
constexpr inline auto acq_rel = std::memory_order_relaxed;
#endif

#endif
