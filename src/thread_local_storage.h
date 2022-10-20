#ifndef THREAD_LOCAL_STORAGE_H
#define THREAD_LOCAL_STORAGE_H

#include <atomic>
#include <cstddef>

inline static thread_local struct tls_t
{
private:
    inline static std::atomic<size_t> thread_count{ 0 };

public:
    size_t index = thread_count.fetch_add(1, std::memory_order_relaxed);

    void reset_thread_count() noexcept
    {
        thread_count.store(0, std::memory_order_relaxed);
    }
} tls;

#endif
