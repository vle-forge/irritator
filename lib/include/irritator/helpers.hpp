// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_HELPERS_2023
#define ORG_VLEPROJECT_IRRITATOR_HELPERS_2023

#include <irritator/core.hpp>
#include <irritator/error.hpp>

#include <concepts>
#include <type_traits>

namespace irt {

template<typename T>
    requires std::integral<T> or std::floating_point<T>
class bounded_value
{
    T m_lower;
    T m_upper;
    T m_value;

public:
    constexpr bounded_value(const T& lower_, const T& upper_) noexcept
      : m_lower{ lower_ }
      , m_upper{ upper_ }
    {
        debug::ensure(lower_ < upper_);
    }

    constexpr bool is_valid(const T value) const noexcept
    {
        return m_lower <= value && m_value <= m_upper;
    }

    constexpr void set(const T value) noexcept
    {
        m_value = std::clamp(value, m_lower, m_upper);
    }

    constexpr T value() const noexcept { return m_value; }
    constexpr   operator T() noexcept { return m_value; }
    constexpr   operator T() const noexcept { return m_value; }

    constexpr T lower_bound() noexcept { return m_lower; }
    constexpr T upper_bound() noexcept { return m_upper; }
};

template<typename T, T Lower, T Upper>
    requires std::integral<T>
class static_bounded_value
{
    static_assert(Lower < Upper);

    T m_value;

public:
    constexpr static_bounded_value(const T value) noexcept
      : m_value(std::clamp(value, Lower, Upper))
    {}

    constexpr bool is_valid(const T value) noexcept
    {
        return Lower <= value && value <= Upper;
    }

    constexpr void set(const T value) noexcept
    {
        m_value = std::clamp(value, Lower, Upper);
    }

    constexpr T value() const noexcept { return m_value; }
    constexpr   operator T() noexcept { return m_value; }
    constexpr   operator T() const noexcept { return m_value; }

    static constexpr T lower_bound() noexcept { return Lower; }
    static constexpr T upper_bound() noexcept { return Upper; }
};

template<typename T, int LowerNum, int LowerDenom, int UpperNum, int UpperDenom>
    requires std::floating_point<T>
class static_bounded_floating_point
{
public:
    static inline constexpr T lower =
      static_cast<T>(LowerNum) / static_cast<T>(LowerDenom);
    static inline constexpr T upper =
      static_cast<T>(UpperNum) / static_cast<T>(UpperDenom);
    static_assert(lower < upper);

private:
    T m_value;

public:
    constexpr static_bounded_floating_point(const T value) noexcept
      : m_value(std::clamp(value, lower, upper))
    {}

    constexpr bool is_valid(const T value) noexcept
    {
        return lower <= value && value <= upper;
    }

    constexpr void set(const T value) noexcept
    {
        m_value = std::clamp(value, lower, upper);
    }

    constexpr T value() const noexcept { return m_value; }
    constexpr   operator T() noexcept { return m_value; }
    constexpr   operator T() const noexcept { return m_value; }

    static constexpr T lower_bound() noexcept { return lower; }
    static constexpr T upper_bound() noexcept { return upper; }
};

template<typename>
struct is_expected : std::false_type {};

template<typename T>
struct is_expected<::irt::expected<T>> : std::true_type {};

//! @brief Apply the function @c f for all elements of the @c data_array.
template<typename Data, typename Function>
auto for_each_data(Data& d, Function&& f) noexcept -> void
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, void>);

    for (auto& data : d)
        std::invoke(std::forward<Function>(f), data);
}

//! @brief Apply function @c f until an error occurend in @f.
//!
//! For all element in data_array @c d try to call the function @c f.
//! If this function return false or return a @c is_bad status, then the
//! function return this error.
//!
//! @return If @c f returns a boolean, this function return true or false if a
//! call to @c f fail. If @c f returns a @c irt::result, this function return @c
//! irt::result::success() or the firt error that occured in @c f.
template<typename Data, typename Function>
auto try_for_each_data(Data& d, Function&& f) noexcept ->
  typename std::decay<decltype(std::declval<Function>()().value())>::type
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, bool> ||
                  is_expected<return_t>::value);

    if constexpr (std::is_same_v<return_t, bool>) {
        for (auto& elem : d)
            if (!std::invoke(std::forward<Function>(f), elem))
                return false;
        return true;
    }

    if constexpr (is_expected<return_t>::value) {
        for (auto& data : d)
            if (auto ret = std::invoke(std::forward<Function>(f), data); !ret)
                return ret.error();
        return success();
    }
}

//! @brief Call function @c f if @c id exists in @c data_array.
template<typename Data, typename Function>
void if_data_exists_do(Data&                          d,
                       typename Data::identifier_type id,
                       Function&&                     f) noexcept
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, void>);

    if (auto* ptr = d.try_to_get(id); ptr)
        std::invoke(std::forward<Function>(f), *ptr);
}

