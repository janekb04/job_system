#ifndef JOB_H
#define JOB_H

#include <future>
#include <tuple>

#include "assume.h"
#include "compat.h"
#include "scheduler_object.h"
#include "task.h"
#include "value_or_error.h"

template <typename T, bool Noexcept, size_t LocalHeapSize> class basic_job;

template <typename Job> using future_of = typename Job::future;

template <typename T> struct is_job_impl {
  static constexpr bool value = false;
};
template <typename T, bool Noexcept, size_t LocalHeapSize>
struct is_job_impl<basic_job<T, Noexcept, LocalHeapSize>> {
  static constexpr bool value = true;
};
template <typename T>
concept is_job = is_job_impl<T>::value;

template <typename... Jobs> struct when_all_t : std::tuple<Jobs &&...> {
  ALWAYS_INLINE constexpr when_all_t(Jobs &&...jobs) noexcept
      : std::tuple<Jobs &&...>{std::forward<Jobs>(jobs)...} {}
};
template <typename... Jobs>
ALWAYS_INLINE constexpr when_all_t<Jobs &&...>
when_all(Jobs &&...jobs) noexcept {
  return {std::forward<Jobs>(jobs)...};
}
struct get_notifiable_for_this_job_tag_t {};
ALWAYS_INLINE constexpr get_notifiable_for_this_job_tag_t
get_notifiable_for_this_job() noexcept {
  return {};
}
template <typename Promise, typename T> struct promise_return_helper_t {
  template <typename U>
  ALWAYS_INLINE constexpr void return_value(U &&val) noexcept(noexcept(
      static_cast<Promise *>(this)->_return_value_impl(std::move(val)))) {
    static_cast<Promise *>(this)->_return_value_impl(std::move(val));
  }
};
template <typename Promise> struct promise_return_helper_t<Promise, void> {
  ALWAYS_INLINE constexpr void return_void() const noexcept {}
};
template <size_t I, typename T>
ALWAYS_INLINE constexpr decltype(auto) unpack_forward(T &&t) noexcept {
  return std::forward<T>(t);
}

template <typename T, bool Noexcept, size_t LocalHeapSize> class basic_job {
public:
  class promise;
  using handle = std::coroutine_handle<promise>;
  using promise_type = promise;
  class promise : public task, public promise_return_helper_t<promise, T> {
  public: // promise_type interface
    ALWAYS_INLINE basic_job get_return_object() noexcept {
      return {handle::from_promise(*this)};
    }
    ALWAYS_INLINE constexpr auto initial_suspend() const noexcept {
      struct awaiter {
        ALWAYS_INLINE constexpr bool await_ready() const noexcept {
          return false;
        }
        ALWAYS_INLINE constexpr void await_resume() const noexcept {}
        ALWAYS_INLINE void await_suspend(handle h) const {
          auto &p = h.promise();
          scheduler.add_ready_to_launch(p);
        }
      };
      return awaiter{};
    }
    ALWAYS_INLINE constexpr std::suspend_always final_suspend() const noexcept {
      signal_completion();
      return {};
    };

    ALWAYS_INLINE constexpr auto
    await_transform(get_notifiable_for_this_job_tag_t) const noexcept {
      struct awaiter {
        ALWAYS_INLINE constexpr bool await_ready() const noexcept {
          return true;
        }
        ALWAYS_INLINE constexpr notifiable await_resume() const noexcept {
          return p._get_notifiable();
        }
        ALWAYS_INLINE [[noreturn]] constexpr void
        await_suspend(handle) const noexcept {
          ASSUME_UNREACHABLE;
        }
        promise &p;
      };
      return awaiter{*this};
    }

    template <typename Job>
      requires is_job<std::decay_t<Job>>
    ALWAYS_INLINE auto await_transform(Job &&j) {
      return await_transform(when_all(std::forward<Job>(j)));
    }

    template <typename... Jobs, size_t... I>
      requires(is_job<std::decay_t<Jobs>> && ...)
    ALWAYS_INLINE auto await_transform(when_all_t<Jobs &&...> jobs) {
      return await_transform_impl(std::move(jobs),
                                  std::index_sequence_for<Jobs...>{});
    }

    ALWAYS_INLINE void unhandled_exception() noexcept {
      if constexpr (Noexcept) {
        ASSUME_UNREACHABLE;
      } else {
        _return_value_storage.set_exception(std::current_exception());
      }
    };

  private:
    template <typename... Jobs, size_t... I>
      requires(is_job<std::decay_t<Jobs>> && ...)
    ALWAYS_INLINE auto await_transform_impl(when_all_t<Jobs &&...> jobs,
                                            std::index_sequence<I...>) {
      struct awaiter {
        ALWAYS_INLINE awaiter(promise &p, when_all_t<Jobs &&...> jobs)
            : ns{unpack_forward<I>(p.get_notifiable())...},
              fs{std::get<I>(jobs).get_future()...} {
          (std::get<I>(jobs).get_future().notify_on_job_completion(ns[I]), ...);
        }
        ALWAYS_INLINE constexpr bool await_ready() const noexcept {
          return false;
        }
        constexpr decltype(auto) await_resume() const noexcept {
          if constexpr (sizeof...(Jobs) == 1) {
            return (std::forward_like<Jobs>(std::get<0>(fs).get()), ...);
          } else {
            return std::tuple<decltype(std::forward_like<Jobs>(
                std::get<I>(fs).get()))...>{
                std::forward_like<Jobs>(std::get<I>(fs).get())...};
          }
        }
        ALWAYS_INLINE std::coroutine_handle<> await_suspend(handle h) {
          return h.promise()._suspend();
        }
        notifiable ns[sizeof...(Jobs)];
        std::tuple<future_of<Jobs>...> fs;
      };

      reset_sync(sizeof...(Jobs));
      return awaiter{*this, std::move(jobs)};
    }

  public: // interface to future
    ALWAYS_INLINE std::add_lvalue_reference_t<T>
    get_return_value() noexcept(noexcept(_return_value_storage.get())) {
      return _return_value_storage.get();
    }
    ALWAYS_INLINE void notify_on_job_completion(notifiable &n) {
      notify_on_completion(n);
    }

  private: // interface to promise_return_helper_t
    template <typename U>
    ALWAYS_INLINE void _return_value_impl(U &&val) noexcept(
        noexcept(_return_value_storage.set_value(std::move(val)))) {
      _return_value_storage.set_value(std::move(val));
    }
    friend class promise_return_helper_t<promise, T>;

  private: // helper methods
    ALWAYS_INLINE std::coroutine_handle<> _suspend() {
      return std::coroutine_handle<task>::from_promise(
          scheduler.get_next_task());
    }
    friend class notifiable;

  private:
    value_or_exception<T, !Noexcept> _return_value_storage;
  };
  class future {
  public:
    ALWAYS_INLINE constexpr future(promise &p) noexcept : _p{p} {}
    ALWAYS_INLINE constexpr T &get() const noexcept(Noexcept) {
      return _p.get_return_value();
    }
    ALWAYS_INLINE void notify_on_job_completion(notifiable &n) const noexcept {
      _p.notify_on_job_completion(n);
    }

  private:
    promise &_p;
  };

public:
  ALWAYS_INLINE future get_future() const noexcept {
    return future{_h.promise()};
  }
  ALWAYS_INLINE constexpr ~basic_job() {
    if (_h) [[likely]] {
      _h.destroy();
    }
  }

private:
  ALWAYS_INLINE constexpr basic_job(handle h) noexcept : _h{std::move(h)} {}

private:
  handle _h;
};

template <typename T> using job = basic_job<T, true, 0>;

#endif
