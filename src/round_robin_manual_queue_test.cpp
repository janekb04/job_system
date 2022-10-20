#include "mpsc_queue.h"
#include <atomic>
#include <memory>
#include <thread>
#include <vector>
#include <iostream>
#include <barrier>

int main()
{
    for (int p_count = 1; p_count <= 20; ++p_count)
    {
        std::vector<std::thread> producers;
        producers.reserve(p_count);
        std::vector<mpsc_queue<int, 1 << 28>> Qs{ size_t(p_count) };
        std::vector<size_t>                   seen(p_count * 1'000'000, false);
        std::atomic<int>                      done_enqueuing = 0;
        std::atomic<int>                      round_robin{ 0 };

        std::barrier barrier{ p_count, [&]()
                              {
                                  std::atomic_thread_fence(std::memory_order_acquire);
                                  for (int i = 0; i < seen.size(); ++i)
                                  {
                                      if (!seen[i])
                                      {
                                          std::cout << "Missing " << i << "  (" << i % 1'000'000 << "th value of producer no. " << i / 1'000'000 << " of " << p_count << ')' << std::endl;
                                      }
                                  }
                              } };

        for (int i = 0; i < p_count; ++i)
        {
            producers.emplace_back([&, index = i]()
                                   {
                std::vector<int> vals;
                vals.resize(1'000'000);
                for (int i = 0; i < vals.size(); ++i)
                {
                    vals[i] = i + index * 1'000'000;
                }
                int enqueued = 0;
                while (true)
                {
                    if (rand() % 2 == 0 && enqueued < vals.size())
                    {
                        int q = round_robin.fetch_add(1, std::memory_order_relaxed) % Qs.size();
                        Qs[q].enqueue(vals[enqueued++]);
                        if (enqueued == vals.size())
                        {
                            done_enqueuing.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
                    else
                    {
                        int* val = Qs[index].try_dequeue(100);
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
