#ifndef VALUE_OR_ERROR_H
#define VALUE_OR_ERROR_H

#include <exception>
#include <utility>

template<typename T, bool AcceptsExceptions = true>
class value_or_exception;
template<>
class value_or_exception<void, false>
{
    public:
    constexpr void get() noexcept
    {
    }
};
template<>
class value_or_exception<void, true>
{
    private:
    std::exception_ptr exception;

    public:
    value_or_exception() noexcept :
        exception{}
    {
    }
    void set_exception(std::exception_ptr&& ptr) noexcept(
        std::is_nothrow_move_assignable_v<std::exception_ptr>)
    {
        exception = std::move(ptr);
    }
    void get()
    {
        if (exception) [[unlikely]]
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
    constexpr store_as_union() noexcept :
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
    constexpr store_as_pair() noexcept :
        exception{}
    {
    }
};
template<typename T>
class value_or_exception<T, false>
{
    private:
    alignas(alignof(T)) char data[sizeof(T)];

    public:
    constexpr value_or_exception() noexcept
    {
    }
    constexpr void
    set_value(T&& val) noexcept(std::is_nothrow_move_constructible_v<T>)
    {
        new (static_cast<T*>(static_cast<void*>(data))) T{ std::move(val) };
    }
    constexpr T& get() noexcept
    {
        return *static_cast<T*>(static_cast<void*>(data));
    }
};
template<typename T>
    requires(sizeof(store_as_union<T>) < sizeof(store_as_pair<T>))
class value_or_exception<T, true> : public store_as_union<T>
{
    public:
    constexpr value_or_exception() noexcept :
        store_as_union<T>{}
    {
    }
    constexpr void
    set_value(T&& val) noexcept(
        std::is_nothrow_move_constructible_v<T>)
    {
        new (&store_as_union<T>::value) T{ std::move(val) };
    }
    constexpr void set_exception(std::exception_ptr&& ptr) noexcept(
        std::is_nothrow_move_constructible_v<std::exception_ptr>)
    {
        new (&store_as_union<T>::exception) std::exception_ptr{ std::move(ptr) };
        store_as_union<T>::has_exception = true;
    }
    constexpr T& get()
    {
        if (store_as_union<T>::has_exception) [[unlikely]]
        {
            std::rethrow_exception(store_as_union<T>::exception);
        }
        return store_as_union<T>::value;
    }
};
template<typename T>
class value_or_exception<T, true> : public store_as_pair<T>
{
    public:
    constexpr value_or_exception() noexcept :
        store_as_pair<T>{}
    {
    }
    constexpr void
    set_value(T&& val) noexcept(
        std::is_nothrow_move_constructible_v<T>)
    {
        new (static_cast<T*>(static_cast<void*>(&store_as_pair<T>::value))) T{ std::move(val) };
    }
    constexpr void set_exception(std::exception_ptr&& ptr) noexcept(
        std::is_nothrow_move_assignable_v<std::exception_ptr>)
    {
        store_as_pair<T>::exception     = std::move(ptr);
        store_as_pair<T>::has_exception = true;
    }
    constexpr T& get()
    {
        if (&store_as_pair<T>::exception) [[unlikely]]
        {
            std::rethrow_exception(store_as_pair<T>::exception);
        }
        return store_as_pair<T>::value;
    }
};

#endif
