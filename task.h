#ifndef TASK_H
#define TASK_H

#include "assume.h"
#include "scheduler_object.h"
#include <atomic>

class task;

class notifiable {
  task *_to_notify;
  notifiable *_next;
  ALWAYS_INLINE constexpr notifiable(task &t) noexcept
      : _to_notify{&t}, _next{nullptr} {}
  friend class task;
};

class task {
  size_t _sync = 0;
  notifiable *_to_notify_head = nullptr;

private:
  template <typename Func>
  ALWAYS_INLINE constexpr void _for_each_to_notify(Func &&func) const
      noexcept(std::is_nothrow_invocable_v<Func, task &>) {
    notifiable *p = _to_notify_head;
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
    _sync = count;
  }
  ALWAYS_INLINE void notify_on_completion(notifiable &to_append) noexcept {
    // std::atomic_ref head{_to_notify_head};
    // to_append._next = head.exchange(&to_append, acq_rel);
    to_append._next = std::exchange(_to_notify_head, &to_append);
  }
  [[gnu::noinline]] void signal_completion() const noexcept {
    _for_each_to_notify([](task &to_notify) {
      // std::atomic_ref sync{to_notify._sync};
      // if(sync.fetch_sub(1, acq_rel) == 1) [[unlikely]] {
      if (to_notify._sync-- == 1) [[unlikely]] {
        scheduler.add_ready_to_resume(to_notify);
      }
    });
  }
};

#endif
