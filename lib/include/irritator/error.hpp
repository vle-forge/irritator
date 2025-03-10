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

/**
 * @a error_code represents a platform-dependent error code value. Each
 * @a error_code object holds an error code value originating from the
 * operating system or some low-level interface and a value to identify the
 * category which corresponds to the said interface. The error code values are
 * not required to be unique across different error categories.
 */
class error_code
{
public:
    static constexpr std::int16_t generic_error_category =
      0; // errno or std::errc
    static constexpr std::int16_t system_error_category = 1;
    static constexpr std::int16_t stream_error_category = 2;
    static constexpr std::int16_t future_error_category = 3;

private:
    std::int16_t ec  = 0;
    std::int16_t cat = generic_error_category;

public:
    constexpr error_code() noexcept = default;

    constexpr error_code(std::int16_t error, std::int16_t category) noexcept
      : ec(error)
      , cat(category)
    {}

    template<typename ErrorCodeEnum>
        requires(std::is_enum_v<ErrorCodeEnum> and
                 std::is_convertible_v<std::underlying_type_t<ErrorCodeEnum>,
                                       std::int16_t>)
    constexpr inline error_code(ErrorCodeEnum e, std::int16_t category) noexcept
      : ec(static_cast<std::int16_t>(e))
      , cat(category)
    {}

    //!< Returns the platform dependent error code value.
    constexpr std::int16_t value() const noexcept { return ec; }

    //!< Returns the error category of the error code.
    constexpr std::int16_t category() const noexcept { return cat; }

    //!< Checks if the error code value is valid, i.e. non-zero.
    //!< @return @a false if @a value() == 0, true otherwise.
    constexpr explicit operator bool() const noexcept { return ec != 0; }
};

inline error_code new_error_code(
  std::integral auto e,
  std::int16_t       category = error_code::generic_error_category) noexcept
{
    debug::ensure(std::cmp_less_equal(e, INT16_MAX));
    debug::ensure(std::cmp_greater_equal(e, INT16_MIN));

    if (on_error_callback) {
        on_error_callback();
    }

    return error_code(static_cast<std::int16_t>(e), category);
}

template<typename ErrorCodeEnum>
    requires(std::is_enum_v<ErrorCodeEnum>)
inline error_code new_error_code(
  ErrorCodeEnum e,
  std::int16_t  category = error_code::generic_error_category) noexcept
{
    debug::ensure(std::cmp_less_equal(
      static_cast<std::underlying_type_t<ErrorCodeEnum>>(e), INT16_MAX));
    debug::ensure(std::cmp_greater_equal(
      static_cast<std::underlying_type_t<ErrorCodeEnum>>(e), INT16_MIN));

    if (on_error_callback) {
        on_error_callback();
    }

    return error_code(static_cast<std::int16_t>(e), category);
}

template<typename Value>
class expected
{
private:
    static_assert(not std::is_reference_v<Value>);
    static_assert(not std::is_function_v<Value>);
    static_assert(not std::is_same_v<std::remove_cv_t<Value>, std::in_place_t>);
    static_assert(not std::is_same_v<std::remove_cv_t<Value>, error_code>);

public:
    using value_type = Value;
    using error_type = error_code;
    using this_type  = expected<value_type>;

private:
    union storage_type {
        std::monostate _;
        Value          val;
        error_code     ec;

        constexpr storage_type() noexcept {}
        constexpr ~storage_type() noexcept {}
    } storage;

    enum class state_type : std::int8_t { value, error } state;

public:
    constexpr expected() noexcept
      : state{ state_type::value }
    {
        if constexpr (std::is_default_constructible_v<value_type>)
            std::construct_at(std::addressof(storage.val));
    }

    constexpr ~expected() noexcept = default;

    constexpr ~expected() noexcept
        requires(not std::is_trivially_destructible_v<value_type>)
    {
        if (state == state_type::value)
            std::destroy_at(std::addressof(storage.val));
    }

    constexpr expected(const value_type& value_) noexcept
      : state{ state_type::value }
    {
        std::construct_at(std::addressof(storage.val), value_);
    }

    constexpr expected(value_type&& value_) noexcept
      : state{ state_type::value }
    {
        std::construct_at(std::addressof(storage.val), std::move(value_));
    }

