#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "compat.h"
#include <exception>
#include <thread>
#include <vector>
#include <atomic>
#include <algorithm>
#include <ranges>
#include <barrier>
#include "set_affinity.h"
#include "round_robin_queue.h"

class task;

class executor
{
    alignas(std::hardware_destructive_interference_size) std::vector<std::thread> workers;
    alignas(std::hardware_destructive_interference_size) size_t worker_count;
    alignas(std::hardware_destructive_interference_size) std::atomic_flag run_flag;
    alignas(std::hardware_destructive_interference_size) std::atomic_flag terminate_flag;
    alignas(std::hardware_destructive_interference_size) std::atomic<bool> sleep_allowed{ false };
    alignas(std::hardware_destructive_interference_size) round_robin_queue<task, 1 << 24> queue;
    alignas(std::hardware_destructive_interference_size) std::vector<std::coroutine_handle<>> worker_main_coroutine_handles;
    alignas(std::hardware_destructive_interference_size) std::barrier<> work_done;
    static constexpr size_t MAX_TRIES = 1024;

public:
    inline static std::unique_ptr<executor> _instance;

private:
    struct worker_main_coroutine_return_object
    {
        struct promise_type
        {
            ALWAYS_INLINE constexpr auto initial_suspend() const noexcept
            {
                struct awaiter
                {
                    ALWAYS_INLINE constexpr bool await_ready() const noexcept
                    {
                        return false;
                    }
                    ALWAYS_INLINE constexpr void await_resume() const noexcept
                    {
                    }
                    ALWAYS_INLINE std::coroutine_handle<> await_suspend(std::coroutine_handle<> h)
                    {
                        executor::instance().set_worker_main_coroutine_handle(h);
                        return executor::instance().get_next_task();
                    }
                };
                return awaiter{};
            }
            ALWAYS_INLINE constexpr std::suspend_always final_suspend() const noexcept
            {
                return {};
            }
            ALWAYS_INLINE worker_main_coroutine_return_object get_return_object() noexcept
            {
                return { *this };
            }
            ALWAYS_INLINE constexpr void unhandled_exception() const noexcept
            {
            }
            ALWAYS_INLINE constexpr void return_void() const noexcept
            {
            }
        };
        worker_main_coroutine_return_object(promise_type& p) :
            h{ std::coroutine_handle<promise_type>::from_promise(p) }
        {
        }
        ~worker_main_coroutine_return_object()
        {
            if (h)
            {
                h.destroy();
            }
        }
        std::coroutine_handle<promise_type> h;
    };
    static worker_main_coroutine_return_object worker_main_coroutine() noexcept
    {
        co_return;
    }
    static void worker_func(int thread_index) noexcept
    {
        set_this_thread_affinity(thread_index);
        while (true)
        {
            executor::instance().run_flag.wait(false, std::memory_order_acquire);
            if (executor::instance().terminate_flag.test(std::memory_order_relaxed))
            {
                break;
            }
            worker_main_coroutine();
            executor::instance().work_done.arrive_and_wait();
        }
    }
    executor(size_t worker_count) :
        worker_count{ worker_count },
        queue{ worker_count },
        worker_main_coroutine_handles{ worker_count },
        terminate_flag{ false },
        work_done{ static_cast<ptrdiff_t>(worker_count + 1) }
    {
        tls.reset_thread_count();
        workers.reserve(worker_count);
        for (int i = 0; i < worker_count; ++i)
        {
            auto& thread = workers.emplace_back(worker_func, i);
        }
    }

public:
    static void instantiate(size_t worker_count = std::thread::hardware_concurrency())
    {
        // if (_instance)
        // {
        //     std::terminate(); // DEBUG: can instantiate only once
        // }
        _instance.reset(new executor{ worker_count });
    }
    static executor& instance() noexcept
    {
        return *_instance;
    }

public:
    ~executor()
    {
        terminate_flag.test_and_set(std::memory_order_relaxed);
        start_workers();
        for (auto& thread : workers)
        {
            thread.join();
        }
    }
    void start_workers()
    {
        run_flag.test_and_set(std::memory_order_release);
        run_flag.notify_all();
    }
    void signal_finish()
    {
        run_flag.clear(std::memory_order_relaxed);
    }
    void signal_can_sleep(bool can_sleep)
    {
        bool could_sleep_before = sleep_allowed.exchange(can_sleep, std::memory_order_relaxed);
        if (could_sleep_before && !can_sleep)
        {
            sleep_allowed.notify_all();
        }
    }
    void wait_until_work_done()
    {
        work_done.arrive_and_wait();
    }
    std::coroutine_handle<> get_next_task()
    {
        while (true)
        {
            task* p_task = reinterpret_cast<task*>(queue.try_dequeue(MAX_TRIES));
            if (p_task)
            {
                return std::coroutine_handle<task>::from_promise(*p_task);
            }
            if (!run_flag.test(std::memory_order_relaxed))
            {
                return get_worker_main_coroutine_handle();
            }
            if (sleep_allowed.load(std::memory_order_relaxed))
            {
                sleep_allowed.wait(true, std::memory_order_relaxed);
            }
        }
    }
    void set_worker_main_coroutine_handle(std::coroutine_handle<> h)
    {
        worker_main_coroutine_handles[tls.index] = h;
    }
    std::coroutine_handle<> get_worker_main_coroutine_handle()
    {
        return worker_main_coroutine_handles[tls.index];
    }
    ALWAYS_INLINE void add_ready_to_launch(task& t)
    {
        queue.enqueue(t);
    }
    ALWAYS_INLINE void add_ready_to_resume(task& t)
    {
        add_ready_to_launch(t);
    }
};

#endif
