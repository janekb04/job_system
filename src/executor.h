#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "std_compatibility.h"
#include "mpmc_queue.h"
#include "thread_local_queue_buffer.h"
#include "defines.h"
#include "worker.h"
#include <chrono>
#include <thread>
#include <memory>
#include <barrier>
#include <type_traits>
#include <vector>
#include <functional>

class promise_base;

class executor
{
#ifdef EXECUTOR_GLOBAL_QUEUE_SIZE
    inline static constexpr size_t global_queue_size = EXECUTOR_GLOBAL_QUEUE_SIZE;
#else
    inline static constexpr size_t global_queue_size = 1024 * 1024;
#endif
    using global_queue_t = mpmc_queue<promise_base*, global_queue_size>;

private:
    inline static std::unique_ptr<executor> instance;
    inline static std::atomic<bool>         instance_initialized;

    executor(size_t num_threads) :
        workers{ num_threads },
        queues{ num_threads },
        global_queue{ std::make_unique<global_queue_t>() },
        running{ false },
        should_terminate{ false },
        batch_done{ -1 },
        workers_idle_between_batches{ int(num_threads + 1) },
        workers_waiting_to_start_batch{ int(num_threads + 1) }
    {
        for (int i = 0; i < num_threads; ++i)
        {
            workers[i] = std::thread{
                worker::main_func(),
                i,
                std::ref(running),
                std::ref(should_terminate),
                std::ref(batch_done),
                std::ref(workers_idle_between_batches),
                std::ref(workers_waiting_to_start_batch),
                on_worker_ready_for_begin
            };
        }
    }

    static void on_worker_ready_for_begin()
    {
        instance_initialized.wait(false, memory_order_acquire);
        instance->queues[worker::index()].reset();
    }

public:
    ALWAYS_INLINE static void instantiate(size_t num_workers = std::thread::hardware_concurrency())
    {
        instance.reset(new executor(num_workers));
        instance_initialized.store(true, memory_order_release);
        instance_initialized.notify_all();
    }
    ALWAYS_INLINE static decltype(auto) run(auto&& first_job_coroutine) noexcept
    {
        // Before workers arrive at this barrier, they call
        // on_worker_ready_for_begin and reset their local queue
        instance->workers_idle_between_batches.arrive_and_wait();
        // At this point all workers finished the previous batch
        // They are waiting at the second barier

        // During this time, it is safe to enqueue the first job
        // and do any other initialization
        instance->global_queue->reset2();
        auto main_job = first_job_coroutine();
        instance->batch_done.store(instance->workers.size(), memory_order_relaxed);
        instance->running.store(true, memory_order_release);
        std::atomic_thread_fence(memory_order_seq_cst);

        instance->workers_waiting_to_start_batch.arrive_and_wait(); // Tell the workers to begin work
        instance->begin_time = std::chrono::high_resolution_clock::now();

        instance->batch_done.wait(instance->workers.size(), memory_order_acquire);
        instance->global_queue->reset();
        for (int i = instance->workers.size() - 1; i >= 1; --i)
        {
            instance->batch_done.wait(i, memory_order_acquire);
        }

        return main_job.get();
    }
    ALWAYS_INLINE static void done() noexcept
    {
        instance->running.store(false, memory_order_relaxed);
        instance->time = std::chrono::high_resolution_clock::now() - instance->begin_time;
    }
    ALWAYS_INLINE static void destroy() noexcept
    {
        instance->workers_idle_between_batches.arrive_and_wait();
        instance->running.store(true, memory_order_relaxed);
        instance->batch_done.store(-1, memory_order_relaxed);
        instance->running.notify_all();
        instance->should_terminate.store(true, memory_order_relaxed);
        instance->workers_waiting_to_start_batch.arrive_and_wait();
        for (auto& worker : instance->workers)
        {
            worker.join();
        }
        instance.reset();
        instance_initialized.store(false, memory_order_release);
    }
    ALWAYS_INLINE static auto get_time() noexcept
    {
        return instance->time;
    }

public:
    ALWAYS_INLINE [[nodiscard]] static std::coroutine_handle<> pop() noexcept
    {
        if (!instance->running.load(memory_order_relaxed))
        {
            return worker::main_coroutine();
        }
        auto result = instance->queues[worker::index()].dequeue(*instance->global_queue);
        if (!instance->running.load(memory_order_relaxed))
        {
            return worker::main_coroutine();
        }
        return std::coroutine_handle<promise_base>::from_promise(*result);
    }
    ALWAYS_INLINE static void push(promise_base& p) noexcept
    {
        instance->queues[worker::index()].enqueue(&p);
    }
    ALWAYS_INLINE static void publish() noexcept
    {
        instance->queues[worker::index()].publish(*instance->global_queue);
    }

private:
    std::vector<std::thread>                              workers;
    std::vector<thread_local_queue_buffer<promise_base*>> queues;
    std::unique_ptr<global_queue_t>                       global_queue; // Control byte assumes little endian
    std::atomic<bool>                                     running;
    std::atomic<bool>                                     should_terminate;
    std::atomic<int>                                      batch_done;
    std::barrier<>                                        workers_idle_between_batches;
    std::barrier<>                                        workers_waiting_to_start_batch;
    std::chrono::high_resolution_clock::time_point        begin_time;
    std::chrono::high_resolution_clock::duration          time;
};

namespace detail::worker_coroutine
{
    ALWAYS_INLINE static std::coroutine_handle<> executor_pop() NOEXCEPT
    {
        return executor::pop();
    }
} // namespace detail::worker_coroutine

#endif
