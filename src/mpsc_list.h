#ifndef MPSC_LIST_H
#define MPSC_LIST_H

#include "defines.h"
#include <atomic>
#include <concepts>

class mpsc_list_node
{
    mpsc_list_node* _next;

    template<typename T>
        requires std::derived_from<T, mpsc_list_node>
    friend class mpsc_list;
};

template<typename T>
    requires std::derived_from<T, mpsc_list_node>
class mpsc_list
{
private:
    std::atomic<T*> head{ nullptr };

private:
    static constexpr size_t COMPLETED_SENTINEL = 8;

public:
    ALWAYS_INLINE constexpr mpsc_list() NOEXCEPT = default;
    constexpr mpsc_list(mpsc_list&& other) NOEXCEPT :
        head{ other.head.exchange(reinterpret_cast<T*>(COMPLETED_SENTINEL), std::memory_order_relaxed) }
    {
    }

    bool try_enqueue(T& to_append)
    {
        T* old_head = head.load(std::memory_order_relaxed);
        do
        {
            if (reinterpret_cast<size_t>(old_head) == COMPLETED_SENTINEL)
                UNLIKELY
                {
                    return false;
                }
            to_append._next = old_head;
        } while (!head.compare_exchange_weak(old_head, &to_append, std::memory_order_release, std::memory_order_relaxed));
        return true;
    }

    template<typename Func>
    void complete_and_iterate(Func&& func) noexcept(std::is_nothrow_invocable_v<Func, T&>)
    {
        T* p = head.exchange(reinterpret_cast<T*>(COMPLETED_SENTINEL), std::memory_order_acquire);
        while (p)
            LIKELY
            {
                T* cur  = p;
                T* next = static_cast<T*>(p->_next);
                p       = next;
                func(*cur);
            }
    }
};

#endif
