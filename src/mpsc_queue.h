#ifndef TASK_QUEUE_H
#define TASK_QUEUE_H

#include <array>
#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <cassert>
#include "assume.h"

template<typename T, size_t SZ>
class mpsc_queue
{
public:
    std::array<std::atomic<T*>, SZ> _data{}; // TODO: investigate if using volatile would be faster
    struct alignas(uint64_t) iters_t
    {
        // Need to do be in this order because of endianness
        uint32_t _end{ 0 };
        uint32_t _begin{ 0 };
    };
    std::atomic<iters_t> _iters;
    static_assert(std::atomic<iters_t>::is_always_lock_free);

public:
    T* try_dequeue(size_t max_tries)
    {
        for (int num_tries = 0; num_tries < max_tries; ++num_tries)
        {
            auto iters = _iters.load(std::memory_order_relaxed);
            if (iters._begin == iters._end)
            {
                continue;
            };
            T* ret = _data[iters._begin % SZ].load(std::memory_order_relaxed);
            if (!ret)
            {
                continue;
            }
            _data[iters._begin % SZ].store(nullptr, std::memory_order_relaxed);
            reinterpret_cast<std::atomic<uint64_t>*>(&_iters)->fetch_add(1ull << 32ull, std::memory_order_relaxed);
            return ret;
        }
        return nullptr;
    }
    void enqueue(T& t)
    {
        size_t iters      = reinterpret_cast<std::atomic<uint64_t>*>(&_iters)->fetch_add(1, std::memory_order_relaxed);
        auto [end, begin] = *reinterpret_cast<iters_t*>(&iters);
        _data[end % SZ].store(&t, std::memory_order_relaxed);
    }
};

#endif
