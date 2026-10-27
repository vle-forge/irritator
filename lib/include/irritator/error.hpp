// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_ERROR_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_ERROR_HPP

#include <irritator/macros.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>

namespace irt {

/**
 * Signature of a function called during a @a make_error call.
 */
using error_handler = void(void) noexcept;

/**
 * A global function used by the @a make_error function to enhance debug.
 * Default is to use the irt::debug::breakpoint function in debug mode and empty
 * in release mode.
 *
 * @attention This is a plain (non atomic) global pointer: set it once, before
 * any thread that can call @a make_error is started.
 */
inline error_handler* on_error_callback = nullptr;

/**
 * Defines all part of the irritator project.
 */
enum class category : std::int16_t {
    generic, /**< Equivalent to std::generic_error. */
    modeling,
    project,
    simulation,
};

enum class simulation_errc : std::int16_t {
    none = 0,

    memory_error, /**< ENOMEM error. No more memory available. */
    range_error,  /**< ERANGE error, A value is not within the range of allowed
                     values. */

    file_access_error, /**< Build from registered, directory and file name
                          error. */
    file_eof_error,    /**< End of file reach too quickly. */
    file_open_error,   /**< Fail to open the file with common API. */
    file_empty,        /**< The file can not be empty. */

    user_directory_access_fail,
    user_file_access_error,
    executable_access_fail,
    user_component_directory_access_fail,

    external_source_binary_file_unknown,
    external_source_binary_file_access_error,
    external_source_binary_file_size_error,
    external_source_binary_file_eof_error,
    external_source_constant_unknown,
    external_source_constant_size_error,
    external_source_random_unknown,
    external_source_text_file_unknown,
    external_source_text_file_access_error,
    external_source_text_file_size_error,
    external_source_text_file_eof_error,

    observers_container_full,

    scheduler_container_full,

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

    abstract_log_input_error, // value[0] <= 0

    constant_value_error,
    constant_offset_error,

    generator_ta_initialization_error,
    generator_source_initialization_error,

    hsm_top_state_error,
    hsm_next_state_error,

    queue_ta_error,
    queue_fifo_error,

    time_func_offset_error,
    time_func_timestep_error,
    time_func_function_error,

    simulation_wrapper_not_enough_memory,
    simulation_wrapper_source_error,
    simulation_wrapper_too_many_embedded_simulation_error,
    simulation_wrapper_parameter_error,
    simulation_wrapper_input_error,
    simulation_wrapper_output_error,

    simulation_wrapper_embedded_simulation_source_error,
    simulation_wrapper_embedded_simulation_initialization_error,
    simulation_wrapper_embedded_simulation_search_error,
    simulation_wrapper_embedded_simulation_observation_error,
    simulation_wrapper_embedded_simulation_finalization_error,
};

enum class project_errc : std::int16_t {
    none = 0,

    memory_error,
    range_error,

    empty_project,

    file_access_error, /**< Build from registered, directory and file name
                          error. */
    file_eof_error,    /**< End of file reach too quickly. */
    file_open_error,   /**< Fail to open the file with common API. */
    file_empty,        /**< The file can not be empty. */

    json_invalid_format, /**< The JSON file is invalid. */
    json_invalid_component_format,
    json_invalid_project_format,
    json_dependency_error,

    import_error,
    component_cache_error,
    component_unknown,
    component_port_x_unknown,
    component_port_y_unknown,
};

enum class modeling_errc : std::int16_t {
    none = 0,
    memory_error,

    recorded_directory_error,
    directory_error,
    file_error,

    file_access_error, /**< Build from registered, directory and file name
       error. */
    file_eof_error,    /**< End of file reach too quickly. */
    file_open_error,   /**< Fail to open the file with common API. */
    file_empty,        /**< The file can not be empty. */

    json_invalid_format, /**< The JSON file is invalid. */
    json_invalid_component_format,
    json_invalid_project_format,
    json_dependency_error,

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
 * Satisfied by the enumerations that can be stored into an @a error_code.
 */
template<typename E>
concept error_enum = std::same_as<E, simulation_errc> ||
                     std::same_as<E, modeling_errc> ||
                     std::same_as<E, project_errc>;

namespace detail {

template<error_enum E>
inline constexpr category category_of_v = std::same_as<E, simulation_errc>
                                            ? category::simulation
                                          : std::same_as<E, modeling_errc>
                                            ? category::modeling
                                            : category::project;

} // namespace detail

/**
 * @a error_code represents a platform-dependent error code value. Each
 * @a error_code object holds an error code value originating from the
 * operating system or some low-level interface and a value to identify the
 * category which corresponds to the said interface. The error code values are
 * not required to be unique across different error categories.
 *
 * A default constructed @a error_code (value 0) means "no error". All the other
 * constructors reject the value 0.
 */
class error_code
{
private:
    std::int16_t m_ec  = 0;
    category     m_cat = category::generic;

public:
    constexpr error_code() noexcept = default;

