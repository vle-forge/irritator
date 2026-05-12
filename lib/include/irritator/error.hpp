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
    none         = 0,
    memory_error = 1,
    apply_change_error,

};

enum class file_errc : std::int16_t {
    none         = 0,
    memory_error = 1, /**< Fail to allocate memory. */
    eof_error,        /**< End of file reach too quickly. */
    open_error,       /**< Fail to open the file with common API. */
    empty,            /**< The file can not be empty. */
};

enum class fs_errc : std::int16_t {
    none                       = 0,
    user_directory_access_fail = 1,
    user_file_access_error,
    executable_access_fail,
    user_component_directory_access_fail,
};

enum class simulation_errc : std::int16_t {
    none     = 0,
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
    none         = 0,
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
    none         = 0,
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
    none         = 0,
    memory_error = 1,

    invalid_format, /**< The JSON file is invalid. */
    invalid_component_format,
    invalid_project_format,
    arg_error,
    file_error,
    dependency_error,
};

enum class modeling_errc : std::int16_t {
    none         = 0,
    memory_error = 1,

    recorded_directory_error,
    directory_error,
    file_error,

    component_type_mismatch,
    component_not_found,
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

    // Constructeur pour enum class connu
    template<typename ErrorCodeEnum>
        requires(std::is_enum_v<ErrorCodeEnum>)
    constexpr error_code(ErrorCodeEnum e) noexcept
    {
        static_assert(std::is_same_v<ErrorCodeEnum, simulation_errc> ||
                        std::is_same_v<ErrorCodeEnum, timeline_errc> ||
                        std::is_same_v<ErrorCodeEnum, file_errc> ||
                        std::is_same_v<ErrorCodeEnum, fs_errc> ||
                        std::is_same_v<ErrorCodeEnum, json_errc> ||
                        std::is_same_v<ErrorCodeEnum, modeling_errc> ||
                        std::is_same_v<ErrorCodeEnum, external_source_errc> ||
                        std::is_same_v<ErrorCodeEnum, project_errc>,
                      "Unsupported error enum type");
        m_ec = static_cast<std::int16_t>(e);

        if constexpr (std::is_same_v<ErrorCodeEnum, simulation_errc>) {
            m_cat = category::simulation;
        } else if constexpr (std::is_same_v<ErrorCodeEnum, timeline_errc>) {
            m_cat = category::timeline;
        } else if constexpr (std::is_same_v<ErrorCodeEnum, file_errc>) {
            m_cat = category::file;
        } else if constexpr (std::is_same_v<ErrorCodeEnum, fs_errc>) {
            m_cat = category::fs;
        } else if constexpr (std::is_same_v<ErrorCodeEnum, json_errc>) {
            m_cat = category::json;
        } else if constexpr (std::is_same_v<ErrorCodeEnum, modeling_errc>) {
            m_cat = category::modeling;
        } else if constexpr (std::is_same_v<ErrorCodeEnum,
                                            external_source_errc>) {
            m_cat = category::external_source;
        } else if constexpr (std::is_same_v<ErrorCodeEnum, project_errc>) {
            m_cat = category::project;
        } else {
            m_cat = category::generic;
        }

        debug::ensure(static_cast<std::underlying_type_t<ErrorCodeEnum>>(e) !=
                        0 &&
                      "Error code must not be 'none' (0)");
    }

    constexpr error_code(std::int16_t error, category cat) noexcept
      : m_ec(error)
      , m_cat(cat)
    {
        debug::ensure(error != 0);
    }

    constexpr std::int16_t value() const noexcept { return m_ec; }
    constexpr category     cat() const noexcept { return m_cat; }
    constexpr explicit     operator bool() const noexcept { return m_ec != 0; }

    constexpr bool operator==(const error_code& other) const noexcept
    {
        return m_ec == other.m_ec && m_cat == other.m_cat;
    }
};

template<typename ErrorCodeEnum>
    requires(std::is_enum_v<ErrorCodeEnum>)