//! @brief Call function @c f_if if @c id exists in @c data_array otherwise call
//! the function @c f_else.
template<typename Data, typename FunctionIf, typename FunctionElse>
auto if_data_exists_do(Data&                          d,
                       typename Data::identifier_type id,
                       FunctionIf&&                   f_if,
                       FunctionElse&&                 f_else) noexcept
  -> std::invoke_result_t<FunctionIf, typename Data::value_type&>
{
    using ret_if_t =
      std::invoke_result_t<FunctionIf, typename Data::value_type&>;
    using ret_else_t = std::invoke_result_t<FunctionElse>;

    static_assert(std::is_same_v<ret_if_t, ret_else_t>);

    if (auto* ptr = d.try_to_get(id); ptr)
        return std::invoke(std::forward<FunctionIf>(f_if), *ptr);
    else
        return std::invoke(std::forward<FunctionElse>(f_else));
}

//! @brief Apply function @c f for all element in vector @c.
//!
//! For each element in vector @c (type must be @c typename @c
//! Data::identifier_type with @c Data a @c data_array) call the function
//! @c f. If the @c vec vector is no-const then, undefined element are
//! removed from the vector.
template<typename Data, typename Vector, typename Function>
void for_specified_data(Data& d, Vector& vec, Function&& f) noexcept
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, void>);

    if constexpr (std::is_const_v<Vector>) {
        for (auto i = 0, e = vec.ssize(); i != e; ++i)
            if_data_exists_do(d, vec[i], std::forward<Function>(f));
    } else {
        auto i = 0;

        while (i < vec.ssize()) {
            if (auto* ptr = d.try_to_get(vec[i]); ptr) {
                std::invoke(std::forward<Function>(f), *ptr);
                ++i;
            } else {
                vec.swap_pop_back(i);
            }
        }
    }
}

//! @brief If @c pred returns true, remove data from `data_array`.
//!
//! Remove data from @c data_array @c d when the predicate
//! function @c pred returns true. Otherwise do noting.
template<typename Data, typename Predicate>
void remove_data_if(Data& d, Predicate&& pred) noexcept
{
    static_assert(std::is_const_v<Data> == false);

    using value_type = typename Data::value_type;

    value_type* current = nullptr;
    value_type* to_del  = nullptr;

    while (d.next(current)) {
        if (to_del) {
            d.free(*to_del);
            to_del = nullptr;
        }

        if (std::invoke(std::forward<Predicate>(pred), *current) == true)
            to_del = current;
    }

    if (to_del) {
        d.free(*to_del);
        to_del = nullptr;
    }
}

//! @brief If @c pred returns true, remove datas.
//!
//! Remove data from @c vector @c vec and @c data_array @c d when the predicate
//! function @c pred returns true. Otherwise do noting.
template<typename Data, typename Vector, typename Predicate>
void remove_specified_data_if(Data& d, Vector& vec, Predicate&& pred) noexcept
{
    static_assert(std::is_const_v<Data> == false &&
                  std::is_const_v<Vector> == false);

    auto i = 0;

    while (i < vec.ssize()) {
        if (auto* ptr = d.try_to_get(vec[i]); ptr) {
            if (std::invoke(std::forward<Predicate>(pred), *ptr)) {
                d.free(*ptr);
                vec.swap_pop_back(i);
            } else {
                ++i;
            }
        } else {
            vec.swap_pop_back(i);
        }
    }
}

//! @brief Search in @c vec and element of @c d that valid the predicate
//! @c pred.
//!
//! Do a `O(n)` search in @c vec to search the first element which the call to
//! @c pref function returns true. All invalid identifier in the vector @c vec
//! will be removed.
//!
//! @return A `nullptr` if no element in @c vec validates the @c pred predicte
//! otherwise returns the first element.
template<typename Data, typename Vector, typename Predicate>
auto find_specified_data_if(Data& d, Vector& vec, Predicate&& pred) noexcept ->
  typename Data::value_type*
{
    static_assert(std::is_const_v<Data> == false &&
                  std::is_const_v<Vector> == false);

    auto i = 0;

    while (i < vec.ssize()) {
        if (auto* ptr = d.try_to_get(vec[i]); ptr) {
            if (std::invoke(std::forward<Predicate>(pred), *ptr))
                return ptr;
            else
                ++i;
        } else {
            vec.swap_pop_back(i);
        }
    }

    return nullptr;
}

inline bool all_char_valid(const std::string_view v) noexcept
{
    for (auto c : v)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.'))
            return false;

    return true;
}

inline bool is_valid_irt_filename(const std::string_view v) noexcept
{
    return !v.empty() && v[0] != '.' && v[0] != '-' && all_char_valid(v) &&
           v.ends_with(".irt");
}

inline bool is_valid_dot_filename(const std::string_view v) noexcept
{
    return !v.empty() && v[0] != '.' && v[0] != '-' && all_char_valid(v) &&
           v.ends_with(".dot");
}

template<std::size_t Size>
inline void add_extension(small_string<Size>&    str,
                          const std::string_view extension) noexcept
{
    const std::decay_t<decltype(str)> tmp(str);

    if (auto dot = tmp.sv().find_last_of('.'); dot != std::string_view::npos) {
        format(str, "{}{}", tmp.sv().substr(0, dot), extension);
    } else {
        format(str, "{}{}", tmp.sv(), extension);
    }
}

} // namespace irt

#endif
