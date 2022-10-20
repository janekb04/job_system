#include "round_robin_queue.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>
#include <barrier>
#include <random>

int main()
{
    constexpr size_t cnt = 10'000'000;
    for (int p_count = 1; p_count <= 20; ++p_count)
    {
        std::vector<std::thread> producers;
        producers.reserve(p_count);
        std::vector<size_t>             seen(p_count * cnt, false);
        std::atomic<int>                done_enqueuing = 0;
        round_robin_queue<int, 1 << 28> Q{ size_t(p_count) };
        tls.reset_thread_count();

        std::barrier barrier{ p_count, [&]()
                              {
                                  std::atomic_thread_fence(std::memory_order_acquire);
                                  for (int i = 0; i < seen.size(); ++i)
                                  {
                                      if (!seen[i])
                                      {
                                          std::cout << "Missing " << i << "  (" << i % cnt << "th value of producer no. " << i / cnt << " of " << p_count << ')' << std::endl;
                                      }
                                  }
                              } };

        for (int i = 0; i < p_count; ++i)
        {
            producers.emplace_back([&, index = i]()
                                   {
                std::minstd_rand rng{ static_cast<unsigned>(index) };
                std::vector<int> vals;
                vals.resize(cnt);
                for (int i = 0; i < vals.size(); ++i)
                {
                    vals[i] = i + index * cnt;
                }
                int enqueued = 0;
                while (true)
                {
                    if (rng() % 50 == 0 && enqueued < vals.size())
                    {
                        Q.enqueue(vals[enqueued++]);
                        if (enqueued == vals.size())
                        {
                            done_enqueuing.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    else
                    {
                        int* val = Q.try_dequeue(100);
                        if (val)
                        {
                            if (seen[*val])
                            {
                                std::cout << "Duplicate " << *val << std::endl;
                            }
                            seen[*val] = true;
                        }
                        else
                        {
                            if (done_enqueuing.load(std::memory_order_relaxed) == p_count)
                            {
                                break;
                            }
                        }
                    }
                }
                std::atomic_thread_fence(std::memory_order_release);
                barrier.arrive_and_wait(); });
        }
        for (auto& p : producers)
        {
            p.join();
        }
        std::atomic_thread_fence(std::memory_order_seq_cst);
        std::cout << "Tested " << p_count << " producers" << std::endl;
    }
}