    template<error_enum ErrorCodeEnum>
    constexpr error_code(ErrorCodeEnum e) noexcept
      : m_ec(static_cast<std::int16_t>(e))
      , m_cat(detail::category_of_v<ErrorCodeEnum>)
    {
        debug::ensure(m_ec != 0 && "Error code must not be 'none' (0)");
    }

    constexpr error_code(std::int16_t error, category cat) noexcept
      : m_ec(error)
      , m_cat(cat)
    {
        debug::ensure(error != 0);
    }

    constexpr error_code(std::errc error) noexcept
      : m_ec{ static_cast<std::int16_t>(error) }
      , m_cat{ category::generic }
    {
        debug::ensure(static_cast<int>(error) != 0);
    }

    constexpr std::int16_t value() const noexcept { return m_ec; }
    constexpr category     cat() const noexcept { return m_cat; }
    constexpr explicit     operator bool() const noexcept { return m_ec != 0; }

    // C++20 synthesizes operator!= from this one.
    constexpr bool operator==(const error_code& other) const noexcept = default;
};

template<error_enum ErrorCodeEnum>
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

inline error_code make_error(std::errc e) noexcept
{
    if (on_error_callback)
        on_error_callback();

    return error_code(e);
}

/**
 * Name of a category: "generic", "modeling", "project" or "simulation".
 */
[[nodiscard]] constexpr std::string_view category_name(category cat) noexcept
{
    switch (cat) {
    case category::generic:
        return "generic";
    case category::modeling:
        return "modeling";
    case category::project:
        return "project";
    case category::simulation:
        return "simulation";
    }

    return "unknown";
}

template<typename T>
class expected;

namespace detail {

template<typename T>
struct is_expected : std::false_type {};

template<typename T>
struct is_expected<expected<T>> : std::true_type {};

template<typename T>
inline constexpr bool
  is_expected_v = is_expected<std::remove_cvref_t<T>>::value;

} // namespace detail

namespace detail {

enum class expected_state : std::int8_t { value, error };

/**
 * Storage of an expected<T>. The union is specialized instead of being given a
 * conditionally defaulted destructor (P0848), which is not reliably supported
 * (MSVC): when T is trivially destructible the union has no destructor.
 */
template<typename T, bool = std::is_trivially_destructible_v<T>>
union expected_union {
    T          val;
    error_code ec;

    constexpr expected_union() noexcept {}
};

template<typename T>
union expected_union<T, false> {
    T          val;
    error_code ec;

    constexpr expected_union() noexcept {}
    constexpr ~expected_union() {}
};

template<typename T>
struct expected_base {
    expected_union<T> storage;
    expected_state    state;

    constexpr explicit expected_base(expected_state s) noexcept
      : state(s)
    {}

    constexpr void destroy_value() noexcept
    {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            if (state == expected_state::value)
                std::destroy_at(std::addressof(storage.val));
        }
    }

    // @a ec is taken by value on purpose: it may refer to our own error.
    constexpr void set_error(error_code ec) noexcept
    {
        debug::ensure(static_cast<bool>(ec));
        destroy_value();
        std::construct_at(std::addressof(storage.ec), ec);
        state = expected_state::error;
    }
};

/**
 * Destruction, copy and move of an expected<T>, chosen by partial
 * specialization (no conditionally trivial special members, see above).
 *
 * T trivially copyable: nothing is declared, the compiler generated special
 * members are trivial, so is expected<T> (it can be returned in registers).
 */
template<typename T, bool = std::is_trivially_copyable_v<T>>
struct expected_ops : expected_base<T> {
    using expected_base<T>::expected_base;
};

/**
 * Otherwise: the copy and move operations exist only when T provides them and
 * are noexcept only when T's operation is.
 */
template<typename T>
struct expected_ops<T, false> : expected_base<T> {
    using expected_base<T>::expected_base;

    constexpr ~expected_ops() { this->destroy_value(); }

    constexpr expected_ops(const expected_ops& other) noexcept(
      std::is_nothrow_copy_constructible_v<T>)
        requires(std::is_copy_constructible_v<T>)
      : expected_base<T>(other.state)
    {
        if (this->state == expected_state::value)
            std::construct_at(std::addressof(this->storage.val),
                              other.storage.val);
        else
            std::construct_at(std::addressof(this->storage.ec),
                              other.storage.ec);
    }

