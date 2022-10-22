#ifndef VALUE_OR_EXCEPTION_H
#define VALUE_OR_EXCEPTION_H

#include "defines.h"
#include <exception>
#include <utility>

// Primary template
template<typename T, bool AcceptsExceptions = true>
class value_or_exception;
// Specialization for void-returning no-throwing functions
template<>
class value_or_exception<void, false>
{
public:
    ALWAYS_INLINE constexpr void get() noexcept
    {
    }
};
// Specialization for void-returning throwing functions
template<>
class value_or_exception<void, true>
{
private:
    std::exception_ptr exception;

public:
    ALWAYS_INLINE value_or_exception() noexcept :
        exception{}
    {
    }
    ALWAYS_INLINE void set_exception(std::exception_ptr&& ptr) noexcept(std::is_nothrow_move_assignable_v<std::exception_ptr>)
    {
        exception = std::move(ptr);
    }
    ALWAYS_INLINE void get()
    {
        if (exception)
            UNLIKELY
            {
                std::rethrow_exception(exception);
            }
    }
};

template<typename T>
class store_as_union
{
protected:
    union
    {
        T                  value;
        std::exception_ptr exception;
    };
    bool has_exception;

public:
    ALWAYS_INLINE constexpr store_as_union() noexcept :
        has_exception{ false }
    {
    }
};

template<typename T>
class store_as_pair
{
protected:
    alignas(alignof(T)) char value[sizeof(T)];
    std::exception_ptr exception;

public:
    ALWAYS_INLINE constexpr store_as_pair() noexcept :
        exception{}
    {
    }
};
// Specialization for non-void-returning no-throwing functions
template<typename T>
class value_or_exception<T, false>
{
private:
    alignas(alignof(T)) char data[sizeof(T)];

public:
    ALWAYS_INLINE constexpr value_or_exception() noexcept
    {
    }
    ALWAYS_INLINE constexpr void set_value(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        new (static_cast<T*>(static_cast<void*>(data))) T{ std::move(val) };
    }
    ALWAYS_INLINE [[nodiscard]] constexpr T& get() noexcept
    {
        return *static_cast<T*>(static_cast<void*>(data));
    }
};
// Specialization for non-void-returning throwing functions
template<typename T>
    requires(sizeof(store_as_union<T>) < sizeof(store_as_pair<T>))
class value_or_exception<T, true> : public store_as_union<T>
{
public:
    ALWAYS_INLINE constexpr value_or_exception() noexcept :
        store_as_union<T>{}
    {
    }
    ALWAYS_INLINE constexpr void set_value(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        new (&store_as_union<T>::value) T{ std::move(val) };
    }
    ALWAYS_INLINE constexpr void set_exception(std::exception_ptr&& ptr) noexcept(std::is_nothrow_move_constructible_v<std::exception_ptr>)
    {
        new (&store_as_union<T>::exception) std::exception_ptr{ std::move(ptr) };
        store_as_union<T>::has_exception = true;
    }
    ALWAYS_INLINE [[nodiscard]] constexpr T& get()
    {
        if (store_as_union<T>::has_exception)
            UNLIKELY
            {
                std::rethrow_exception(store_as_union<T>::exception);
            }
        return store_as_union<T>::value;
    }
};
// Specialization for non-void-returning throwing functions
template<typename T>
class value_or_exception<T, true> : public store_as_pair<T>
{
public:
    ALWAYS_INLINE constexpr value_or_exception() noexcept :
        store_as_pair<T>{}
    {
    }
    ALWAYS_INLINE constexpr void set_value(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        new (static_cast<T*>(static_cast<void*>(&store_as_pair<T>::value))) T{ std::move(val) };
    }
    ALWAYS_INLINE constexpr void set_exception(std::exception_ptr&& ptr) noexcept(std::is_nothrow_move_assignable_v<std::exception_ptr>)
    {
        store_as_pair<T>::exception     = std::move(ptr);
        store_as_pair<T>::has_exception = true;
    }
    ALWAYS_INLINE [[nodiscard]] constexpr T& get()
    {
        if (&store_as_pair<T>::exception)
            UNLIKELY
            {
                std::rethrow_exception(store_as_pair<T>::exception);
            }
        return store_as_pair<T>::value;
    }
};

#endif
