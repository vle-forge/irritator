// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_ERROR_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_ERROR_HPP

#include <functional>
#include <irritator/macros.hpp>

#include <memory>
#include <string_view>
#include <utility>

namespace irt {

/**
 * Signature of a function called during a @a new_error call.
 */
using error_handler = void(void) noexcept;

/**
 * A global function used by the @a new_error function to enhance debug. Default
 * is to use the irt::debug::breakpoint function in debug mode and empty in
 * release mode.
 */
inline error_handler* on_error_callback = nullptr;

/**
 * Defines all part of the irritator project.
 */
enum class category : std::int16_t {
    generic, /**< Equivalent to std::generic_error. */
    system,  /**< Equivalent to std::system_error. */
    stream,  /**< Equivalent to std::stream_error. */
    future,  /**< Equivalent to std::future_error. */

    fs,
    file,

    json,

    modeling,

    tree_node,
    grid_observer,
    graph_observer,
    variable_observer,
    file_observers,

    project,
    external_source,

    simulation,
    hsm,

    timeline,
};

enum class timeline_errc : std::int16_t {
    memory_error = 1,
    apply_change_error,

};

enum class file_errc : std::int16_t {
    memory_error = 1, /**< Fail to allocate memory. */
    eof_error,        /**< End of file reach too quickly. */
    open_error,       /**< Fail to open the file with common API. */
    empty,            /**< The file can not be empty. */
};

enum class fs_errc : std::int16_t {
    user_directory_access_fail = 1,
    user_file_access_error,
    executable_access_fail,
    user_component_directory_access_fail,
};

enum class simulation_errc : std::int16_t {
    messages = 1,
    nodes,
    dated_messages,
    models,
    hsms,

    observers,
    observers_container_full,

    scheduler,
    external_sources,
    ta_abnormal,

    models_container_full,

    emitting_output_ports_full,
    hsm_unknown,
    connection_incompatible,
    connection_already_exists,
    connection_container_full,
    messages_container_full,
    input_port_error,
    output_port_error,
    dated_messages_container_full,

    embedded_simulation_source_error,
    embedded_simulation_initialization_error,
    embedded_simulation_search_error,
    embedded_simulation_finalization_error,

    abstract_compare_output_value_error,
    abstract_compare_a_b_value_error,
    abstract_filter_threshold_condition_error,
    abstract_integrator_dq_error,
    abstract_integrator_x_error,
    abstract_multiplier_value_error,
    abstract_power_n_error,
    abstract_sum_value_error,
    abstract_wsum_coeff_error,
    abstract_wsum_value_error,
    abstract_inverse_input_error, // value[0] == 0
    abstract_log_input_error,     // value[0] <= 0

    constant_value_error,
    constant_offset_error,
    generator_ta_initialization_error,
    generator_source_initialization_error,
    hsm_top_state_error,
    hsm_next_state_error,
    queue_ta_error,
    time_func_offset_error,
    time_func_timestep_error,
    time_func_function_error,
};

enum class external_source_errc : std::int16_t {
    memory_error = 1,

    binary_file_unknown,
    binary_file_access_error,
    binary_file_size_error,
    binary_file_eof_error,

    constant_unknown,

    random_unknown,

    text_file_unknown,
    text_file_access_error,
    text_file_size_error,
    text_file_eof_error,
};

enum class project_errc : std::int16_t {
    memory_error = 1,

    empty_project,

    file_access_error,

    import_error,
    component_cache_error,
    component_unknown,
    component_port_x_unknown,
    component_port_y_unknown,
};

enum class json_errc : std::int16_t {
    memory_error = 1,

    invalid_format, /**< The JSON file is invalid. */
    invalid_component_format,
    invalid_project_format,
    arg_error,
    file_error,
    dependency_error,
};

enum class modeling_errc : std::int16_t {
    memory_error = 1,

    recorded_directory_error,
    directory_error,
    file_error,

    component_load_error,
    component_container_full,
    component_input_container_full,
    component_output_container_full,

    dot_buffer_empty,
    dot_memory_insufficient,
    dot_file_unreachable,
    dot_format_illegible,

    graph_input_connection_container_full,
    graph_output_connection_container_full,
    graph_input_connection_already_exists,
    graph_output_connection_already_exists,
    graph_connection_container_full,
    graph_connection_already_exist,
    graph_children_container_full,

    grid_input_connection_container_full,
    grid_output_connection_container_full,
    grid_connection_container_full,
    grid_connection_already_exist,
    grid_children_container_full,

    hsm_input_connection_container_full,
    hsm_output_connection_container_full,
    hsm_connection_container_full,
    hsm_connection_already_exist,
    hsm_children_container_full,

    simulation_container_full,

