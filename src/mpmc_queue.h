#ifndef MPMC_QUEUE_H
#define MPMC_QUEUE_H

#include "defines.h"
#include "std_compatibility.h"
#include <type_traits>
#include <span>
#include <climits>
#include <cstring>
#include <memory>
#include <atomic>

constexpr size_t cache_line_bytes = std::hardware_destructive_interference_size;

template<typename T, size_t SZ>
    requires std::is_trivial_v<T> && std::is_trivially_destructible_v<T> && (cache_line_bytes % sizeof(T) == 0)
class mpmc_queue
{
    static constexpr size_t elems_per_block = cache_line_bytes / sizeof(T);

private:
    struct alignas(cache_line_bytes) cache_line
    {
        char              data[cache_line_bytes - 1];
        std::atomic<char> ready_to_dequeue;
    };
    static_assert(sizeof(cache_line) == cache_line_bytes);

    alignas(cache_line_bytes) cache_line data[sizeof(T) * SZ / cache_line_bytes];
    alignas(cache_line_bytes) std::atomic<size_t> reader{ 0 };
    alignas(cache_line_bytes) std::atomic<size_t> writer{ 0 };

private:
    void enqueue_single(T* src, cache_line& dst)
    {
        std::memcpy(std::assume_aligned<cache_line_bytes>(dst.data), src, cache_line_bytes - 1);
        dst.ready_to_dequeue.fetch_or((char)((1 << 7)), memory_order_release);
    }

public:
    ALWAYS_INLINE constexpr void
    enqueue(T* begin, size_t block_count) noexcept
    {
        const size_t to_write = writer.fetch_add(block_count, memory_order_relaxed);
        for (int i = 0; i < block_count; ++i)
        {
            enqueue_single(begin + i * elems_per_block, data[to_write + i]);
        }
    }
    ALWAYS_INLINE [[nodiscard]] constexpr T* dequeue(size_t block_count) noexcept
    {
        const size_t to_read           = reader.fetch_add(block_count, memory_order_relaxed);
        cache_line&  last_line_to_read = data[to_read + block_count - 1];
        while (!(last_line_to_read.ready_to_dequeue.load(memory_order_acquire) & (1 << 7)))
        {
        }
        for (int i = 0; i < block_count; ++i)
        {
            cache_line& read_line = data[to_read + i];
            read_line.ready_to_dequeue.fetch_and(~(char)((1 << 7)), memory_order_release);
        }
        return reinterpret_cast<T*>(data[to_read].data);
    }
    ALWAYS_INLINE constexpr void reset() noexcept
    {
        for (auto& cache_line : data)
        {
            cache_line.ready_to_dequeue.fetch_or((char)((1 << 7)), memory_order_release);
        }
    }
    ALWAYS_INLINE constexpr void reset2() noexcept
    {
        for (auto& cache_line : data)
        {
            cache_line.ready_to_dequeue.fetch_and(~((char)((1 << 7))), memory_order_release);
        }
        reader.store(0, memory_order_relaxed);
        writer.store(0, memory_order_relaxed);
    }
};

#endif
