#ifndef JOB_H
#define JOB_H

#include "defines.h"
#include "std_compatibility.h"
#include "promise.h"
#include "future.h"
#include "when_all.h"
#include "misc_utilities.h"

template<typename T, bool Noexcept>
class job
{
public:
    using promise_type = promise<T, Noexcept>;
    using future_type  = future<T, Noexcept>;

    ALWAYS_INLINE constexpr job(promise_type& p) NOEXCEPT :
        h{ std::coroutine_handle<promise_type>::from_promise(p) }
    {
    }

    ALWAYS_INLINE constexpr ~job() NOEXCEPT
    {
        // For some reason, if this check is uncommented, the program crashes with a segfault or a bus error. TODO: Investigate.
        // ASSERT(h && h.done());

        h.destroy();
    }

    ALWAYS_INLINE [[nodiscard]] constexpr future<T, Noexcept> get_future() const NOEXCEPT
    {
        return h.promise();
    }

    // This function seems redundant, but it's needed to work around
    // some weird behaviour (probably a compiler bug).
    // Basically, the caller of the initial job cannot directly request
    // the return value of the job. For some reason, a random value is returned.
    // To fix that, this function is noinline, which forces the compiler
    // to read the result from memory, instead of wherever it is getting it currently.
    [[gnu::noinline]] [[nodiscard]] decltype(auto) get() const noexcept(Noexcept)
    {
        return get_future().get();
    }

    ALWAYS_INLINE [[nodiscard]] friend constexpr auto operator co_await(auto&& self) NOEXCEPT
        requires std::same_as<std::remove_cvref_t<decltype(self)>, job<T, Noexcept>>
    {
        return when_all(std::forward<decltype(self)>(self));
    }

private:
    std::coroutine_handle<promise_type> h;
};

#endif
