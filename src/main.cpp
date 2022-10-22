#include "executor.h"
#include "job.h"
#include <iostream>

template<int I, bool Init = false>
static job<int, true> test_coro()
{
    if constexpr (I == 0 || I == 1)
    {
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

int main()
{
    executor::instantiate(1);
    int result;
    {
        auto j = test_coro<23, true>();
        executor::run();
        result = j.get_future().get();
    }
    std::cout << result << std::endl;
    executor::destroy();
}
