#ifndef WORKER_H
#define WORKER_H

#include "defines.h"
#include "std_compatibility.h"
#include "set_affinity.h"
#include "worker_coroutine.h"
#include <barrier>
#include <atomic>

class worker
{
private:
    inline thread_local static int                                                  thread_index;
    inline thread_local static std::coroutine_handle<worker_coroutine_promise_type> main_coro;

public:
    ALWAYS_INLINE [[nodiscard]] static int index() noexcept
    {
        return thread_index;
    }
    ALWAYS_INLINE [[nodiscard]] constexpr static auto main_coroutine() noexcept
    {
        return main_coro;
    }
    ALWAYS_INLINE [[nodiscard]] constexpr static auto main_func() noexcept
    {
        return [](int                                       index,
                  std::reference_wrapper<std::atomic<bool>> running,
                  std::reference_wrapper<std::atomic<bool>> should_terminate,
                  std::reference_wrapper<std::atomic<bool>> batch_done,
                  std::reference_wrapper<std::barrier<>>    ready_barrier,
                  std::reference_wrapper<std::barrier<>>    ready_barrier2)
        {
            thread_index = index;
            set_this_thread_affinity(index);
            while (true)
            {
                ready_barrier.get().arrive_and_wait();
                running.get().wait(false, memory_order_relaxed);
                ready_barrier2.get().arrive_and_wait();
                std::atomic_thread_fence(memory_order_seq_cst);
                if (should_terminate.get().load(memory_order_relaxed) == true)
                {
                    return;
                }
                []() -> worker_coroutine_return_object
                {
                    co_return;
                }();
                batch_done.get().store(true, memory_order_relaxed);
                batch_done.get().notify_all();
            }
        };
    }

private:
    friend constexpr void detail::worker_coroutine::set_worker_main_coroutine(std::coroutine_handle<worker_coroutine_promise_type> h) NOEXCEPT;
};

namespace detail::worker_coroutine
{
    ALWAYS_INLINE constexpr void set_worker_main_coroutine(std::coroutine_handle<worker_coroutine_promise_type> h) NOEXCEPT
    {
        worker::main_coro = h;
    }
    ALWAYS_INLINE static std::coroutine_handle<> executor_pop() NOEXCEPT;
} // namespace detail::worker_coroutine

#endif