    constexpr expected_ops(expected_ops&& other) noexcept(
      std::is_nothrow_move_constructible_v<T>)
        requires(std::is_move_constructible_v<T>)
      : expected_base<T>(other.state)
    {
        if (this->state == expected_state::value)
            std::construct_at(std::addressof(this->storage.val),
                              std::move(other.storage.val));
        else
            std::construct_at(std::addressof(this->storage.ec),
                              other.storage.ec);
    }

    constexpr expected_ops& operator=(const expected_ops& other) noexcept(
      std::is_nothrow_copy_constructible_v<T> &&
      std::is_nothrow_copy_assignable_v<T>)
        requires(std::is_copy_constructible_v<T> &&
                 std::is_copy_assignable_v<T>)
    {
        if (this == &other)
            return *this;

        if (this->state == expected_state::value &&
            other.state == expected_state::value) {
            this->storage.val = other.storage.val;
        } else if (other.state == expected_state::value) {
            // Here we hold an error: error_code is trivial, nothing to destroy.
            std::construct_at(std::addressof(this->storage.val),
                              other.storage.val);
            this->state = expected_state::value;
        } else {
            this->set_error(other.storage.ec);
        }

        return *this;
    }

    constexpr expected_ops& operator=(expected_ops&& other) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_assignable_v<T>)
        requires(std::is_move_constructible_v<T> &&
                 std::is_move_assignable_v<T>)
    {
        if (this == &other)
            return *this;

        if (this->state == expected_state::value &&
            other.state == expected_state::value) {
            this->storage.val = std::move(other.storage.val);
        } else if (other.state == expected_state::value) {
            std::construct_at(std::addressof(this->storage.val),
                              std::move(other.storage.val));
            this->state = expected_state::value;
        } else {
            this->set_error(other.storage.ec);
        }

        return *this;
    }
};

} // namespace detail

/**
 * A value of type @a T or an @a error_code.
 *
 * Irritator does not use exceptions. Consequently, the monadic functions
 * (@a and_then, @a or_else, @a transform, @a map, @a map_error,
 * @a unwrap_or_else) are `noexcept` and expect callbacks that do not throw
 * (an exception leaving a callback calls std::terminate). The special member
 * functions are available only when @a T provides them and are `noexcept` only
 * when the corresponding operation of @a T is.
 *
 * The error state never holds the "no error" code (0): it is checked with
 * debug::ensure.
 *
 * When @a T is trivially copyable, so is expected<T>, which lets the ABI return
 * it in registers (see detail::expected_ops). The special members are not
 * declared here: they come from detail::expected_ops.
 */
template<typename T>
class [[nodiscard]] expected : private detail::expected_ops<T>
{
    static_assert(!std::is_reference_v<T>, "expected<T> cannot be a reference");
    static_assert(!std::is_void_v<T>,
                  "Use expected<void> specialization for void");
    static_assert(!std::is_same_v<std::remove_cv_t<T>, error_code>,
                  "expected<error_code> is not allowed");

public:
    using value_type = T;
    using error_type = error_code;
    using this_type  = expected<T>;

private:
    using ops_type   = detail::expected_ops<T>;
    using state_type = detail::expected_state;

    using ops_type::destroy_value;
    using ops_type::set_error;
    using ops_type::state;
    using ops_type::storage;

    // Accessors used by the monadic implementations to preserve the
    // const/reference qualification of the expected they are applied on.

    template<typename Self>
    [[nodiscard]] static constexpr auto&& forward_value(Self&& self) noexcept
    {
        return std::forward<Self>(self).storage.val;
    }

    template<typename Self>
    [[nodiscard]] static constexpr auto&& forward_error(Self&& self) noexcept
    {
        return std::forward<Self>(self).storage.ec;
    }

    template<typename Self, typename Fn, typename... Args>
    static constexpr auto and_then_impl(Self&& self,
                                        Fn&&   f,
                                        Args&&... args) noexcept
    {
        using R = std::remove_cvref_t<std::invoke_result_t<
          Fn, decltype(forward_value(std::forward<Self>(self))), Args...>>;
        static_assert(detail::is_expected_v<R>,
                      "and_then: the callback must return an irt::expected");

        if (self.has_value())
            return std::invoke(std::forward<Fn>(f),
                               forward_value(std::forward<Self>(self)),
                               std::forward<Args>(args)...);

        return R(self.storage.ec);
    }

    template<typename Self, typename Fn, typename... Args>
    static constexpr auto or_else_impl(Self&& self,
                                       Fn&&   f,
                                       Args&&... args) noexcept
    {
        using R = std::remove_cvref_t<std::invoke_result_t<
          Fn, decltype(forward_error(std::forward<Self>(self))), Args...>>;
        static_assert(std::is_same_v<R, this_type>,
                      "or_else: the callback must return the same expected");

        if (self.has_value())
            return this_type(forward_value(std::forward<Self>(self)));

        return std::invoke(std::forward<Fn>(f),
                           forward_error(std::forward<Self>(self)),
                           std::forward<Args>(args)...);
    }