    generic_input_connection_container_full,
    generic_output_connection_container_full,
    generic_input_connection_container_already_exist,
    generic_output_connection_container_already_exist,
    generic_connection_container_full,
    generic_connection_already_exist,
    generic_connection_compatibility_error,
    generic_children_container_full,
};

/**
 * @a error_code represents a platform-dependent error code value. Each
 * @a error_code object holds an error code value originating from the
 * operating system or some low-level interface and a value to identify the
 * category which corresponds to the said interface. The error code values are
 * not required to be unique across different error categories.
 */
class error_code
{
private:
    std::int16_t m_ec  = 0;
    category     m_cat = category::generic;

public:
    constexpr error_code() noexcept = default;

    template<typename ErrorCodeEnum>
        requires(std::is_enum_v<ErrorCodeEnum>)
    constexpr error_code(ErrorCodeEnum e) noexcept
    {
        if constexpr (std::is_same_v<simulation_errc, ErrorCodeEnum>) {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::simulation;
        } else if constexpr (std::is_same_v<timeline_errc, ErrorCodeEnum>) {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::timeline;
        } else if constexpr (std::is_same_v<project_errc, ErrorCodeEnum>) {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::project;
        } else if constexpr (std::is_same_v<modeling_errc, ErrorCodeEnum>) {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::modeling;
        } else if constexpr (std::is_same_v<json_errc, ErrorCodeEnum>) {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::json;
        } else if constexpr (std::is_same_v<external_source_errc,
                                            ErrorCodeEnum>) {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::external_source;
        } else if constexpr (std::is_same_v<fs_errc, ErrorCodeEnum>) {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::fs;
        } else if constexpr (std::is_same_v<file_errc, ErrorCodeEnum>) {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::file;
        } else {
            m_ec  = static_cast<std::int16_t>(e);
            m_cat = category::generic;
        }
    }

    constexpr error_code(std::int16_t error, category cat) noexcept
      : m_ec(error)
      , m_cat(cat)
    {}

    template<typename ErrorCodeEnum>
        requires(std::is_enum_v<ErrorCodeEnum> and
                 std::is_convertible_v<std::underlying_type_t<ErrorCodeEnum>,
                                       std::int16_t>)
    constexpr inline error_code(ErrorCodeEnum e_, category cat_) noexcept
      : m_ec(static_cast<std::int16_t>(e_))
      , m_cat(cat_)
    {}

    //!< Returns the platform dependent error code value.
    constexpr std::int16_t value() const noexcept { return m_ec; }

    //!< Returns the error category of the error code.
    constexpr enum category cat() const noexcept { return m_cat; }

    //!< Checks if the error code value is valid, i.e. non-zero.
    //!< @return @a false if @a value() == 0, true otherwise.
    constexpr explicit operator bool() const noexcept { return m_ec != 0; }
};

template<typename ErrorCodeEnum>
    requires(std::is_enum_v<ErrorCodeEnum>)
inline error_code new_error(ErrorCodeEnum e) noexcept
{
    if (on_error_callback)
        on_error_callback();

    return error_code(e);
}

constexpr inline error_code new_error(std::integral auto e,
                                      enum category      cat) noexcept
{
    debug::ensure(std::cmp_greater_equal(e, INT16_MIN));
    debug::ensure(std::cmp_less_equal(e, INT16_MAX));

    if (on_error_callback)
        on_error_callback();

