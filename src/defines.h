#ifndef DEFINES_H
#define DEFINES_H

#include <atomic>

#define ALWAYS_INLINE      //[[gnu::always_inline]]
#define ASSUME_UNREACHABLE __builtin_unreachable()
#define ASSERT(...)                          \
    do                                       \
    {                                        \
        if (!static_cast<bool>(__VA_ARGS__)) \
        {                                    \
            std::terminate();                \
        }                                    \
    } while (false)

#ifdef DEBUG_ATOMIC_ALL_SEQ_CST
constexpr auto memory_order_relaxed = std::memory_order_seq_cst;
constexpr auto memory_order_acquire = std::memory_order_seq_cst;
constexpr auto memory_order_release = std::memory_order_seq_cst;
constexpr auto memory_order_acq_rel = std::memory_order_seq_cst;
constexpr auto memory_order_seq_cst = std::memory_order_seq_cst;
#else
constexpr auto memory_order_relaxed = std::memory_order_relaxed;
constexpr auto memory_order_acquire = std::memory_order_acquire;
constexpr auto memory_order_release = std::memory_order_release;
constexpr auto memory_order_acq_rel = std::memory_order_acq_rel;
constexpr auto memory_order_seq_cst = std::memory_order_seq_cst;
#endif

#ifdef DEBUG_LIKELY_UNLIKELY_NOOP
#    define LIKELY
#    define UNLIKELY
#else
#    define LIKELY   [[likely]]
#    define UNLIKELY [[unlikely]]
#endif

#endif
