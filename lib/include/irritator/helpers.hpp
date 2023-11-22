// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_HELPERS_2023
#define ORG_VLEPROJECT_IRRITATOR_HELPERS_2023

#include <irritator/core.hpp>
#include <irritator/error.hpp>

#include <type_traits>

namespace irt {

template<typename T>
struct limiter
{
    T lower;
    T upper;

    constexpr limiter(const T& lower_, const T& upper_) noexcept
      : lower{ lower_ }
      , upper{ upper_ }
    {
        irt_assert(lower_ < upper_);
    }

    constexpr bool is_valid(const T value) const noexcept
    {
        return lower <= value && value <= upper;
    }
};

template<typename T, T Lower, T Upper>
struct static_limiter
{
    static_assert(Lower < Upper);

    constexpr bool is_valid(const T value) const noexcept
    {
        return Lower <= value && value <= Upper;
    }
};

template<typename>
struct is_result : std::false_type
{};

template<typename T>
struct is_result<::boost::leaf::result<T>> : std::true_type
{};

//! @brief Apply the function @c f for all elements of the @c data_array.
template<typename Data, typename Function>
auto for_each_data(Data& d, Function&& f) noexcept -> void
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, void>);

    for (auto& data : d)
        f(data);
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

    static_assert(std::is_same_v<return_t, bool> || is_result<return_t>::value);

    if constexpr (std::is_same_v<return_t, bool>) {
        for (auto& elem : d)
            if (!f(elem))
                return false;
        return true;
    }

    if constexpr (is_result<return_t>::value) {
        for (auto& data : d)
            if (auto ret = f(data); !ret)
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
        f(*ptr);
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
        return f_if(*ptr);
    else
        return f_else();
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
        for (unsigned i = 0, e = vec.size(); i != e; ++i)
            if_data_exists_do(d, vec[i], f);
    } else {
        unsigned i = 0;

        while (i < vec.size()) {
            if (auto* ptr = d.try_to_get(vec[i]); ptr) {
                f(*ptr);
                ++i;
            } else {
                vec.swap_pop_back(i);
            }
        }
    }
}

template<typename Data, typename Predicate>
void remove_data_if(Data& d, Predicate&& pred) noexcept
{
    static_assert(std::is_const_v<Data> == false);

    using value_type = typename Data::value_type;

    value_type* to_del = nullptr;
    value_type* ptr    = nullptr;

    while (d.next(ptr)) {
        if (to_del) {
            d.free(*to_del);
            to_del = nullptr;
        }

        if (pred(*ptr)) {
            to_del = ptr;
        }
    }

    if (to_del) {
        d.free(*to_del);
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

    for (unsigned i = 0; i < vec.size(); ++i) {
        if (auto* ptr = d.try_to_get(vec[i]); ptr) {
            if (pred(*ptr)) {
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

    for (unsigned i = 0; i < vec.size(); ++i) {
        if (auto* ptr = d.try_to_get(vec[i]); ptr) {
            if (pred(*ptr))
                return ptr;

            ++i;
        } else {
            vec.swap_pop_back(i);
        }
    }

    return nullptr;
}

} // namespace irt

#endif
