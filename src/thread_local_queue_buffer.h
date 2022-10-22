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

            char*  src     = reinterpret_cast<char*>(block);
            char*  dst     = reinterpret_cast<char*>(data + end);
            char*  dst_end = std::min(dst + cache_line_bytes, reinterpret_cast<char*>(data + size));
            size_t sz1     = dst_end - dst;
            std::memcpy(dst, src, sz1);

            src += sz1;
            dst        = reinterpret_cast<char*>(data);
            dst_end    = reinterpret_cast<char*>(data) + (cache_line_bytes - sz1);
            size_t sz2 = dst_end - dst;
            if (sz2 > 0)
            {
                std::memcpy(dst, src, sz2);
            }

            end += elems_per_block;
        }
        --end;
        return data[end];
    }
    template<typename MPMCQueue>
    ALWAYS_INLINE constexpr void publish(MPMCQueue& target) noexcept
    {
        char* src = reinterpret_cast<char*>(data + begin);
        char* src_end;
        if (end < begin)
        {
            src_end = reinterpret_cast<char*>(data + size);
        }
        else
        {
            src_end = reinterpret_cast<char*>(data + end);
        }

        size_t sz = src_end - src;
        sz -= sz % cache_line_bytes;

        size_t block_count = sz / cache_line_bytes;

        if (block_count > 0)
        {
            target.enqueue(reinterpret_cast<T*>(src), block_count);
            begin += block_count * elems_per_block;
        }

        if (end < begin)
        {
            return;
        }

        src     = reinterpret_cast<char*>(data + begin);
        src_end = reinterpret_cast<char*>(data + end);
        sz      = src_end - src;
        sz -= sz % cache_line_bytes;
        block_count = sz / cache_line_bytes;

        if (block_count > 0)
        {
            target.enqueue(reinterpret_cast<T*>(src), block_count);
            begin += block_count * elems_per_block;
        }
    }
    ALWAYS_INLINE [[nodiscard]] constexpr bool empty() const noexcept
    {
        return begin == end;
    }
    ALWAYS_INLINE constexpr void reset() noexcept
    {
        begin = 0;
        end   = 0;
    }

private:
    uint8_t begin;
    uint8_t end;
    alignas(cache_line_bytes) T data[size];
};

#endif
