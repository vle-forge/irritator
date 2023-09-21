// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_HELPERS_2023
#define ORG_VLEPROJECT_IRRITATOR_HELPERS_2023

#include <irritator/core.hpp>

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

    constexpr bool is_valid(const T& value) const noexcept
    {
        return lower <= value && value <= upper;
    }
};

template<typename Data, typename Function>
void for_each_data(Data& d, Function&& f) noexcept
{
    using value_type = typename Data::value_type;

    if constexpr (std::is_const_v<Data>) {
        const value_type* ptr = nullptr;
        while (d.next(ptr)) {
            f(*ptr);
        }
    } else {
        value_type* ptr = nullptr;
        while (d.next(ptr)) {
            f(*ptr);
        }
    }
}

/**
 * @brief Apply function @c f until an error occurend in @f.
 * @details For all element in data_array @c d try to call the function @c f. If
 * this function return false or return a @c is_bad status, then the function
 * return this error.
 *
 * @return If @c f returns a boolean, this function return true or false if a
 * call to @c f fail. If @c f returns a irt::status, this function return @c
 * irt::status::success or the firt error that occured in @c f.
 */
template<typename Data, typename Function>
auto try_for_each_data(Data& d, Function&& f) noexcept
  -> std::invoke_result_t<Function, typename Data::value_type&>
{
    using return_type =
      std::invoke_result_t<Function, typename Data::value_type&>;
    using value_type = typename Data::value_type;

    static_assert(std::is_same_v<return_type, bool> ||
                  std::is_same_v<return_type, irt::status>);

    if constexpr (std::is_same_v<return_type, bool>) {
        value_type* ptr = nullptr;
        while (d.next(ptr)) {
            if (!f(*ptr)) {
                return false;
            }
        }
        return true;
    } else if constexpr (std::is_same_v<return_type, irt::status>) {
        value_type* ptr = nullptr;
        while (d.next(ptr)) {
            if (auto ret = f(*ptr); is_bad(ret)) {
                return ret;
            }
        }
        return status::success;
    }
}

template<typename Data, typename Function, typename Return>
auto if_data_exists_return(Data&                          d,
                           typename Data::identifier_type id,
                           Function&&                     f,
                           const Return& default_return) noexcept -> Return
{
    static_assert(
      std::is_same_v<std::invoke_result_t<Function, typename Data::value_type&>,
                     Return>);

    if constexpr (std::is_const_v<Data>) {
        const auto* ptr = d.try_to_get(id);
        return ptr ? f(*ptr) : default_return;
    } else {
        auto* ptr = d.try_to_get(id);
        return ptr ? f(*ptr) : default_return;
    }
}

template<typename Data, typename Function>
auto if_data_exists_do(Data&                          d,
                       typename Data::identifier_type id,
                       Function&&                     f) noexcept -> void
{
    if (auto* ptr = d.try_to_get(id); ptr)
        f(*ptr);
}

/**
 * @brief Apply function @c f for all element in vector @c.
 * @details For each element in vector @c (type must be @c typename @c
 * Data::identifier_type with @c Data a @c data_array) call the function @c f.
 * If the @c vec vector is no-const then, undefined element are removed from the
 * vector.
 */
template<typename Data, typename Vector, typename Function>
void for_specified_data(Data& d, Vector& vec, Function&& f) noexcept
{
    if constexpr (std::is_const_v<Vector>) {
        for (unsigned i = 0, e = vec.size(); i != e; ++i) {
            if (auto* ptr = d.try_to_get(vec[i]); ptr)
                f(*ptr);
        }
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

/**
 * @brief Apply function @c f for all element in vector @c and stop if @c f
 * fail.
 * @details For each element in vector @c (type must be @c typename @c
 * Data::identifier_type with @c Data a @c data_array) call the function @c f.
 * If the call failed, then function return false or the bad status. (see @c
 * try_for_each_data function) If the @c vec vector is no-const then, undefined
 * element are removed from the vector.
 */
template<typename Data, typename Vector, typename Function>
auto try_for_specified_data(Data& d, Vector& vec, Function&& f) noexcept
  -> std::invoke_result_t<Function, typename Data::value_type&>
{
    using return_type =
      std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_type, bool> ||
                  std::is_same_v<return_type, irt::status>);

    if constexpr (std::is_const_v<Vector>) {
        if constexpr (std::is_same_v<return_type, bool>) {
            for (unsigned i = 0, e = vec.size(); i != e; ++i) {
                if (const auto* ptr = d.try_to_get(vec[i]); ptr)
                    if (!f(*ptr))
                        return false;
            }

            return true;
        } else if constexpr (std::is_same_v<return_type, irt::status>) {
            for (unsigned i = 0, e = vec.size(); i != e; ++i) {
                if (const auto* ptr = d.try_to_get(vec[i]); ptr)
                    if (auto ret = f(*ptr); is_bad(ret))
                        return ret;
            }

            return status::success;
        }
    } else {
        if constexpr (std::is_same_v<return_type, bool>) {
            unsigned i = 0;

            while (i < vec.size()) {
                if (auto* ptr = d.try_to_get(vec[i]); ptr) {
                    if (!f(*ptr))
                        return false;
                    ++i;
                } else {
                    vec.swap_pop_back(i);
                }
            }

            return true;
        } else if constexpr (std::is_same_v<return_type, irt::status>) {

            unsigned i = 0;

            while (i < vec.size()) {
                if (auto* ptr = d.try_to_get(vec[i]); ptr) {
                    if (auto ret = f(*ptr); is_bad(ret))
                        return ret;
                    ++i;
                } else {
                    vec.swap_pop_back(i);
                }
            }

            return status::success;
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

} // namespace irt

#endif
