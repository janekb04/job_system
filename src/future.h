#ifndef FUTURE_H
#define FUTURE_H

#include "defines.h"
#include "promise.h"
#include "promise_base.h"
#include "when_all.h"

template<typename T, bool Noexcept>
class future
{
public:
    using result_type = T;

public:
    ALWAYS_INLINE constexpr future() NOEXCEPT = delete;
    ALWAYS_INLINE constexpr future(promise<T, Noexcept>& p) NOEXCEPT :
        p{ p }
    {
    }
    ALWAYS_INLINE constexpr future(const future&) NOEXCEPT = default;
    ALWAYS_INLINE [[nodiscard]] constexpr T& get() const noexcept(Noexcept)
    {
        return p.get_return_value();
    }
    ALWAYS_INLINE [[nodiscard]] friend constexpr auto operator co_await(auto&& self) NOEXCEPT
        requires std::same_as<std::remove_cvref_t<decltype(self)>, future<T, Noexcept>>
    {
        return when_all(std::forward<decltype(self)>(self));
    }

private:
    ALWAYS_INLINE [[nodiscard]] constexpr bool add_dependent_to_promise(promise_notifier& n) NOEXCEPT
    {
        return p.add_dependent(n);
    }

private:
    promise<T, Noexcept>& p;
    template<typename... Futures>
    friend class when_all_awaiter;
};

#endif
