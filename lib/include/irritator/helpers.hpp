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

struct qss_integrator_tag {
    enum parameter_names : u8 { X = 0, dQ };
};

struct qss_cross_tag {
    enum parameter_names : u8 { threshold, up_value, bottom_value };
};

struct qss_multiplier_tag {};
struct qss_flipflop_tag {};
struct qss_filter_tag {
    enum parameter_names : u8 { lower_bound = 0, upper_bound };
};

struct qss_power_tag {
    enum parameter_names : u8 { exponent = 0 };
};

struct qss_square_tag {};
struct qss_sum_2_tag {};
struct qss_sum_3_tag {};
struct qss_sum_4_tag {};

struct qss_wsum_2_tag {
    enum parameter_names : u8 { coeff1 = 0, coeff2 };
};

struct qss_wsum_3_tag {
    enum parameter_names : u8 { coeff1 = 0, coeff2, coeff3 };
};

struct qss_wsum_4_tag {
    enum parameter_names : u8 { coeff1 = 0, coeff2, coeff3, coeff4 };
};

struct qss_inverse_tag {};
struct qss_integer_tag {};
struct qss_compare_tag {
    enum parameter_names : u8 { equal = 0, not_equal };
};

struct qss_gain_tag {
    enum parameter_names : u8 { k = 0 };
};

struct qss_sin_tag {};
struct qss_cos_tag {};
struct qss_exp_tag {};
struct qss_log_tag {};

struct counter_tag {
    enum parameter_names : u8 { i_obs_type = 0 };
};

struct queue_tag {
    enum parameter_names : u8 { sigma = 0 };
};

struct dynamic_queue_tag {
    enum parameter_names : u8 { source_ta = 0 };
};

struct priority_queue_tag {
    enum parameter_names : u8 { sigma = 0, source_ta = 0 };
};

struct generator_tag {
    enum parameter_names : u8 { i_options = 0, source_ta, source_value };
};

struct constant_tag {
    enum parameter_names : u8 { value = 0, offset, i_type = 0, i_port };
};

struct time_func_tag {
    enum parameter_names : u8 { offset = 0, timestep, i_type = 0 };
};
struct accumulator_2_tag {};
struct logical_and_2_tag {};
struct logical_and_3_tag {};
struct logical_or_2_tag {};
struct logical_or_3_tag {};
struct logical_invert_tag {};
struct hsm_wrapper_tag {
    enum parameter_names : u8 {
        r1 = 0,
        r2,
        timer,
        id = 0,
        i1,
        i2,
        source_value
    };
};

/** Dispatch the callable @c f with @c args argument according to @c type.
 *
 * This function is useful to: (1) avoid using dynamic polymorphism (i.e.,
 * virtual) based on the @c dynamics_type variables and to (2) provide the
 * same source code for same dynamics type like abstract classes.
 *
 * @param type
 * @param f
 * @param args
 *
 * @verbatim
 * dispatch(mdl.type, []<typename Tag>(
 *     const Tag t, const float x, const float y) -> bool {
 *         if constexpr(std::is_same_v(t, hsm_wrapper_tag)) {
 *             // todo
 *         }
 *     }));
 * @endverbatim
 */
