#ifndef WORKER_COROUTINE_H
#define WORKER_COROUTINE_H

#include "std_compatibility.h"

class worker_coroutine_promise_type;

namespace detail::worker_coroutine
{
    ALWAYS_INLINE constexpr void                 set_worker_main_coroutine(std::coroutine_handle<worker_coroutine_promise_type>) noexcept;
    ALWAYS_INLINE static std::coroutine_handle<> executor_pop() noexcept;
} // namespace detail::worker_coroutine

class worker_coroutine_return_object
{
public:
    using promise_type = worker_coroutine_promise_type;
    ALWAYS_INLINE constexpr ~worker_coroutine_return_object() noexcept
    {
        h.destroy();
    }

private:
    ALWAYS_INLINE constexpr worker_coroutine_return_object(promise_type& p) noexcept :
        h{ std::coroutine_handle<promise_type>::from_promise(p) }
    {
    }

private:
    std::coroutine_handle<promise_type> h;
    friend promise_type;
};

struct worker_coroutine_initial_awaiter
{
    ALWAYS_INLINE constexpr bool await_ready() const noexcept
    {
        return false;
    }
    ALWAYS_INLINE constexpr void await_resume() const noexcept
    {
    }
    ALWAYS_INLINE std::coroutine_handle<> await_suspend(std::coroutine_handle<worker_coroutine_promise_type> h) const noexcept
    {
        using namespace detail::worker_coroutine;
        set_worker_main_coroutine(h);
        return executor_pop();
    }
};

struct worker_coroutine_promise_type
{
    ALWAYS_INLINE constexpr worker_coroutine_initial_awaiter initial_suspend() const noexcept
    {
        return {};
    }
    ALWAYS_INLINE constexpr std::suspend_always final_suspend() const noexcept
    {
        return {};
    }
    ALWAYS_INLINE constexpr worker_coroutine_return_object get_return_object() noexcept
    {
        return *this;
    }
    ALWAYS_INLINE constexpr void return_void() const noexcept
    {
    }
    ALWAYS_INLINE constexpr void unhandled_exception() const noexcept
    {
    }
};

#endif
