#define EXECUTOR_GLOBAL_QUEUE_SIZE 1024 * 1024

#include "executor.h"
#include "job.h"
#include "set_affinity.h"
#include <iostream>

template<int I, bool Init = false>
static job<int, true> test_coro()
{
    if constexpr (I == 0 || I == 1)
    {
        if constexpr (Init)
            executor::done();
        co_return 1;
    }
    else
    {
        auto [r0, r1] = co_await when_all(test_coro<I - 1>(), test_coro<I - 2>());
        if constexpr (Init)
            executor::done();
        co_return r0 + r1;
    }
}

template<int I>
static void test()
{
    int result;
    {
        auto j = test_coro<I, true>();
        executor::run();
        result = j.get_future().get();
    }
    std::cout << result << std::endl;
}

int main()
{
    set_this_process_priority_high();
    // TODO: fix occasional spontaneous segfaults
    for (int i = 1; i <= std::thread::hardware_concurrency(); ++i)
    {
        // Multiple calls to instantiate work
        // Works with any number of worker threads
        executor::instantiate(i);
        // Multiple calls to run work
        test<27>();
        test<27>();
        test<27>();
        test<27>();
        executor::destroy();
    }
}
