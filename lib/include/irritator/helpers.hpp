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

    value_type* ptr = nullptr;
    while (d.next(ptr)) {
        f(*ptr);
    }
}

template<typename Data, typename Function>
void for_each_data(const Data& d, Function&& f) noexcept
{
    using value_type = typename Data::value_type;

    const value_type* ptr = nullptr;
    while (d.next(ptr)) {
        f(*ptr);
    }
}

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

    auto* ptr = d.try_to_get(id);

    return ptr ? f(*ptr) : default_return;
}

template<typename Data, typename Function>
auto if_data_exists_do(Data&                          d,
                       typename Data::identifier_type id,
                       Function&&                     f) noexcept -> void
{
    if (auto* ptr = d.try_to_get(id); ptr)
        f(*ptr);
}

template<typename Data, typename Function>
void for_specified_data(Data&                                   d,
                        vector<typename Data::identifier_type>& vec,
                        Function&&                              f) noexcept
{
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

template<typename Data, typename Function>
void for_specified_data(const Data&                             d,
                        vector<typename Data::identifier_type>& vec,
                        Function&&                              f) noexcept
{
    unsigned i = 0;

    while (i < vec.size()) {
        if (const auto* ptr = d.try_to_get(vec[i]); ptr) {
            f(*ptr);
            ++i;
        } else {
            vec.swap_pop_back(i);
        }
    }
}

template<typename Data, typename Function>
void for_specified_data(Data&                                         d,
                        const vector<typename Data::identifier_type>& vec,
                        Function&& f) noexcept
{
    for (unsigned i = 0, e = vec.size(); i != e; ++i) {
        if (auto* ptr = d.try_to_get(vec[i]); ptr)
            f(*ptr);
    }
}

template<typename Data, typename Function>
void for_specified_data(const Data&                                   d,
                        const vector<typename Data::identifier_type>& vec,
                        Function&& f) noexcept
{
    for (unsigned i = 0, e = vec.size(); i != e; ++i) {
        if (const auto* ptr = d.try_to_get(vec[i]); ptr)
            f(*ptr);
    }
}

template<typename Data, typename Function>
auto try_for_specified_data(Data&                                         d,
                            const vector<typename Data::identifier_type>& vec,
                            Function&& f) noexcept
  -> std::invoke_result_t<Function, typename Data::value_type&>
{
    using return_type =
      std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_type, bool> ||
                  std::is_same_v<return_type, irt::status>);

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
}

template<typename Data, typename Function>
auto try_for_specified_data(Data&                                   d,
                            vector<typename Data::identifier_type>& vec,
                            Function&&                              f) noexcept
  -> std::invoke_result_t<Function, typename Data::value_type&>
{
    using return_type =
      std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_type, bool> ||
                  std::is_same_v<return_type, irt::status>);

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

template<typename Data, typename Predicate>
void remove_data_if(Data& d, Predicate&& pred) noexcept
{
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

template<typename Data, typename Predicate>
void remove_specified_data_if(Data&                                   d,
                              vector<typename Data::identifier_type>& vec,
                              Predicate&& pred) noexcept
{
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
