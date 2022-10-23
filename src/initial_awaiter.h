#ifndef INITIAL_AWAITER_H
#define INITIAL_AWAITER_H

#include "defines.h"
#include "std_compatibility.h"
#include "promise_base.h"
#include "executor.h"

struct initial_awaiter
{
    ALWAYS_INLINE [[nodiscard]] constexpr bool await_ready() const NOEXCEPT
    {
        return false;
    }
    ALWAYS_INLINE void await_resume() const NOEXCEPT
    {
        executor::publish();
    }
    template<typename Promise>
    ALWAYS_INLINE void await_suspend(std::coroutine_handle<Promise> h) const NOEXCEPT
    {
        executor::push(h.promise());
    }
};

#endif
