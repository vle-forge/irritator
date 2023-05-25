// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_HELPERS_2023
#define ORG_VLEPROJECT_IRRITATOR_HELPERS_2023

#include "irritator/core.hpp"
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

template<typename Data, typename Function, typename... Args>
void for_each_data(Data& d, Function&& f, Args... args) noexcept
{
    using value_type = typename Data::value_type;

    value_type* ptr = nullptr;
    while (d.next(ptr)) {
        f(*ptr, args...);
    }
}

template<typename Data, typename Function, typename... Args>
void for_each_data(const Data& d, Function&& f, Args... args) noexcept
{
    using value_type = typename Data::value_type;

    const value_type* ptr = nullptr;
    while (d.next(ptr)) {
        f(*ptr, args...);
    }
}

template<typename Data, typename FunctionIf, typename FunctionElse>
auto if_or_else_data_exists_do(Data&                          d,
                               typename Data::identifier_type id,
                               FunctionIf&&                   if_cb,
                               FunctionElse&&                 else_cb) noexcept
  -> std::invoke_result_t<FunctionIf, typename Data::value_type&>
{
    using r1 = std::invoke_result_t<FunctionIf, typename Data::value_type&>;
    using r2 = std::invoke_result_t<FunctionElse>;

    static_assert(std::is_same_v<r1, r2>);

    if constexpr (std::is_void_v<r1>) {
        if (auto* ptr = d.try_to_get(id); ptr)
            if_cb(*ptr);
        else
            else_cb();
    } else {
        auto* ptr = d.try_to_get(id);

        return ptr ? if_cb(*ptr) : else_cb();
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

template<typename Data, typename Function, typename... Args>
void for_specified_data(Data&                                   d,
                        vector<typename Data::identifier_type>& vec,
                        Function&&                              f,
                        Args... args) noexcept
{
    unsigned i = 0;

    while (i < vec.size()) {
        if (auto* ptr = d.try_to_get(vec[i]); ptr) {
            f(*ptr, args...);
            ++i;
        } else {
            vec.swap_pop_back(i);
        }
    }
}

template<typename Data, typename Function, typename... Args>
void for_specified_data(const Data&                             d,
                        vector<typename Data::identifier_type>& vec,
                        Function&&                              f,
                        Args... args) noexcept
{
    unsigned i = 0;

    while (i < vec.size()) {
        if (const auto* ptr = d.try_to_get(vec[i]); ptr) {
            f(*ptr, args...);
            ++i;
        } else {
            vec.swap_pop_back(i);
        }
    }
}

template<typename Data, typename Function, typename... Args>
void for_specified_data(Data&                                         d,
                        const vector<typename Data::identifier_type>& vec,
                        Function&&                                    f,
                        Args... args) noexcept
{
    for (unsigned i = 0, e = vec.size(); i != e; ++i) {
        if (auto* ptr = d.try_to_get(vec[i]); ptr)
            f(*ptr, args...);
    }
}

template<typename Data, typename Function, typename... Args>
void for_specified_data(const Data&                                   d,
                        const vector<typename Data::identifier_type>& vec,
                        Function&&                                    f,
                        Args... args) noexcept
{
    for (unsigned i = 0, e = vec.size(); i != e; ++i) {
        if (const auto* ptr = d.try_to_get(vec[i]); ptr)
            f(*ptr, args...);
    }
}

} // namespace irt

#endif