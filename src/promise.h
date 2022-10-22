#ifndef PROMISE_H
#define PROMISE_H

#include "std_compatibility.h"
#include "promise_base.h"
#include "when_all_awaiter.h"
#include "initial_awaiter.h"
#include "final_awaiter.h"
#include "value_or_exception.h"
#include "promise_return_helper.h"

template<typename T, bool Noexcept>
class job;

template<typename T, bool Noexcept>
class promise : public promise_base, public promise_return_helper<promise<T, Noexcept>, T>
{
public:
    ALWAYS_INLINE [[nodiscard]] constexpr job<T, Noexcept> get_return_object() noexcept
    {
        return *this;
    }
    ALWAYS_INLINE [[nodiscard]] constexpr initial_awaiter initial_suspend() noexcept
    {
        return {};
    }
    ALWAYS_INLINE [[nodiscard]] constexpr final_awaiter final_suspend() noexcept
    {
        return {};
    }
    ALWAYS_INLINE [[nodiscard]] constexpr T& get_return_value() noexcept(Noexcept)
    {
        return return_value_storage.get();
    }

    ALWAYS_INLINE constexpr void unhandled_exception() noexcept
    {
        if constexpr (Noexcept)
            ASSUME_UNREACHABLE;
        else
            return_value_storage.set_exception(std::current_exception());
    }

private:
    [[no_unique_address]] value_or_exception<T, !Noexcept> return_value_storage;
    friend struct promise_return_helper<promise<T, Noexcept>, T>;
};

#endif
