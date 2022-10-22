#ifndef MPMC_QUEUE_H
#define MPMC_QUEUE_H

#include "std_compatibility.h"
#include <type_traits>
#include <span>
#include <climits>
#include <cstring>
#include <memory>

constexpr size_t cache_line_bytes = std::hardware_destructive_interference_size;

template<typename T, size_t SZ, size_t ControlByte, size_t ControlBit>
    requires std::is_trivial_v<T> && std::is_trivially_destructible_v<T> && (cache_line_bytes % sizeof(T) == 0)
class mpmc_queue
{
    static constexpr size_t elems_per_block = cache_line_bytes / sizeof(T);

private:
    alignas(cache_line_bytes) char data[sizeof(T) * SZ];
    alignas(cache_line_bytes) std::atomic<size_t> reader{ 0 };
    alignas(cache_line_bytes) std::atomic<size_t> writer{ 0 };

public:
    ALWAYS_INLINE constexpr void enqueue(T* begin, size_t block_count) noexcept
    {
        for (int i = 0; i < block_count; ++i)
        {
            char* const block_begin = reinterpret_cast<char*>(begin) + cache_line_bytes * i;
            block_begin[ControlByte] |= (1ull << ControlBit);
        }
        const size_t to_write = writer.fetch_add(block_count, memory_order_relaxed);
        char* const  src      = std::assume_aligned<cache_line_bytes>(reinterpret_cast<char*>(begin));
        char* const  dest     = std::assume_aligned<cache_line_bytes>(data + cache_line_bytes * to_write);
        std::memcpy(dest, src, block_count * cache_line_bytes);
    }
    ALWAYS_INLINE [[nodiscard]] constexpr T* dequeue(size_t block_count) noexcept
    {
        const size_t         to_read      = reader.fetch_add(block_count, memory_order_relaxed);
        char* const          begin        = std::assume_aligned<cache_line_bytes>(data + cache_line_bytes * to_read);
        char* const          last_block   = std::assume_aligned<cache_line_bytes>(begin + cache_line_bytes * (block_count - 1));
        volatile char* const control_byte = last_block + ControlByte;
        while (!(*control_byte & (1ull << ControlBit)))
        {
        }
        for (int i = 0; i < block_count; ++i)
        {
            char* const block_begin = begin + cache_line_bytes * i;
            block_begin[ControlByte] ^= (1ull << ControlBit);
        }
        return std::assume_aligned<cache_line_bytes>(reinterpret_cast<T*>(begin));
    }
    ALWAYS_INLINE constexpr void reset() noexcept
    {
        for (char* block_begin = data; block_begin < data + sizeof(data); block_begin += cache_line_bytes)
        {
            block_begin[ControlByte] |= (1ull << ControlBit);
        }
        reader.store(0, memory_order_relaxed);
        writer.store(0, memory_order_relaxed);
    }
    ALWAYS_INLINE constexpr void reset2() noexcept
    {
        for (char* block_begin = data; block_begin < data + sizeof(data); block_begin += cache_line_bytes)
        {
            block_begin[ControlByte] = 0;
        }
    }
};

#endif