    constexpr expected(const expected& other) noexcept
      : state{ other.state }
    {
        static_assert(std::is_copy_constructible_v<value_type>);

        if (state == state_type::error) {
            std::construct_at(std::addressof(storage.ec), other.storage.ec);
        } else {
            std::construct_at(std::addressof(storage.val), other.storage.val);
        }
    }

    constexpr expected(expected&& other) noexcept
      : state{ other.state }
    {
        static_assert(std::is_move_constructible_v<value_type>);

        if (state == state_type::error) {
            std::construct_at(std::addressof(storage.ec),
                              std::move(other.storage.ec));
        } else {
            std::construct_at(std::addressof(storage.val),
                              std::move(other.storage.val));
        }
    }

    constexpr expected(const error_code& ec) noexcept
      : state{ state_type::error }
    {
        std::construct_at(std::addressof(storage.ec), ec);
    }

    constexpr expected(error_code&& ec) noexcept
      : state{ state_type::error }
    {
        std::construct_at(std::addressof(storage.ec), std::move(ec));
    }

    template<typename... Args>
    constexpr explicit expected(std::in_place_t, Args&&... args) noexcept
      : state{ state_type::error }
    {
        std::construct_at(storage.val, std::forward<Args>(args)...);
    }

    constexpr this_type& operator=(const this_type& other) noexcept
    {
        static_assert(std::is_copy_constructible_v<value_type>);

        if constexpr (not std::is_trivially_destructible_v<value_type>) {
            if (state == state_type::value)
                std::destroy(storage.val);
        }

        state = other.state;

        if (state == state_type::error) {
            std::construct_at(std::addressof(storage.ec), other.storage.ec);
        } else {
            std::construct_at(std::addressof(storage.val), other.storage.val);
        }

        return *this;
    }

    constexpr this_type& operator=(this_type&& other) noexcept
    {
        static_assert(std::is_move_constructible_v<value_type>);

        if constexpr (not std::is_trivially_destructible_v<value_type>) {
            if (state == state_type::value)
                std::destroy(storage.val);
        }

        state = other.state;

        if (state == state_type::error) {
            std::construct_at(std::addressof(storage.ec),
                              std::move(other.storage.ec));
        } else {
            std::construct_at(std::addressof(storage.val),
                              std::move(other.storage.val));
        }

        return *this;
    }

    constexpr this_type& operator=(const value_type& value) noexcept
    {
        static_assert(std::is_copy_constructible_v<value_type>,
                      "Value not copy assignable");

        if constexpr (not std::is_trivially_destructible_v<value_type>) {
            if (state == state_type::value)
                std::destroy(storage.val);
        }

        state = state_type::value;
        std::construct_at(std::addressof(storage.val), value);

        return *this;
    }

    constexpr this_type& operator=(value_type&& value) noexcept
    {
        static_assert(std::is_move_constructible_v<Value>,
                      "Value not move assignable");

        if constexpr (not std::is_trivially_destructible_v<value_type>) {
            if (state == state_type::value)
                std::destroy(storage.val);
        }

        state = state_type::value;
        std::construct_at(std::addressof(storage.val), std::move(value));

        return *this;
    }

    constexpr this_type& operator=(const error_code& ec) noexcept
    {
        if constexpr (not std::is_trivially_destructible_v<value_type>) {
            if (state == state_type::value)
                std::destroy(storage.val);
        }

        state = state_type::error;
        std::construct_at(std::addressof(storage.ec), ec);

        return *this;
    }

    constexpr this_type& operator=(error_code&& ec) noexcept
    {
        if constexpr (not std::is_trivially_destructible_v<value_type>) {
            if (state == state_type::value)
                std::destroy(storage.val);
        }

        state = state_type::error;
        std::construct_at(std::addressof(storage.ec), std::move(ec));

        return *this;
    }

    constexpr value_type& value() & noexcept
    {
        debug::ensure(has_value());
        return storage.val;
    }

    constexpr const value_type& value() const& noexcept
    {
        debug::ensure(has_value());
        return storage.val;
    }

    constexpr value_type&& value() && noexcept
    {
        debug::ensure(has_value());
        return std::move(storage.val);
    }

    constexpr const value_type&& value() const&& noexcept
    {
        debug::ensure(has_value());
        return std::move(storage.val);
    }