    return error_code(static_cast<std::int16_t>(e), cat);
}

template<typename ErrorCodeEnum>
    requires(std::is_enum_v<ErrorCodeEnum>)
inline error_code new_error(ErrorCodeEnum e, enum category cat) noexcept
{
    debug::ensure(std::cmp_less_equal(
      static_cast<std::underlying_type_t<ErrorCodeEnum>>(e), INT16_MAX));
    debug::ensure(std::cmp_greater_equal(
      static_cast<std::underlying_type_t<ErrorCodeEnum>>(e), INT16_MIN));

    if (on_error_callback)
        on_error_callback();

    return error_code(static_cast<std::int16_t>(e), cat);
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
        std::int32_t _;
        Value        val;
        error_code   ec;

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

    [[nodiscard]]
    constexpr bool has_error() const noexcept
    {
        return not has_value();
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
    error_code storage;

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
            std::construct_at(std::addressof(storage), other.storage);
    }

    constexpr expected(expected&& other) noexcept
      : state{ other.state }
    {
        if (state == state_type::error)
            std::construct_at(std::addressof(storage),
                              std::move(other.storage));
    }

    constexpr expected(const error_code& ec) noexcept
      : state{ state_type::error }
    {
        std::construct_at(std::addressof(storage), ec);
    }

    constexpr expected(error_code&& ec) noexcept
      : state{ state_type::error }
    {
        std::construct_at(std::addressof(storage), std::move(ec));
    }

    constexpr this_type& operator=(const this_type& other) noexcept
    {
        state = other.state;

        if (state == state_type::error)
            std::construct_at(std::addressof(storage), other.storage);

        return *this;
    }

    constexpr this_type& operator=(this_type&& other) noexcept
    {
        state = other.state;

        if (state == state_type::error)
            std::construct_at(std::addressof(storage),
                              std::move(other.storage));

        return *this;
    }

    constexpr this_type& operator=(const error_code& ec) noexcept
    {
        state = state_type::error;
        std::construct_at(std::addressof(storage), ec);

        return *this;
    }

    constexpr this_type& operator=(error_code&& ec) noexcept
    {
        state = state_type::error;
        std::construct_at(std::addressof(storage), std::move(ec));

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
    constexpr bool has_error() const noexcept
    {
        return not has_value();
    }

    [[nodiscard]]
    constexpr error_type& error() & noexcept
    {
        debug::ensure(not has_value());
        return storage;
    }

    [[nodiscard]]
    constexpr const error_type& error() const& noexcept
    {
        debug::ensure(not has_value());
        return storage;
    }

    template<typename Fn, typename... Args>
    constexpr auto and_then(Fn&& f, Args&&... args) & noexcept
    {
        if (has_value())
            return std::invoke(std::forward<Fn>(f),
                               std::forward<Args>(args)...);
        else
            return storage;
    }

    template<typename Fn, typename... Args>
    constexpr auto and_then(Fn&& f, Args&&... args) const& noexcept
    {
        if (has_value())
            return std::invoke(std::forward<Fn>(f),
                               std::forward<Args>(args)...);
        else
            return this_type(storage);
    }

    template<typename Fn, typename... Args>
    constexpr auto and_then(Fn&& f, Args&&... args) && noexcept
    {
        if (has_value())
            return std::invoke(std::forward<Fn>(f), args...);
        else
            return this_type(storage);
    }

    template<typename Fn, typename... Args>
    constexpr auto and_then(Fn&& f, Args&&... args) const&& noexcept
    {
        if (has_value())
            return std::invoke(std::forward<Fn>(f), args...);
        else
            return this_type(storage);
    }

    template<typename Fn, typename... Args>
    constexpr auto or_else(Fn&& f, Args&&... args) & noexcept
    {
        if (has_value())
            return *this;
        else
            return std::invoke(std::forward<Fn>(f), storage, args...);
    }

    template<typename Fn, typename... Args>
    constexpr auto or_else(Fn&& f, Args&&... args) const& noexcept
    {
        if (has_value())
            return *this;
        else
            return std::invoke(std::forward<Fn>(f), storage, args...);
    }

    template<typename Fn, typename... Args>
    constexpr auto or_else(Fn&& f, Args&&... args) && noexcept
    {
        if (has_value())
            return *this;
        else
            return std::invoke(
              std::forward<Fn>(f), std::move(storage), args...);
    }

    template<typename Fn, typename... Args>
    constexpr auto or_else(Fn&& f, Args&&... args) const&& noexcept
    {
        if (has_value())
            return *this;
        else
            return std::invoke(
              std::forward<Fn>(f), std::move(storage), args...);
    }

    void swap(this_type& other) noexcept
    {
        using std::swap;

        swap(storage, other.storage);
    }
};

using status = expected<void>;

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
constexpr inline status success() noexcept
{
    // Default initialize the status object using the brace initialization,
    // which will set the status to the default "success" state.
    status successful_status{};
    return successful_status;
}

template<typename T, typename T2>
constexpr bool operator==(const expected<T>&  lhs,
                          const expected<T2>& rhs) noexcept
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
constexpr bool operator==(const expected<T>& lhs, const T2& rhs) noexcept
{
    if (not lhs.has_value())
        return false;

    if (not std::is_void_v<T>)
        return lhs.value() == rhs;

    return false;
}

template<typename T>
constexpr bool operator==(const expected<T>& lhs,
                          const error_code&  rhs) noexcept
{
    if (lhs.has_value())
        return false;

    return lhs.error() == rhs;
}

constexpr bool operator==(const error_code& lhs, const error_code& rhs) noexcept
{
    return lhs.value() == rhs.value() and lhs.cat() == rhs.cat();
}

template<typename T, typename T2>
constexpr bool operator!=(const expected<T>&  lhs,
                          const expected<T2>& rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename T, typename T2>
constexpr bool operator!=(const expected<T>& lhs, const T2& rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename T>
constexpr bool operator!=(const expected<T>& lhs,
                          const error_code&  rhs) noexcept
{
    return !(lhs == rhs);
}

constexpr bool operator!=(const error_code& lhs, const error_code& rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename T>
constexpr void swap(expected<T>& lhs, expected<T>& rhs) noexcept
{
    lhs.swap(rhs);
}

} // namespace irt

#define irt_check(r)                                                           \
    {                                                                          \
        auto&& irt_check_value__ = (r);                                        \
        if (not irt_check_value__)                                             \
            return irt_check_value__.error();                                  \
    }

#endif
