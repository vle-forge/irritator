// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_ERROR_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_ERROR_HPP

#include <irritator/macros.hpp>

#include <memory>
#include <string_view>

#define BOOST_LEAF_EMBEDDED
#include <boost/leaf.hpp>

#define irt_check BOOST_LEAF_CHECK
#define irt_auto BOOST_LEAF_AUTO

namespace irt {

template<typename T, T... value>
using match = boost::leaf::match<T, value...>;

template<class T>
using result = boost::leaf::result<T>;

using status = result<void>;

using error_handler = void(void) noexcept;

inline error_handler* on_error_callback = nullptr;

//! Common error to report that container (@c data_array, @c vector or @c
//! list) are full and need a @c resize() or a @c free().
struct container_full_error {};

//! Common error to report an object already exists in the container.
struct already_exist_error {};

//! Common error to report an incompatibility between objects for example when
//! trying to connect two models boolean and continue.
struct incompatibility_error {};

//! Common error to report an object is unknown for examples an undefined or
//! an already delete identifier from a @c data_array.
struct unknown_error {};

//! Common error to report an object is not completely defined for example a @c
//! component with bad @c registered_id, @c dir_path_id or @c file_path_id.
struct undefined_error {};

//! Report an error in the argument pass to a function. Must be rarely used,
//! prefer @c irt_assert to @c std::abort the application and fix the source
//! code.
struct argument_error {};

enum class fs_error {
    user_directory_access_fail,
    user_directory_file_access_fail,
    user_component_directory_access_fail,
    executable_access_fail,
};

//! Report an error in the @c std::filesystem library.
struct filesystem_error {
    fs_error type = fs_error::executable_access_fail;
};

struct e_file_name {
    e_file_name() noexcept = default;

    e_file_name(std::string_view str) noexcept
    {
        const auto len = str.size() > sizeof value ? sizeof value : str.size();

        std::uninitialized_copy_n(str.data(), len, value);
    }

    char value[64];
};

struct e_errno {
    e_errno() noexcept = default;

    e_errno(int value_ = errno) noexcept
      : value{ value_ }
    {}

    int value;
};

/** To report a error during the allocation process. */
struct e_memory {
    std::size_t request{};  //!< Requested capacity in bytes.
    std::size_t capacity{}; //!< Current capacity in bytes. Can be nul if
                            //!< current capacity is not available. */

    e_memory(std::integral auto req, std::integral auto cap) noexcept
      : request{ static_cast<size_t>(req) }
      , capacity{ static_cast<size_t>(cap) }
    {}
};

struct e_json {
    e_json() noexcept = default;

    e_json(long long unsigned int offset_, std::string_view str) noexcept
      : offset(offset_)
    {
        const auto len =
          str.size() > sizeof error_code ? sizeof error_code : str.size();

        std::uninitialized_copy_n(str.data(), len, error_code);
    }

    long long unsigned int offset;
    char                   error_code[24];
};

struct e_ulong_id {
    long long unsigned int value;
};

struct e_2_ulong_id {
    long long unsigned int value[2];
};

struct e_3_ulong_id {
    long long unsigned int value[3];
};

/**
 * @brief a readability function for returning successful results;
 *
 * For functions that return `status`, rather than returning `{}` to default
 * initialize the status object as "success", use this function to make it more
 * clear to the reader.
 *
 * EXAMPLE:
 *
 *     irt::status some_function() {
 *        return irt::success();
 *     }
 *
 * @return status - that is always successful
 */
inline status success()
{
    // Default initialize the status object using the brace initialization,
    // which will set the status to the default "success" state.
    status successful_status{};
    return successful_status;
}

template<class TryBlock, class... H>
[[nodiscard]] constexpr auto attempt(TryBlock&& p_try_block, H&&... p_handlers)
{
    return boost::leaf::try_handle_some(p_try_block, p_handlers...);
}

template<class TryBlock, class... H>
[[nodiscard]] constexpr auto attempt_all(TryBlock&& p_try_block,
                                         H&&... p_handlers)
{
    return boost::leaf::try_handle_all(p_try_block, p_handlers...);
}

template<class... Item>
[[nodiscard]] constexpr auto on_error(Item&&... p_item)
{
    return boost::leaf::on_error(std::forward<Item>(p_item)...);
}

template<class... Item>
[[nodiscard]] inline auto new_error(Item&&... p_item)
{
    if (on_error_callback) {
        on_error_callback();
    }

    return boost::leaf::new_error(std::forward<Item>(p_item)...);
}

} // namespace irt

#endif
