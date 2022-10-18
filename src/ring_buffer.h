#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "assume.h"
#include "compat.h"
#include <atomic>
#include <type_traits>

constexpr size_t SZ = 1 << 30;

template<typename T>
    requires std::is_trivially_destructible_v<T>
class ring_buffer_base
{
    protected:
    alignas(T) char _raw_storage[SZ * sizeof(T)];
    ALWAYS_INLINE constexpr T* _data() noexcept
    {
        return static_cast<T*>(static_cast<void*>(_raw_storage));
    }
    ALWAYS_INLINE constexpr const T* _data() const noexcept
    {
        return static_cast<T*>(static_cast<void*>(_raw_storage));
    }
};
// template <typename T> class ring_buffer_mt : public ring_buffer_base<T> {
//   struct {
//     alignas(std::hardware_destructive_interference_size) unsigned short
//     _front =
//         0;
//     alignas(std::hardware_destructive_interference_size) unsigned short _back
//     =
//         0;
//   } _iters;
//   using ring_buffer_base<T>::_data;

// public:
//   ALWAYS_INLINE constexpr void
//   push_back(T elem) noexcept(std::is_move_constructible_v<T>) {
//     std::atomic_ref back{_iters._back};
//     unsigned short old_back = back.fetch_add(1, acq_rel); // wraps around
//     new (_data() + old_back) T{std::move(elem)};
//   }
//   ALWAYS_INLINE constexpr T &&pop_front() noexcept {
//     std::atomic_ref front{_iters._front};
//     unsigned short old_front = front.fetch_add(1, acq_rel); // wraps around
//     return std::move(_data()[old_front]);
//   }
// };
template<typename T>
class ring_buffer : public ring_buffer_base<T>
{
    int _front = 0;
    int _back  = 0;
    using ring_buffer_base<T>::_data;

    public:
    ALWAYS_INLINE constexpr void
    push_back(T elem) noexcept(std::is_move_constructible_v<T>)
    {
        new (_data() + _back) T{ std::move(elem) };
        _back = (_back + 1) % SZ;
    }
    ALWAYS_INLINE constexpr T&& pop_front() noexcept
    {
        T&& r  = std::move(_data()[_front]);
        _front = (_front + 1) % SZ;
        return std::move(r);
    }
    ALWAYS_INLINE constexpr bool empty() noexcept
    {
        return _front == _back;
    }
};

#endif
