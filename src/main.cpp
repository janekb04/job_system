#define EXECUTOR_GLOBAL_QUEUE_SIZE 1024 * 1024

#include "executor.h"
#include "job.h"
#include "set_affinity.h"
#include <chrono>
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
NOINLINE int test_normal(int& invocations)
{
    ++invocations;
    if constexpr (I == 0 || I == 1)
    {
        return 1;
    }
    else
    {
        int result = test_normal<I - 1>(invocations) + test_normal<I - 2>(invocations);
        return result;
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
    std::cout << "    fib(" << I << ") = " << result << " in: ";
    auto coro_time = executor::get_time();
    std::cout << "coro[" << coro_time.count() / 1e6 << "ms], ";
    {
        auto start       = std::chrono::high_resolution_clock::now();
        int  invocations = 0;
        auto result2     = test_normal<I>(invocations);
        if (result != result2)
        {
            // I know: it must be the call to test_normal
            // removing std::cout makes it eliminated

            // std::cout << "Result mismatch: " << result << " != " << result2 << std::endl;
        }
        auto end  = std::chrono::high_resolution_clock::now();
        auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        std::cout << "normal[" << time.count() / 1e6 << "ms], ";
        std::cout << "overhead per invocation = " << (coro_time - time).count() / float(invocations) << "ns\n";
    }
}

int main()
{
    set_this_process_priority_high();
    // TODO: fix occasional spontaneous segfaults
    for (int i = 1; i <= std::thread::hardware_concurrency(); ++i)
    {
        std::cout << "Testing with " << i << " threads:\n";
        // Multiple calls to instantiate work
        // Works with any number of worker threads
        executor::instantiate(i);
        // Multiple calls to run work
        test<10>();
        test<10>();
        test<10>();
        test<10>();
        test<10>();
        test<10>();
        test<10>();
        test<27>();
        test<27>();
        test<27>();
        test<27>();
        test<27>();
        test<27>();
        test<27>();
        executor::destroy();
    }
}
