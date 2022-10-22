#ifndef WHEN_ALL_H
#define WHEN_ALL_H

#include "std_compatibility.h"
#include "defines.h"
#include <tuple>
#include <functional>
#include <utility>

template<typename T, bool Noexcept>
class job;

namespace detail::when_all
{
    template<typename T>
    struct is_job_impl
    {
        static constexpr bool value = false;
    };
    template<typename T, bool Noexcept>
    struct is_job_impl<job<T, Noexcept>>
    {
        static constexpr bool value = true;
    };
    template<typename T>
    concept is_job = is_job_impl<T>::value;

    template<typename T>
    auto get_job_or_its_future(T&& something)
    {
        if constexpr (is_job<std::remove_cvref_t<T>>)
        {
            return something.get_future();
        }
        else
        {
            return something;
        }
    }
} // namespace detail::when_all

template<typename... Futures>
struct when_all_t : std::tuple<Futures...>
{
    when_all_t() = delete;
    // TODO: See if rvalue/lvalue-ness is preserved
    ALWAYS_INLINE constexpr when_all_t(Futures... futures) noexcept :
        std::tuple<Futures...>{ static_cast<Futures>(futures)... }
    {
    }
    using tuple_type = std::tuple<Futures&&...>;
};

template<typename... Awaitables>
    requires(sizeof...(Awaitables) <= 249)
ALWAYS_INLINE constexpr auto when_all(Awaitables&&... awaitables) noexcept
{
    using namespace detail::when_all;
    return when_all_t{ get_job_or_its_future(awaitables)... };
}

#endif