    [[nodiscard]]
    constexpr bool has_value() const noexcept
    {
        return state_type::value == state;
    }

    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    template<typename U>
        requires(std::is_convertible_v<U, value_type>)
    [[nodiscard]]
    constexpr auto value_or(U&& default_value) const& noexcept
    {
        if (has_value()) {
            return value();
        } else {
            return static_cast<value_type>(std::forward<U>(default_value));
        }
    }

    template<typename U>
        requires(not std::is_void_v<value_type> and
                 std::is_convertible_v<U, value_type>)
    [[nodiscard]]
    constexpr auto value_or(U&& default_value) && noexcept
    {
        if (has_value()) {
            return std::move(storage.val);
        } else {
            return static_cast<value_type>(std::forward<U>(default_value));
        }
    }

    [[nodiscard]]
    constexpr error_type& error() & noexcept
    {
        debug::ensure(not has_value());
        return storage.ec;
    }

    [[nodiscard]]
    constexpr const error_type& error() const& noexcept
    {
        debug::ensure(not has_value());
        return storage.ec;
    }

    void swap(this_type& other) noexcept
    {
        using std::swap;

        swap(storage, other.storage);
    }

    template<typename... Args>
    constexpr value_type& emplace(Args&&... args) noexcept
    {
        if constexpr (not std::is_trivially_destructible_v<value_type>) {
            if (state == state_type::value)
                std::destroy(storage.val);
        }

        std::construct_at(std::addressof(storage.val),
                          std::forward<Args>(args)...);

        return value();
    }

    template<typename U>
    value_type value_or(const U& default_value) const noexcept
    {
        if (has_value()) {
            return value();
        } else {
            return default_value;
        }
    }

    value_type* operator->() noexcept
    {
        debug::ensure(has_value());

        return std::addressof(storage.val);
    }

    const value_type* operator->() const noexcept
    {
        debug::ensure(has_value());

        return std::addressof(storage.val);
    }

    value_type& operator*() noexcept
    {
        debug::ensure(has_value());

        return storage.val;
    }

    const value_type& operator*() const noexcept
    {
        debug::ensure(has_value());

        return storage.val;
    }

    template<typename Fn, typename... Args>
    constexpr auto and_then(Fn&& f, Args&&... args) & noexcept
    {
        if (has_value())
            return std::invoke(
              std::forward<Fn>(f), storage.val, std::forward<Args>(args)...);
        else
            return storage.ec;
    }

    template<typename Fn, typename... Args>
    constexpr auto and_then(Fn&& f, Args&&... args) const& noexcept
    {
        if (has_value())
            return std::invoke(
              std::forward<Fn>(f), storage.val, std::forward<Args>(args)...);
        else
            return this_type(storage.ec);
    }

    template<typename Fn, typename... Args>
    constexpr auto and_then(Fn&& f, Args&&... args) && noexcept
    {
        if (has_value())
            return std::invoke(
              std::forward<Fn>(f), std::move(storage.val), args...);
        else
            return this_type(storage.ec);
    }

    template<typename Fn, typename... Args>
    constexpr auto and_then(Fn&& f, Args&&... args) const&& noexcept
    {
        if (has_value())
            return std::invoke(
              std::forward<Fn>(f), std::move(storage.val), args...);
        else
            return this_type(storage.ec);
    }

    template<typename Fn, typename... Args>
    constexpr auto or_else(Fn&& f, Args&&... args) & noexcept
    {
        if (has_value())
            return this_type(storage.val);
        else
            return std::invoke(std::forward<Fn>(f), storage.ec, args...);
    }

    template<typename Fn, typename... Args>
    constexpr auto or_else(Fn&& f, Args&&... args) const& noexcept
    {
        if (has_value())
            return this_type(storage.val);
        else
            return std::invoke(std::forward<Fn>(f), storage.ec, args...);
    }

    template<typename Fn, typename... Args>
    constexpr auto or_else(Fn&& f, Args&&... args) && noexcept
    {
        if (has_value())
            return this_type(std::move(storage.val));
        else
            return std::invoke(
              std::forward<Fn>(f), std::move(storage.ec), args...);
    }

    template<typename Fn, typename... Args>
    constexpr auto or_else(Fn&& f, Args&&... args) const&& noexcept
    {
        if (has_value())
            return this_type(std::move(storage.val));
        else
            return std::invoke(
              std::forward<Fn>(f), std::move(storage.ec), args...);
    }
};

