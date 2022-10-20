#include "mpsc_queue.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

auto             Q        = std::make_unique<mpsc_queue<int, 1 << 28>>();
std::atomic_flag can_exit = {};

int main()
{
    for (int p_count = 0; p_count < 20; ++p_count)
    {
        can_exit.clear();
        std::vector<std::thread> producers;
        std::thread              consumer;
        producers.reserve(p_count);
        for (int i = 1; i <= p_count; ++i)
        {
            producers.emplace_back([index = i]()
                                   {
                std::vector<int> vals;
                vals.resize(1'000'000);
                for (int i = 0; i < vals.size(); ++i)
                {
                    vals[i] = i + index * 1'000'000;
                }
                for (int j = 0; j < 1'000'000; ++j)
                {
                    Q->enqueue(vals[j]);
                }
                can_exit.wait(false); });
        }
        std::vector<bool> seen((p_count + 1) * 1'000'000, false);
        auto              verify = [&]()
        {
            for (int i = 1; i < seen.size(); ++i)
            {
                if (!seen[i])
                {
                    std::cout << "Missing " << i << "  (" << i % 1'000'000 << "th value of producer no. " << i / 1'000'000 << " of " << p_count << ')' << std::endl;
                }
            }
        };
        int dequeued = 0;

        consumer = std::thread([&]()
                               {
            std::vector<int> vals;
            vals.resize(1'000'000);
            for (int i = 0; i < vals.size(); ++i)
            {
                vals[i] = i;
            }
            int own = 1;
            while (dequeued < seen.size() - 1)
            {
                int* val = Q->try_dequeue(100);
                if (val)
                {
                    if (seen[*val])
                    {
                        std::cout << "Duplicate " << *val << std::endl;
                    }
                    seen[*val] = true;
                    ++dequeued;
                }
                if (own < 1'000'000)
                    Q->enqueue(vals[own++]);
            }
            can_exit.test_and_set();
            can_exit.notify_all(); });
        for (auto& p : producers)
        {
            p.join();
        }
        consumer.join();
        std::atomic_thread_fence(std::memory_order_seq_cst);

        verify();
        std::cout << "Tested " << p_count << " producers" << std::endl;
    }
}
