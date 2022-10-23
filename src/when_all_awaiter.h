
#ifndef WHEN_ALL_AWAITER_H
#define WHEN_ALL_AWAITER_H

#include "defines.h"
#include "when_all.h"
#include "misc_utilities.h"
#include "promise_base.h"
#include "executor.h"

class promise_notifier;

template<typename... Futures>
class when_all_awaiter
{
    using index_sequence_for_this = std::index_sequence_for<Futures...>;

private:
    template<size_t... I>
    ALWAYS_INLINE constexpr when_all_awaiter(promise_base& p, when_all_t<Futures...> futures, std::index_sequence<I...>) :
        ns{ unpack<I>(p).get_notifier()... },
        fs{ std::move(futures) }
    {
    }
    template<size_t... I>
    ALWAYS_INLINE constexpr bool await_ready(std::index_sequence<I...>) NOEXCEPT
    {
        int already_done_dependencies_count = (static_cast<int>(std::get<I>(fs).add_dependent_to_promise(ns[I])) + ...);
        if (already_done_dependencies_count == sizeof...(Futures))
            UNLIKELY
            {
                return true;
            }
        return false;
    }
    template<size_t I>
    ALWAYS_INLINE [[nodiscard]] constexpr decltype(auto) get_future_result() NOEXCEPT
    {
        return std::forward_like<std::tuple_element<I, typename when_all_t<Futures...>::tuple_type>>(std::get<I>(fs).get());
    }
    template<size_t... I>
    ALWAYS_INLINE [[nodiscard]] constexpr decltype(auto) await_resume(std::index_sequence<I...>) NOEXCEPT
    {
        if constexpr (sizeof...(Futures) == 1)
        {
            return std::forward<decltype(get_future_result<0>())>(get_future_result<0>());
        }
        else
        {
            return std::tuple{ std::forward<decltype(get_future_result<I>())>(get_future_result<I>())... };
        }
    }

public:
    ALWAYS_INLINE constexpr when_all_awaiter(promise_base& p, when_all_t<Futures...> futures) :
        when_all_awaiter{ p, std::move(futures), index_sequence_for_this{} }
    {
    }

    ALWAYS_INLINE [[nodiscard]] constexpr bool await_ready() NOEXCEPT
    {
        return await_ready(index_sequence_for_this{});
    }
    ALWAYS_INLINE [[nodiscard]] constexpr decltype(auto) await_resume() NOEXCEPT
    {
        return await_resume(index_sequence_for_this{});
    }

    template<typename Promise>
    ALWAYS_INLINE constexpr std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise>) NOEXCEPT
    {
        return executor::pop();
    }

private:
    promise_notifier       ns[sizeof...(Futures)];
    when_all_t<Futures...> fs;
};

#endif
