#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "std_compatibility.h"
#include "mpmc_queue.h"
#include "thread_local_queue_buffer.h"
#include "defines.h"
#include "worker.h"
#include <thread>
#include <memory>
#include <barrier>
#include <vector>
#include <functional>

class promise_base;

class executor
{
private:
    inline static std::unique_ptr<executor> instance;
    executor(size_t num_threads) :
        workers{ num_threads },
        queues{ num_threads },
        global_queue{},
        running{ false },
        should_terminate{ false },
        batch_done{ false }
    {
        for (int i = 0; i < num_threads; ++i)
        {
            workers[i] = std::thread{ worker::main_func(), i, std::ref(running), std::ref(should_terminate), std::ref(batch_done) };
        }
    }
    ALWAYS_INLINE static void reset() noexcept
    {
        instance->global_queue.reset();
        for (auto& queue : instance->queues)
        {
            queue.reset();
        }
    }

public:
    ALWAYS_INLINE static void instantiate(size_t num_workers = std::thread::hardware_concurrency())
    {
        instance.reset(new executor(num_workers));
    }
    ALWAYS_INLINE static void run() noexcept
    {
        instance->running.store(true, memory_order_relaxed);
        instance->running.notify_all();
        instance->batch_done.store(false, memory_order_relaxed);
        instance->batch_done.wait(false);
        reset();
    }
    ALWAYS_INLINE static void done() noexcept
    {
        instance->running.store(false, memory_order_relaxed);
    }
    ALWAYS_INLINE static void destroy() noexcept
    {
        instance->running.store(true, memory_order_relaxed);
        instance->running.notify_all();
        instance->should_terminate.store(true, memory_order_relaxed);
        for (auto& worker : instance->workers)
        {
            worker.join();
        }
        instance.reset();
    }

public:
    ALWAYS_INLINE [[nodiscard]] static std::coroutine_handle<> pop() noexcept
    {
        if (!instance->running.load(memory_order_relaxed))
        {
            return worker::main_coroutine();
        }
        auto result = std::coroutine_handle<promise_base>::from_promise(*instance->queues[worker::index()].dequeue(instance->global_queue));
        if (!instance->running.load(memory_order_relaxed))
        {
            return worker::main_coroutine();
        }
        return result;
    }
    ALWAYS_INLINE static void push(promise_base& p) noexcept
    {
        instance->queues[worker::index()].enqueue(&p);
    }
    ALWAYS_INLINE static void publish() noexcept
    {
        instance->queues[worker::index()].publish(instance->global_queue);
    }

private:
    std::vector<std::thread>                                               workers;
    std::vector<thread_local_queue_buffer<promise_base*>>                  queues;
    mpmc_queue<promise_base*, 1 << 20, cache_line_bytes - 1, CHAR_BIT - 1> global_queue; // Control byte assumes little endian
    std::atomic<bool>                                                      running;
    std::atomic<bool>                                                      should_terminate;
    std::atomic<bool>                                                      batch_done;
};

namespace detail::worker_coroutine
{
    ALWAYS_INLINE static std::coroutine_handle<> executor_pop() noexcept
    {
        return executor::pop();
    }
} // namespace detail::worker_coroutine

#endif
