#ifndef ROUND_ROBIN_QUEUE_H
#define ROUND_ROBIN_QUEUE_H

#include <atomic>
#include <cstddef>
#include <vector>
#include "mpsc_queue.h"
#include "thread_local_storage.h"

template<typename T, size_t SZ>
class round_robin_queue
{
    size_t                         thread_count;
    std::atomic<size_t>            round_robin;
    std::vector<mpsc_queue<T, SZ>> queues;

public:
    round_robin_queue(size_t thread_count) :
        thread_count{ thread_count },
        round_robin{ 0 },
        queues{ thread_count }
    {
    }
    T* try_dequeue(size_t max_tries)
    {
        return queues[tls.index].try_dequeue(max_tries);
    }
    void enqueue(T& t)
    {
        queues[round_robin.fetch_add(1, std::memory_order_relaxed) % thread_count].enqueue(t);
    }
};

#endif
