#ifndef TASK_GRAPH_NODE_H
#define TASK_GRAPH_NODE_H

#include "defines.h"
#include "misc_utilities.h"
#include "mpsc_list.h"
#include <atomic>

template<auto OnNotify = nullptr>
class task_graph_node;

template<auto OnNotify>
class task_graph_node_notifier : public mpsc_list_node
{
    using node = task_graph_node<OnNotify>;

public:
    ALWAYS_INLINE constexpr task_graph_node_notifier(node& task) noexcept :
        to_notify{ task }
    {
    }

private:
    node& to_notify;
    friend node;
};

template<>
class task_graph_node<nullptr>
{
};

template<auto OnNotify>
class task_graph_node : public task_graph_node<nullptr>
{
    using notifier = task_graph_node_notifier<OnNotify>;
    std::atomic<size_t> counter;
    mpsc_list<notifier> list{};

public:
    ALWAYS_INLINE constexpr void set_dependency_count(size_t count) noexcept
    {
        deatomize(counter) = count;
    }
    ALWAYS_INLINE constexpr void notify_dependents() noexcept
    {
        list.complete_and_iterate([](notifier& n) noexcept
                                  {
            if (n.to_notify.counter.fetch_sub(1, memory_order_relaxed) == 1)
            {
                OnNotify(n.to_notify);
            } });
    }
    ALWAYS_INLINE [[nodiscard]] constexpr bool add_dependent(notifier& n) noexcept
    {
        if (list.try_enqueue(n))
        {
            return false;
        }
        else
        {
            n.to_notify.counter.fetch_sub(1, memory_order_relaxed);
            return true;
        }
    }
};

#endif