constexpr error_code make_error(ErrorCodeEnum e) noexcept
{
    if (on_error_callback)
        on_error_callback();

    return error_code(e);
}

inline error_code make_error(std::int16_t e, category cat) noexcept
{
    if (on_error_callback)
        on_error_callback();

    debug::ensure(e != 0);
    return error_code(e, cat);
}

template<typename T>
class expected
{
    static_assert(!std::is_reference_v<T>, "expected<T> cannot be a reference");
    static_assert(!std::is_same_v<T, void>,
                  "Use expected<void> specialization for void");
    static_assert(!std::is_same_v<T, error_code>,
                  "expected<error_code> is not allowed");

public:
    using value_type = T;
    using error_type = error_code;
    using this_type  = expected<T>;

private:
    union storage_type {
        T          val;
        error_code ec;

        constexpr storage_type() noexcept {}
        ~storage_type() noexcept {}
    } storage;

    enum class state_type : std::int8_t { value, error } state;

    constexpr void destroy_value() noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            if (state == state_type::value) {
                std::destroy_at(std::addressof(storage.val));
            }
        }
    }

    constexpr void destroy_error() noexcept
    {
        if (state == state_type::error) {
            std::destroy_at(std::addressof(storage.ec));
        }
    }

public:
    constexpr expected() noexcept
        requires(std::is_default_constructible_v<T>)
      : state(state_type::value)
    {
        std::construct_at(std::addressof(storage.val));
    }

    constexpr ~expected() noexcept
    {
        destroy_value();
        destroy_error();
    }

    constexpr expected(const T& value) noexcept
      : state(state_type::value)
    {
        std::construct_at(std::addressof(storage.val), value);
    }

    constexpr expected(T&& value) noexcept
      : state(state_type::value)
    {
        std::construct_at(std::addressof(storage.val), std::move(value));
    }

    constexpr expected(const error_code& ec) noexcept
      : state(state_type::error)
    {
        std::construct_at(std::addressof(storage.ec), ec);
    }

    constexpr expected(error_code&& ec) noexcept
      : state(state_type::error)
    {
        std::construct_at(std::addressof(storage.ec), std::move(ec));
    }

    template<typename... Args>
    constexpr explicit expected(std::in_place_t, Args&&... args) noexcept
      : state(state_type::value)
    {
        std::construct_at(std::addressof(storage.val),
                          std::forward<Args>(args)...);
    }

    constexpr expected(const expected& other) noexcept
      : state(other.state)
    {
        if (state == state_type::value) {
            std::construct_at(std::addressof(storage.val), other.storage.val);
        } else {
            std::construct_at(std::addressof(storage.ec), other.storage.ec);
        }
    }

    constexpr expected(expected&& other) noexcept
      : state(other.state)
    {
        if (state == state_type::value) {
            std::construct_at(std::addressof(storage.val),
                              std::move(other.storage.val));
        } else {
            std::construct_at(std::addressof(storage.ec),
                              std::move(other.storage.ec));
        }
    }

    constexpr expected& operator=(const expected& other) noexcept
    {
        if (this != &other) {
            destroy_value();
            destroy_error();
            state = other.state;
            if (state == state_type::value) {
                std::construct_at(std::addressof(storage.val),
                                  other.storage.val);
            } else {
                std::construct_at(std::addressof(storage.ec), other.storage.ec);
            }
        }
        return *this;
    }

    constexpr expected& operator=(expected&& other) noexcept
    {
        if (this != &other) {
            destroy_value();
            destroy_error();
            state = other.state;
            if (state == state_type::value) {
                std::construct_at(std::addressof(storage.val),
                                  std::move(other.storage.val));
            } else {
                std::construct_at(std::addressof(storage.ec),
                                  std::move(other.storage.ec));
            }
        }
        return *this;
    }

    constexpr expected& operator=(const T& value) noexcept
    {
        destroy_value();
        destroy_error();
        state = state_type::value;
        std::construct_at(std::addressof(storage.val), value);
        return *this;
    }

    constexpr expected& operator=(T&& value) noexcept
    {
        destroy_value();
        destroy_error();
        state = state_type::value;
        std::construct_at(std::addressof(storage.val), std::move(value));
        return *this;
    }

    constexpr expected& operator=(const error_code& ec) noexcept
    {
        destroy_value();
        destroy_error();
        state = state_type::error;
        std::construct_at(std::addressof(storage.ec), ec);
        return *this;
    }

    constexpr expected& operator=(error_code&& ec) noexcept
    {
        destroy_value();
        destroy_error();
        state = state_type::error;
        std::construct_at(std::addressof(storage.ec), std::move(ec));
        return *this;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return state == state_type::value;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return !has_value();
    }

    [[nodiscard]] constexpr T& value() & noexcept
    {
        debug::ensure(has_value());
        return storage.val;
    }

    [[nodiscard]] constexpr const T& value() const& noexcept
    {
        debug::ensure(has_value());
        return storage.val;
    }

    [[nodiscard]] constexpr T&& value() && noexcept
    {
        debug::ensure(has_value());
        return std::move(storage.val);
    }

    [[nodiscard]] constexpr const T&& value() const&& noexcept
    {
        debug::ensure(has_value());
        return std::move(storage.val);
    }

    [[nodiscard]] constexpr error_type& error() & noexcept
    {
        debug::ensure(has_error());
        return storage.ec;
    }

    [[nodiscard]] constexpr const error_type& error() const& noexcept
    {
        debug::ensure(has_error());
        return storage.ec;
    }

    constexpr void swap(expected& other) noexcept
    {
        if (this != &other) {
            if (state == other.state) {
                if (state == state_type::value) {
                    using std::swap;
                    swap(storage.val, other.storage.val);
                } else {
                    using std::swap;
                    swap(storage.ec, other.storage.ec);
                }
            } else {
                if (state == state_type::value) {
                    other.destroy_error();
                    std::construct_at(std::addressof(other.storage.val),
                                      std::move(storage.val));
                    destroy_value();
                    std::construct_at(std::addressof(storage.ec),
                                      std::move(other.storage.ec));
                    std::swap(state, other.state);
                } else {
                    other.destroy_value();
                    std::construct_at(std::addressof(other.storage.ec),
                                      std::move(storage.ec));
                    destroy_error();
                    std::construct_at(std::addressof(storage.val),
                                      std::move(other.storage.val));
                    std::swap(state, other.state);
                }
            }
        }
    }

    template<typename... Args>
    constexpr T& emplace(Args&&... args) noexcept
    {
        destroy_value();
        destroy_error();
        state = state_type::value;
        std::construct_at(std::addressof(storage.val),
                          std::forward<Args>(args)...);
        return value();
    }

    template<typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) const& noexcept
        requires(std::is_convertible_v<U, T>)
    {
        return has_value() ? value()
                           : static_cast<T>(std::forward<U>(default_value));
    }

    template<typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) && noexcept
        requires(std::is_convertible_v<U, T>)
    {
        return has_value() ? std::move(value())
                           : static_cast<T>(std::forward<U>(default_value));
    }

    constexpr T* operator->() noexcept
    {
        debug::ensure(has_value());
        return std::addressof(storage.val);
    }

    constexpr const T* operator->() const noexcept
    {
        debug::ensure(has_value());
        return std::addressof(storage.val);
    }

    constexpr T& operator*() noexcept
    {
        debug::ensure(has_value());
        return storage.val;
    }

    constexpr const T& operator*() const noexcept
    {
        debug::ensure(has_value());
        return storage.val;
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f, Args&&... args) & noexcept
    {
        if (has_value()) {
            return std::invoke(
              std::forward<Fn>(f), storage.val, std::forward<Args>(args)...);
        } else {
            return this_type(storage.ec);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f,
                                          Args&&... args) const& noexcept
    {
        if (has_value()) {
            return std::invoke(
              std::forward<Fn>(f), storage.val, std::forward<Args>(args)...);
        } else {
            return this_type(storage.ec);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f, Args&&... args) && noexcept
    {
        if (has_value()) {
            return std::invoke(std::forward<Fn>(f),
                               std::move(storage.val),
                               std::forward<Args>(args)...);
        } else {
            return this_type(std::move(storage.ec));
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f,
                                          Args&&... args) const&& noexcept
    {
        if (has_value()) {
            return std::invoke(std::forward<Fn>(f),
                               std::move(storage.val),
                               std::forward<Args>(args)...);
        } else {
            return this_type(std::move(storage.ec));
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) & noexcept
    {
        if (has_value()) {
            return this_type(storage.val);
        } else {
            return std::invoke(
              std::forward<Fn>(f), storage.ec, std::forward<Args>(args)...);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) const& noexcept
    {
        if (has_value()) {
            return this_type(storage.val);
        } else {
            return std::invoke(
              std::forward<Fn>(f), storage.ec, std::forward<Args>(args)...);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) && noexcept
    {
        if (has_value()) {
            return this_type(std::move(storage.val));
        } else {
            return std::invoke(std::forward<Fn>(f),
                               std::move(storage.ec),
                               std::forward<Args>(args)...);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f,
                                         Args&&... args) const&& noexcept
    {
        if (has_value()) {
            return this_type(std::move(storage.val));
        } else {
            return std::invoke(std::forward<Fn>(f),
                               std::move(storage.ec),
                               std::forward<Args>(args)...);
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) & noexcept
    {
        if (has_value()) {
            return this_type(std::invoke(std::forward<Fn>(f), storage.val));
        } else {
            return this_type(storage.ec);
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) const& noexcept
    {
        if (has_value()) {
            return this_type(std::invoke(std::forward<Fn>(f), storage.val));
        } else {
            return this_type(storage.ec);
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) && noexcept
    {
        if (has_value()) {
            return this_type(
              std::invoke(std::forward<Fn>(f), std::move(storage.val)));
        } else {
            return this_type(std::move(storage.ec));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) const&& noexcept
    {
        if (has_value()) {
            return this_type(
              std::invoke(std::forward<Fn>(f), std::move(storage.val)));
        } else {
            return this_type(std::move(storage.ec));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) & noexcept
    {
        if (has_value()) {
            return this_type(std::invoke(std::forward<Fn>(f), storage.val));
        } else {
            return this_type(storage.ec);
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) const& noexcept
    {
        if (has_value()) {
            return this_type(std::invoke(std::forward<Fn>(f), storage.val));
        } else {
            return this_type(storage.ec);
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) && noexcept
    {
        if (has_value()) {
            return this_type(
              std::invoke(std::forward<Fn>(f), std::move(storage.val)));
        } else {
            return this_type(std::move(storage.ec));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) const&& noexcept
    {
        if (has_value()) {
            return this_type(
              std::invoke(std::forward<Fn>(f), std::move(storage.val)));
        } else {
            return this_type(std::move(storage.ec));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) & noexcept
    {
        if (has_value()) {
            return *this;
        } else {
            return this_type(std::invoke(std::forward<Fn>(f), storage.ec));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) const& noexcept
    {
        if (has_value()) {
            return *this;
        } else {
            return this_type(std::invoke(std::forward<Fn>(f), storage.ec));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) && noexcept
    {
        if (has_value()) {
            return std::move(*this);
        } else {
            return this_type(
              std::invoke(std::forward<Fn>(f), std::move(storage.ec)));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) const&& noexcept
    {
        if (has_value()) {
            return std::move(*this);
        } else {
            return this_type(
              std::invoke(std::forward<Fn>(f), std::move(storage.ec)));
        }
    }

    template<typename U>
    [[nodiscard]] constexpr T unwrap_or(U&& default_value) const& noexcept
        requires(std::is_convertible_v<U, T>)
    {
        return has_value() ? value()
                           : static_cast<T>(std::forward<U>(default_value));
    }

    template<typename U>
    [[nodiscard]] constexpr T unwrap_or(U&& default_value) && noexcept
        requires(std::is_convertible_v<U, T>)
    {
        return has_value() ? std::move(value())
                           : static_cast<T>(std::forward<U>(default_value));
    }

    template<typename Fn>
    [[nodiscard]] constexpr T unwrap_or_else(Fn&& f) const& noexcept
    {
        return has_value() ? value()
                           : std::invoke(std::forward<Fn>(f), storage.ec);
    }

    template<typename Fn>
    [[nodiscard]] constexpr T unwrap_or_else(Fn&& f) && noexcept
    {
        return has_value()
                 ? std::move(value())
                 : std::invoke(std::forward<Fn>(f), std::move(storage.ec));
    }
};

template<>
class expected<void>
{
public:
    using value_type = void;
    using error_type = error_code;
    using this_type  = expected<void>;

private:
    error_code storage;
    bool       has_error_ = false;

public:
    constexpr expected() noexcept
      : has_error_(false)
    {}

    constexpr ~expected() noexcept = default;

    constexpr expected(const error_code& ec) noexcept
      : storage(ec)
      , has_error_(true)
    {}

    constexpr expected(error_code&& ec) noexcept
      : storage(std::move(ec))
      , has_error_(true)
    {}

    constexpr expected(const expected& other) noexcept
      : storage(other.storage)
      , has_error_(other.has_error_)
    {}

    constexpr expected(expected&& other) noexcept
      : storage(std::move(other.storage))
      , has_error_(other.has_error_)
    {}

    constexpr expected& operator=(const expected& other) noexcept
    {
        if (this != &other) {
            storage    = other.storage;
            has_error_ = other.has_error_;
        }
        return *this;
    }

    constexpr expected& operator=(expected&& other) noexcept
    {
        if (this != &other) {
            storage    = std::move(other.storage);
            has_error_ = other.has_error_;
        }
        return *this;
    }

    constexpr expected& operator=(const error_code& ec) noexcept
    {
        storage    = ec;
        has_error_ = true;
        return *this;
    }

    constexpr expected& operator=(error_code&& ec) noexcept
    {
        storage    = std::move(ec);
        has_error_ = true;
        return *this;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return !has_error_;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return has_error_;
    }

    [[nodiscard]] constexpr error_type& error() & noexcept
    {
        debug::ensure(has_error_);
        return storage;
    }

    [[nodiscard]] constexpr const error_type& error() const& noexcept
    {
        debug::ensure(has_error_);
        return storage;
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f, Args&&... args) & noexcept
    {
        if (has_value()) {
            std::invoke(std::forward<Fn>(f), std::forward<Args>(args)...);
            return expected<void>();
        } else {
            return expected<void>(storage);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f,
                                          Args&&... args) const& noexcept
    {
        if (has_value()) {
            std::invoke(std::forward<Fn>(f), std::forward<Args>(args)...);
            return expected<void>();
        } else {
            return expected<void>(storage);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f, Args&&... args) && noexcept
    {
        if (has_value()) {
            std::invoke(std::forward<Fn>(f), std::forward<Args>(args)...);
            return expected<void>();
        } else {
            return expected<void>(std::move(storage));
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f,
                                          Args&&... args) const&& noexcept
    {
        if (has_value()) {
            std::invoke(std::forward<Fn>(f), std::forward<Args>(args)...);
            return expected<void>();
        } else {
            return expected<void>(std::move(storage));
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) & noexcept
    {
        if (has_value()) {
            return *this;
        } else {
            return std::invoke(
              std::forward<Fn>(f), storage, std::forward<Args>(args)...);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) const& noexcept
    {
        if (has_value()) {
            return *this;
        } else {
            return std::invoke(
              std::forward<Fn>(f), storage, std::forward<Args>(args)...);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) && noexcept
    {
        if (has_value()) {
            return std::move(*this);
        } else {
            return std::invoke(std::forward<Fn>(f),
                               std::move(storage),
                               std::forward<Args>(args)...);
        }
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f,
                                         Args&&... args) const&& noexcept
    {
        if (has_value()) {
            return std::move(*this);
        } else {
            return std::invoke(std::forward<Fn>(f),
                               std::move(storage),
                               std::forward<Args>(args)...);
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) & noexcept
    {
        if (has_value()) {
            return *this;
        } else {
            return this_type(std::invoke(std::forward<Fn>(f), storage));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) const& noexcept
    {
        if (has_value()) {
            return *this;
        } else {
            return this_type(std::invoke(std::forward<Fn>(f), storage));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) && noexcept
    {
        if (has_value()) {
            return std::move(*this);
        } else {
            return this_type(
              std::invoke(std::forward<Fn>(f), std::move(storage)));
        }
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) const&& noexcept
    {
        if (has_value()) {
            return std::move(*this);
        } else {
            return this_type(
              std::invoke(std::forward<Fn>(f), std::move(storage)));
        }
    }

    template<typename Fn>
    constexpr void unwrap_or_else(Fn&& f) const& noexcept
    {
        if (!has_value()) {
            std::invoke(std::forward<Fn>(f), storage);
        }
    }

    template<typename Fn>
    constexpr void unwrap_or_else(Fn&& f) && noexcept
    {
        if (!has_value()) {
            std::invoke(std::forward<Fn>(f), std::move(storage));
        }
    }

    constexpr void swap(expected& other) noexcept
    {
        using std::swap;
        swap(storage, other.storage);
        swap(has_error_, other.has_error_);
    }
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
constexpr inline expected<void> success() noexcept { return expected<void>(); }

template<typename T>
constexpr void swap(expected<T>& lhs, expected<T>& rhs) noexcept
{
    lhs.swap(rhs);
}

template<typename T, typename U>
[[nodiscard]] constexpr bool operator==(const expected<T>& lhs,
                                        const expected<U>& rhs) noexcept
{
    if (lhs.has_value() != rhs.has_value()) {
        return false;
    }
    if (lhs.has_value()) {
        if constexpr (!std::is_void_v<T> && !std::is_void_v<U>) {
            return lhs.value() == rhs.value();
        } else {
            return true;
        }
    } else {
        return lhs.error() == rhs.error();
    }
}

template<typename T, typename U>
[[nodiscard]] constexpr bool operator==(const expected<T>& lhs,
                                        const U&           rhs) noexcept
{
    if (!lhs.has_value()) {
        return false;
    }
    if constexpr (!std::is_void_v<T>) {
        return lhs.value() == rhs;
    } else {
        return false;
    }
}

template<typename T>
[[nodiscard]] constexpr bool operator==(const expected<T>& lhs,
                                        const error_code&  rhs) noexcept
{
    if (lhs.has_value()) {
        return false;
    }
    return lhs.error() == rhs;
}

template<typename T, typename U>
[[nodiscard]] constexpr bool operator!=(const expected<T>& lhs,
                                        const expected<U>& rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename T, typename U>
[[nodiscard]] constexpr bool operator!=(const expected<T>& lhs,
                                        const U&           rhs) noexcept
{
    return !(lhs == rhs);
}

template<typename T>
[[nodiscard]] constexpr bool operator!=(const expected<T>& lhs,
                                        const error_code&  rhs) noexcept
{
    return !(lhs == rhs);
}

[[nodiscard]] constexpr bool operator!=(const error_code& lhs,
                                        const error_code& rhs) noexcept
{
    return !(lhs == rhs);
}

using status = expected<void>;

} // namespace irt

#define irt_check(r)                                                           \
    {                                                                          \
        auto&& irt_check_value__ = (r);                                        \
        if (not irt_check_value__)                                             \
            return irt_check_value__.error();                                  \
    }

#endif