template<typename Function, typename... Args>
constexpr auto dispatch(const dynamics_type type,
                        Function&&          f,
                        Args&&... args) noexcept
{
    switch (type) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss3_integrator:
        return std::invoke(std::forward<Function>(f),
                           qss_integrator_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss3_multiplier:
        return std::invoke(std::forward<Function>(f),
                           qss_multiplier_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_cross:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
        return std::invoke(std::forward<Function>(f),
                           qss_cross_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_flipflop:
    case dynamics_type::qss2_flipflop:
    case dynamics_type::qss3_flipflop:
        return std::invoke(std::forward<Function>(f),
                           qss_flipflop_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_filter:
    case dynamics_type::qss2_filter:
    case dynamics_type::qss3_filter:
        return std::invoke(std::forward<Function>(f),
                           qss_filter_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_power:
    case dynamics_type::qss2_power:
    case dynamics_type::qss3_power:
        return std::invoke(std::forward<Function>(f),
                           qss_power_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_square:
    case dynamics_type::qss2_square:
    case dynamics_type::qss3_square:
        return std::invoke(std::forward<Function>(f),
                           qss_square_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss3_sum_2:
        return std::invoke(std::forward<Function>(f),
                           qss_sum_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss3_sum_3:
        return std::invoke(std::forward<Function>(f),
                           qss_sum_3_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss3_sum_4:
        return std::invoke(std::forward<Function>(f),
                           qss_sum_4_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss3_wsum_2:
        return std::invoke(std::forward<Function>(f),
                           qss_wsum_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss3_wsum_3:
        return std::invoke(std::forward<Function>(f),
                           qss_wsum_3_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_wsum_4:
        return std::invoke(std::forward<Function>(f),
                           qss_wsum_4_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_inverse:
    case dynamics_type::qss2_inverse:
    case dynamics_type::qss3_inverse:
        return std::invoke(std::forward<Function>(f),
                           qss_inverse_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_integer:
    case dynamics_type::qss2_integer:
    case dynamics_type::qss3_integer:
        return std::invoke(std::forward<Function>(f),
                           qss_integer_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_compare:
    case dynamics_type::qss2_compare:
    case dynamics_type::qss3_compare:
        return std::invoke(std::forward<Function>(f),
                           qss_compare_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_gain:
    case dynamics_type::qss2_gain:
    case dynamics_type::qss3_gain:
        return std::invoke(std::forward<Function>(f),
                           qss_gain_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_sin:
    case dynamics_type::qss2_sin:
    case dynamics_type::qss3_sin:
        return std::invoke(std::forward<Function>(f),
                           qss_sin_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_cos:
    case dynamics_type::qss2_cos:
    case dynamics_type::qss3_cos:
        return std::invoke(std::forward<Function>(f),
                           qss_cos_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_log:
    case dynamics_type::qss2_log:
    case dynamics_type::qss3_log:
        return std::invoke(std::forward<Function>(f),
                           qss_log_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::qss1_exp:
    case dynamics_type::qss2_exp:
    case dynamics_type::qss3_exp:
        return std::invoke(std::forward<Function>(f),
                           qss_exp_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::counter:
        return std::invoke(std::forward<Function>(f),
                           counter_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::queue:
        return std::invoke(
          std::forward<Function>(f), queue_tag{}, std::forward<Args>(args)...);
    case dynamics_type::dynamic_queue:
        return std::invoke(std::forward<Function>(f),
                           dynamic_queue_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::priority_queue:
        return std::invoke(std::forward<Function>(f),
                           priority_queue_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::generator:
        return std::invoke(std::forward<Function>(f),
                           generator_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::constant:
        return std::invoke(std::forward<Function>(f),
                           constant_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::accumulator_2:
        return std::invoke(std::forward<Function>(f),
                           accumulator_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::time_func:
        return std::invoke(std::forward<Function>(f),
                           time_func_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::logical_and_2:
        return std::invoke(std::forward<Function>(f),
                           logical_and_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::logical_and_3:
        return std::invoke(std::forward<Function>(f),
                           logical_and_3_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::logical_or_2:
        return std::invoke(std::forward<Function>(f),
                           logical_or_2_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::logical_or_3:
        return std::invoke(std::forward<Function>(f),
                           logical_or_3_tag{},
                           std::forward<Args>(args)...);
    case dynamics_type::logical_invert:
        return std::invoke(std::forward<Function>(f),
                           logical_invert_tag{},
                           std::forward<Args>(args)...);

    case dynamics_type::hsm_wrapper:
        return std::invoke(std::forward<Function>(f),
                           hsm_wrapper_tag{},
                           std::forward<Args>(args)...);
    }

    unreachable();
}

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

/** A for-each-condition function which reads each element of the vector @c Vec
 * and apply the function @c Fn. If the function @c Fn returns false, the
 * element is removed from the vector using the @c vector<T>::erase .
 *
 * @code
 *   for_each_cond(ed.visualisation_eds, [&](const auto v) noexcept -> bool {
 *       if (v.tn_id == ed.pj.tree_nodes.get_id(tn)) {
 *           auto* ged = app.graph_eds.try_to_get(v.graph_ed_id);
 *           auto* obs = ed.pj.graph_observers.try_to_get(v.graph_obs_id);
 *
 *           if (not(ged and obs))
 *               return false;
 *
 *           ged->show(app, ed, tn, *obs);
 *       }
 *
 *       return true;
 *   });
 * @endcode
 */
template<typename Vec, typename Fn, typename... Args>
void for_each_cond(Vec& vec, Fn&& fn, Args&&... args) noexcept
{
    for (auto it = vec.begin(); it != vec.end();) {
        if (fn(*it, args...))
            ++it;
        else
            it = vec.erase(it);
    }
}

//! @brief Apply the function @c f for all elements of the @c data_array.
template<typename Data, typename Function>
auto for_each_data(Data& d, Function&& f) noexcept -> void
{
    using return_t = std::invoke_result_t<Function, typename Data::value_type&>;

    static_assert(std::is_same_v<return_t, void>);

    for (auto& data : d)
        std::invoke(std::forward<Function>(f), data);
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

constexpr inline source_type get_source_type(std::integral auto type) noexcept
{
    return (0 <= type and type < 4) ? enum_cast<source_type>(type)
                                    : source_type::constant;
}

constexpr inline u64 from_source(const source_type   type,
                                 const source_any_id id) noexcept
{
    switch (type) {
    case source_type::binary_file:
        return u32s_to_u64(ordinal(type), ordinal(id.binary_file_id));

    case source_type::constant:
        return u32s_to_u64(ordinal(type), ordinal(id.constant_id));

    case source_type::random:
        return u32s_to_u64(ordinal(type), ordinal(id.random_id));

    case source_type::text_file:
        return u32s_to_u64(ordinal(type), ordinal(id.text_file_id));
    }

    unreachable();
}

constexpr inline u64 from_source(const source& src) noexcept
{
    switch (src.type) {
    case source_type::binary_file:
        return u32s_to_u64(ordinal(src.type), ordinal(src.id.binary_file_id));

    case source_type::constant:
        return u32s_to_u64(ordinal(src.type), ordinal(src.id.constant_id));

    case source_type::random:
        return u32s_to_u64(ordinal(src.type), ordinal(src.id.random_id));

    case source_type::text_file:
        return u32s_to_u64(ordinal(src.type), ordinal(src.id.text_file_id));
    }

    unreachable();
}

constexpr inline std::pair<source_type, source_any_id> get_source(
  const u64 parameter) noexcept
{
    const auto    p_type = left(parameter);
    const auto    p_id   = right(parameter);
    const auto    type   = get_source_type(p_type);
    source_any_id id     = undefined<constant_source_id>();

    switch (type) {
    case source_type::constant:
        id.constant_id = enum_cast<constant_source_id>(p_id);
        break;

    case source_type::text_file:
        id.text_file_id = enum_cast<text_file_source_id>(p_id);
        break;

    case source_type::binary_file:
        id.binary_file_id = enum_cast<binary_file_source_id>(p_id);
        break;

    case source_type::random:
        id.random_id = enum_cast<random_source_id>(p_id);
        break;
    }

    return std::make_pair(type, id);
}

} // namespace irt

#endif
