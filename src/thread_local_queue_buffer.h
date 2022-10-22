#ifndef THREAD_LOCAL_QUEUE_BUFFER_H
#define THREAD_LOCAL_QUEUE_BUFFER_H

#include <cstdint>
#include <limits>
#include "mpmc_queue.h"

template<typename T>
class thread_local_queue_buffer
{
    static constexpr size_t size            = std::numeric_limits<uint8_t>::max() + 1;
    static constexpr size_t elems_per_block = cache_line_bytes / sizeof(T);

public:
    ALWAYS_INLINE constexpr void enqueue(T elem) noexcept
    {
        data[end] = elem;
        ++end;
    }
    template<typename MPMCQueue>
    ALWAYS_INLINE constexpr T dequeue(MPMCQueue& fallback) noexcept
    {
        if (empty())
        {
            T* block = fallback.dequeue(1);
            std::memcpy(data + end, block, cache_line_bytes);
            end += elems_per_block;
        }
        --end;
        return data[end];
    }
    template<typename MPMCQueue>
    ALWAYS_INLINE constexpr void publish(MPMCQueue& target) noexcept
    {
        const uint8_t count        = (end - begin);
        const uint8_t count_blocks = count / elems_per_block;
        if (count_blocks == 0)
        {
            return;
        }
        const uint8_t new_begin = begin + count - count % elems_per_block;
        target.enqueue(data + begin, count_blocks);
        begin = new_begin;
    }
    ALWAYS_INLINE [[nodiscard]] constexpr bool empty() const noexcept
    {
        return begin == end;
    }

private:
    uint8_t begin;
    uint8_t end;
    alignas(cache_line_bytes) T data[size];
};

#endif