template<>
class expected<void>
{
public:
    using value_type = void;
    using error_type = error_code;
    using this_type  = expected<value_type>;

private:
    union storage_type {
        std::monostate _;
        error_code     ec;

        constexpr storage_type() noexcept {}
        constexpr ~storage_type() noexcept {}
    } storage;

    enum class state_type : std::int8_t { value, error } state;

public:
    constexpr expected() noexcept
      : state{ state_type::value }
    {}

    constexpr ~expected() noexcept = default;

    constexpr expected(const expected& other) noexcept
      : state{ other.state }
    {
        if (state == state_type::error)
            std::construct_at(std::addressof(storage.ec), other.storage.ec);
    }

    constexpr expected(expected&& other) noexcept
      : state{ other.state }
    {
        if (state == state_type::error)
            std::construct_at(std::addressof(storage.ec),
                              std::move(other.storage.ec));
    }

    constexpr expected(const error_code& ec) noexcept
      : state{ state_type::error }
    {
        std::construct_at(std::addressof(storage.ec), ec);
    }

    constexpr expected(error_code&& ec) noexcept
      : state{ state_type::error }
    {
        std::construct_at(std::addressof(storage.ec), std::move(ec));
    }

    constexpr this_type& operator=(const this_type& other) noexcept
    {
        state = other.state;

        if (state == state_type::error)
            std::construct_at(std::addressof(storage.ec), other.storage.ec);

        return *this;
    }

    constexpr this_type& operator=(this_type&& other) noexcept
    {
        state = other.state;

        if (state == state_type::error)
            std::construct_at(std::addressof(storage.ec),
                              std::move(other.storage.ec));

        return *this;
    }

    constexpr this_type& operator=(const error_code& ec) noexcept
    {
        state = state_type::error;
        std::construct_at(std::addressof(storage.ec), ec);

        return *this;
    }

    constexpr this_type& operator=(error_code&& ec) noexcept
    {
        state = state_type::error;
        std::construct_at(std::addressof(storage.ec), std::move(ec));

        return *this;
    }

    [[nodiscard]]
    constexpr bool has_value() const noexcept
    {
        return state_type::value == state;
    }

    [[nodiscard]]
    constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]]
    constexpr error_type& error() & noexcept
    {
        debug::ensure(not has_value());
        return storage.ec;
    }

    [[nodiscard]]
    constexpr const error_type& error() const& noexcept
    {
        debug::ensure(not has_value());
        return storage.ec;
    }

    void swap(this_type& other) noexcept
    {
        using std::swap;

        swap(storage, other.storage);
    }
};

template<typename T, typename T2>
constexpr bool operator==(const expected<T>& lhs, const expected<T2>& rhs)
{
    if (lhs.has_value() != rhs.has_value())
        return false;

    if (not std::is_void_v<T> and not std::is_void_v<T2>) {
        if (lhs.has_value())
            return lhs.value() == rhs.value();
    }

    return lhs.error() == rhs.error();
}

template<typename T, typename T2>
constexpr bool operator==(const expected<T>& lhs, const T2& rhs)
{
    if (not lhs.has_value())
        return false;

    if (not std::is_void_v<T>)
        return lhs.value() == rhs;

    return false;
}

template<typename T>
constexpr bool operator==(const expected<T>& lhs, const error_code& rhs)
{
    if (lhs.has_value())
        return false;

    return lhs.error() == rhs;
}

constexpr bool operator==(const error_code& lhs, const error_code& rhs)
{
    return lhs.value() == rhs.value() and lhs.category() == rhs.category();
}

template<typename T, typename T2>
constexpr bool operator!=(const expected<T>& lhs, const expected<T2>& rhs)
{
    return !(lhs == rhs);
}

template<typename T, typename T2>
constexpr bool operator!=(const expected<T>& lhs, const T2& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
constexpr bool operator!=(const expected<T>& lhs, const error_code& rhs)
{
    return !(lhs == rhs);
}

constexpr bool operator!=(const error_code& lhs, const error_code& rhs)
{
    return !(lhs == rhs);
}

template<typename T>
constexpr void swap(expected<T>& lhs, expected<T>& rhs)
{
    lhs.swap(rhs);
}

} // namespace irt

#endif
