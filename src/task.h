#ifndef TASK_H
#define TASK_H

#include "assume.h"
#include <atomic>
#include <cstddef>
#include "executor.h"
#include "mpsc_list.h"

class task;

class notifiable : public mpsc_list_node
{
    task* _to_notify;
    ALWAYS_INLINE constexpr notifiable(task& t) noexcept
        :
        _to_notify{ &t }
    {
    }
    friend class task;
};

class task : protected mpsc_list<notifiable>
{
    std::atomic<size_t> _sync = 0;

public:
    ALWAYS_INLINE constexpr task(task&& other) :
        mpsc_list<notifiable>{ std::move(other) },
        _sync{ 0 } // the move ctor is called only when the task is already done
    {
    }
    ALWAYS_INLINE constexpr task() noexcept = default;
    ALWAYS_INLINE constexpr notifiable get_notifiable() noexcept
    {
        return { *this };
    }
    ALWAYS_INLINE constexpr void reset_sync(size_t count) noexcept
    {
        deatomize(_sync) = count; // DEBUG: this is ok
    }
    ALWAYS_INLINE bool notify_on_completion(notifiable& to_append) noexcept
    {
        if (this->try_enqueue(to_append))
        {
            return false;
        }
        else
        {
            to_append._to_notify->_sync.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
    }
    ALWAYS_INLINE void signal_completion() noexcept
    {
        this->complete_and_iterate([](notifiable& n) noexcept
                                   {
                if(n._to_notify->_sync.fetch_sub(1, std::memory_order_relaxed) == 1) [[unlikely]]
                {
                    executor::instance().add_ready_to_resume(*n._to_notify);
                } });
    }
};

#endif
