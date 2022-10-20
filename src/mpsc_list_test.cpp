#include "mpsc_list.h"
#include <atomic>
#include <cassert>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>

struct int_wrap : public mpsc_list_node
{
    int val;
    int_wrap() = default;
    int_wrap(int v) :
        val{ v }
    {
    }
};

std::atomic_flag can_exit = {};

int main()
{
    for (int p_count = 0; p_count < 20; ++p_count)
    {
        mpsc_list<int_wrap> list;
        can_exit.clear();
        std::vector<std::thread> producers;
        std::thread              consumer;
        producers.reserve(p_count);
        std::vector<size_t> seen((p_count + 1) * 1'000'000, false);

        auto see = [&](int val)
        {
            if (seen[val])
            {
                std::cout << "Duplicate " << val << "  (" << val % 1'000'000 << "th value of producer no. " << val / 1'000'000 << " of " << p_count << ')' << std::endl;
            }
            seen[val] = true;
        };
        for (int i = 1; i <= p_count; ++i)
        {
            producers.emplace_back([&, index = i]()
                                   {
                std::vector<int_wrap> vals;
                vals.resize(1'000'000);
                for (int i = 0; i < vals.size(); ++i)
                {
                    vals[i] = i + index * 1'000'000;
                }
                int j = 0;
                 for (; j < 1'000'000; ++j)
                {
                    if(!list.try_enqueue(vals[j]))
                    {
                        break;
                    }
                }
                for (; j < 1'000'000; ++j)
                {
                    see(vals[j].val);
                }
                can_exit.wait(false); });
        }
        auto verify = [&]()
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
            std::vector<int_wrap> vals;
            vals.resize(1'000'000);
            for (int i = 0; i < vals.size(); ++i)
            {
                vals[i] = i;
            }
            int own = 1;
            int iters_to_complete = rand() % vals.size();
            for (; own < vals.size(); ++own)
            {
                if (own == iters_to_complete)
                {
                    break;
                }
                bool success = list.try_enqueue(vals[own]);
                if(!success)
                {
                    std::cout << "Consumer failed to enqueue " << own << std::endl;
                }
            }
            list.complete_and_iterate([&](int_wrap& val)
                                      {
                see(val.val) ; });
            for (; own < 1'000'000; ++own)
            {
                see(vals[own].val);
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