    template<typename Self, typename Fn>
    static constexpr auto transform_impl(Self&& self, Fn&& f) noexcept
    {
        using U = std::remove_cvref_t<std::invoke_result_t<
          Fn, decltype(forward_value(std::forward<Self>(self)))>>;

        if constexpr (std::is_void_v<U>) {
            // Written expected<U> (U is void here) and not expected<void>: a
            // non dependent name would force the implicit instantiation of the
            // primary template with void, before its specialization below.
            if (self.has_value()) {
                std::invoke(std::forward<Fn>(f),
                            forward_value(std::forward<Self>(self)));
                return expected<U>();
            }
            return expected<U>(self.storage.ec);
        } else {
            if (self.has_value())
                return expected<U>(
                  std::in_place,
                  std::invoke(std::forward<Fn>(f),
                              forward_value(std::forward<Self>(self))));
            return expected<U>(self.storage.ec);
        }
    }

    template<typename Self, typename Fn>
    static constexpr auto map_error_impl(Self&& self, Fn&& f) noexcept
    {
        using R = std::invoke_result_t<Fn, decltype(forward_error(
                                             std::forward<Self>(self)))>;
        static_assert(std::is_convertible_v<R, error_code>,
                      "map_error: the callback must return an error_code");

        if (self.has_value())
            return this_type(forward_value(std::forward<Self>(self)));

        return this_type(error_code(std::invoke(
          std::forward<Fn>(f), forward_error(std::forward<Self>(self)))));
    }

public:
    constexpr expected() noexcept(std::is_nothrow_default_constructible_v<T>)
        requires(std::is_default_constructible_v<T>)
      : ops_type(state_type::value)
    {
        std::construct_at(std::addressof(storage.val));
    }

    constexpr expected(const T& v) noexcept(
      std::is_nothrow_copy_constructible_v<T>)
        requires(std::is_copy_constructible_v<T>)
      : ops_type(state_type::value)
    {
        std::construct_at(std::addressof(storage.val), v);
    }

    constexpr expected(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>)
        requires(std::is_move_constructible_v<T>)
      : ops_type(state_type::value)
    {
        std::construct_at(std::addressof(storage.val), std::move(v));
    }

    constexpr expected(error_code ec) noexcept
      : ops_type(state_type::error)
    {
        debug::ensure(static_cast<bool>(ec));
        std::construct_at(std::addressof(storage.ec), ec);
    }

    template<typename... Args>
        requires(std::is_constructible_v<T, Args...>)
    constexpr explicit expected(std::in_place_t, Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...>)
      : ops_type(state_type::value)
    {
        std::construct_at(std::addressof(storage.val),
                          std::forward<Args>(args)...);
    }

    // Assignment from a value: when we already hold a value, T::operator= is
    // used (no destroy-then-construct, so assigning from our own value is as
    // safe as T::operator= is, and the capacity of containers is preserved).

    constexpr expected& operator=(const T& v) noexcept(
      std::is_nothrow_copy_constructible_v<T> &&
      std::is_nothrow_copy_assignable_v<T>)
        requires(std::is_copy_constructible_v<T> &&
                 std::is_copy_assignable_v<T>)
    {
        if (state == state_type::value) {
            storage.val = v;
        } else {
            std::construct_at(std::addressof(storage.val), v);
            state = state_type::value;
        }

        return *this;
    }

