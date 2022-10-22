#ifndef PROMISE_BASE_H
#define PROMISE_BASE_H

#include "when_all.h"
#include "executor.h"

#include "task_graph_node.h"

namespace detail::promise_base
{
    ALWAYS_INLINE static void on_notify_helper(task_graph_node<>& task) noexcept;
}

class promise_notifier
{
public:
    ALWAYS_INLINE constexpr promise_notifier(task_graph_node<detail::promise_base::on_notify_helper>& task) noexcept :
        notifier{ task }
    {
    }

private:
    task_graph_node_notifier<detail::promise_base::on_notify_helper> notifier;
    friend class promise_base;
};

template<typename... Futures>
class when_all_awaiter;

class promise_base
{
public:
    template<typename... Futures>
    ALWAYS_INLINE [[nodiscard]] constexpr when_all_awaiter<Futures...> await_transform(when_all_t<Futures...> futures) noexcept
    {
        set_dependency_count(sizeof...(Futures));
        return { *this, std::move(futures) };
    }
    ALWAYS_INLINE [[nodiscard]] constexpr promise_notifier get_notifier() noexcept
    {
        return this_task;
    }

private:
    ALWAYS_INLINE constexpr void on_notify() noexcept
    {
        executor::push(*this);
    }

public:
    ALWAYS_INLINE [[nodiscard]] constexpr bool add_dependent(promise_notifier& n) noexcept
    {
        return this_task.add_dependent(n.notifier);
    }
    ALWAYS_INLINE constexpr void notify_dependents() noexcept
    {
        this_task.notify_dependents();
    }

private:
    ALWAYS_INLINE constexpr void set_dependency_count(size_t count) noexcept
    {
        this_task.set_dependency_count(count);
    }

private:
    task_graph_node<detail::promise_base::on_notify_helper> this_task;
    friend void                                             detail::promise_base::on_notify_helper(task_graph_node<>& task) noexcept;
};

namespace detail::promise_base
{
    ALWAYS_INLINE static void on_notify_helper(task_graph_node<>& task) noexcept
    {
        // This is safe because `this_task` is at offset 0 inside promise_base
        reinterpret_cast<class promise_base*>(&task)->on_notify();
    }
} // namespace detail::promise_base

#endif
