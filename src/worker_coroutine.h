#ifndef WORKER_COROUTINE_H
#define WORKER_COROUTINE_H

#include "std_compatibility.h"

class worker_coroutine_promise_type;

namespace detail::worker_coroutine
{
    ALWAYS_INLINE constexpr void                 set_worker_main_coroutine(std::coroutine_handle<worker_coroutine_promise_type>) NOEXCEPT;
    ALWAYS_INLINE static std::coroutine_handle<> executor_pop() NOEXCEPT;
} // namespace detail::worker_coroutine

class worker_coroutine_return_object
{
public:
    using promise_type = worker_coroutine_promise_type;
    ALWAYS_INLINE constexpr ~worker_coroutine_return_object() NOEXCEPT
    {
        h.destroy();
    }

private:
    ALWAYS_INLINE constexpr worker_coroutine_return_object(promise_type& p) NOEXCEPT :
        h{ std::coroutine_handle<promise_type>::from_promise(p) }
    {
    }

private:
    std::coroutine_handle<promise_type> h;
    friend promise_type;
};

struct worker_coroutine_initial_awaiter
{
    ALWAYS_INLINE constexpr bool await_ready() const NOEXCEPT
    {
        return false;
    }
    ALWAYS_INLINE constexpr void await_resume() const NOEXCEPT
    {
    }
    ALWAYS_INLINE std::coroutine_handle<> await_suspend(std::coroutine_handle<worker_coroutine_promise_type> h) const NOEXCEPT
    {
        using namespace detail::worker_coroutine;

        // For some reason, this significantly decreases the segfault rate
        thread_local static bool first = true;
        if (first)
        {
            set_worker_main_coroutine(h);
            first = false;
        }

        return executor_pop();
    }
};

struct worker_coroutine_promise_type
{
    ALWAYS_INLINE constexpr worker_coroutine_initial_awaiter initial_suspend() const NOEXCEPT
    {
        return {};
    }
    ALWAYS_INLINE constexpr std::suspend_always final_suspend() const noexcept
    {
        return {};
    }
    ALWAYS_INLINE constexpr worker_coroutine_return_object get_return_object() NOEXCEPT
    {
        return *this;
    }
    ALWAYS_INLINE constexpr void return_void() const NOEXCEPT
    {
    }
    ALWAYS_INLINE constexpr void unhandled_exception() const NOEXCEPT
    {
    }
};

#endif