    constexpr expected& operator=(T&& v) noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_move_assignable_v<T>)
        requires(std::is_move_constructible_v<T> &&
                 std::is_move_assignable_v<T>)
    {
        if (state == state_type::value) {
            storage.val = std::move(v);
        } else {
            std::construct_at(std::addressof(storage.val), std::move(v));
            state = state_type::value;
        }

        return *this;
    }

    constexpr expected& operator=(error_code ec) noexcept
    {
        set_error(ec);
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

    // error_code is a small trivial type: from an rvalue it is returned by
    // value.
    [[nodiscard]] constexpr error_type error() && noexcept
    {
        debug::ensure(has_error());
        return storage.ec;
    }

    [[nodiscard]] constexpr error_type error() const&& noexcept
    {
        debug::ensure(has_error());
        return storage.ec;
    }

    constexpr void swap(expected& other) noexcept(
      std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T>)
        requires(std::is_move_constructible_v<T> && std::is_swappable_v<T>)
    {
        if (this == &other)
            return;

        if (state == state_type::value && other.state == state_type::value) {
            using std::swap;
            swap(storage.val, other.storage.val);
        } else if (state == state_type::error &&
                   other.state == state_type::error) {
            std::swap(storage.ec, other.storage.ec);
        } else if (state == state_type::value) {
            // *this holds a value, other holds an error. The error is saved
            // first: constructing the value in other overwrites the union.
            const error_code ec = other.storage.ec;

            std::construct_at(std::addressof(other.storage.val),
                              std::move(storage.val));
            other.state = state_type::value;

            set_error(ec);
        } else {
            other.swap(*this);
        }
    }

    /**
     * Destroys the current content and constructs a new value from @a args.
     * @a args may refer to the current value (`e.emplace(e->field)`): in this
     * case the new value is built in a temporary before the old one dies.
     */
    template<typename... Args>
        requires(std::is_constructible_v<T, Args...>)
    constexpr T& emplace(Args&&... args) noexcept(
      std::is_nothrow_constructible_v<T, Args...> &&
      (!std::is_move_constructible_v<T> ||
       std::is_nothrow_move_constructible_v<T>))
    {
        if (state == state_type::value) {
            if constexpr (std::is_move_constructible_v<T>) {
                T tmp(std::forward<Args>(args)...);
                destroy_value();
                std::construct_at(std::addressof(storage.val), std::move(tmp));
            } else {
                destroy_value();
                std::construct_at(std::addressof(storage.val),
                                  std::forward<Args>(args)...);
            }
        } else {
            std::construct_at(std::addressof(storage.val),
                              std::forward<Args>(args)...);
            state = state_type::value;
        }

        return storage.val;
    }

    template<typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) const& noexcept(
      std::is_nothrow_copy_constructible_v<T> &&
      std::is_nothrow_constructible_v<T, U>)
        requires(std::is_convertible_v<U, T>)
    {
        if (has_value())
            return storage.val;

        return static_cast<T>(std::forward<U>(default_value));
    }

    template<typename U>
    [[nodiscard]] constexpr T value_or(U&& default_value) && noexcept(
      std::is_nothrow_move_constructible_v<T> &&
      std::is_nothrow_constructible_v<T, U>)
        requires(std::is_convertible_v<U, T>)
    {
        if (has_value())
            return std::move(storage.val);

        return static_cast<T>(std::forward<U>(default_value));
    }

    // Same as value_or, kept for source compatibility.
    template<typename U>
    [[nodiscard]] constexpr T unwrap_or(U&& default_value) const& noexcept(
      noexcept(std::declval<const expected&>().value_or(
        std::forward<U>(default_value))))
        requires(std::is_convertible_v<U, T>)
    {
        return value_or(std::forward<U>(default_value));
    }

    template<typename U>
    [[nodiscard]] constexpr T unwrap_or(U&& default_value) && noexcept(noexcept(
      std::declval<expected&&>().value_or(std::forward<U>(default_value))))
        requires(std::is_convertible_v<U, T>)
    {
        return std::move(*this).value_or(std::forward<U>(default_value));
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

    [[nodiscard]] constexpr T& operator*() & noexcept
    {
        debug::ensure(has_value());
        return storage.val;
    }

    [[nodiscard]] constexpr const T& operator*() const& noexcept
    {
        debug::ensure(has_value());
        return storage.val;
    }

    [[nodiscard]] constexpr T&& operator*() && noexcept
    {
        debug::ensure(has_value());
        return std::move(storage.val);
    }

    [[nodiscard]] constexpr const T&& operator*() const&& noexcept
    {
        debug::ensure(has_value());
        return std::move(storage.val);
    }

    /**
     * Calls @a f(value, args...) if there is a value, which must return an
     * expected<U> (the value type can change: `expected<int>` can be chained
     * to `expected<std::string>`). Otherwise propagates the error as an
     * expected<U>.
     */
    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f, Args&&... args) & noexcept
    {
        return and_then_impl(*this, std::forward<Fn>(f),
                             std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f,
                                          Args&&... args) const& noexcept
    {
        return and_then_impl(*this, std::forward<Fn>(f),
                             std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f, Args&&... args) && noexcept
    {
        return and_then_impl(std::move(*this), std::forward<Fn>(f),
                             std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f,
                                          Args&&... args) const&& noexcept
    {
        return and_then_impl(std::move(*this), std::forward<Fn>(f),
                             std::forward<Args>(args)...);
    }

    /**
     * Returns *this if there is a value, otherwise @a f(error, args...) which
     * must return an expected<T>.
     */
    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) & noexcept
    {
        return or_else_impl(*this, std::forward<Fn>(f),
                            std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) const& noexcept
    {
        return or_else_impl(*this, std::forward<Fn>(f),
                            std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) && noexcept
    {
        return or_else_impl(std::move(*this), std::forward<Fn>(f),
                            std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f,
                                         Args&&... args) const&& noexcept
    {
        return or_else_impl(std::move(*this), std::forward<Fn>(f),
                            std::forward<Args>(args)...);
    }

    /**
     * Returns an expected<U> holding @a f(value) where U is the (decayed)
     * result type of @a f, or expected<void> if @a f returns void. Otherwise
     * propagates the error.
     */
    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) & noexcept
    {
        return transform_impl(*this, std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) const& noexcept
    {
        return transform_impl(*this, std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) && noexcept
    {
        return transform_impl(std::move(*this), std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) const&& noexcept
    {
        return transform_impl(std::move(*this), std::forward<Fn>(f));
    }

    // Same as transform, kept for source compatibility.
    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) & noexcept
    {
        return transform(std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) const& noexcept
    {
        return transform(std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) && noexcept
    {
        return std::move(*this).transform(std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) const&& noexcept
    {
        return std::move(*this).transform(std::forward<Fn>(f));
    }

    /**
     * Returns *this if there is a value, otherwise an expected holding the
     * error_code returned by @a f(error).
     */
    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) & noexcept
    {
        return map_error_impl(*this, std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) const& noexcept
    {
        return map_error_impl(*this, std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) && noexcept
    {
        return map_error_impl(std::move(*this), std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) const&& noexcept
    {
        return map_error_impl(std::move(*this), std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr T unwrap_or_else(Fn&& f) const& noexcept
    {
        if (has_value())
            return storage.val;

        return static_cast<T>(std::invoke(std::forward<Fn>(f), storage.ec));
    }

    template<typename Fn>
    [[nodiscard]] constexpr T unwrap_or_else(Fn&& f) && noexcept
    {
        if (has_value())
            return std::move(storage.val);

        return static_cast<T>(std::invoke(std::forward<Fn>(f), storage.ec));
    }
};

/**
 * Specialization for functions that return no value: only a success or an
 * error. It is an @a error_code in disguise: the value 0 means success, so
 * the object has the size of an error_code, is trivially copyable and is
 * returned in a register.
 */
template<>
class [[nodiscard]] expected<void>
{
public:
    using value_type = void;
    using error_type = error_code;
    using this_type  = expected<void>;

private:
    error_code storage; // value 0 means success

    template<typename Self>
    [[nodiscard]] static constexpr auto&& forward_error(Self&& self) noexcept
    {
        return std::forward<Self>(self).storage;
    }

    template<typename Self, typename Fn, typename... Args>
    static constexpr auto and_then_impl(Self&& self,
                                        Fn&&   f,
                                        Args&&... args) noexcept
    {
        using R = std::remove_cvref_t<std::invoke_result_t<Fn, Args...>>;
        static_assert(detail::is_expected_v<R>,
                      "and_then: the callback must return an irt::expected");

        if (self.has_value())
            return std::invoke(std::forward<Fn>(f),
                               std::forward<Args>(args)...);

        return R(self.storage);
    }

    template<typename Self, typename Fn, typename... Args>
    static constexpr auto or_else_impl(Self&& self,
                                       Fn&&   f,
                                       Args&&... args) noexcept
    {
        using R = std::remove_cvref_t<std::invoke_result_t<
          Fn, decltype(forward_error(std::forward<Self>(self))), Args...>>;
        static_assert(std::is_same_v<R, this_type>,
                      "or_else: the callback must return an expected<void>");

        if (self.has_value())
            return this_type();

        return std::invoke(std::forward<Fn>(f),
                           forward_error(std::forward<Self>(self)),
                           std::forward<Args>(args)...);
    }

    template<typename Self, typename Fn>
    static constexpr auto transform_impl(Self&& self, Fn&& f) noexcept
    {
        using U = std::remove_cvref_t<std::invoke_result_t<Fn>>;

        // Written with U (and not expected<void>) in the void case, see
        // expected<T>::transform_impl.
        if constexpr (std::is_void_v<U>) {
            if (self.has_value()) {
                std::invoke(std::forward<Fn>(f));
                return expected<U>();
            }
            return expected<U>(self.storage);
        } else {
            if (self.has_value())
                return expected<U>(std::in_place,
                                   std::invoke(std::forward<Fn>(f)));
            return expected<U>(self.storage);
        }
    }

    template<typename Self, typename Fn>
    static constexpr auto map_error_impl(Self&& self, Fn&& f) noexcept
    {
        using R = std::invoke_result_t<Fn, decltype(forward_error(
                                             std::forward<Self>(self)))>;
        static_assert(std::is_convertible_v<R, error_code>,
                      "map_error: the callback must return an error_code");

        if (self.has_value())
            return this_type();

        return this_type(error_code(std::invoke(
          std::forward<Fn>(f), forward_error(std::forward<Self>(self)))));
    }

public:
    constexpr expected() noexcept = default;

    constexpr expected(error_code ec) noexcept
      : storage(ec)
    {
        debug::ensure(static_cast<bool>(ec));
    }

    // Copy, move and destruction are the compiler generated (trivial) ones.

    constexpr expected& operator=(error_code ec) noexcept
    {
        debug::ensure(static_cast<bool>(ec));
        storage = ec;
        return *this;
    }

    [[nodiscard]] constexpr bool has_value() const noexcept
    {
        return !static_cast<bool>(storage);
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    [[nodiscard]] constexpr bool has_error() const noexcept
    {
        return static_cast<bool>(storage);
    }

    [[nodiscard]] constexpr error_type& error() & noexcept
    {
        debug::ensure(has_error());
        return storage;
    }

    [[nodiscard]] constexpr const error_type& error() const& noexcept
    {
        debug::ensure(has_error());
        return storage;
    }

    [[nodiscard]] constexpr error_type error() && noexcept
    {
        debug::ensure(has_error());
        return storage;
    }

    [[nodiscard]] constexpr error_type error() const&& noexcept
    {
        debug::ensure(has_error());
        return storage;
    }

    /**
     * Calls @a f(args...) if there is no error and returns its result, which
     * must be an expected<...> (a failing @a f is therefore propagated).
     * Otherwise propagates the error as the expected<...> returned by @a f.
     */
    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f, Args&&... args) & noexcept
    {
        return and_then_impl(*this, std::forward<Fn>(f),
                             std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f,
                                          Args&&... args) const& noexcept
    {
        return and_then_impl(*this, std::forward<Fn>(f),
                             std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f, Args&&... args) && noexcept
    {
        return and_then_impl(std::move(*this), std::forward<Fn>(f),
                             std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto and_then(Fn&& f,
                                          Args&&... args) const&& noexcept
    {
        return and_then_impl(std::move(*this), std::forward<Fn>(f),
                             std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) & noexcept
    {
        return or_else_impl(*this, std::forward<Fn>(f),
                            std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) const& noexcept
    {
        return or_else_impl(*this, std::forward<Fn>(f),
                            std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f, Args&&... args) && noexcept
    {
        return or_else_impl(std::move(*this), std::forward<Fn>(f),
                            std::forward<Args>(args)...);
    }

    template<typename Fn, typename... Args>
    [[nodiscard]] constexpr auto or_else(Fn&& f,
                                         Args&&... args) const&& noexcept
    {
        return or_else_impl(std::move(*this), std::forward<Fn>(f),
                            std::forward<Args>(args)...);
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) & noexcept
    {
        return map_error_impl(*this, std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) const& noexcept
    {
        return map_error_impl(*this, std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) && noexcept
    {
        return map_error_impl(std::move(*this), std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map_error(Fn&& f) const&& noexcept
    {
        return map_error_impl(std::move(*this), std::forward<Fn>(f));
    }

    /**
     * Returns an expected<U> holding @a f() if there is no error, where U is
     * the (decayed) result type of @a f, or a status if @a f returns void.
     * Otherwise propagates the error: @a f is not called.
     */
    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) & noexcept
    {
        return transform_impl(*this, std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) const& noexcept
    {
        return transform_impl(*this, std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) && noexcept
    {
        return transform_impl(std::move(*this), std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto transform(Fn&& f) const&& noexcept
    {
        return transform_impl(std::move(*this), std::forward<Fn>(f));
    }

    // Same as transform, kept for source compatibility with expected<T>::map.
    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) & noexcept
    {
        return transform(std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) const& noexcept
    {
        return transform(std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) && noexcept
    {
        return std::move(*this).transform(std::forward<Fn>(f));
    }

    template<typename Fn>
    [[nodiscard]] constexpr auto map(Fn&& f) const&& noexcept
    {
        return std::move(*this).transform(std::forward<Fn>(f));
    }

    /**
     * Calls @a f(error) when there is an error (does nothing otherwise).
     */
    template<typename Fn>
    constexpr void unwrap_or_else(Fn&& f) const& noexcept
    {
        if (has_error())
            std::invoke(std::forward<Fn>(f), storage);
    }

    template<typename Fn>
    constexpr void unwrap_or_else(Fn&& f) && noexcept
    {
        if (has_error())
            std::invoke(std::forward<Fn>(f), storage);
    }

    constexpr void swap(expected& other) noexcept
    {
        using std::swap;
        swap(storage, other.storage);
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

/**
 * Anything @a make_error accepts as a single argument.
 */
template<typename E>
concept error_source = error_enum<E> || std::same_as<E, std::errc>;

/**
 * Returns a failure (a status holding `make_error(e)`) if @a failed is true,
 * otherwise a success.
 *
 * `make_error`, hence @a on_error_callback, is called only on failure: the
 * successful path does not trigger the debugger hook.
 *
 * EXAMPLE:
 *
 *     irt_check(irt::fail_if(n <= 0, simulation_errc::range_error));
 */
template<error_source E>
[[nodiscard]] constexpr expected<void> fail_if(bool failed, E e) noexcept
{
    if (failed)
        return expected<void>(make_error(e));

    return expected<void>();
}

/**
 * Turns a pointer, typically the result of an id lookup that returns nullptr
 * when nothing is found, into an expected<T*>: the pointer if it is not null,
 * otherwise `make_error(e)` (called only in this case). Since an expected can
 * not hold a reference, a pointer is the way to return an element.
 *
 * EXAMPLE:
 *
 *     irt_assign(mdl, irt::require(sim.models.try_to_get(id),
 *                                  simulation_errc::models_container_full));
 */
template<typename T, error_source E>
[[nodiscard]] constexpr expected<T*> require(T* ptr, E e) noexcept
{
    if (ptr)
        return expected<T*>(ptr);

    return expected<T*>(make_error(e));
}

template<typename T>
constexpr void swap(expected<T>& lhs,
                    expected<T>& rhs) noexcept(noexcept(lhs.swap(rhs)))
    requires requires { lhs.swap(rhs); }
{
    lhs.swap(rhs);
}

namespace detail {

template<typename T, typename U>
concept equality_comparable_values = requires(const T& a, const U& b) {
    { a == b } -> std::convertible_to<bool>;
};

} // namespace detail

template<typename T, typename U>
    requires(!std::is_void_v<T> && !std::is_void_v<U> &&
             detail::equality_comparable_values<T, U>)
[[nodiscard]] constexpr bool operator==(const expected<T>& lhs,
                                        const expected<U>& rhs) noexcept
{
    if (lhs.has_value() != rhs.has_value())
        return false;

    if (lhs.has_value())
        return static_cast<bool>(lhs.value() == rhs.value());

    return lhs.error() == rhs.error();
}

[[nodiscard]] constexpr bool operator==(const expected<void>& lhs,
                                        const expected<void>& rhs) noexcept
{
    if (lhs.has_value() != rhs.has_value())
        return false;

    return lhs.has_value() || lhs.error() == rhs.error();
}

template<typename T, typename U>
    requires(!std::is_void_v<T> && !detail::is_expected_v<U> &&
             !std::is_same_v<U, error_code> &&
             detail::equality_comparable_values<T, U>)
[[nodiscard]] constexpr bool operator==(const expected<T>& lhs,
                                        const U&           rhs) noexcept
{
    return lhs.has_value() && static_cast<bool>(lhs.value() == rhs);
}

template<typename T>
[[nodiscard]] constexpr bool operator==(const expected<T>& lhs,
                                        const error_code&  rhs) noexcept
{
    return lhs.has_error() && lhs.error() == rhs;
}

using status = expected<void>;

} // namespace irt

/**
 * Returns the error of @a r from the calling function if @a r is an error.
 *
 * @attention This is a statement: the call must be followed by a ';'.
 */
#define irt_check(r)                                                           \
    do {                                                                       \
        auto&& irt_check_result_ = (r);                                        \
        if (not irt_check_result_)                                             \
            return irt_check_result_.error();                                  \
    } while (0)

/**
 * Declares @a var as a reference to the value held by the expected @a expr, or
 * returns its error from the calling function (as irt_check does).
 *
 *     irt::status use(int i) noexcept
 *     {
 *         irt_assign(p, irt::require(find(i), simulation_errc::range_error));
 *         irt_assign(s, parse(*p));
 *         ...
 *     }
 *
 * `var` refers to the value stored in a temporary that lives until the end of
 * the enclosing scope: nothing is copied or moved (use std::move(var) to take a
 * move-only value). The name of this temporary is built from `var`, so no
 * __LINE__ or __COUNTER__ is needed and the macro can be used several times in
 * a scope with different names.
 *
 * @attention The macro declares two variables in the current scope: it can not
 * be the unbraced body of an `if`, `for`, etc. It is not usable with a status
 * (it holds no value): use irt_check.
 */
#define irt_assign(var, expr)                                                  \
    auto&& irt_result_##var = (expr);                                          \
    if (not irt_result_##var)                                                  \
        return irt_result_##var.error();                                       \
    auto&& var = *irt_result_##var

#endif
