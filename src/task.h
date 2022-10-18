#ifndef TASK_H
#define TASK_H

#include "assume.h"
#include "scheduler_object.h"
#include <atomic>
#include <cstddef>

class task;

class notifiable {
  task *_to_notify;
  notifiable *_next;
  ALWAYS_INLINE constexpr notifiable(task &t) noexcept
      : _to_notify{&t}, _next{nullptr} {}
  friend class task;
};

class task {
  std::atomic<size_t> _sync = 0;
  std::atomic<notifiable *> _to_notify_head = nullptr;
  static constexpr size_t COMPLETED_SENTINEL = 1;

private:
  template <typename Func>
  ALWAYS_INLINE constexpr void _for_each_to_notify(Func &&func) noexcept(
      std::is_nothrow_invocable_v<Func, task &>) {
    notifiable *p = _to_notify_head.exchange(
        reinterpret_cast<notifiable *>(COMPLETED_SENTINEL),
        std::memory_order_acquire);
    while (p) [[likely]] {
      task *to_notify = p->_to_notify;
      p = p->_next; // prefetch the next node from memory
      func(*to_notify);
    }
  }

public:
  ALWAYS_INLINE constexpr notifiable get_notifiable() noexcept {
    return {*this};
  }
  ALWAYS_INLINE constexpr void reset_sync(size_t count) noexcept {
    deatomize(_sync) = count;
  }
  ALWAYS_INLINE constexpr void sync_atomic_synchronize() noexcept {
    _sync.store(deatomize(_sync), std::memory_order_relaxed);
  }
  ALWAYS_INLINE constexpr bool is_ready_before_launch() const noexcept {
    return deatomize(_sync) == 0;
  }
  ALWAYS_INLINE void notify_on_completion(notifiable &to_append) noexcept {
    notifiable *old_value = _to_notify_head.load(std::memory_order_relaxed);
    while (!_to_notify_head.compare_exchange_weak(
        old_value,
        reinterpret_cast<size_t>(old_value) == COMPLETED_SENTINEL ? old_value
                                                                  : &to_append,
        std::memory_order_relaxed)) {
    }
    if (reinterpret_cast<size_t>(old_value) == COMPLETED_SENTINEL) {
      --deatomize(to_append._to_notify->_sync); // non-atomic on purpose
    } else {
      to_append._next = old_value;
      std::atomic_thread_fence(std::memory_order_release);
    }
  }
  [[gnu::noinline]] void signal_completion() noexcept {
    _for_each_to_notify([](task &to_notify) {
      if (to_notify._sync.fetch_sub(1, std::memory_order_relaxed) == 1)
          [[unlikely]] {
        scheduler.add_ready_to_resume(to_notify);
      }
    });
  }
};

#endif
