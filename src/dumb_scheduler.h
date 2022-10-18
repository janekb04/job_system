#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "compat.h"
#include "ring_buffer.h"
#include <functional>
#include <memory>
#include <utility>

class task;

// TODO: this is a single-threaded minimal and inefficient implementation
class dumb_scheduler
{
    std::unique_ptr<ring_buffer<std::reference_wrapper<task>>> ready =
        std::make_unique<ring_buffer<std::reference_wrapper<task>>>();

    public:
    ALWAYS_INLINE void add_ready_to_launch(task& t)
    {
        ready->push_back(t);
    }
    ALWAYS_INLINE void add_ready_to_resume(task& t)
    {
        add_ready_to_launch(t);
    }
    ALWAYS_INLINE task& get_next_task()
    {
        task& t = ready->pop_front();
        return t;
    }
    ALWAYS_INLINE bool done()
    {
        return ready->empty();
    }
    ALWAYS_INLINE void run()
    {
        while (!done()) [[likely]]
        {
            std::coroutine_handle<task>::from_promise(get_next_task()).resume();
        }
    }
};

#endif
