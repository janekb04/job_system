#ifndef FINAL_AWAITER_H
#define FINAL_AWAITER_H

#include "defines.h"
#include "std_compatibility.h"
#include "promise_base.h"
#include "executor.h"

struct final_awaiter
{
    ALWAYS_INLINE [[nodiscard]] constexpr bool await_ready() const noexcept
    {
        return false;
    }
    ALWAYS_INLINE [[noreturn]] void await_resume() const noexcept
    {
        ASSUME_UNREACHABLE;
    }
    template<typename Promise>
    ALWAYS_INLINE [[nodiscard]] std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) const noexcept
    {
        h.promise().notify_dependents();
        return executor::pop();
    }
};

#endif
