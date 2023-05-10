// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2020
#define ORG_VLEPROJECT_IRRITATOR_2020

#include <algorithm>
#include <limits>

#ifdef __has_include
#if __has_include(<numbers>)
#include <numbers>
#define irt_have_numbers 1
#else
#define irt_have_numbers 0
#endif
#endif

#include <array>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <concepts>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

/*****************************************************************************
 *
 * Helper macros
 *
 ****************************************************************************/

#ifndef irt_assert
#include <cassert>
#define irt_assert(_expr) assert(_expr)
#endif

namespace irt {

#if defined(IRRITATOR_ENABLE_DEBUG)
inline volatile bool is_fatal_breakpoint = false;
#else
constexpr static inline bool is_fatal_breakpoint = false;
#endif

} // namespace irt

#ifndef NDEBUG
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) &&         \
  __GNUC__ >= 2
#define irt_breakpoint()                                                       \
    do {                                                                       \
        if (::irt::is_fatal_breakpoint)                                        \
            __asm__ __volatile__("int $03");                                   \
    } while (0)
#elif (defined(_MSC_VER) || defined(__DMC__)) && defined(_M_IX86)
#define irt_breakpoint()                                                       \
    do {                                                                       \
        if (::irt::is_fatal_breakpoint)                                        \
            __asm int 3h                                                       \
    } while (0)
#elif defined(_MSC_VER)
#define irt_breakpoint()                                                       \
    do {                                                                       \
        if (::irt::is_fatal_breakpoint)                                        \
            __debugbreak();                                                    \
    } while (0)
#elif defined(__alpha__) && !defined(__osf__) && defined(__GNUC__) &&          \
  __GNUC__ >= 2
#define irt_breakpoint()                                                       \
    do {                                                                       \
        if (::irt::is_fatal_breakpoint)                                        \
            __asm__ __volatile__("bpt");                                       \
    } while (0)
#elif defined(__APPLE__)
#define irt_breakpoint()                                                       \
    do {                                                                       \
        if (::irt::is_fatal_breakpoint)                                        \
            __builtin_trap();                                                  \
    } while (0)
#else /* !__i386__ && !__alpha__ */
#define irt_breakpoint()                                                       \
    do {                                                                       \
        if (::irt::is_fatal_breakpoint)                                        \
            raise(SIGTRAP);                                                    \
    } while (0)
#endif /* __i386__ */
#else
#define irt_breakpoint()                                                       \
    do {                                                                       \
    } while (0)
#endif

#define irt_bad_return(status__)                                               \
    do {                                                                       \
        irt_breakpoint();                                                      \
        return status__;                                                       \
    } while (0)

#define irt_return_if_bad(expr__)                                              \
    do {                                                                       \
        auto status__ = (expr__);                                              \
        if (status__ != status::success) {                                     \
            irt_breakpoint();                                                  \
            return status__;                                                   \
        }                                                                      \
    } while (0)

#define irt_return_if_bad_map(expr__, new_status__)                            \
    do {                                                                       \
        auto status__ = (expr__);                                              \
        if (status__ != status::success) {                                     \
            irt_breakpoint();                                                  \
            return new_status__;                                               \
        }                                                                      \
    } while (0)

#define irt_return_if_fail(expr__, status__)                                   \
    do {                                                                       \
        if (!(expr__)) {                                                       \
            irt_breakpoint();                                                  \
            return status__;                                                   \
        }                                                                      \
    } while (0)

#if defined(__GNUC__)
#define irt_unreachable() __builtin_unreachable();
#elif defined(_MSC_VER)
#define irt_unreachable() __assume(0)
#else
#define irt_unreachable()
#endif

namespace irt {

using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using sz  = size_t;
using ssz = ptrdiff_t;
using f32 = float;
using f64 = double;

#ifndef IRRITATOR_REAL_TYPE_F32
using real = f64;
#else
using real                                       = f32;
#endif //  IRRITATOR_REAL_TYPE

//! @brief An helper function to initialize floating point number and disable
//! warnings the IRRITATOR_REAL_TYPE_F64 is defined.
//!
//! @param v The floating point number to convert to float, double or long
//! double.
//! @return A real.
inline constexpr real to_real(long double v) noexcept
{
    return static_cast<real>(v);
}

namespace literals {

//! @brief An helper literal function to initialize floating point number and
//! disable warnings the IRRITATOR_REAL_TYPE_F64 is defined.
//!
//! @param v The floating point number to convert to float, double or long
//! double.
//! @return A real.
inline constexpr real operator"" _r(long double v) noexcept
{
    return static_cast<real>(v);
}

} // namespace literals

/*****************************************************************************
 *
 * Some constants used in core and models
 *
 ****************************************************************************/

constexpr static inline real one   = to_real(1.L);
constexpr static inline real two   = to_real(2.L);
constexpr static inline real three = to_real(3.L);
constexpr static inline real four  = to_real(4.L);
constexpr static inline real zero  = to_real(0.L);

inline u16 make_halfword(u8 a, u8 b) noexcept
{
    return static_cast<u16>((a << 8) | b);
}

inline void unpack_halfword(u16 halfword, u8* a, u8* b) noexcept
{
    *a = static_cast<u8>((halfword >> 8) & 0xff);
    *b = static_cast<u8>(halfword & 0xff);
}

inline u32 make_word(u16 a, u16 b) noexcept
{
    return (static_cast<u32>(a) << 16) | static_cast<u32>(b);
}

inline void unpack_word(u32 word, u16* a, u16* b) noexcept
{
    *a = static_cast<u16>((word >> 16) & 0xffff);
    *b = static_cast<u16>(word & 0xffff);
}

inline u64 make_doubleword(u32 a, u32 b) noexcept
{
    return (static_cast<u64>(a) << 32) | static_cast<u64>(b);
}

inline void unpack_doubleword(u64 doubleword, u32* a, u32* b) noexcept
{
    *a = static_cast<u32>((doubleword >> 32) & 0xffffffff);
    *b = static_cast<u32>(doubleword & 0xffffffff);
}

inline u32 unpack_doubleword_left(u64 doubleword) noexcept
{
    return static_cast<u32>((doubleword >> 32) & 0xffffffff);
}

inline u32 unpack_doubleword_right(u64 doubleword) noexcept
{
    return static_cast<u32>(doubleword & 0xffffffff);
}

template<typename Integer>
constexpr typename std::make_unsigned<Integer>::type to_unsigned(Integer value)
{
    irt_assert(value >= 0);
    return static_cast<typename std::make_unsigned<Integer>::type>(value);
}

template<class C>
constexpr int length(const C& c) noexcept
{
    return static_cast<int>(c.size());
}

template<class T, size_t N>
constexpr int length(const T (&array)[N]) noexcept
{
    (void)array;

    return static_cast<int>(N);
}

template<typename Identifier>
constexpr Identifier undefined() noexcept
{
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    return static_cast<Identifier>(0);
}

template<typename Identifier>
constexpr bool is_undefined(Identifier id) noexcept
{
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    return id == undefined<Identifier>();
}

template<typename Identifier>
constexpr bool is_defined(Identifier id) noexcept
{
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    return id != undefined<Identifier>();
}

//! @brief A simple enumeration to integral helper function.
//! @tparam Enum An enumeration
//! @tparam Integer The underlying_type deduce from @c Enum
//! @param e The element in enumeration to convert.
//! @return An integral.
template<class Enum, class Integer = typename std::underlying_type<Enum>::type>
constexpr Integer ordinal(Enum e) noexcept
{
    static_assert(std::is_enum<Enum>::value,
                  "Identifier must be a enumeration");
    return static_cast<Integer>(e);
}

//! @brief A simpole integral to enumeration helper function.
//! @tparam Enum An enumeration
//! @tparam Integer The underlying_type deduce from @c Enum
//! @param i The integral to convert.
//! @return A element un enumeration.
template<class Enum, class Integer = typename std::underlying_type<Enum>::type>
constexpr Enum enum_cast(Integer i) noexcept
{
    static_assert(std::is_enum<Enum>::value,
                  "Identifier must be a enumeration");
    return static_cast<Enum>(i);
}

template<typename Target, typename Source>
constexpr inline bool is_numeric_castable(Source arg) noexcept
{
    static_assert(std::is_integral<Source>::value, "Integer required.");
    static_assert(std::is_integral<Target>::value, "Integer required.");

    using arg_traits    = std::numeric_limits<Source>;
    using result_traits = std::numeric_limits<Target>;

    if constexpr (result_traits::digits == arg_traits::digits &&
                  result_traits::is_signed == arg_traits::is_signed)
        return true;

    if constexpr (result_traits::digits > arg_traits::digits)
        return result_traits::is_signed || arg >= 0;

    if (arg_traits::is_signed &&
        arg < static_cast<Source>(result_traits::min()))
        return false;

    return arg <= static_cast<Source>(result_traits::max());
}

template<typename Target, typename Source>
constexpr inline Target numeric_cast(Source arg) noexcept
{
    bool is_castable = is_numeric_castable<Target, Source>(arg);
    irt_assert(is_castable);

    return static_cast<Target>(arg);
}

//! @brief returns an iterator to the result or end if not found
//!
//! Binary search function which returns an iterator to the result or end if
//! not found using the lower_bound standard function.
template<typename Iterator, typename T>
constexpr Iterator binary_find(Iterator begin, Iterator end, const T& value)
{
    begin = std::lower_bound(begin, end, value);
    return ((begin != end) && !(value < *begin));
}

//! @brief returns an iterator to the result or end if not found
//!
//! Binary search function which returns an iterator to the result or end if
//! not found using the lower_bound standard function. Compare functor must
//! use interoperable *begin type as first argument and T as second argument.
//! @code
//! struct my_int { int x };
//! vector<my_int> buffer;
//! binary_find(std::begin(buffer), std::end(buffer),
//!     5,
//!     [](auto left, auto right) noexcept -> bool
//!     {
//!         if constexpr(std::is_same_v<decltype(left), int)
//!             return left < right->x;
//!         else
//!             return left->x < right;
//!     });
//! @endcode
template<typename Iterator, typename T, typename Compare>
constexpr Iterator binary_find(Iterator begin,
                               Iterator end,
                               const T& value,
                               Compare  comp)
{
    begin = std::lower_bound(begin, end, value, comp);
    return (begin != end && !comp(value, *begin)) ? begin : end;
}

//! Enumeration class used everywhere in irritator to produce log data.
enum class log_level
{
    emergency,
    alert,
    critical,
    error,
    warning,
    notice,
    info,
    debug
};

/*****************************************************************************
 *
 * Return status of many function
 *
 ****************************************************************************/

enum class status
{
    success,
    unknown_dynamics,
    block_allocator_bad_capacity,
    block_allocator_not_enough_memory,
    head_allocator_bad_capacity,
    head_allocator_not_enough_memory,
    simulation_not_enough_model,
    simulation_not_enough_message,
    simulation_not_enough_connection,
    vector_init_capacity_error,
    vector_not_enough_memory,
    data_array_init_capacity_error,
    data_array_not_enough_memory,
    source_unknown,
    source_empty,
    model_connect_output_port_unknown,
    model_connect_already_exist,
    model_connect_bad_dynamics,
    model_queue_bad_ta,
    model_queue_full,
    model_dynamic_queue_source_is_null,
    model_dynamic_queue_full,
    model_priority_queue_source_is_null,
    model_priority_queue_full,
    model_integrator_dq_error,
    model_integrator_X_error,
    model_integrator_internal_error,
    model_integrator_output_error,
    model_integrator_running_without_x_dot,
    model_integrator_ta_with_bad_x_dot,
    model_quantifier_bad_quantum_parameter,
    model_quantifier_bad_archive_length_parameter,
    model_quantifier_shifting_value_neg,
    model_quantifier_shifting_value_less_1,
    model_time_func_bad_init_message,
    model_hsm_bad_top_state,
    model_hsm_bad_next_state,
    model_filter_threshold_condition_not_satisfied,

    modeling_too_many_description_open,
    modeling_too_many_file_open,
    modeling_too_many_directory_open,
    modeling_registred_path_access_error,
    modeling_directory_access_error,
    modeling_file_access_error,
    modeling_component_save_error,

    gui_not_enough_memory,
    io_not_enough_memory,
    io_filesystem_error,
    io_filesystem_make_directory_error,
    io_filesystem_not_directory_error,

    io_project_component_empty,
    io_project_component_type_error,
    io_project_file_error,
    io_project_file_component_type_error,
    io_project_file_component_path_error,
    io_project_file_component_directory_error,
    io_project_file_component_file_error,
    io_project_file_parameters_error,
    io_project_file_parameters_access_error,
    io_project_file_parameters_type_error,
    io_project_file_parameters_init_error,

    io_file_format_error,
    io_file_format_source_number_error,
    io_file_source_full,
    io_file_format_model_error,
    io_file_format_model_number_error,
    io_file_format_model_unknown,
    io_file_format_dynamics_unknown,
    io_file_format_dynamics_limit_reach,
    io_file_format_dynamics_init_error,
};

constexpr unsigned status_size() noexcept
{
    const auto id = ordinal(status::io_file_format_dynamics_init_error);
    return static_cast<unsigned>(id + 1);
}

constexpr bool is_success(status s) noexcept { return s == status::success; }

constexpr bool is_bad(status s) noexcept { return s != status::success; }

template<typename T, typename... Args>
constexpr bool match(const T& s, Args... args) noexcept
{
    return ((s == args) || ... || false);
}

template<class T, class... Rest>
constexpr bool are_all_same() noexcept
{
    return (std::is_same_v<T, Rest> && ...);
}

template<class T>
typename std::enable_if<!std::numeric_limits<T>::is_integer, bool>::type
almost_equal(T x, T y, int ulp)
{
    return std::fabs(x - y) <=
             std::numeric_limits<T>::epsilon() * std::fabs(x + y) * ulp ||
           std::fabs(x - y) < std::numeric_limits<T>::min();
}

/*****************************************************************************
 *
 * Allocator
 *
 ****************************************************************************/

using global_alloc_function_type = void*(size_t size) noexcept;
using global_free_function_type  = void(void* ptr) noexcept;

static inline void* malloc_wrapper(size_t size) noexcept
{
    return std::malloc(size);
}

static inline void free_wrapper(void* ptr) noexcept
{
    if (ptr)
        std::free(ptr);
}

static inline global_alloc_function_type* g_alloc_fn{ malloc_wrapper };
static inline global_free_function_type*  g_free_fn{ free_wrapper };

/*****************************************************************************
 *
 * Definition of Time
 *
 * TODO:
 * - enable template definition of float or [float|double,bool absolute]
 *   representation of time?
 *
 ****************************************************************************/

using time = real;

template<typename T>
struct time_domain
{};

template<>
struct time_domain<time>
{
    using time_type = time;

    static constexpr const real infinity =
      std::numeric_limits<real>::infinity();
    static constexpr const real negative_infinity =
      -std::numeric_limits<real>::infinity();
    static constexpr const real zero = 0;

    static constexpr bool is_infinity(time t) noexcept
    {
        return t == infinity || t == negative_infinity;
    }

    static constexpr bool is_zero(time t) noexcept { return t == zero; }
};

/*****************************************************************************
 *
 * Containers
 *
 ****************************************************************************/

//! @brief Compute the best size to fit the small storage size.
template<size_t N>
using small_storage_size_t = std::conditional_t<
  (N < std::numeric_limits<uint8_t>::max()),
  uint8_t,
  std::conditional_t<
    (N < std::numeric_limits<uint16_t>::max()),
    uint16_t,
    std::conditional_t<
      (N < std::numeric_limits<uint32_t>::max()),
      uint32_t,
      std::conditional_t<(N < std::numeric_limits<uint32_t>::max()),
                         uint64_t,
                         size_t>>>>;

//! @brief A vector like class with dynamic allocation.
//! @tparam T Any type (trivial or not).
template<typename T>
class vector
{
    static_assert(std::is_nothrow_destructible_v<T> ||
                  std::is_trivially_destructible_v<T>);

    T*  m_data     = nullptr;
    i32 m_size     = 0;
    i32 m_capacity = 0;

public:
    using size_type       = sz;
    using iterator        = T*;
    using const_iterator  = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    constexpr vector() noexcept = default;
    vector(int capacity) noexcept;
    vector(int capacity, int size) noexcept;
    vector(int capacity, int size, const T& default_value) noexcept;
    ~vector() noexcept;

    constexpr vector(const vector& other) noexcept;
    constexpr vector& operator=(const vector& other) noexcept;
    constexpr vector(vector&& other) noexcept;
    constexpr vector& operator=(vector&& other) noexcept;

    void resize(std::integral auto size) noexcept;
    void reserve(std::integral auto new_capacity) noexcept;

    void destroy() noexcept; // clear all elements and free memory (size = 0,
                             // capacity = 0 after).

    constexpr void clear() noexcept; // clear all elements (size = 0 after).

    constexpr T*       data() noexcept;
    constexpr const T* data() const noexcept;

    constexpr reference       front() noexcept;
    constexpr const_reference front() const noexcept;
    constexpr reference       back() noexcept;
    constexpr const_reference back() const noexcept;

    constexpr reference       operator[](std::integral auto index) noexcept;
    constexpr const_reference operator[](
      std::integral auto index) const noexcept;

    constexpr iterator       begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr bool     can_alloc(std::integral auto number = 1) const noexcept;
    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
    constexpr int      capacity() const noexcept;
    constexpr bool     empty() const noexcept;
    constexpr bool     full() const noexcept;

    template<typename... Args>
    constexpr reference emplace_back(Args&&... args) noexcept;

    constexpr int find(const T& t) const noexcept;

    constexpr void pop_back() noexcept;
    constexpr void swap_pop_back(std::integral auto index) noexcept;

    constexpr void erase(iterator it) noexcept;
    constexpr void erase(iterator begin, iterator end) noexcept;

private:
    i32 compute_new_capacity(i32 new_capacity) const;
};

//! @brief A vector like class but without dynamic allocation.
//! @tparam T Any type (trivial or not).
//! @tparam length The capacity of the vector.
template<typename T, int length>
class small_vector
{
public:
    static_assert(length >= 1);
    static_assert(std::is_nothrow_destructible_v<T> ||
                  std::is_trivially_destructible_v<T>);

    using size_type = small_storage_size_t<length>;

private:
    alignas(8) std::byte m_buffer[length * sizeof(T)];
    size_type m_size; // number of T element in the m_buffer.

public:
    using iterator        = T*;
    using const_iterator  = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    constexpr small_vector() noexcept;
    constexpr small_vector(const small_vector& other) noexcept;
    constexpr ~small_vector() noexcept;

    constexpr small_vector& operator=(const small_vector& other) noexcept;
    constexpr small_vector(small_vector&& other) noexcept            = delete;
    constexpr small_vector& operator=(small_vector&& other) noexcept = delete;

    constexpr status resize(std::integral auto capacity) noexcept;
    constexpr void   clear() noexcept;

    constexpr reference       front() noexcept;
    constexpr const_reference front() const noexcept;
    constexpr reference       back() noexcept;
    constexpr const_reference back() const noexcept;

    constexpr T*       data() noexcept;
    constexpr const T* data() const noexcept;

    constexpr reference       operator[](std::integral auto index) noexcept;
    constexpr const_reference operator[](
      std::integral auto index) const noexcept;

    constexpr iterator       begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr bool     can_alloc(std::integral auto number = 1) noexcept;
    constexpr int      available() const noexcept;
    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
    constexpr int      capacity() const noexcept;
    constexpr bool     empty() const noexcept;
    constexpr bool     full() const noexcept;

    template<typename... Args>
    constexpr reference emplace_back(Args&&... args) noexcept;
    constexpr void      pop_back() noexcept;
    constexpr void      swap_pop_back(std::integral auto index) noexcept;
};

//! @brief A small_string without heap allocation.
template<int length = 8>
class small_string
{
public:
    static_assert(length >= 2);

    using size_type = small_storage_size_t<length>;

private:
    char      m_buffer[length];
    size_type m_size;

public:
    using iterator        = char*;
    using const_iterator  = const char*;
    using reference       = char&;
    using const_reference = const char&;

    constexpr small_string() noexcept;
    constexpr small_string(const small_string& str) noexcept;
    constexpr small_string(small_string&& str) noexcept;
    constexpr small_string(const char* str) noexcept;
    constexpr small_string(const std::string_view str) noexcept;

    constexpr small_string& operator=(const small_string& str) noexcept;
    constexpr small_string& operator=(small_string&& str) noexcept;
    constexpr small_string& operator=(const char* str) noexcept;
    constexpr small_string& operator=(const std::string_view str) noexcept;

    constexpr void assign(const std::string_view str) noexcept;
    constexpr void clear() noexcept;
    void           resize(std::integral auto size) noexcept;
    constexpr bool empty() const noexcept;

    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
    constexpr int      capacity() const noexcept;

    constexpr reference       operator[](std::integral auto index) noexcept;
    constexpr const_reference operator[](
      std::integral auto index) const noexcept;

    constexpr std::string_view   sv() const noexcept;
    constexpr std::u8string_view u8sv() const noexcept;
    constexpr const char*        c_str() const noexcept;

    constexpr iterator       begin() noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr bool operator==(const small_string& rhs) const noexcept;
    constexpr bool operator!=(const small_string& rhs) const noexcept;
    constexpr bool operator>(const small_string& rhs) const noexcept;
    constexpr bool operator<(const small_string& rhs) const noexcept;
    constexpr bool operator==(const char* rhs) const noexcept;
    constexpr bool operator!=(const char* rhs) const noexcept;
    constexpr bool operator>(const char* rhs) const noexcept;
    constexpr bool operator<(const char* rhs) const noexcept;
};

//! @brief A ring-buffer based on a fixed size container. m_head point to the
//! first element can be dequeue while m_tail point to the first constructible
//! element in the ring.
//! @tparam T Any type (trivial or not).
template<class T>
class ring_buffer
{
public:
    using size_type       = u64;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    static_assert((std::is_nothrow_constructible_v<T> ||
                   std::is_nothrow_move_constructible_v<
                     T>)&&std::is_nothrow_destructible_v<T>);

public:
    T* buffer = nullptr;

private:
    i32 m_head     = 0;
    i32 m_tail     = 0;
    i32 m_capacity = 0;

    constexpr i32 advance(i32 position) const noexcept;
    constexpr i32 back(i32 position) const noexcept;

public:
    class iterator;
    class const_iterator;

    class iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = T&;
        using const_pointer     = const T*;
        using const_reference   = const T&;

        friend ring_buffer;

    private:
        ring_buffer* ring;
        i32          i;

        iterator(ring_buffer* ring_, i32 i_) noexcept
          : ring(ring_)
          , i(i_)
        {
        }

    public:
        ring_buffer* buffer() noexcept { return ring; }
        i32          index() noexcept { return i; }

        iterator() noexcept
          : ring(nullptr)
          , i(0)
        {
        }

        iterator(const iterator& other) noexcept
          : ring(other.ring)
          , i(other.i)
        {
        }

        iterator& operator=(const iterator& other) noexcept
        {
            if (this != &other) {
                ring = const_cast<ring_buffer*>(other.ring);
                i    = other.i;
            }

            return *this;
        }

        reference operator*() const noexcept { return ring->buffer[i]; }
        pointer   operator->() const noexcept { return &ring->buffer[i]; }

        iterator& operator++() noexcept
        {
            if (ring) {
                i = ring->advance(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        iterator operator++(int) noexcept
        {
            iterator orig(*this);
            ++(*this);

            return orig;
        }

        iterator& operator--() noexcept
        {
            if (ring) {
                i = ring->back(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        iterator operator--(int) noexcept
        {
            iterator orig(*this);
            --(*orig);

            return orig;
        }

        bool operator==(const_iterator rhs) const noexcept
        {
            return ring == rhs.ring && i == rhs.i;
        }

        bool operator==(iterator rhs) const noexcept
        {
            return ring == rhs.ring && i == rhs.i;
        }

        bool operator!=(const_iterator rhs) const noexcept
        {
            return !(ring == rhs.ring && i == rhs.i);
        }

        bool operator!=(iterator rhs) const noexcept
        {
            return !(ring == rhs.ring && i == rhs.i);
        }

        void reset() noexcept
        {
            ring = nullptr;
            i    = 0;
        }
    };

    class const_iterator
    {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;
        using reference         = T&;
        using const_pointer     = const T*;
        using const_reference   = const T&;

        friend ring_buffer;

    private:
        const ring_buffer* ring;
        i32                i;

        const_iterator(const ring_buffer* ring_, i32 i_) noexcept
          : ring(ring_)
          , i(i_)
        {
        }

    public:
        const ring_buffer* buffer() const noexcept { return ring; }
        i32                index() const noexcept { return i; }

        const_iterator() noexcept
          : ring(nullptr)
          , i(0)
        {
        }

        const_iterator(const const_iterator& other) noexcept
          : ring(other.ring)
          , i(other.i)
        {
        }

        const_iterator& operator=(const const_iterator& other) noexcept
        {
            if (this != &other) {
                ring = const_cast<const ring_buffer*>(other.ring);
                i    = other.i;
            }

            return *this;
        }

        const_reference operator*() const noexcept { return ring->buffer[i]; }
        const_pointer   operator->() const noexcept { return &ring->buffer[i]; }

        const_iterator& operator++() noexcept
        {
            if (ring) {
                i = ring->advance(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            const_iterator orig(*this);
            ++(*this);

            return orig;
        }

        const_iterator& operator--() noexcept
        {
            if (ring) {
                i = ring->back(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        const_iterator operator--(int) noexcept
        {
            const_iterator orig(*this);
            --(*orig);

            return orig;
        }

        bool operator==(const_iterator rhs) const noexcept
        {
            return ring == rhs.ring && i == rhs.i;
        }

        bool operator==(iterator rhs) const noexcept
        {
            return ring == rhs.ring && i == rhs.i;
        }

        bool operator!=(const_iterator rhs) const noexcept
        {
            return !(ring == rhs.ring && i == rhs.i);
        }

        bool operator!=(iterator rhs) const noexcept
        {
            return !(ring == rhs.ring && i == rhs.i);
        }

        void reset() noexcept
        {
            ring = nullptr;
            i    = 0;
        }
    };

    friend class iterator;
    friend class const_iterator;

    constexpr ring_buffer() noexcept = default;
    constexpr ring_buffer(std::integral auto capacity) noexcept;
    constexpr ~ring_buffer() noexcept;

    constexpr ring_buffer(const ring_buffer& rhs) noexcept;
    constexpr ring_buffer& operator=(const ring_buffer& rhs) noexcept;
    constexpr ring_buffer(ring_buffer&& rhs) noexcept;
    constexpr ring_buffer& operator=(ring_buffer&& rhs) noexcept;

    constexpr void swap(ring_buffer& rhs) noexcept;
    constexpr void clear() noexcept;
    constexpr void reset(std::integral auto capacity) noexcept;

    template<typename... Args>
    constexpr bool emplace_front(Args&&... args) noexcept;
    template<typename... Args>
    constexpr bool emplace_back(Args&&... args) noexcept;

    constexpr bool push_front(const T& item) noexcept;
    constexpr void pop_front() noexcept;
    constexpr bool push_back(const T& item) noexcept;
    constexpr void pop_back() noexcept;
    constexpr void erase_after(iterator not_included) noexcept;
    constexpr void erase_before(iterator not_included) noexcept;

    template<typename... Args>
    constexpr bool emplace_enqueue(Args&&... args) noexcept;

    template<typename... Args>
    constexpr void force_emplace_enqueue(Args&&... args) noexcept;

    constexpr void force_enqueue(const T& item) noexcept;
    constexpr bool enqueue(const T& item) noexcept;
    constexpr void dequeue() noexcept;

    constexpr T*       data() noexcept;
    constexpr const T* data() const noexcept;
    constexpr T&       operator[](std::integral auto index) noexcept;
    constexpr const T& operator[](std::integral auto index) const noexcept;

    constexpr T&       front() noexcept;
    constexpr const T& front() const noexcept;
    constexpr T&       back() noexcept;
    constexpr const T& back() const noexcept;

    constexpr iterator       head() noexcept;
    constexpr const_iterator head() const noexcept;
    constexpr iterator       tail() noexcept;
    constexpr const_iterator tail() const noexcept;

    constexpr iterator       begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
    constexpr i32      capacity() const noexcept;
    constexpr i32      available() const noexcept;
    constexpr bool     empty() const noexcept;
    constexpr bool     full() const noexcept;
    constexpr i32      index_from_begin(i32 index) const noexcept;
    constexpr i32      head_index() const noexcept;
    constexpr i32      tail_index() const noexcept;
};

//! @brief A small array of reals.
//!
//! This class in mainly used to store message and observation in the simulation
//! kernel.
template<int length>
struct fixed_real_array
{
    using value_type = real;

    static_assert(length >= 1, "fixed_real_array length must be >= 1");

    real data[length]{};

    constexpr unsigned size() const noexcept
    {
        for (unsigned i = length; i != 0u; --i)
            if (data[i - 1u])
                return i;

        return 0u;
    }

    constexpr int ssize() const noexcept { return static_cast<int>(size()); }

    constexpr fixed_real_array() noexcept                            = default;
    constexpr ~fixed_real_array() noexcept                           = default;
    constexpr fixed_real_array(const fixed_real_array& rhs) noexcept = default;
    constexpr fixed_real_array(fixed_real_array&& rhs) noexcept      = default;

    constexpr fixed_real_array& operator=(
      const fixed_real_array& rhs) noexcept = default;
    constexpr fixed_real_array& operator=(fixed_real_array&& rhs) noexcept =
      default;

    template<typename... Args>
    constexpr fixed_real_array(Args&&... args) noexcept
      : data{ std::forward<Args>(args)... }
    {
        auto size = sizeof...(args);
        for (; size < length; ++size)
            data[size] = zero;
    }

    template<typename U,
             typename = std::enable_if_t<std::is_convertible_v<U, irt::real>>>
    constexpr fixed_real_array(std::initializer_list<U> ilist) noexcept
    {
        irt_assert(ilist.size() <= length);

        int index = 0;
        for (const U& element : ilist) {
            data[index++] = element;
        }
    }

    constexpr real operator[](std::integral auto i) const noexcept
    {
        return data[i];
    }

    constexpr real& operator[](std::integral auto i) noexcept
    {
        return data[i];
    }

    constexpr void reset() noexcept { std::fill_n(data, length, zero); }
};

using message             = fixed_real_array<3>;
using dated_message       = fixed_real_array<4>;
using observation_message = fixed_real_array<5>;

/*****************************************************************************
 *
 * Flat list
 *
 ****************************************************************************/

template<typename T>
class block_allocator
{
public:
    using value_type = T;

    static_assert(std::is_trivially_destructible_v<T>,
                  "T must be trivially destructible");

    union block
    {
        block*                                                     next;
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
    };

private:
    block* m_blocks{ nullptr };    // contains all preallocated blocks
    block* m_free_head{ nullptr }; // a free list
    i32    m_size{ 0 };            // number of active elements allocated
    i32    m_max_size{ 0 }; // number of elements allocated (with free_head)
    i32    m_capacity{ 0 }; // capacity of the allocator

public:
    block_allocator() noexcept = default;

    block_allocator(const block_allocator&)            = delete;
    block_allocator& operator=(const block_allocator&) = delete;

    block_allocator(block_allocator&& rhs) noexcept
      : m_blocks{ rhs.m_blocks }
      , m_free_head{ rhs.m_free_head }
      , m_size{ rhs.m_size }
      , m_max_size{ rhs.m_max_size }
      , m_capacity{ rhs.m_capacity }
    {
        rhs.m_blocks    = nullptr;
        rhs.m_free_head = nullptr;
        rhs.m_size      = 0;
        rhs.m_max_size  = 0;
        rhs.m_capacity  = 0;
    }

    block_allocator& operator=(block_allocator&& rhs) noexcept
    {
        if (this != &rhs) {
            m_blocks        = rhs.m_blocks;
            m_free_head     = rhs.m_free_head;
            m_size          = rhs.m_size;
            m_max_size      = rhs.m_max_size;
            m_capacity      = rhs.m_capacity;
            rhs.m_blocks    = nullptr;
            rhs.m_free_head = nullptr;
            rhs.m_size      = 0;
            rhs.m_max_size  = 0;
            rhs.m_capacity  = 0;
        }
        return *this;
    }

    ~block_allocator() noexcept
    {
        if (m_blocks)
            g_free_fn(m_blocks);
    }

    status init(std::integral auto new_capacity) noexcept
    {
        if (std::cmp_less_equal(new_capacity, 0))
            return status::block_allocator_bad_capacity;

        if (std::cmp_greater_equal(
              new_capacity, std::numeric_limits<decltype(m_capacity)>::max()))
            return status::block_allocator_bad_capacity;

        if (std::cmp_not_equal(new_capacity, m_capacity)) {
            if (m_blocks)
                g_free_fn(m_blocks);

            const auto new_size = static_cast<sz>(new_capacity) * sizeof(block);
            m_blocks            = static_cast<block*>(g_alloc_fn(new_size));
            if (m_blocks == nullptr)
                return status::block_allocator_not_enough_memory;
        }

        m_size      = 0;
        m_max_size  = 0;
        m_capacity  = static_cast<decltype(m_capacity)>(new_capacity);
        m_free_head = nullptr;

        return status::success;
    }

    status copy_to(block_allocator& dst) const noexcept
    {
        dst.reset();
        dst.m_size      = m_size;
        dst.m_max_size  = m_max_size;
        dst.m_capacity  = m_max_size;
        dst.m_free_head = nullptr;

        std::uninitialized_copy_n(m_blocks, m_max_size, dst.m_blocks);

        if (m_free_head) {
            dst.m_free_head = &m_blocks[m_free_head - m_blocks];

            const auto* src_free_list = m_free_head->next;
            auto*       dst_free_list = dst.m_free_head;
            dst_free_list->next       = nullptr;

            while (src_free_list) {
                const auto ptr_diff = src_free_list - m_blocks;
                dst_free_list->next = &dst.m_blocks[ptr_diff];

                src_free_list = src_free_list->next;
                dst_free_list = dst_free_list->next;
            }

            dst_free_list->next = nullptr;
        }

        return status::success;
    }

    void swap(block_allocator& rhs) noexcept
    {
        std::swap(m_blocks, rhs.m_blocks);
        std::swap(m_free_head, rhs.m_free_head);
        std::swap(m_size, rhs.m_size);
        std::swap(m_max_size, rhs.m_max_size);
        std::swap(m_capacity, rhs.m_capacity);
    }

    void reset() noexcept
    {
        if (m_capacity > 0) {
            m_size      = 0;
            m_max_size  = 0;
            m_free_head = nullptr;
        }
    }

    T* alloc() noexcept
    {
        block* new_block = nullptr;

        if (m_free_head != nullptr) {
            new_block   = m_free_head;
            m_free_head = m_free_head->next;
        } else {
            irt_assert(m_max_size < m_capacity);
            new_block = reinterpret_cast<block*>(&m_blocks[m_max_size++]);
        }

        ++m_size;

        return reinterpret_cast<T*>(new_block);
    }

    u32 index(const T* ptr) const noexcept
    {
        irt_assert(ptr - reinterpret_cast<T*>(m_blocks) > 0);
        auto ptrdiff = ptr - reinterpret_cast<T*>(m_blocks);
        return static_cast<u32>(ptrdiff);
    }

    u32 alloc_index() noexcept
    {
        block* new_block = nullptr;

        if (m_free_head != nullptr) {
            new_block   = m_free_head;
            m_free_head = m_free_head->next;
        } else {
            irt_assert(m_max_size < m_capacity);
            new_block = reinterpret_cast<block*>(&m_blocks[m_max_size++]);
        }

        ++m_size;

        irt_assert(new_block - m_blocks >= 0);
        return static_cast<u32>(new_block - m_blocks);
    }

    bool can_alloc() noexcept
    {
        return m_free_head != nullptr || m_max_size < m_capacity;
    }

    void free(T* n) noexcept
    {
        irt_assert(n);

        block* ptr = reinterpret_cast<block*>(n);

        ptr->next   = m_free_head;
        m_free_head = ptr;

        --m_size;

        if (m_size == 0) {         // A special part: if it no longer exists
            m_max_size  = 0;       // we reset the free list and the number
            m_free_head = nullptr; // of elements allocated.
        }
    }

    void free(std::integral auto index) noexcept
    {
        irt_assert(std::cmp_greater_equal(index, 0));
        irt_assert(std::cmp_less_equal(index, m_max_size));

        auto* to_free = reinterpret_cast<T*>(&(m_blocks[index]));
        free(to_free);
    }

    bool can_alloc(std::integral auto number) const noexcept
    {
        return std::cmp_less(static_cast<decltype(m_size)>(number) + m_size,
                             m_capacity);
    }

    value_type& operator[](std::integral auto index) noexcept
    {
        irt_assert(std::cmp_greater_equal(index, 0));
        irt_assert(std::cmp_less_equal(index, m_max_size));

        return *reinterpret_cast<T*>(&(m_blocks[index]));
    }

    const value_type& operator[](std::integral auto index) const noexcept
    {
        irt_assert(std::cmp_greater_equal(index, 0));
        irt_assert(std::cmp_less_equal(index, m_max_size));

        return *reinterpret_cast<T*>(&(m_blocks[index]));
    }

    unsigned size() const noexcept { return static_cast<unsigned>(m_size); }
    int      ssize() const noexcept { return m_size; }
    int      max_size() const noexcept { return m_max_size; }
    int      capacity() const noexcept { return m_capacity; }
};

template<typename T>
struct list_view_node
{
    using value_type = T;

    static_assert(std::is_trivially_destructible_v<T>,
                  "T must be trivially destructible");

    T   value;
    u32 prev;
    u32 next;
};

template<typename T>
class list_view;

template<typename T>
class list_view_const;

template<typename ContainerType, typename T>
class list_view_iterator_impl
{
public:
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value             = T;
    using pointer           = T*;
    using reference         = T&;
    using container_type    = ContainerType;

    template<typename X>
    friend class list_view;

    template<typename X>
    friend class list_view_const;

private:
    container_type& lst;
    u32             id;

public:
    list_view_iterator_impl(container_type& lst_, u32 id_) noexcept
      : lst(lst_)
      , id(id_)
    {
    }

    list_view_iterator_impl(const list_view_iterator_impl& other) noexcept
      : lst(other.lst)
      , id(other.id)
    {
    }

    list_view_iterator_impl& operator=(
      const list_view_iterator_impl& other) noexcept
    {
        list_view_iterator_impl tmp(other);

        irt_assert(&tmp.lst == &lst);
        std::swap(tmp.id, id);

        return *this;
    }

    reference operator*() const { return lst.m_allocator[id].value; }

    pointer operator->() { return &lst.m_allocator[id].value; }

    list_view_iterator_impl& operator++()
    {
        if (id == static_cast<u32>(-1)) {
            id = unpack_doubleword_left(lst.m_list);
        } else {
            id = lst.m_allocator[id].next;
        }

        return *this;
    }

    list_view_iterator_impl operator++(int)
    {
        list_view_iterator_impl tmp = *this;
        if (id == static_cast<u32>(-1)) {
            id = unpack_doubleword_left(lst.m_list);
        } else {
            id = lst.m_allocator[id].next;
        }
        return tmp;
    }

    list_view_iterator_impl& operator--()
    {
        if (id == static_cast<u32>(-1)) {
            id = unpack_doubleword_right(lst.m_list);
        } else {
            id = lst.m_allocator[id].prev;
        }
        return *this;
    }

    list_view_iterator_impl operator--(int)
    {
        list_view_iterator_impl tmp = *this;
        if (id == static_cast<u32>(-1)) {
            id = unpack_doubleword_right(lst.m_list);
        } else {
            id = lst.m_allocator[id].prev;
        }
        return tmp;
    }

    friend bool operator==(const list_view_iterator_impl& a,
                           const list_view_iterator_impl& b)
    {
        return &a.lst == &b.lst && a.id == b.id;
    }

    friend bool operator!=(const list_view_iterator_impl& a,
                           const list_view_iterator_impl& b)
    {
        return &a.lst != &b.lst || a.id != b.id;
    }
};

template<typename T>
class list_view
{
public:
    using node_type       = list_view_node<T>;
    using allocator_type  = block_allocator<node_type>;
    using value_type      = T;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using this_type       = list_view<T>;
    using iterator        = list_view_iterator_impl<this_type, T>;
    using const_iterator  = list_view_iterator_impl<this_type, const T>;

    // friend class list_view_iterator_impl<this_type, T>;
    template<typename X, typename Y>
    friend class list_view_iterator_impl;

private:
    allocator_type& m_allocator;
    u64&            m_list;

public:
    list_view(allocator_type& allocator, u64& id) noexcept
      : m_allocator(allocator)
      , m_list(id)
    {
    }

    ~list_view() noexcept = default;

    void clear() noexcept
    {
        u32 begin = unpack_doubleword_left(m_list);
        while (begin != static_cast<u32>(-1)) {
            auto to_delete = begin;
            begin          = m_allocator[begin].next;
            m_allocator.free(to_delete);
        }

        m_list = static_cast<u64>(-1);
    }

    bool empty() const noexcept { return m_list == static_cast<u64>(-1); }

    iterator begin() noexcept
    {
        return iterator(*this, unpack_doubleword_left(m_list));
    }

    iterator end() noexcept { return iterator(*this, static_cast<u32>(-1)); }

    const_iterator begin() const noexcept
    {
        return const_iterator(*this, unpack_doubleword_left(m_list));
    }

    const_iterator end() const noexcept
    {
        return const_iterator(*this, static_cast<u32>(-1));
    }

    reference front() noexcept
    {
        irt_assert(!empty());

        return m_allocator[unpack_doubleword_left(m_list)].value;
    }

    const_reference front() const noexcept
    {
        irt_assert(!empty());

        return m_allocator[unpack_doubleword_left(m_list)].value;
    }

    reference back() noexcept
    {
        irt_assert(!empty());

        return m_allocator[unpack_doubleword_right(m_list)].value;
    }

    const_reference back() const noexcept
    {
        irt_assert(!empty());

        return m_allocator[unpack_doubleword_right(m_list)].value;
    }

    //! @brief Inserts a new element into the container directly before pos.
    template<typename... Args>
    iterator emplace(iterator pos, Args&&... args) noexcept
    {
        if (pos.id == static_cast<u32>(-1))
            return emplace_back(std::forward<Args>(args)...);

        if (m_allocator[pos.id].prev == static_cast<u32>(-1))
            return emplace_front(std::forward<Args>(args)...);

        u32 new_node = m_allocator.alloc_index();
        std::construct_at(&m_allocator[new_node].value,
                          std::forward<Args>(args)...);

        m_allocator[new_node].prev = pos.id;
        m_allocator[new_node].next = m_allocator[pos.id].next;
        m_allocator[pos.id].next   = new_node;

        return iterator(*this, new_node);
    }

    //! @brief Erases the specified elements from the container.
    //!
    //! If pos refers to the last element, then the end() iterator is returned.
    //! If last==end() prior to removal, then the updated end() iterator is
    //! returned. If [first, last) is an empty range, then last is returned.
    //!
    //! @return iIterator following the last removed element.
    iterator erase(iterator pos) noexcept
    {
        if (pos.id == static_cast<u32>(-1))
            return end();

        if (m_allocator[pos.id].prev == static_cast<u32>(-1)) {
            pop_front();
            return begin();
        }

        if (m_allocator[pos.id].next == static_cast<u32>(-1)) {
            pop_back();
            return end();
        }

        auto prev = m_allocator[pos.id].prev;
        auto next = m_allocator[pos.id].next;

        m_allocator[prev].next = next;
        m_allocator[next].prev = prev;

        std::destroy_at(&m_allocator[pos.id].value);

        m_allocator.free(pos.id);

        return iterator(*this, next);
    }

    template<typename... Args>
    iterator emplace_front(Args&&... args) noexcept
    {
        irt_assert(m_allocator.can_alloc());

        u32 new_node = m_allocator.alloc_index();
        std::construct_at(&m_allocator[new_node].value,
                          std::forward<Args>(args)...);

        u32 first, last;
        unpack_doubleword(m_list, &first, &last);

        if (m_list == static_cast<u64>(-1)) {
            m_allocator[new_node].prev = static_cast<u32>(-1);
            m_allocator[new_node].next = static_cast<u32>(-1);
            first                      = new_node;
            last                       = new_node;
        } else {
            m_allocator[new_node].prev = static_cast<u32>(-1);
            m_allocator[new_node].next = first;
            m_allocator[first].prev    = new_node;
            first                      = new_node;
        }

        m_list = make_doubleword(first, last);

        return iterator(*this, new_node);
    }

    template<typename... Args>
    iterator emplace_back(Args&&... args) noexcept
    {
        irt_assert(m_allocator.can_alloc());

        u32 new_node = m_allocator.alloc_index();
        std::construct_at(&m_allocator[new_node].value,
                          std::forward<Args>(args)...);

        u32 first, last;
        unpack_doubleword(m_list, &first, &last);

        if (m_list == static_cast<u64>(-1)) {
            m_allocator[new_node].prev = static_cast<u32>(-1);
            m_allocator[new_node].next = static_cast<u32>(-1);
            first                      = new_node;
            last                       = new_node;
        } else {
            m_allocator[new_node].next = static_cast<u32>(-1);
            m_allocator[new_node].prev = last;
            m_allocator[last].next     = new_node;
            last                       = new_node;
        }

        m_list = make_doubleword(first, last);

        return iterator(*this, new_node);
    }

    iterator push_back(const T& t) noexcept
    {
        irt_assert(m_allocator.can_alloc());

        u32 new_node = m_allocator.alloc_index();
        std::construct_at(&m_allocator[new_node].value, t);

        u32 first, last;
        unpack_doubleword(m_list, &first, &last);

        if (m_list == static_cast<u64>(-1)) {
            m_allocator[new_node].prev = static_cast<u32>(-1);
            m_allocator[new_node].next = static_cast<u32>(-1);
            first                      = new_node;
            last                       = new_node;
        } else {
            m_allocator[new_node].next = static_cast<u32>(-1);
            m_allocator[new_node].prev = last;
            m_allocator[last].next     = new_node;
            last                       = new_node;
        }

        m_list = make_doubleword(first, last);

        return iterator(*this, new_node);
    }

    void pop_front() noexcept
    {
        if (m_list == static_cast<u64>(-1))
            return;

        u32 begin, end;
        unpack_doubleword(m_list, &begin, &end);

        u32 to_delete = begin;
        begin         = m_allocator[to_delete].next;

        if (begin == static_cast<u32>(-1))
            end = static_cast<u32>(-1);
        else
            m_allocator[begin].prev = static_cast<u32>(-1);

        std::destroy_at(&m_allocator[to_delete].value);

        m_allocator.free(to_delete);
        m_list = make_doubleword(begin, end);
    }

    void pop_back() noexcept
    {
        if (m_list == static_cast<u64>(-1))
            return;

        u32 begin, end;
        unpack_doubleword(m_list, &begin, &end);

        u32 to_delete = end;
        end           = m_allocator[to_delete].prev;

        if (end == static_cast<u32>(-1))
            begin = static_cast<u32>(-1);
        else
            m_allocator[end].next = static_cast<u32>(-1);

        std::destroy_at(&m_allocator[to_delete].value);

        m_allocator.free(to_delete);
        m_list = make_doubleword(begin, end);
    }
};

template<typename T>
class list_view_const
{
public:
    using node_type       = list_view_node<T>;
    using allocator_type  = block_allocator<node_type>;
    using value_type      = T;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using this_type       = list_view_const<T>;
    using iterator        = list_view_iterator_impl<this_type, T>;
    using const_iterator  = list_view_iterator_impl<this_type, const T>;

    template<typename X, typename Y>
    friend class list_view_iterator_impl;

private:
    const allocator_type& m_allocator;
    const u64             m_list;

public:
    list_view_const(const allocator_type& allocator, const u64 id) noexcept
      : m_allocator(allocator)
      , m_list(id)
    {
    }

    ~list_view_const() noexcept = default;

    bool empty() const noexcept { return m_list == static_cast<u64>(-1); }

    const_iterator begin() noexcept
    {
        return const_iterator(*this, unpack_doubleword_left(m_list));
    }

    const_iterator end() noexcept
    {
        return const_iterator(*this, static_cast<u32>(-1));
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(*this, unpack_doubleword_left(m_list));
    }

    const_iterator end() const noexcept
    {
        return const_iterator(*this, static_cast<u32>(-1));
    }

    const_reference front() noexcept
    {
        irt_assert(!empty());

        return m_allocator[unpack_doubleword_left(m_list)].value;
    }

    const_reference front() const noexcept
    {
        irt_assert(!empty());

        return m_allocator[unpack_doubleword_left(m_list)].value;
    }

    const_reference back() noexcept
    {
        irt_assert(!empty());

        return m_allocator[unpack_doubleword_right(m_list)].value;
    }

    const_reference back() const noexcept
    {
        irt_assert(!empty());

        return m_allocator[unpack_doubleword_right(m_list)].value;
    }
};

/*****************************************************************************
 *
 * data-array
 *
 ****************************************************************************/

enum class model_id : u64;
enum class dynamics_id : u64;
enum class message_id : u64;
enum class observer_id : u64;

template<typename Identifier>
constexpr auto get_index(Identifier id) noexcept
{
    static_assert(std::is_enum<Identifier>::value,
                  "Identifier must be a enumeration");

    if constexpr (std::is_same_v<u32, std::underlying_type_t<Identifier>>)
        return static_cast<u16>(static_cast<u32>(id) & 0xffff);
    else
        return static_cast<u32>(static_cast<u64>(id) & 0xffffffff);
}

template<typename Identifier>
constexpr auto get_key(Identifier id) noexcept
{
    static_assert(std::is_enum<Identifier>::value,
                  "Identifier must be a enumeration");

    if constexpr (std::is_same_v<u32, std::underlying_type_t<Identifier>>)
        return static_cast<u16>((static_cast<u32>(id) >> 16) & 0xffff);
    else
        return static_cast<u32>((static_cast<u64>(id) >> 32) & 0xffffffff);
}

template<typename Identifier>
constexpr bool is_valid(Identifier id) noexcept
{
    return get_key(id) > 0;
}

//! @brief An optimized fixed size array for dynamics objects.
//!
//! A container to handle everything from trivial, pod or object.
//! - linear memory/iteration
//! - O(1) alloc/free
//! - stable indices
//! - weak references
//! - zero overhead dereferences
//!
//! @tparam T The type of object the data_array holds.
//! @tparam Identifier A enum class identifier to store identifier unsigned
//!     number.
//! @todo Make Identifier split key|id automatically for all unsigned.
template<typename T, typename Identifier>
class data_array
{
public:
    static_assert(
      std::is_enum_v<Identifier>,
      "Identifier must be a enumeration: enum class id : u32 or u64;");

    static_assert(std::is_same_v<std::underlying_type_t<Identifier>, u32> ||
                    std::is_same_v<std::underlying_type_t<Identifier>, u64>,
                  "Identifier underlying type must be u32 or u64");

    using underlying_id_type = std::conditional_t<
      std::is_same_v<u32, std::underlying_type_t<Identifier>>,
      u32,
      u64>;

    using index_type =
      std::conditional_t<std::is_same_v<u32, underlying_id_type>, u16, u32>;

    using identifier_type = Identifier;
    using value_type      = T;

private:
    struct item
    {
        T          item;
        Identifier id;
    };

    item*      m_items     = nullptr; // items array
    index_type m_max_size  = 0;       // number of valid item
    index_type m_max_used  = 0;       // highest index ever allocated
    index_type m_capacity  = 0;       // capacity of the array
    index_type m_next_key  = 1;       // [1..2^32-64] (never == 0)
    index_type m_free_head = none;    // index of first free entry

    //! Build a new identifier merging m_next_key and the best free index.
    static constexpr identifier_type make_id(index_type key,
                                             index_type index) noexcept;

    //! Build a new m_next_key and avoid key 0.
    static constexpr index_type make_next_key(index_type key) noexcept;

    //! Get the index part of the identifier
    static constexpr index_type get_index(Identifier id) noexcept;

    //! Get the key part of the identifier
    static constexpr index_type get_key(Identifier id) noexcept;

public:
    static constexpr index_type none = std::numeric_limits<index_type>::max();

    data_array() noexcept = default;
    ~data_array() noexcept;

    //! @brief Allocate the underlying buffer of T.
    //!
    //! @details If a buffer is already allocated it will be
    //! deallocated
    //!  before a new allocation.
    //!
    //! @return @c is_success(status) or is_bad(status).
    status init(std::integral auto capacity) noexcept;

    //! @brief Reallocate the underlying buffer.
    //!
    //! @details If the new capacity is smaller than the current capacity then
    //!  do nothing otherwise, a new buffer is allocated and all data is
    //!  copied. Be careful, as for vector, resize operation makes all
    //!  iterators and pointers unusable.
    //!
    //! @return @c is_success(status) or is_bad(status).
    status resize(std::integral auto capacity) noexcept;

    //! @brief Destroy all items in the data_array but keep memory
    //!  allocation.
    //!
    //! @details Run the destructor if @c T is not trivial on outstanding
    //!  items and re-initialize the size.
    void clear() noexcept;

    //! @brief Alloc a new element.
    //!
    //! If m_max_size == m_capacity then this function will abort. Before using
    //! this function, tries @c !can_alloc() for example otherwise use the @c
    //! try_alloc function.
    //!
    //! Use @c m_free_head if not empty or use a new items from buffer
    //! (@m_item[max_used++]). The id is set to from @c next_key++ << 32) |
    //! index to build unique identifier.
    //!
    //! @return A reference to the newly allocated element.
    template<typename... Args>
    T& alloc(Args&&... args) noexcept;

    //! @brief Alloc a new element.
    //!
    //! Use @c m_free_head if not empty or use a new items from buffer
    //! (@m_item[max_used++]). The id is set to from @c next_key++ << 32) |
    //! index to build unique identifier.
    //!
    //! @return A pair with a boolean if the allocation success and a pointer
    //! to the newly element.
    template<typename... Args>
    std::pair<bool, T*> try_alloc(Args&&... args) noexcept;

    //! @brief Free the element @c t.
    //!
    //! Internally, puts the elelent @c t entry on free list and use id to
    //! store next.
    void free(T& t) noexcept;

    //! @brief Free the element pointer by @c id.
    //!
    //! Internally, puts the element @c id entry on free list and use id to
    //! store next.
    void free(Identifier id) noexcept;

    //! @brief Accessor to the id part of the item
    //!
    //! @return @c Identifier.
    Identifier get_id(const T* t) const noexcept;

    //! @brief Accessor to the id part of the item
    //!
    //! @return @c Identifier.
    Identifier get_id(const T& t) const noexcept;

    //! @brief Accessor to the item part of the id.
    //!
    //! @return @c T
    T& get(Identifier id) noexcept;

    //! @brief Accessor to the item part of the id.
    //!
    //! @return @c T
    const T& get(Identifier id) const noexcept;

    //! @brief Get a T from an ID.
    //!
    //! @details Validates ID, then returns item, returns null if invalid.
    //! For cases like AI references and others where 'the thing might have
    //! been deleted out from under me.
    //!
    //! @param id Identifier to get.
    //! @return T or nullptr
    T* try_to_get(Identifier id) const noexcept;

    //! @brief Get a T directly from the index in the array.
    //!
    //! @param index The array.
    //! @return T or nullptr.
    T* try_to_get(std::integral auto index) const noexcept;

    //! @brief Return next valid item.
    //! @code
    //! data_array<int> d;
    //! ...
    //! int* value = nullptr;
    //! while (d.next(value)) {
    //!     std::cout << *value << ' ';
    //! }
    //! @endcode
    //!
    //! Loop item where @c id & 0xffffffff00000000 != 0 (i.e. items not on
    //! free list).
    //!
    //! @return true if the paramter @c t is valid false otherwise.
    bool next(T*& t) noexcept;

    //! @brief Return next valid item.
    //! @code
    //! data_array<int> d;
    //! ...
    //! int* value = nullptr;
    //! while (d.next(value)) {
    //!     std::cout << *value << ' ';
    //! }
    //! @endcode
    //!
    //! Loop item where @c id & 0xffffffff00000000 != 0 (i.e. items not on
    //! free list).
    //!
    //! @return true if the paramter @c t is valid false otherwise.
    bool next(const T*& t) const noexcept;

    constexpr bool       full() const noexcept;
    constexpr unsigned   size() const noexcept;
    constexpr int        ssize() const noexcept;
    constexpr bool       can_alloc(std::integral auto nb) const noexcept;
    constexpr bool       can_alloc() const noexcept;
    constexpr int        max_size() const noexcept;
    constexpr int        max_used() const noexcept;
    constexpr int        capacity() const noexcept;
    constexpr index_type next_key() const noexcept;
    constexpr bool       is_free_list_empty() const noexcept;
};

struct record
{
    record() noexcept = default;

    record(real x_dot_, time date_) noexcept
      : x_dot{ x_dot_ }
      , date{ date_ }
    {
    }

    real x_dot{ 0 };
    time date{ time_domain<time>::infinity };
};

//! @brief Pairing heap implementation.
//!
//! A pairing heap is a type of heap data structure with relatively simple
//! implementation and excellent practical amortized performance, introduced
//! by Michael Fredman, Robert Sedgewick, Daniel Sleator, and Robert Tarjan
//! in 1986.
//!
//! https://en.wikipedia.org/wiki/Pairing_heap
class heap
{
private:
    struct node
    {
        time     tn;
        model_id id;

        node* prev;
        node* next;
        node* child;
    };

    u32   m_size{ 0 };
    u32   max_size{ 0 };
    u32   capacity{ 0 };
    node* root{ nullptr };
    node* nodes{ nullptr };
    node* free_list{ nullptr };

public:
    using handle = node*;

    heap() = default;

    ~heap() noexcept
    {
        if (nodes)
            g_free_fn(nodes);
    }

    status init(std::integral auto new_capacity) noexcept
    {
        if (std::cmp_equal(new_capacity, 0))
            return status::head_allocator_bad_capacity;

        if (std::cmp_greater(new_capacity,
                             std::numeric_limits<decltype(capacity)>::max()))
            return status::head_allocator_bad_capacity;

        if (std::cmp_not_equal(new_capacity, capacity)) {
            if (nodes)
                g_free_fn(nodes);

            nodes = static_cast<node*>(
              g_alloc_fn(static_cast<sz>(new_capacity) * sizeof(node)));
            if (nodes == nullptr)
                return status::head_allocator_not_enough_memory;
        }

        m_size    = 0;
        max_size  = 0;
        capacity  = static_cast<decltype(capacity)>(new_capacity);
        root      = nullptr;
        free_list = nullptr;

        return status::success;
    }

    void clear()
    {
        m_size    = 0;
        max_size  = 0;
        root      = nullptr;
        free_list = nullptr;
    }

    handle insert(time tn, model_id id) noexcept
    {
        node* new_node;

        if (free_list) {
            new_node  = free_list;
            free_list = free_list->next;
        } else {
            new_node = &nodes[max_size++];
        }

        new_node->tn    = tn;
        new_node->id    = id;
        new_node->prev  = nullptr;
        new_node->next  = nullptr;
        new_node->child = nullptr;

        insert(new_node);

        return new_node;
    }

    void destroy(handle elem) noexcept
    {
        irt_assert(elem);

        if (m_size == 0) {
            clear();
        } else {
            elem->prev  = nullptr;
            elem->child = nullptr;
            elem->id    = static_cast<model_id>(0);

            elem->next = free_list;
            free_list  = elem;
        }
    }

    void insert(handle elem) noexcept
    {
        elem->prev  = nullptr;
        elem->next  = nullptr;
        elem->child = nullptr;

        ++m_size;

        if (root == nullptr)
            root = elem;
        else
            root = merge(elem, root);
    }

    void remove(handle elem) noexcept
    {
        irt_assert(elem);

        if (elem == root) {
            pop();
            return;
        }

        irt_assert(m_size > 0);

        m_size--;
        detach_subheap(elem);

        if (elem->prev) { /* Not use pop() before. Use in interactive code */
            elem = merge_subheaps(elem);
            root = merge(root, elem);
        }
    }

    handle pop() noexcept
    {
        irt_assert(m_size > 0);

        m_size--;

        auto* top = root;

        if (top->child == nullptr)
            root = nullptr;
        else
            root = merge_subheaps(top);

        top->child = top->next = top->prev = nullptr;

        return top;
    }

    void decrease(handle elem) noexcept
    {
        if (elem->prev == nullptr)
            return;

        detach_subheap(elem);
        root = merge(root, elem);
    }

    void increase(handle elem) noexcept
    {
        remove(elem);
        insert(elem);
    }

    unsigned size() const noexcept { return static_cast<unsigned>(m_size); }
    int      ssize() const noexcept { return static_cast<int>(m_size); }

    bool full() const noexcept { return m_size == capacity; }
    bool empty() const noexcept { return root == nullptr; }

    handle top() const noexcept { return root; }

    void merge(heap& src) noexcept
    {
        if (this == &src)
            return;

        if (root == nullptr) {
            root = src.root;
            return;
        }

        root = merge(root, src.root);
        m_size += src.m_size;
    }

private:
    static node* merge(node* a, node* b) noexcept
    {
        if (a->tn < b->tn) {
            if (a->child != nullptr)
                a->child->prev = b;

            if (b->next != nullptr)
                b->next->prev = a;

            a->next  = b->next;
            b->next  = a->child;
            a->child = b;
            b->prev  = a;

            return a;
        }

        if (b->child != nullptr)
            b->child->prev = a;

        if (a->prev != nullptr && a->prev->child != a)
            a->prev->next = b;

        b->prev  = a->prev;
        a->prev  = b;
        a->next  = b->child;
        b->child = a;

        return b;
    }

    static node* merge_right(node* a) noexcept
    {
        node* b{ nullptr };

        for (; a != nullptr; a = b->next) {
            if ((b = a->next) == nullptr)
                return a;

            b = merge(a, b);
        }

        return b;
    }

    static node* merge_left(node* a) noexcept
    {
        node* b = a->prev;
        for (; b != nullptr; b = a->prev)
            a = merge(b, a);

        return a;
    }

    static node* merge_subheaps(node* a) noexcept
    {
        a->child->prev = nullptr;

        node* e = merge_right(a->child);
        return merge_left(e);
    }

    void detach_subheap(node* elem) noexcept
    {
        if (elem->prev->child == elem)
            elem->prev->child = elem->next;
        else
            elem->prev->next = elem->next;

        if (elem->next != nullptr)
            elem->next->prev = elem->prev;

        elem->prev = nullptr;
        elem->next = nullptr;
    }
};

struct simulation;

/*****************************************************************************
 *
 * @c source and @c source_id are data from files or random generators.
 *
 ****************************************************************************/

static constexpr int external_source_chunk_size = 512;
static constexpr int default_max_client_number  = 32;

using chunk_type = std::array<double, external_source_chunk_size>;

struct source;

enum class distribution_type
{
    bernouilli,
    binomial,
    cauchy,
    chi_squared,
    exponential,
    exterme_value,
    fisher_f,
    gamma,
    geometric,
    lognormal,
    negative_binomial,
    normal,
    poisson,
    student_t,
    uniform_int,
    uniform_real,
    weibull,
};

//! Use a buffer with a set of double real to produce external data. This
//! external source can be shared between @c undefined number of  @c source.
struct constant_source
{
    small_string<23> name;
    chunk_type       buffer;
    u32              length = 0u;

    constant_source() noexcept = default;
    constant_source(const constant_source& src) noexcept;

    status init() noexcept;
    status init(source& src) noexcept;
    status update(source& src) noexcept;
    status restore(source& src) noexcept;
    status finalize(source& src) noexcept;
};

//! Use a file with a set of double real in binary mode (little endian) to
//! produce external data. This external source can be shared between @c
//! max_clients sources. Each client read a @c external_source_chunk_size real
//! of the set.
//!
//! source::chunk_id[0] is used to store the client identifier.
//! source::chunk_id[1] is used to store the current position in the file.
struct binary_file_source
{
    small_string<23>   name;
    vector<chunk_type> buffers; // buffer, size is defined by max_clients
    vector<u64>        offsets; // offset, size is defined by max_clients
    u32                max_clients = 1; // number of source max (must be >= 1).
    u64                max_reals   = 0; // number of real in the file.

    std::filesystem::path file_path;
    std::ifstream         ifs;
    u32                   next_client = 0;
    u64                   next_offset = 0;

    binary_file_source() noexcept = default;
    binary_file_source(const binary_file_source& other) noexcept;

    status init() noexcept;
    void   finalize() noexcept;
    status init(source& src) noexcept;
    status update(source& src) noexcept;
    status restore(source& src) noexcept;
    status finalize(source& src) noexcept;
};

//! Use a file with a set of double real in ascii text file to produce
//! external data. This external source can not be shared between @c source.
//!
//! source::chunk_id[0] is used to store the current position in the file to
//! simplify restore operation.
struct text_file_source
{
    small_string<23> name;
    chunk_type       buffer;
    u64              offset;

    std::filesystem::path file_path;
    std::ifstream         ifs;

    text_file_source() noexcept = default;
    text_file_source(const text_file_source& other) noexcept;

    status init() noexcept;
    void   finalize() noexcept;
    status init(source& src) noexcept;
    status update(source& src) noexcept;
    status restore(source& src) noexcept;
    status finalize(source& src) noexcept;
};

//! Use a prng to produce set of double real. This external source can be
//! shared between @c max_clients sources. Each client read a @c
//  external_source_chunk_size real of the set.
//!
//! The source::chunk_id[0-5] array is used to store the prng state.
struct random_source
{
    using counter_type = std::array<u64, 4>;
    using key_type     = std::array<u64, 4>;
    using result_type  = std::array<u64, 4>;

    small_string<23>           name;
    vector<chunk_type>         buffers;
    vector<std::array<u64, 4>> counters;
    u32          max_clients   = 1; // number of source max (must be >= 1).
    u32          start_counter = 0; // provided by @c external_source class.
    u32          next_client   = 0;
    counter_type ctr;
    key_type     key; // provided by @c external_source class.

    distribution_type distribution = distribution_type::uniform_int;
    double a = 0, b = 1, p, mean, lambda, alpha, beta, stddev, m, s, n;
    int    a32, b32, t32, k32;

    random_source() noexcept = default;
    random_source(const random_source& other) noexcept;

    status init() noexcept;
    void   finalize() noexcept;
    status init(source& src) noexcept;
    status update(source& src) noexcept;
    status restore(source& src) noexcept;
    status finalize(source& src) noexcept;
};

enum class constant_source_id : u64;
enum class binary_file_source_id : u64;
enum class text_file_source_id : u64;
enum class random_source_id : u64;

//! @brief Reference external source from a model.
//!
//! @details A @c source references a external source (file, PRNG, etc.). Model
//! auses the source to get data external to the simulation.
struct source
{
    enum class source_type : i16
    {
        none,
        binary_file, /* Best solution to reproductible simulation. Each client
                        take a part of the stream (substream). */
        constant,    /* Just an easy source to use mode. */
        random,      /* How to retrieve old position in debug mode? */
        text_file    /* How to retreive old position in debug mode? */
    };

    static inline constexpr int source_type_count = 5;

    enum class operation_type
    {
        initialize, // Use to initialize the buffer at simulation init step.
        update,     // Use to update the buffer when all values are read.
        restore,    // Use to restore the buffer when debug mode activated.
        finalize    // Use to clear the buffer at simulation finalize step.
    };

    std::span<double> buffer;
    u64               id   = 0;
    source_type       type = source_type::none;
    i16 index = 0; // The index of the next double to read in current chunk.
    std::array<u64, 4> chunk_id; // Current chunk. Use when restore is apply.

    //! Call to reset the position in the current chunk.
    void reset() noexcept { index = 0u; }

    //! Clear the source (buffer and external source access)
    void clear() noexcept
    {
        buffer = std::span<double>();
        id     = 0u;
        type   = source_type::none;
        index  = 0;
        std::fill_n(chunk_id.data(), chunk_id.size(), 0);
    }

    //! Try to get next double in the buffer.
    //!
    //! @param value stores the new double read from the buffer.
    //! @return true if success, false otherwise.
    bool next(double& value) noexcept
    {
        if (std::cmp_greater_equal(index, buffer.size()))
            return false;

        value = buffer[static_cast<sz>(index)];
        index++;
        return true;
    }
};

struct external_source
{
    data_array<constant_source, constant_source_id>       constant_sources;
    data_array<binary_file_source, binary_file_source_id> binary_file_sources;
    data_array<text_file_source, text_file_source_id>     text_file_sources;
    data_array<random_source, random_source_id>           random_sources;

    u64 seed[2] = { 0xdeadbeef12345678U, 0xdeadbeef12345678U };

    status init(std::integral auto size) noexcept
    {
        irt_return_if_fail(is_numeric_castable<u32>(size),
                           status::data_array_init_capacity_error);

        irt_return_if_bad(constant_sources.init(size));
        irt_return_if_bad(binary_file_sources.init(size));
        irt_return_if_bad(text_file_sources.init(size));
        irt_return_if_bad(random_sources.init(size));
        return status::success;
    }

    //! Call the @c init function for all sources (@c constant_source, @c
    //! binary_file_source etc.).
    status prepare() noexcept;

    //! Call the @c finalize function for all sources (@c constant_source, @c
    //! binary_file_source etc.). Usefull to close opened files.
    void finalize() noexcept;

    status import_from(const external_source& srcs);

    status dispatch(source& src, const source::operation_type op) noexcept;

    //! Call the @c data_array<T, Id>::clear() function for all sources.
    void clear() noexcept;
};

//! To be used in model declaration to initialize a source instance according
//! to the type of the external source.
inline status initialize_source(simulation& sim, source& src) noexcept;

//! To be used in model declaration to get a new real from a source instance
//! according to the type of the external source.
inline status update_source(simulation& sim, source& src, double& val) noexcept;

//! To be used in model declaration to finalize and clear a source instance
//! according to the type of the external source.
inline status finalize_source(simulation& sim, source& src) noexcept;

/*****************************************************************************
 *
 * DEVS Model / Simulation entities
 *
 ****************************************************************************/

enum class dynamics_type : i32
{
    qss1_integrator,
    qss1_multiplier,
    qss1_cross,
    qss1_filter,
    qss1_power,
    qss1_square,
    qss1_sum_2,
    qss1_sum_3,
    qss1_sum_4,
    qss1_wsum_2,
    qss1_wsum_3,
    qss1_wsum_4,
    qss2_integrator,
    qss2_multiplier,
    qss2_cross,
    qss2_filter,
    qss2_power,
    qss2_square,
    qss2_sum_2,
    qss2_sum_3,
    qss2_sum_4,
    qss2_wsum_2,
    qss2_wsum_3,
    qss2_wsum_4,
    qss3_integrator,
    qss3_multiplier,
    qss3_cross,
    qss3_filter,
    qss3_power,
    qss3_square,
    qss3_sum_2,
    qss3_sum_3,
    qss3_sum_4,
    qss3_wsum_2,
    qss3_wsum_3,
    qss3_wsum_4,
    integrator,
    quantifier,
    adder_2,
    adder_3,
    adder_4,
    mult_2,
    mult_3,
    mult_4,
    counter,
    queue,
    dynamic_queue,
    priority_queue,
    generator,
    constant,
    cross,
    time_func,
    accumulator_2,
    filter,
    logical_and_2,
    logical_and_3,
    logical_or_2,
    logical_or_3,
    logical_invert,
    hsm_wrapper
};

constexpr i8 dynamics_type_last() noexcept
{
    return static_cast<i8>(dynamics_type::hsm_wrapper);
}

constexpr sz dynamics_type_size() noexcept
{
    return static_cast<sz>(dynamics_type_last() + 1);
}

struct observer
{
    static constexpr u8 flags_none           = 0;
    static constexpr u8 flags_data_available = 1 << 0;
    static constexpr u8 flags_buffer_full    = 1 << 1;
    static constexpr u8 flags_data_lost      = 1 << 2;

    observer(std::string_view name_,
             u64              user_id_   = 0,
             i32              user_type_ = 0) noexcept;

    void reset() noexcept;
    void clear() noexcept;
    void update(observation_message msg) noexcept;

    bool full() const noexcept;

    ring_buffer<observation_message> buffer;

    model_id      model     = undefined<model_id>();
    u64           user_id   = 0;
    i32           user_type = 0;
    dynamics_type type      = dynamics_type::qss1_integrator;

    small_string<15> name;
    u8               flags;
};

struct node
{
    node() = default;

    node(const model_id model_, const i8 port_index_) noexcept
      : model(model_)
      , port_index(port_index_)
    {
    }

    model_id model      = undefined<model_id>();
    i8       port_index = 0;
};

struct output_message
{
    output_message() noexcept  = default;
    ~output_message() noexcept = default;

    message  msg;
    model_id model = undefined<model_id>();
    i8       port  = 0;
};

using output_port = u64;
using input_port  = u64;

inline bool have_message(const u64 port) noexcept
{
    return port != static_cast<u64>(-1);
}

bool can_alloc_message(const simulation& sim, int alloc_number) noexcept;
bool can_alloc_node(const simulation& sim, int alloc_number) noexcept;
bool can_alloc_dated_message(const simulation& sim, int alloc_number) noexcept;

list_view<message> append_message(simulation& sim, input_port& port) noexcept;
list_view_const<message> get_message(const simulation& sim,
                                     const input_port  port) noexcept;

list_view<node>       append_node(simulation& sim, output_port& port) noexcept;
list_view_const<node> get_node(const simulation& sim,
                               const output_port port) noexcept;

list_view<record>       append_archive(simulation& sim, u64& id) noexcept;
list_view_const<record> get_archive(const simulation& sim,
                                    const u64         id) noexcept;

list_view<dated_message>       append_dated_message(simulation& sim,
                                                    u64&        id) noexcept;
list_view_const<dated_message> get_dated_message(const simulation& sim,
                                                 const u64         id) noexcept;

status send_message(simulation&  sim,
                    output_port& p,
                    real         r1,
                    real         r2 = zero,
                    real         r3 = zero) noexcept;

/////////////////////////////////////////////////////////////////////////////
//
// Some template type-trait to detect function and attribute in DEVS model.
//
/////////////////////////////////////////////////////////////////////////////

template<typename T>
concept has_lambda_function = requires(T t, simulation& sim) {
                                  {
                                      t.lambda(sim)
                                      } -> std::convertible_to<status>;
                              };

template<typename T>
concept has_transition_function =
  requires(T t, simulation& sim, time s, time e, time r) {
      {
          t.transition(sim, s, e, r)
          } -> std::convertible_to<status>;
  };

template<typename T>
concept has_observation_function =
  requires(T t, time s, time e) {
      {
          t.observation(s, e)
          } -> std::convertible_to<observation_message>;
  };

template<typename T>
concept has_initialize_function = requires(T t, simulation& sim) {
                                      {
                                          t.initialize(sim)
                                          } -> std::convertible_to<status>;
                                  };

template<typename T>
concept has_finalize_function = requires(T t, simulation& sim) {
                                    {
                                        t.finalize(sim)
                                        } -> std::convertible_to<status>;
                                };

template<typename T>
concept has_input_port = requires(T t) {
                             {
                                 t.x
                             };
                         };

template<typename T>
concept has_output_port = requires(T t) {
                              {
                                  t.y
                              };
                          };

constexpr observation_message qss_observation(real X,
                                              real u,
                                              time t,
                                              time e) noexcept
{
    return { t, X + u * e };
}

constexpr observation_message qss_observation(real X,
                                              real u,
                                              real mu,
                                              time t,
                                              time e) noexcept
{
    return { t, X + u * e + mu * e * e / two, u + mu * e };
}

constexpr observation_message qss_observation(real X,
                                              real u,
                                              real mu,
                                              real pu,
                                              time t,
                                              time e) noexcept
{
    return { t,
             X + u * e + (mu * e * e) / two + (pu * e * e * e) / three,
             u + mu * e + pu * e * e,
             mu / two + pu * e };
}

struct integrator
{
    input_port  x[3];
    output_port y[1];
    time        sigma = time_domain<time>::zero;

    enum port_name
    {
        port_quanta,
        port_x_dot,
        port_reset
    };

    enum class state
    {
        init,
        wait_for_quanta,
        wait_for_x_dot,
        wait_for_both,
        running
    };

    real default_current_value = 0.0;
    real default_reset_value   = 0.0;
    u64  archive               = static_cast<u64>(-1);

    real  current_value     = 0.0;
    real  reset_value       = 0.0;
    real  up_threshold      = 0.0;
    real  down_threshold    = 0.0;
    real  last_output_value = 0.0;
    real  expected_value    = 0.0;
    bool  reset             = false;
    state st                = state::init;

    integrator() = default;

    integrator(const integrator& other) noexcept
      : default_current_value(other.default_current_value)
      , default_reset_value(other.default_reset_value)
      , archive(static_cast<u64>(-1))
      , current_value(other.current_value)
      , reset_value(other.reset_value)
      , up_threshold(other.up_threshold)
      , down_threshold(other.down_threshold)
      , last_output_value(other.last_output_value)
      , expected_value(other.expected_value)
      , reset(other.reset)
      , st(other.st)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        current_value     = default_current_value;
        reset_value       = default_reset_value;
        up_threshold      = 0.0;
        down_threshold    = 0.0;
        last_output_value = 0.0;
        expected_value    = 0.0;
        reset             = false;
        st                = state::init;
        archive           = static_cast<u64>(-1);
        sigma             = time_domain<time>::zero;

        return status::success;
    }

    status finalize(simulation& sim) noexcept
    {
        append_archive(sim, archive).clear();

        return status::success;
    }

    status external(simulation& sim, time t) noexcept
    {
        if (have_message(x[port_quanta])) {
            auto lst = get_message(sim, x[port_quanta]);
            for (const auto& msg : lst) {
                up_threshold   = msg.data[0];
                down_threshold = msg.data[1];

                if (st == state::wait_for_quanta)
                    st = state::running;

                if (st == state::wait_for_both)
                    st = state::wait_for_x_dot;
            }
        }

        if (have_message(x[port_x_dot])) {
            auto lst          = get_message(sim, x[port_x_dot]);
            auto archive_list = append_archive(sim, archive);
            for (const auto& msg : lst) {
                archive_list.emplace_back(msg.data[0], t);

                if (st == state::wait_for_x_dot)
                    st = state::running;

                if (st == state::wait_for_both)
                    st = state::wait_for_quanta;
            }
        }

        if (have_message(x[port_reset])) {
            auto lst = get_message(sim, x[port_reset]);
            for (const auto& msg : lst) {
                reset_value = msg.data[0];
                reset       = true;
            }
        }

        if (st == state::running) {
            current_value  = compute_current_value(sim, t);
            expected_value = compute_expected_value(sim);
        }

        return status::success;
    }

    status internal(simulation& sim, time t) noexcept
    {
        switch (st) {
        case state::running: {
            last_output_value = expected_value;
            auto lst          = append_archive(sim, archive);

            const real last_derivative_value = lst.back().x_dot;
            lst.clear();
            lst.emplace_back(last_derivative_value, t);
            current_value = expected_value;
            st            = state::wait_for_quanta;
            return status::success;
        }
        case state::init:
            st                = state::wait_for_both;
            last_output_value = current_value;
            return status::success;
        default:
            return status::model_integrator_internal_error;
        }
    }

    status transition(simulation& sim, time t, time /*e*/, time r) noexcept
    {
        if (!have_message(x[port_quanta]) && !have_message(x[port_x_dot]) &&
            !have_message(x[port_reset])) {
            irt_return_if_bad(internal(sim, t));
        } else {
            if (time_domain<time>::is_zero(r))
                irt_return_if_bad(internal(sim, t));

            irt_return_if_bad(external(sim, t));
        }

        return ta(sim);
    }

    status lambda(simulation& sim) noexcept
    {
        switch (st) {
        case state::running:
            return send_message(sim, y[0], expected_value);
        case state::init:
            return send_message(sim, y[0], current_value);
        default:
            return status::model_integrator_output_error;
        }

        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, last_output_value };
    }

    status ta(simulation& sim) noexcept
    {
        if (st == state::running) {
            irt_return_if_fail(static_cast<u64>(-1) != archive,
                               status::model_integrator_running_without_x_dot);

            auto       lst                = get_archive(sim, archive);
            const auto current_derivative = lst.back().x_dot;

            if (current_derivative == time_domain<time>::zero) {
                sigma = time_domain<time>::infinity;
                return status::success;
            }

            if (current_derivative > 0) {
                irt_return_if_fail((up_threshold - current_value) >= 0,
                                   status::model_integrator_ta_with_bad_x_dot);

                sigma = (up_threshold - current_value) / current_derivative;
                return status::success;
            }

            irt_return_if_fail((down_threshold - current_value) <= 0,
                               status::model_integrator_ta_with_bad_x_dot);

            sigma = (down_threshold - current_value) / current_derivative;
            return status::success;
        }

        sigma = time_domain<time>::infinity;
        return status::success;
    }

    real compute_current_value(simulation& sim, time t) noexcept
    {
        if (static_cast<u64>(-1) == archive)
            return reset ? reset_value : last_output_value;

        auto lst  = get_archive(sim, archive);
        auto val  = reset ? reset_value : last_output_value;
        auto end  = lst.end();
        auto it   = lst.begin();
        auto next = lst.begin();

        if (next != end)
            ++next;

        for (; next != end; it = next++)
            val += (next->date - it->date) * it->x_dot;

        val += (t - lst.back().date) * lst.back().x_dot;

        if (up_threshold < val) {
            return up_threshold;
        } else if (down_threshold > val) {
            return down_threshold;
        } else {
            return val;
        }
    }

    real compute_expected_value(simulation& sim) noexcept
    {
        auto       lst                = get_archive(sim, archive);
        const auto current_derivative = lst.back().x_dot;

        if (current_derivative == 0)
            return current_value;

        if (current_derivative > 0)
            return up_threshold;

        return down_threshold;
    }
};

/*****************************************************************************
 *
 * Qss1 part
 *
 ****************************************************************************/

template<int QssLevel>
struct abstract_integrator;

template<>
struct abstract_integrator<1>
{
    input_port  x[2];
    output_port y[1];
    real        default_X  = zero;
    real        default_dQ = irt::real(0.01);
    real        X;
    real        q;
    real        u;
    time        sigma = time_domain<time>::zero;

    enum port_name
    {
        port_x_dot,
        port_reset
    };

    abstract_integrator() = default;

    abstract_integrator(const abstract_integrator& other) noexcept
      : default_X(other.default_X)
      , default_dQ(other.default_dQ)
      , X(other.X)
      , q(other.q)
      , u(other.u)
      , sigma(other.sigma)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        irt_return_if_fail(std::isfinite(default_X),
                           status::model_integrator_X_error);

        irt_return_if_fail(std::isfinite(default_dQ) && default_dQ > zero,
                           status::model_integrator_X_error);

        X = default_X;
        q = std::floor(X / default_dQ) * default_dQ;
        u = zero;

        sigma = time_domain<time>::zero;

        return status::success;
    }

    status external(simulation& sim, const time e) noexcept
    {
        auto lst     = get_message(sim, x[port_x_dot]);
        auto value_x = lst.front()[0];

        X += e * u;
        u = value_x;

        if (sigma != zero) {
            if (u == zero)
                sigma = time_domain<time>::infinity;
            else if (u > zero)
                sigma = (q + default_dQ - X) / u;
            else
                sigma = (q - default_dQ - X) / u;
        }

        return status::success;
    }

    status reset(simulation& sim) noexcept
    {
        auto  lst   = get_message(sim, x[port_reset]);
        auto& value = lst.front();

        X     = value[0];
        q     = X;
        sigma = time_domain<time>::zero;
        return status::success;
    }

    status internal() noexcept
    {
        X += sigma * u;
        q = X;

        sigma =
          u == zero ? time_domain<time>::infinity : default_dQ / std::abs(u);

        return status::success;
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        if (!have_message(x[port_x_dot]) && !have_message(x[port_reset])) {
            irt_return_if_bad(internal());
        } else {
            if (have_message(x[port_reset])) {
                irt_return_if_bad(reset(sim));
            } else {
                irt_return_if_bad(external(sim, e));
            }
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim, y[0], X + u * sigma);
    }

    observation_message observation(time t, time e) const noexcept
    {
        return qss_observation(X, u, t, e);
    }
};

/*****************************************************************************
 *
 * Qss2 part
 *
 ****************************************************************************/

template<>
struct abstract_integrator<2>
{
    input_port  x[2];
    output_port y[1];
    real        default_X  = zero;
    real        default_dQ = irt::real(0.01);
    real        X;
    real        u;
    real        mu;
    real        q;
    real        mq;
    time        sigma = time_domain<time>::zero;

    enum port_name
    {
        port_x_dot,
        port_reset
    };

    abstract_integrator() = default;

    abstract_integrator(const abstract_integrator& other) noexcept
      : default_X(other.default_X)
      , default_dQ(other.default_dQ)
      , X(other.X)
      , u(other.u)
      , mu(other.mu)
      , q(other.q)
      , mq(other.mq)
      , sigma(other.sigma)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        irt_return_if_fail(std::isfinite(default_X),
                           status::model_integrator_X_error);

        irt_return_if_fail(std::isfinite(default_dQ) && default_dQ > zero,
                           status::model_integrator_X_error);

        X = default_X;

        u  = zero;
        mu = zero;
        q  = X;
        mq = zero;

        sigma = time_domain<time>::zero;

        return status::success;
    }

    status external(simulation& sim, const time e) noexcept
    {
        auto       lst         = get_message(sim, x[port_x_dot]);
        const real value_x     = lst.front()[0];
        const real value_slope = lst.front()[1];

        X += (u * e) + (mu / two) * (e * e);
        u  = value_x;
        mu = value_slope;

        if (sigma != zero) {
            q += mq * e;
            const real a = mu / two;
            const real b = u - mq;
            real       c = X - q + default_dQ;
            real       s;
            sigma = time_domain<time>::infinity;

            if (a == zero) {
                if (b != zero) {
                    s = -c / b;
                    if (s > zero)
                        sigma = s;

                    c = X - q - default_dQ;
                    s = -c / b;
                    if ((s > zero) && (s < sigma))
                        sigma = s;
                }
            } else {
                s = (-b + std::sqrt(b * b - four * a * c)) / two / a;
                if (s > zero)
                    sigma = s;

                s = (-b - std::sqrt(b * b - four * a * c)) / two / a;
                if ((s > zero) && (s < sigma))
                    sigma = s;

                c = X - q - default_dQ;
                s = (-b + std::sqrt(b * b - four * a * c)) / two / a;
                if ((s > zero) && (s < sigma))
                    sigma = s;

                s = (-b - std::sqrt(b * b - four * a * c)) / two / a;
                if ((s > zero) && (s < sigma))
                    sigma = s;
            }

            if (((X - q) > default_dQ) || ((q - X) > default_dQ))
                sigma = time_domain<time>::zero;
        }

        return status::success;
    }

    status internal() noexcept
    {
        X += u * sigma + mu / two * sigma * sigma;
        q = X;
        u += mu * sigma;
        mq = u;

        sigma = mu == zero ? time_domain<time>::infinity
                           : std::sqrt(two * default_dQ / std::abs(mu));

        return status::success;
    }

    status reset(simulation& sim) noexcept
    {
        auto lst         = get_message(sim, x[port_reset]);
        auto value_reset = lst.front()[0];

        X     = value_reset;
        q     = X;
        sigma = time_domain<time>::zero;
        return status::success;
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        if (!have_message(x[port_x_dot]) && !have_message(x[port_reset]))
            irt_return_if_bad(internal());
        else {
            if (have_message(x[port_reset])) {
                irt_return_if_bad(reset(sim));
            } else {
                irt_return_if_bad(external(sim, e));
            }
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(
          sim, y[0], X + u * sigma + mu * sigma * sigma / two, u + mu * sigma);
    }

    observation_message observation(time t, time e) const noexcept
    {
        return qss_observation(X, u, mu, t, e);
    }
};

template<>
struct abstract_integrator<3>
{
    input_port  x[2];
    output_port y[1];
    real        default_X  = zero;
    real        default_dQ = irt::real(0.01);
    real        X;
    real        u;
    real        mu;
    real        pu;
    real        q;
    real        mq;
    real        pq;
    time        sigma = time_domain<time>::zero;

    enum port_name
    {
        port_x_dot,
        port_reset
    };

    abstract_integrator() = default;

    abstract_integrator(const abstract_integrator& other) noexcept
      : default_X(other.default_X)
      , default_dQ(other.default_dQ)
      , X(other.X)
      , u(other.u)
      , mu(other.mu)
      , pu(other.pu)
      , q(other.q)
      , mq(other.mq)
      , pq(other.pq)
      , sigma(other.sigma)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        irt_return_if_fail(std::isfinite(default_X),
                           status::model_integrator_X_error);

        irt_return_if_fail(std::isfinite(default_dQ) && default_dQ > zero,
                           status::model_integrator_X_error);

        X     = default_X;
        u     = zero;
        mu    = zero;
        pu    = zero;
        q     = default_X;
        mq    = zero;
        pq    = zero;
        sigma = time_domain<time>::zero;

        return status::success;
    }

    status external(simulation& sim, const time e) noexcept
    {
        auto       lst              = get_message(sim, x[port_x_dot]);
        auto&      value            = lst.front();
        const real value_x          = value[0];
        const real value_slope      = value[1];
        const real value_derivative = value[2];

#if irt_have_numbers == 1
        constexpr real pi_div_3 = std::numbers::pi_v<real> / three;
#else
        constexpr real pi_div_3 = 1.0471975511965976;
#endif

        X  = X + u * e + (mu * e * e) / two + (pu * e * e * e) / three;
        u  = value_x;
        mu = value_slope;
        pu = value_derivative;

        if (sigma != zero) {
            q      = q + mq * e + pq * e * e;
            mq     = mq + two * pq * e;
            auto a = mu / two - pq;
            auto b = u - mq;
            auto c = X - q - default_dQ;
            auto s = zero;

            if (pu != zero) {
                a       = three * a / pu;
                b       = three * b / pu;
                c       = three * c / pu;
                auto v  = b - a * a / three;
                auto w  = c - b * a / three + two * a * a * a / real(27);
                auto i1 = -w / two;
                auto i2 = i1 * i1 + v * v * v / real(27);

                if (i2 > zero) {
                    i2     = std::sqrt(i2);
                    auto A = i1 + i2;
                    auto B = i1 - i2;

                    if (A > zero)
                        A = std::pow(A, one / three);
                    else
                        A = -std::pow(std::abs(A), one / three);
                    if (B > zero)
                        B = std::pow(B, one / three);
                    else
                        B = -std::pow(std::abs(B), one / three);

                    s = A + B - a / three;
                    if (s < zero)
                        s = time_domain<time>::infinity;
                } else if (i2 == zero) {
                    auto A = i1;
                    if (A > zero)
                        A = std::pow(A, one / three);
                    else
                        A = -std::pow(std::abs(A), one / three);
                    auto x1 = two * A - a / three;
                    auto x2 = -(A + a / three);
                    if (x1 < zero) {
                        if (x2 < zero) {
                            s = time_domain<time>::infinity;
                        } else {
                            s = x2;
                        }
                    } else if (x2 < zero) {
                        s = x1;
                    } else if (x1 < x2) {
                        s = x1;
                    } else {
                        s = x2;
                    }
                } else {
                    auto arg = w * std::sqrt(real(27) / (-v)) / (two * v);
                    arg      = std::acos(arg) / three;
                    auto y1  = two * std::sqrt(-v / three);
                    auto y2  = -y1 * std::cos(pi_div_3 - arg) - a / three;
                    auto y3  = -y1 * std::cos(pi_div_3 + arg) - a / three;
                    y1       = y1 * std::cos(arg) - a / three;
                    if (y1 < zero) {
                        s = time_domain<time>::infinity;
                    } else if (y3 < zero) {
                        s = y1;
                    } else if (y2 < zero) {
                        s = y3;
                    } else {
                        s = y2;
                    }
                }
                c  = c + real(6) * default_dQ / pu;
                w  = c - b * a / three + two * a * a * a / real(27);
                i1 = -w / two;
                i2 = i1 * i1 + v * v * v / real(27);
                if (i2 > zero) {
                    i2     = std::sqrt(i2);
                    auto A = i1 + i2;
                    auto B = i1 - i2;
                    if (A > zero)
                        A = std::pow(A, one / three);
                    else
                        A = -std::pow(std::abs(A), one / three);
                    if (B > zero)
                        B = std::pow(B, one / three);
                    else
                        B = -std::pow(std::abs(B), one / three);
                    sigma = A + B - a / three;

                    if (s < sigma || sigma < zero) {
                        sigma = s;
                    }
                } else if (i2 == zero) {
                    auto A = i1;
                    if (A > zero)
                        A = std::pow(A, one / three);
                    else
                        A = -std::pow(std::abs(A), one / three);
                    auto x1 = two * A - a / three;
                    auto x2 = -(A + a / three);
                    if (x1 < zero) {
                        if (x2 < zero) {
                            sigma = time_domain<time>::infinity;
                        } else {
                            sigma = x2;
                        }
                    } else if (x2 < zero) {
                        sigma = x1;
                    } else if (x1 < x2) {
                        sigma = x1;
                    } else {
                        sigma = x2;
                    }
                    if (s < sigma) {
                        sigma = s;
                    }
                } else {
                    auto arg = w * std::sqrt(real(27) / (-v)) / (two * v);
                    arg      = std::acos(arg) / three;
                    auto y1  = two * std::sqrt(-v / three);
                    auto y2  = -y1 * std::cos(pi_div_3 - arg) - a / three;
                    auto y3  = -y1 * std::cos(pi_div_3 + arg) - a / three;
                    y1       = y1 * std::cos(arg) - a / three;
                    if (y1 < zero) {
                        sigma = time_domain<time>::infinity;
                    } else if (y3 < zero) {
                        sigma = y1;
                    } else if (y2 < zero) {
                        sigma = y3;
                    } else {
                        sigma = y2;
                    }
                    if (s < sigma) {
                        sigma = s;
                    }
                }
            } else {
                if (a != zero) {
                    auto x1 = b * b - four * a * c;
                    if (x1 < zero) {
                        s = time_domain<time>::infinity;
                    } else {
                        x1      = std::sqrt(x1);
                        auto x2 = (-b - x1) / two / a;
                        x1      = (-b + x1) / two / a;
                        if (x1 < zero) {
                            if (x2 < zero) {
                                s = time_domain<time>::infinity;
                            } else {
                                s = x2;
                            }
                        } else if (x2 < zero) {
                            s = x1;
                        } else if (x1 < x2) {
                            s = x1;
                        } else {
                            s = x2;
                        }
                    }
                    c  = c + two * default_dQ;
                    x1 = b * b - four * a * c;
                    if (x1 < zero) {
                        sigma = time_domain<time>::infinity;
                    } else {
                        x1      = std::sqrt(x1);
                        auto x2 = (-b - x1) / two / a;
                        x1      = (-b + x1) / two / a;
                        if (x1 < zero) {
                            if (x2 < zero) {
                                sigma = time_domain<time>::infinity;
                            } else {
                                sigma = x2;
                            }
                        } else if (x2 < zero) {
                            sigma = x1;
                        } else if (x1 < x2) {
                            sigma = x1;
                        } else {
                            sigma = x2;
                        }
                    }
                    if (s < sigma)
                        sigma = s;
                } else {
                    if (b != zero) {
                        auto x1 = -c / b;
                        auto x2 = x1 - two * default_dQ / b;
                        if (x1 < zero)
                            x1 = time_domain<time>::infinity;
                        if (x2 < zero)
                            x2 = time_domain<time>::infinity;
                        if (x1 < x2) {
                            sigma = x1;
                        } else {
                            sigma = x2;
                        }
                    }
                }
            }

            if ((std::abs(X - q)) > default_dQ)
                sigma = time_domain<time>::zero;
        }

        return status::success;
    }

    status internal() noexcept
    {
        X = X + u * sigma + (mu * sigma * sigma) / two +
            (pu * sigma * sigma * sigma) / three;
        q  = X;
        u  = u + mu * sigma + pu * std::pow(sigma, two);
        mq = u;
        mu = mu + two * pu * sigma;
        pq = mu / two;

        sigma = pu == zero
                  ? time_domain<time>::infinity
                  : std::pow(std::abs(three * default_dQ / pu), one / three);

        return status::success;
    }

    status reset(simulation& sim) noexcept
    {
        auto span = get_message(sim, x[port_reset]);

        X     = span.front()[0];
        q     = X;
        sigma = time_domain<time>::zero;

        return status::success;
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        if (!have_message(x[port_x_dot]) && !have_message(x[port_reset]))
            irt_return_if_bad(internal());
        else {
            if (have_message(x[port_reset]))
                irt_return_if_bad(reset(sim));
            else
                irt_return_if_bad(external(sim, e));
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim,
                            y[0],
                            X + u * sigma + (mu * sigma * sigma) / two +
                              (pu * sigma * sigma * sigma) / three,
                            u + mu * sigma + pu * sigma * sigma,
                            mu / two + pu * sigma);
    }

    observation_message observation(time t, time e) const noexcept
    {
        return qss_observation(X, u, mu, pu, t, e);
    }
};

using qss1_integrator = abstract_integrator<1>;
using qss2_integrator = abstract_integrator<2>;
using qss3_integrator = abstract_integrator<3>;

template<int QssLevel>
struct abstract_power
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port  x[1];
    output_port y[1];
    time        sigma;

    real value[QssLevel];
    real default_n;

    abstract_power() noexcept = default;

    abstract_power(const abstract_power& other) noexcept
      : default_n(other.default_n)
    {
        std::copy_n(other.value, QssLevel, value);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::fill_n(value, QssLevel, zero);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1)
            return send_message(sim, y[0], std::pow(value[0], default_n));

        if constexpr (QssLevel == 2)
            return send_message(sim,
                                y[0],
                                std::pow(value[0], default_n),
                                default_n * std::pow(value[0], default_n - 1) *
                                  value[1]);

        if constexpr (QssLevel == 3)
            return send_message(
              sim,
              y[0],
              std::pow(value[0], default_n),
              default_n * std::pow(value[0], default_n - 1) * value[1],
              default_n * (default_n - 1) * std::pow(value[0], default_n - 2) *
                  (value[1] * value[1] / two) +
                default_n * std::pow(value[0], default_n - 1) * value[2]);

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        sigma = time_domain<time>::infinity;

        if (have_message(x[0])) {
            auto  list = get_message(sim, x[0]);
            auto& msg  = list.front();

            if constexpr (QssLevel == 1) {
                value[0] = msg[0];
            }

            if constexpr (QssLevel == 2) {
                value[0] = msg[0];
                value[1] = msg[1];
            }

            if constexpr (QssLevel == 3) {
                value[0] = msg[0];
                value[1] = msg[1];
                value[2] = msg[2];
            }

            sigma = time_domain<time>::zero;
        }

        return status::success;
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            auto X = std::pow(value[0], default_n);
            return { t, X };
        }

        if constexpr (QssLevel == 2) {
            auto X = std::pow(value[0], default_n);
            auto u = default_n * std::pow(value[0], default_n - 1) * value[1];

            return qss_observation(X, u, t, e);
        }

        if constexpr (QssLevel == 3) {
            auto X  = std::pow(value[0], default_n);
            auto u  = default_n * std::pow(value[0], default_n - 1) * value[1];
            auto mu = default_n * (default_n - 1) *
                        std::pow(value[0], default_n - 2) *
                        (value[1] * value[1] / two) +
                      default_n * std::pow(value[0], default_n - 1) * value[2];

            return qss_observation(X, u, mu, t, e);
        }
    }
};

using qss1_power = abstract_power<1>;
using qss2_power = abstract_power<2>;
using qss3_power = abstract_power<3>;

template<int QssLevel>
struct abstract_square
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port  x[1];
    output_port y[1];
    time        sigma;

    real value[QssLevel];

    abstract_square() noexcept = default;

    abstract_square(const abstract_square& other) noexcept
    {
        std::copy_n(other.value, QssLevel, value);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::fill_n(value, QssLevel, zero);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1)
            return send_message(sim, y[0], value[0] * value[0]);

        if constexpr (QssLevel == 2)
            return send_message(
              sim, y[0], value[0] * value[0], two * value[0] * value[1]);

        if constexpr (QssLevel == 3)
            return send_message(sim,
                                y[0],
                                value[0] * value[0],
                                two * value[0] * value[1],
                                (two * value[0] * value[2]) +
                                  (value[1] * value[1]));

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        sigma = time_domain<time>::infinity;

        if (have_message(x[0])) {
            auto  lst = get_message(sim, x[0]);
            auto& msg = lst.front();

            if constexpr (QssLevel == 1) {
                value[0] = msg[0];
            }

            if constexpr (QssLevel == 2) {
                value[0] = msg[0];
                value[1] = msg[1];
            }

            if constexpr (QssLevel == 3) {
                value[0] = msg[0];
                value[1] = msg[1];
                value[2] = msg[2];
            }

            sigma = time_domain<time>::zero;
        }

        return status::success;
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1)
            return { t, value[0] * value[0] };

        if constexpr (QssLevel == 2)
            return qss_observation(
              value[0] * value[0], two * value[0] * value[1], t, e);

        if constexpr (QssLevel == 3)
            return qss_observation(value[0] * value[0],
                                   two * value[0] * value[1],
                                   (two * value[0] * value[2]) +
                                     (value[1] * value[1]),
                                   t,
                                   e);
    }
};

using qss1_square = abstract_square<1>;
using qss2_square = abstract_square<2>;
using qss3_square = abstract_square<3>;

template<int QssLevel, int PortNumber>
struct abstract_sum
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");
    static_assert(PortNumber > 1, "sum model need at least two input port");

    input_port  x[PortNumber];
    output_port y[1];
    time        sigma;

    real values[QssLevel * PortNumber];

    abstract_sum() noexcept = default;

    abstract_sum(const abstract_sum& other) noexcept
      : sigma(other.sigma)
    {
        std::copy_n(other.values, QssLevel * PortNumber, values);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::fill_n(values, QssLevel * PortNumber, zero);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            real value = 0.;
            for (int i = 0; i != PortNumber; ++i)
                value += values[i];

            return send_message(sim, y[0], value);
        }

        if constexpr (QssLevel == 2) {
            real value = 0.;
            real slope = 0.;

            for (int i = 0; i != PortNumber; ++i) {
                value += values[i];
                slope += values[i + PortNumber];
            }

            return send_message(sim, y[0], value, slope);
        }

        if constexpr (QssLevel == 3) {
            real value      = 0.;
            real slope      = 0.;
            real derivative = 0.;

            for (size_t i = 0; i != PortNumber; ++i) {
                value += values[i];
                slope += values[i + PortNumber];
                derivative += values[i + PortNumber + PortNumber];
            }

            return send_message(sim, y[0], value, slope, derivative);
        }

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        bool message = false;

        if constexpr (QssLevel == 1) {
            for (size_t i = 0; i != PortNumber; ++i) {
                auto span = get_message(sim, x[i]);
                for (const auto& msg : span) {
                    values[i] = msg[0];
                    message   = true;
                }
            }
        }

        if constexpr (QssLevel == 2) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (!have_message(x[i])) {
                    values[i] += values[i + PortNumber] * e;
                } else {
                    auto span = get_message(sim, x[i]);
                    for (const auto& msg : span) {
                        values[i]              = msg[0];
                        values[i + PortNumber] = msg[1];
                        message                = true;
                    }
                }
            }
        }

        if constexpr (QssLevel == 3) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (!have_message(x[i])) {
                    values[i] += values[i + PortNumber] * e +
                                 values[i + PortNumber + PortNumber] * e * e;
                    values[i + PortNumber] +=
                      2 * values[i + PortNumber + PortNumber] * e;
                } else {
                    auto span = get_message(sim, x[i]);
                    for (const auto& msg : span) {
                        values[i]                           = msg[0];
                        values[i + PortNumber]              = msg[1];
                        values[i + PortNumber + PortNumber] = msg[2];
                        message                             = true;
                    }
                }
            }
        }

        sigma = message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            real value = zero;

            for (size_t i = 0; i != PortNumber; ++i)
                value += values[i];

            return { t, value };
        }

        if constexpr (QssLevel >= 2) {
            real value_0 = zero, value_1 = zero;

            for (size_t i = 0; i != PortNumber; ++i) {
                value_0 += values[i];
                value_1 += values[i + PortNumber];
            }

            return qss_observation(value_0, value_1, t, e);
        }

        if constexpr (QssLevel >= 3) {
            real value_0 = zero, value_1 = zero, value_2 = zero;

            for (size_t i = 0; i != PortNumber; ++i) {
                value_0 += values[i];
                value_1 += values[i + PortNumber];
                value_2 += values[i + PortNumber + PortNumber];
            }

            return qss_observation(value_0, value_1, value_2, t, e);
        }
    }
};

using qss1_sum_2 = abstract_sum<1, 2>;
using qss1_sum_3 = abstract_sum<1, 3>;
using qss1_sum_4 = abstract_sum<1, 4>;
using qss2_sum_2 = abstract_sum<2, 2>;
using qss2_sum_3 = abstract_sum<2, 3>;
using qss2_sum_4 = abstract_sum<2, 4>;
using qss3_sum_2 = abstract_sum<3, 2>;
using qss3_sum_3 = abstract_sum<3, 3>;
using qss3_sum_4 = abstract_sum<3, 4>;

template<int QssLevel, int PortNumber>
struct abstract_wsum
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");
    static_assert(PortNumber > 1, "sum model need at least two input port");

    input_port  x[PortNumber];
    output_port y[1];
    time        sigma;

    real default_input_coeffs[PortNumber] = { 0 };
    real values[QssLevel * PortNumber];

    abstract_wsum() noexcept = default;

    abstract_wsum(const abstract_wsum& other) noexcept
      : sigma(other.sigma)
    {
        std::copy_n(
          other.default_input_coeffs, PortNumber, default_input_coeffs);
        std::copy_n(other.values, QssLevel * PortNumber, values);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::fill_n(values, QssLevel * PortNumber, zero);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            real value = zero;

            for (int i = 0; i != PortNumber; ++i)
                value += default_input_coeffs[i] * values[i];

            return send_message(sim, y[0], value);
        }

        if constexpr (QssLevel == 2) {
            real value = zero;
            real slope = zero;

            for (int i = 0; i != PortNumber; ++i) {
                value += default_input_coeffs[i] * values[i];
                slope += default_input_coeffs[i] * values[i + PortNumber];
            }

            return send_message(sim, y[0], value, slope);
        }

        if constexpr (QssLevel == 3) {
            real value      = zero;
            real slope      = zero;
            real derivative = zero;

            for (int i = 0; i != PortNumber; ++i) {
                value += default_input_coeffs[i] * values[i];
                slope += default_input_coeffs[i] * values[i + PortNumber];
                derivative +=
                  default_input_coeffs[i] * values[i + PortNumber + PortNumber];
            }

            return send_message(sim, y[0], value, slope, derivative);
        }

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        bool message = false;

        if constexpr (QssLevel == 1) {
            for (size_t i = 0; i != PortNumber; ++i) {
                auto span = get_message(sim, x[i]);
                for (const auto& msg : span) {
                    values[i] = msg[0];
                    message   = true;
                }
            }
        }

        if constexpr (QssLevel == 2) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (!have_message(x[i])) {
                    values[i] += values[i + PortNumber] * e;
                } else {
                    auto span = get_message(sim, x[i]);
                    for (const auto& msg : span) {
                        values[i]              = msg[0];
                        values[i + PortNumber] = msg[1];
                        message                = true;
                    }
                }
            }
        }

        if constexpr (QssLevel == 3) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (!have_message(x[i])) {
                    values[i] += values[i + PortNumber] * e +
                                 values[i + PortNumber + PortNumber] * e * e;
                    values[i + PortNumber] +=
                      2 * values[i + PortNumber + PortNumber] * e;
                } else {
                    auto span = get_message(sim, x[i]);
                    for (const auto& msg : span) {
                        values[i]                           = msg[0];
                        values[i + PortNumber]              = msg[1];
                        values[i + PortNumber + PortNumber] = msg[2];
                        message                             = true;
                    }
                }
            }
        }

        sigma = message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            real value = zero;

            for (int i = 0; i != PortNumber; ++i)
                value += default_input_coeffs[i] * values[i];

            return { t, value };
        }

        if constexpr (QssLevel == 2) {
            real value = zero;
            real slope = zero;

            for (int i = 0; i != PortNumber; ++i) {
                value += default_input_coeffs[i] * values[i];
                slope += default_input_coeffs[i] * values[i + PortNumber];
            }

            return qss_observation(value, slope, t, e);
        }

        if constexpr (QssLevel == 3) {
            real value      = zero;
            real slope      = zero;
            real derivative = zero;

            for (int i = 0; i != PortNumber; ++i) {
                value += default_input_coeffs[i] * values[i];
                slope += default_input_coeffs[i] * values[i + PortNumber];
                derivative +=
                  default_input_coeffs[i] * values[i + PortNumber + PortNumber];
            }

            return qss_observation(value, slope, derivative, t, e);
        }
    }
};

using qss1_wsum_2 = abstract_wsum<1, 2>;
using qss1_wsum_3 = abstract_wsum<1, 3>;
using qss1_wsum_4 = abstract_wsum<1, 4>;
using qss2_wsum_2 = abstract_wsum<2, 2>;
using qss2_wsum_3 = abstract_wsum<2, 3>;
using qss2_wsum_4 = abstract_wsum<2, 4>;
using qss3_wsum_2 = abstract_wsum<3, 2>;
using qss3_wsum_3 = abstract_wsum<3, 3>;
using qss3_wsum_4 = abstract_wsum<3, 4>;

template<int QssLevel>
struct abstract_multiplier
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port  x[2];
    output_port y[1];
    time        sigma;

    real values[QssLevel * 2];

    abstract_multiplier() noexcept = default;

    abstract_multiplier(const abstract_multiplier& other) noexcept
      : sigma(other.sigma)
    {
        std::copy_n(other.values, QssLevel * 2, values);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::fill_n(values, QssLevel * 2, zero);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            return send_message(sim, y[0], values[0] * values[1]);
        }

        if constexpr (QssLevel == 2) {
            return send_message(sim,
                                y[0],
                                values[0] * values[1],
                                values[2 + 0] * values[1] +
                                  values[2 + 1] * values[0]);
        }

        if constexpr (QssLevel == 3) {
            return send_message(
              sim,
              y[0],
              values[0] * values[1],
              values[2 + 0] * values[1] + values[2 + 1] * values[0],
              values[0] * values[2 + 2 + 1] + values[2 + 0] * values[2 + 1] +
                values[2 + 2 + 0] * values[1]);
        }

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        bool message_port_0 = have_message(x[0]);
        bool message_port_1 = have_message(x[1]);
        sigma               = time_domain<time>::infinity;
        auto span_0         = get_message(sim, x[0]);
        auto span_1         = get_message(sim, x[1]);

        for (const auto& msg : span_0) {
            sigma     = time_domain<time>::zero;
            values[0] = msg[0];

            if constexpr (QssLevel >= 2)
                values[2 + 0] = msg[1];

            if constexpr (QssLevel == 3)
                values[2 + 2 + 0] = msg[2];
        }

        for (const auto& msg : span_1) {
            sigma     = time_domain<time>::zero;
            values[1] = msg[0];

            if constexpr (QssLevel >= 2)
                values[2 + 1] = msg[1];

            if constexpr (QssLevel == 3)
                values[2 + 2 + 1] = msg[2];
        }

        if constexpr (QssLevel == 2) {
            if (!message_port_0)
                values[0] += e * values[2 + 0];

            if (!message_port_1)
                values[1] += e * values[2 + 1];
        }

        if constexpr (QssLevel == 3) {
            if (!message_port_0) {
                values[0] += e * values[2 + 0] + values[2 + 2 + 0] * e * e;
                values[2 + 0] += 2 * values[2 + 2 + 0] * e;
            }

            if (!message_port_1) {
                values[1] += e * values[2 + 1] + values[2 + 2 + 1] * e * e;
                values[2 + 1] += 2 * values[2 + 2 + 1] * e;
            }
        }

        return status::success;
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            return { t, values[0] * values[1] };
        }

        if constexpr (QssLevel == 2) {
            return qss_observation(values[0] * values[1],
                                   values[2 + 0] * values[1] +
                                     values[2 + 1] * values[0],
                                   t,
                                   e);
        }

        if constexpr (QssLevel == 3) {
            return qss_observation(
              values[0] * values[1],
              values[2 + 0] * values[1] + values[2 + 1] * values[0],
              values[0] * values[2 + 2 + 1] + values[2 + 0] * values[2 + 1] +
                values[2 + 2 + 0] * values[1],
              t,
              e);
        }
    }
};

using qss1_multiplier = abstract_multiplier<1>;
using qss2_multiplier = abstract_multiplier<2>;
using qss3_multiplier = abstract_multiplier<3>;

struct quantifier
{
    input_port  x[1];
    output_port y[1];
    time        sigma = time_domain<time>::infinity;

    enum class state
    {
        init,
        idle,
        response
    };

    enum class adapt_state
    {
        impossible,
        possible,
        done
    };

    enum class direction
    {
        up,
        down
    };

    real        default_step_size        = real(0.001);
    int         default_past_length      = 3;
    adapt_state default_adapt_state      = adapt_state::possible;
    bool        default_zero_init_offset = false;
    u64         archive                  = static_cast<u64>(-1);
    int         archive_length           = 0;

    real        m_upthreshold      = zero;
    real        m_downthreshold    = zero;
    real        m_offset           = zero;
    real        m_step_size        = zero;
    int         m_step_number      = 0;
    int         m_past_length      = 0;
    bool        m_zero_init_offset = false;
    state       m_state            = state::init;
    adapt_state m_adapt_state      = adapt_state::possible;

    quantifier() noexcept = default;

    quantifier(const quantifier& other) noexcept
      : default_step_size(other.default_step_size)
      , default_past_length(other.default_past_length)
      , default_adapt_state(other.default_adapt_state)
      , default_zero_init_offset(other.default_zero_init_offset)
      , archive(static_cast<u64>(-1))
      , archive_length(0)
      , m_upthreshold(other.m_upthreshold)
      , m_downthreshold(other.m_downthreshold)
      , m_offset(other.m_offset)
      , m_step_size(other.m_step_size)
      , m_step_number(other.m_step_number)
      , m_past_length(other.m_past_length)
      , m_zero_init_offset(other.m_zero_init_offset)
      , m_state(other.m_state)
      , m_adapt_state(other.m_adapt_state)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        m_step_size        = default_step_size;
        m_past_length      = default_past_length;
        m_zero_init_offset = default_zero_init_offset;
        m_adapt_state      = default_adapt_state;
        m_upthreshold      = zero;
        m_downthreshold    = zero;
        m_offset           = zero;
        m_step_number      = 0;
        archive            = static_cast<u64>(-1);
        archive_length     = 0;
        m_state            = state::init;

        irt_return_if_fail(m_step_size > 0,
                           status::model_quantifier_bad_quantum_parameter);

        irt_return_if_fail(
          m_past_length > 2,
          status::model_quantifier_bad_archive_length_parameter);

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status finalize(simulation& sim) noexcept
    {
        append_archive(sim, archive).clear();
        return status::success;
    }

    status external(simulation& sim, time t) noexcept
    {
        real val = 0.0, shifting_factor = 0.0;

        {
            auto span = get_message(sim, x[0]);
            real sum  = zero;
            real nb   = zero;

            for (const auto& elem : span) {
                sum += elem[0];
                ++nb;
            }

            val = sum / nb;
        }

        if (m_state == state::init) {
            init_step_number_and_offset(val);
            update_thresholds();
            m_state = state::response;
            return status::success;
        }

        while ((val >= m_upthreshold) || (val <= m_downthreshold)) {
            m_step_number =
              val >= m_upthreshold ? m_step_number + 1 : m_step_number - 1;

            switch (m_adapt_state) {
            case adapt_state::impossible:
                update_thresholds();
                break;
            case adapt_state::possible:
                store_change(
                  sim, val >= m_upthreshold ? m_step_size : -m_step_size, t);

                shifting_factor = shift_quanta(sim);

                irt_return_if_fail(shifting_factor >= 0,
                                   status::model_quantifier_shifting_value_neg);
                irt_return_if_fail(
                  shifting_factor <= 1,
                  status::model_quantifier_shifting_value_less_1);

                if ((0 != shifting_factor) && (1 != shifting_factor)) {
                    update_thresholds(shifting_factor,
                                      val >= m_upthreshold ? direction::down
                                                           : direction::up);
                    m_adapt_state = adapt_state::done;
                } else {
                    update_thresholds();
                }
                break;
            case adapt_state::done:
                init_step_number_and_offset(val);
                m_adapt_state = adapt_state::possible;
                update_thresholds();
                break;
            }
        }

        m_state = state::response;
        return status::success;
    }

    status internal() noexcept
    {
        if (m_state == state::response)
            m_state = state::idle;

        return status::success;
    }

    status transition(simulation& sim, time t, time /*e*/, time r) noexcept
    {
        if (!have_message(x[0])) {
            irt_return_if_bad(internal());
        } else {
            if (time_domain<time>::is_zero(r))
                irt_return_if_bad(internal());

            irt_return_if_bad(external(sim, t));
        }

        return ta();
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim, y[0], m_upthreshold, m_downthreshold);
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, m_upthreshold, m_downthreshold };
    }

private:
    status ta() noexcept
    {
        sigma = m_state == state::response ? time_domain<time>::zero
                                           : time_domain<time>::infinity;

        return status::success;
    }

    void update_thresholds() noexcept
    {
        const auto step_number = static_cast<real>(m_step_number);

        m_upthreshold   = m_offset + m_step_size * (step_number + one);
        m_downthreshold = m_offset + m_step_size * (step_number - one);
    }

    void update_thresholds(real factor) noexcept
    {
        const auto step_number = static_cast<real>(m_step_number);

        m_upthreshold = m_offset + m_step_size * (step_number + (one - factor));
        m_downthreshold =
          m_offset + m_step_size * (step_number - (one - factor));
    }

    void update_thresholds(real factor, direction d) noexcept
    {
        const auto step_number = static_cast<real>(m_step_number);

        if (d == direction::up) {
            m_upthreshold =
              m_offset + m_step_size * (step_number + (one - factor));
            m_downthreshold = m_offset + m_step_size * (step_number - one);
        } else {
            m_upthreshold = m_offset + m_step_size * (step_number + one);
            m_downthreshold =
              m_offset + m_step_size * (step_number - (one - factor));
        }
    }

    void init_step_number_and_offset(real value) noexcept
    {
        m_step_number = static_cast<int>(std::floor(value / m_step_size));

        if (m_zero_init_offset) {
            m_offset = 0.0;
        } else {
            m_offset = value - static_cast<real>(m_step_number) * m_step_size;
        }
    }

    real shift_quanta(simulation& sim)
    {
        auto lst    = append_archive(sim, archive);
        real factor = 0.0;

        if (oscillating(sim, m_past_length - 1) &&
            ((lst.back().date - lst.front().date) != 0)) {
            real acc = 0.0;
            real local_estim;
            real cnt = 0;

            auto it_2 = lst.begin();
            auto it_0 = it_2++;
            auto it_1 = it_2++;

            for (int i = 0; i < archive_length - 2; ++i) {
                if ((it_2->date - it_0->date) != 0) {
                    if ((lst.back().x_dot * it_1->x_dot) > zero) {
                        local_estim = 1 - (it_1->date - it_0->date) /
                                            (it_2->date - it_0->date);
                    } else {
                        local_estim =
                          (it_1->date - it_0->date) / (it_2->date - it_0->date);
                    }

                    acc += local_estim;
                    cnt += one;
                }
            }

            acc    = acc / cnt;
            factor = acc;
            lst.clear();
            archive_length = 0;
        }

        return factor;
    }

    void store_change(simulation& sim, real val, time t) noexcept
    {
        auto lst = append_archive(sim, archive);
        lst.emplace_back(val, t);
        archive_length++;

        while (archive_length > m_past_length) {
            lst.pop_front();
            archive_length--;
        }
    }

    bool oscillating(simulation& sim, const int range) noexcept
    {
        if ((range + 1) > archive_length)
            return false;

        auto      lst   = get_archive(sim, archive);
        const int limit = archive_length - range;
        auto      it    = lst.end();
        --it;
        auto next = it--;

        for (int i = 0; i < limit; ++i, next = it--)
            if (it->x_dot * next->x_dot > 0)
                return false;

        return true;
    }

    bool monotonous(simulation& sim, const int range) noexcept
    {
        if ((range + 1) > archive_length)
            return false;

        auto lst  = get_archive(sim, archive);
        auto it   = lst.begin();
        auto prev = it++;

        for (int i = 0; i < range; ++i, prev = it++)
            if ((prev->x_dot * it->x_dot) < 0)
                return false;

        return true;
    }
};

template<size_t PortNumber>
struct adder
{
    static_assert(PortNumber > 1, "adder model need at least two input port");

    input_port  x[PortNumber];
    output_port y[1];
    time        sigma;

    real default_values[PortNumber];
    real default_input_coeffs[PortNumber];

    real values[PortNumber];
    real input_coeffs[PortNumber];

    adder() noexcept
    {
        constexpr auto div = one / static_cast<real>(PortNumber);

        std::fill_n(std::begin(default_values), PortNumber, div);
        std::fill_n(std::begin(default_input_coeffs), PortNumber, zero);
    }

    adder(const adder& other) noexcept
      : sigma(other.sigma)
    {
        std::copy_n(other.default_values,
                    std::size(other.default_values),
                    default_values);
        std::copy_n(other.default_input_coeffs,
                    std::size(other.default_input_coeffs),
                    default_input_coeffs);
        std::copy_n(other.values, std::size(other.values), values);
        std::copy_n(
          other.input_coeffs, std::size(other.input_coeffs), input_coeffs);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::copy_n(std::begin(default_values), PortNumber, std::begin(values));

        std::copy_n(std::begin(default_input_coeffs),
                    PortNumber,
                    std::begin(input_coeffs));

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        real to_send = zero;

        for (size_t i = 0; i != PortNumber; ++i)
            to_send += input_coeffs[i] * values[i];

        return send_message(sim, y[0], to_send);
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        bool have_message = false;

        for (size_t i = 0; i != PortNumber; ++i) {
            auto span = get_message(sim, x[i]);
            for (const auto& msg : span) {
                values[i] = msg[0];

                have_message = true;
            }
        }

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        real ret = zero;

        for (size_t i = 0; i != PortNumber; ++i)
            ret += input_coeffs[i] * values[i];

        return { t, ret };
    }
};

template<size_t PortNumber>
struct mult
{
    static_assert(PortNumber > 1, "mult model need at least two input port");

    input_port  x[PortNumber];
    output_port y[1];
    time        sigma;

    real default_values[PortNumber];
    real default_input_coeffs[PortNumber];

    real values[PortNumber];
    real input_coeffs[PortNumber];

    mult() noexcept
    {
        std::fill_n(std::begin(default_values), PortNumber, one);
        std::fill_n(std::begin(default_input_coeffs), PortNumber, zero);
    }

    mult(const mult& other) noexcept
    {
        std::copy_n(other.default_values,
                    std::size(other.default_values),
                    default_values);
        std::copy_n(other.default_input_coeffs,
                    std::size(other.default_input_coeffs),
                    default_input_coeffs);
        std::copy_n(other.values, std::size(other.values), values);
        std::copy_n(
          other.input_coeffs, std::size(other.input_coeffs), input_coeffs);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::copy_n(std::begin(default_values), PortNumber, std::begin(values));

        std::copy_n(std::begin(default_input_coeffs),
                    PortNumber,
                    std::begin(input_coeffs));

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        real to_send = 1.0;

        for (size_t i = 0; i != PortNumber; ++i)
            to_send *= std::pow(values[i], input_coeffs[i]);

        return send_message(sim, y[0], to_send);
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        bool have_message = false;
        for (size_t i = 0; i != PortNumber; ++i) {
            auto span = get_message(sim, x[i]);
            for (const auto& msg : span) {
                values[i]    = msg[0];
                have_message = true;
            }
        }

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        real ret = 1.0;

        for (size_t i = 0; i != PortNumber; ++i)
            ret *= std::pow(values[i], input_coeffs[i]);

        return { t, ret };
    }
};

struct counter
{
    input_port x[1];
    time       sigma;
    i64        number;

    counter() noexcept = default;

    counter(const counter& other) noexcept
      : sigma(other.sigma)
      , number(other.number)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        number = { 0 };
        sigma  = time_domain<time>::infinity;

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        auto list = get_message(sim, x[0]);
        auto it   = list.begin();
        auto end  = list.end();

        for (; it != end; ++it)
            ++number;

        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, static_cast<real>(number) };
    }
};

struct generator
{
    output_port y[1];
    time        sigma;
    real        value;

    real   default_offset = 0.0;
    source default_source_ta;
    source default_source_value;
    bool   stop_on_error = false;

    generator() noexcept = default;

    generator(const generator& other) noexcept
      : sigma(other.sigma)
      , value(other.value)
      , default_offset(other.default_offset)
      , default_source_ta(other.default_source_ta)
      , default_source_value(other.default_source_value)
      , stop_on_error(other.stop_on_error)
    {
    }

    status initialize(simulation& sim) noexcept
    {
        sigma = default_offset;

        if (stop_on_error) {
            irt_return_if_bad(initialize_source(sim, default_source_ta));
            irt_return_if_bad(initialize_source(sim, default_source_value));
        } else {
            (void)initialize_source(sim, default_source_ta);
            (void)initialize_source(sim, default_source_value);
        }

        return status::success;
    }

    status finalize(simulation& sim) noexcept
    {
        irt_return_if_bad(finalize_source(sim, default_source_ta));
        irt_return_if_bad(finalize_source(sim, default_source_value));

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        double local_sigma = 0;
        double local_value = 0;

        if (stop_on_error) {
            irt_return_if_bad(
              update_source(sim, default_source_ta, local_sigma));
            irt_return_if_bad(
              update_source(sim, default_source_value, local_value));
            sigma = static_cast<real>(local_sigma);
            value = static_cast<real>(local_value);
        } else {
            if (is_bad(update_source(sim, default_source_ta, local_sigma)))
                sigma = time_domain<time>::infinity;
            else
                sigma = static_cast<real>(local_sigma);

            if (is_bad(update_source(sim, default_source_value, local_value)))
                value = 0;
            else
                value = static_cast<real>(local_value);
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim, y[0], value);
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, value };
    }
};

struct constant
{
    output_port y[1];
    time        sigma;

    real default_value  = 0.0;
    time default_offset = time_domain<time>::zero;

    enum class init_type : i8
    {
        // A constant value initialized at startup of the simulation. Use the @c
        // default_value.
        constant,

        // The numbers of incoming connections on all input ports of the
        // component. The @c default_value is filled via the component to
        // simulation algorithm. Otherwise, the default value is unmodified.
        incoming_component_all,

        // The number of outcoming connections on all output ports of the
        // component. The @c default_value is filled via the component to
        // simulation algorithm. Otherwise, the default value is unmodified.
        outcoming_component_all,

        // The number of incoming connections on the nth input port of the
        // component. Use the @c port attribute to specify the identifier of the
        // port. The @c default_value is filled via the component to
        // simulation algorithm. Otherwise, the default value is unmodified.
        incoming_component_n,

        // The number of incoming connections on the nth output ports of the
        // component. Use the @c port attribute to specify the identifier of the
        // port. The @c default_value is filled via the component to
        // simulation algorithm. Otherwise, the default value is unmodified.
        outcoming_component_n,
    };

    real      value = 0.0;
    init_type type  = init_type::constant;
    i8        port  = 0;

    constant() noexcept = default;

    constant(const constant& other) noexcept
      : sigma(other.sigma)
      , default_value(other.default_value)
      , default_offset(other.default_offset)
      , value(other.value)
      , type(other.type)
      , port(other.port)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        sigma = default_offset;
        value = default_value;

        return status::success;
    }

    status transition(simulation& /*sim*/,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim, y[0], value);
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, value };
    }
};

inline time compute_wake_up(real threshold, real value_0, real value_1) noexcept
{
    time ret = time_domain<time>::infinity;

    if (value_1 != 0) {
        const auto a = value_1;
        const auto b = value_0 - threshold;
        const auto d = -b * a;

        ret = (d > zero) ? d : time_domain<time>::infinity;
    }

    return ret;
}

inline time compute_wake_up(real threshold,
                            real value_0,
                            real value_1,
                            real value_2) noexcept
{
    time ret = time_domain<time>::infinity;

    if (value_1 != 0) {
        if (value_2 != 0) {
            const auto a = value_2;
            const auto b = value_1;
            const auto c = value_0 - threshold;
            const auto d = b * b - four * a * c;

            if (d > zero) {
                const auto x1 = (-b + std::sqrt(d)) / (two * a);
                const auto x2 = (-b - std::sqrt(d)) / (two * a);

                if (x1 > zero) {
                    if (x2 > zero) {
                        ret = std::min(x1, x2);
                    } else {
                        ret = x1;
                    }
                } else {
                    if (x2 > 0)
                        ret = x2;
                }
            } else if (d == zero) {
                const auto x = -b / (two * a);
                if (x > zero)
                    ret = x;
            }
        } else {
            const auto a = value_1;
            const auto b = value_0 - threshold;
            const auto d = -b * a;

            if (d > zero)
                ret = d;
        }
    }

    return ret;
}

template<int QssLevel>
struct abstract_filter
{
    input_port  x[1];
    output_port y[3];

    time sigma;
    real default_lower_threshold = -std::numeric_limits<real>::infinity();
    real default_upper_threshold = +std::numeric_limits<real>::infinity();
    real lower_threshold;
    real upper_threshold;
    real value[QssLevel];
    bool reach_lower_threshold = false;
    bool reach_upper_threshold = false;

    abstract_filter() noexcept = default;

    abstract_filter(const abstract_filter& other) noexcept
      : sigma(other.sigma)
      , default_lower_threshold(other.default_lower_threshold)
      , default_upper_threshold(other.default_upper_threshold)
      , lower_threshold(other.lower_threshold)
      , upper_threshold(other.upper_threshold)
      , reach_lower_threshold(other.reach_lower_threshold)
      , reach_upper_threshold(other.reach_upper_threshold)
    {
        std::copy_n(other.value, QssLevel, value);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        irt_return_if_fail(
          default_lower_threshold < default_upper_threshold,
          status::model_filter_threshold_condition_not_satisfied);

        lower_threshold       = default_lower_threshold;
        upper_threshold       = default_upper_threshold;
        reach_lower_threshold = false;
        reach_upper_threshold = false;
        std::fill_n(value, QssLevel, zero);

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        if (!have_message(x[0])) {
            if constexpr (QssLevel == 2)
                value[0] += value[1] * e;
            if constexpr (QssLevel == 3) {
                value[0] += value[1] * e + value[2] * e * e;
                value[1] += two * value[2] * e;
            }
        } else {
            auto span = get_message(sim, x[0]);
            for (const auto& msg : span) {
                value[0] = msg[0];
                if constexpr (QssLevel >= 2)
                    value[1] = msg[1];
                if constexpr (QssLevel == 3)
                    value[2] = msg[2];
            }
        }

        reach_lower_threshold = false;
        reach_upper_threshold = false;

        if (value[0] >= upper_threshold) {
            reach_upper_threshold = true;
            sigma                 = time_domain<time>::zero;
        } else if (value[0] <= lower_threshold) {
            reach_lower_threshold = true;
            sigma                 = time_domain<time>::zero;
        } else {
            if constexpr (QssLevel == 1)
                sigma = time_domain<time>::infinity;
            if constexpr (QssLevel == 2)
                sigma = std::min(
                  compute_wake_up(upper_threshold, value[0], value[1]),
                  compute_wake_up(lower_threshold, value[0], value[1]));
            if constexpr (QssLevel == 3)
                sigma =
                  std::min(compute_wake_up(
                             upper_threshold, value[0], value[1], value[2]),
                           compute_wake_up(
                             lower_threshold, value[0], value[1], value[2]));
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            if (reach_upper_threshold) {
                irt_return_if_bad(send_message(sim, y[0], upper_threshold));
                irt_return_if_bad(send_message(sim, y[1], one));
            } else {
                irt_return_if_bad(send_message(sim, y[0], value[0]));
                irt_return_if_bad(send_message(sim, y[1], zero));
            }

            if (reach_lower_threshold) {
                irt_return_if_bad(send_message(sim, y[0], lower_threshold));
                irt_return_if_bad(send_message(sim, y[2], one));
            } else {
                irt_return_if_bad(send_message(sim, y[0], value[0]));
                irt_return_if_bad(send_message(sim, y[2], zero));
            }
        }

        if constexpr (QssLevel == 2) {
            if (reach_upper_threshold) {
                irt_return_if_bad(send_message(sim, y[0], upper_threshold));
                irt_return_if_bad(send_message(sim, y[1], one));
            } else {
                irt_return_if_bad(send_message(sim, y[0], value[0], value[1]));
                irt_return_if_bad(send_message(sim, y[1], zero));
            }

            if (reach_lower_threshold) {
                irt_return_if_bad(send_message(sim, y[0], lower_threshold));
                irt_return_if_bad(send_message(sim, y[2], one));
            } else {
                irt_return_if_bad(send_message(sim, y[0], value[0], value[1]));
                irt_return_if_bad(send_message(sim, y[2], zero));
            }

            return status::success;
        }

        if constexpr (QssLevel == 3) {
            if (reach_upper_threshold) {
                irt_return_if_bad(send_message(sim, y[0], upper_threshold));
                irt_return_if_bad(send_message(sim, y[1], one));
            } else {
                irt_return_if_bad(
                  send_message(sim, y[0], value[0], value[1], value[2]));
                irt_return_if_bad(send_message(sim, y[1], zero));
            }

            if (reach_lower_threshold) {
                irt_return_if_bad(send_message(sim, y[0], lower_threshold));
                irt_return_if_bad(send_message(sim, y[2], one));
            } else {
                irt_return_if_bad(
                  send_message(sim, y[0], value[0], value[1], value[2]));
                irt_return_if_bad(send_message(sim, y[2], zero));
            }

            return status::success;
        }

        return status::success;
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            if (reach_upper_threshold) {
                return { t, upper_threshold };
            } else {
                return { t, value[0] };
            }

            if (reach_lower_threshold) {
                return { t, lower_threshold };
            } else {
                return { t, value[0] };
            }
        }

        if constexpr (QssLevel == 2) {
            if (reach_upper_threshold) {
                return { t, upper_threshold };
            } else {
                return qss_observation(value[0], value[1], t, e);
            }

            if (reach_lower_threshold) {
                return { t, lower_threshold };
            } else {
                return qss_observation(value[0], value[1], t, e);
            }
        }

        if constexpr (QssLevel == 3) {
            if (reach_upper_threshold) {
                return { t, upper_threshold };
            } else {
                return qss_observation(value[0], value[1], value[2], t, e);
            }

            if (reach_lower_threshold) {
                return { t, lower_threshold };
            } else {
                return qss_observation(value[0], value[1], value[2], t, e);
            }
        }
    }
};

using qss1_filter = abstract_filter<1>;
using qss2_filter = abstract_filter<2>;
using qss3_filter = abstract_filter<3>;

struct filter
{
    input_port  x[1];
    output_port y[1];
    time        sigma;

    real default_lower_threshold;
    real default_upper_threshold;

    real         lower_threshold;
    real         upper_threshold;
    irt::message inValue;

    filter() noexcept
    {
        default_lower_threshold = -0.5;
        default_upper_threshold = 0.5;
        sigma                   = time_domain<time>::infinity;
    }

    status initialize() noexcept
    {
        sigma           = time_domain<time>::infinity;
        lower_threshold = default_lower_threshold;
        upper_threshold = default_upper_threshold;

        irt_return_if_fail(
          default_lower_threshold < default_upper_threshold,
          status::model_filter_threshold_condition_not_satisfied);

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim, y[0], inValue[0]);
    }

    status transition(simulation& sim, time, time, time) noexcept
    {
        // @TODO fix transition: take ino account Qss1, Qss2 and Qss3 messages
        //  to build a correct sigma.

        sigma = time_domain<time>::infinity;

        auto span = get_message(sim, x[0]);
        for (const auto& msg : span) {
            if (msg[0] > lower_threshold && msg[0] < upper_threshold) {
                inValue[0] = msg[0];
            } else if (msg[1] < lower_threshold && msg[1] < upper_threshold) {
                inValue[0] = msg[1];
            } else {
                inValue[0] = msg[2];
            }

            sigma = time_domain<time>::zero;
        }

        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, inValue[0] };
    }
};

struct abstract_and_check
{
    template<typename Iterator>
    bool operator()(Iterator begin, Iterator end) const noexcept
    {
        return std::all_of(
          begin, end, [](const bool value) noexcept { return value == true; });
    }
};

struct abstract_or_check
{
    template<typename Iterator>
    bool operator()(Iterator begin, Iterator end) const noexcept
    {
        return std::any_of(
          begin, end, [](const bool value) noexcept { return value == true; });
    }
};

template<typename AbstractLogicalTester, int PortNumber>
struct abstract_logical
{
    input_port  x[PortNumber];
    output_port y[1];
    time        sigma = time_domain<time>::infinity;

    bool default_values[PortNumber];
    bool values[PortNumber];

    bool is_valid      = true;
    bool value_changed = false;

    abstract_logical() noexcept = default;

    status initialize() noexcept
    {
        std::copy_n(std::data(default_values),
                    std::size(default_values),
                    std::data(values));

        AbstractLogicalTester tester;
        is_valid      = tester(std::begin(values), std::end(values));
        sigma         = time_domain<time>::zero;
        value_changed = true;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if (value_changed)
            return send_message(sim, y[0], is_valid ? one : zero);

        return status::success;
    }

    status transition(simulation& sim, time, time, time) noexcept
    {
        const bool old_is_value = is_valid;

        for (int i = 0; i < PortNumber; ++i) {
            if (have_message(x[i])) {
                auto lst = get_message(sim, x[i]);
                auto msg = lst.front();

                if (msg[0] != zero) {
                    if (values[i] == false) {
                        values[i] = true;
                    }
                } else {
                    if (values[i] == true) {
                        values[i] = false;
                    }
                }
            }
        }

        AbstractLogicalTester tester;
        is_valid = tester(std::begin(values), std::end(values));

        if (is_valid != old_is_value) {
            value_changed = true;
            sigma         = time_domain<time>::zero;
        } else {
            value_changed = false;
            sigma         = time_domain<time>::infinity;
        }

        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, is_valid ? one : zero };
    }
};

using logical_and_2 = abstract_logical<abstract_and_check, 2>;
using logical_and_3 = abstract_logical<abstract_and_check, 3>;
using logical_or_2  = abstract_logical<abstract_or_check, 2>;
using logical_or_3  = abstract_logical<abstract_or_check, 3>;

struct logical_invert
{
    input_port  x[1];
    output_port y[1];
    time        sigma;

    bool default_value = false;
    bool value;
    bool value_changed;

    logical_invert() noexcept { sigma = time_domain<time>::infinity; }

    status initialize() noexcept
    {
        value         = default_value;
        sigma         = time_domain<time>::infinity;
        value_changed = false;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if (value_changed)
            return send_message(sim, y[0], value ? zero : one);

        return status::success;
    }

    status transition(simulation& sim, time, time, time) noexcept
    {
        bool value_changed = false;

        if (have_message(x[0])) {
            const auto  lst = get_message(sim, x[0]);
            const auto& msg = lst.front();

            if ((msg[0] != zero && !value) || (msg[0] == zero && value))
                value_changed = true;
        }

        sigma =
          value_changed ? time_domain<time>::zero : time_domain<time>::infinity;
        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, value ? zero : one };
    }
};

/**
 * Hierarchical state machine.
 *
 * \note This implementation have the standard restriction for HSM:
 * 1. You can not call Transition from HSM::event_type::enter and
 * HSM::event_type::exit! These event are provided to execute your
 * construction/destruction. Use custom events for that.
 * 2. You are not allowed to dispatch an event from within an event dispatch.
 * You should queue events if you want such behavior. This restriction is
 * imposed only to prevent the user from creating extremely complicated to trace
 * state machines (which is what we are trying to avoid).
 */
class hierarchical_state_machine
{
public:
    using state_id                      = u8;
    static const u8 max_number_of_state = 254;
    static const u8 invalid_state_id    = 255;

    enum class event_type : u8
    {
        enter = 0,
        exit,
        input_changed
    };

    constexpr static int event_type_count     = 3;
    constexpr static int variable_count       = 8;
    constexpr static int action_type_count    = 16;
    constexpr static int condition_type_count = 20;

    enum class variable : u8
    {
        none = 0,
        port_0,
        port_1,
        port_2,
        port_3,
        var_a,
        var_b,
        constant
    };

    enum class action_type : u8
    {
        none = 0, // no param
        set,      // port identifer + i32 parameter value
        unset,    // port identifier to clear
        reset,    // all port to reset
        output,   // port identifier + i32 parameter value
        affect,
        plus,
        minus,
        negate,
        multiplies,
        divides,
        modulus,
        bit_and,
        bit_or,
        bit_not,
        bit_xor
    };

    enum class condition_type : u8
    {
        none, // No condition (always true)
        port, // Message on port
        a_equal_to_cst,
        a_not_equal_to_cst,
        a_greater_cst,
        a_less_cst,
        a_greater_equal_cst,
        a_less_equal_cst,
        b_equal_to_cst,
        b_not_equal_to_cst,
        b_greater_cst,
        b_less_cst,
        b_greater_equal_cst,
        b_less_equal_cst,
        a_equal_to_b,
        a_not_equal_to_b,
        a_greater_b,
        a_less_b,
        a_greater_equal_b,
        a_less_equal_b,
    };

    //! Action available when state is processed during enter, exit or condition
    //! event.
    //! \note Only on action (value set/unset, devs output, etc.) by
    //! action. To perform more action, use several states.
    struct state_action
    {
        i32         parameter = 0;
        variable    var1      = variable::none;
        variable    var2      = variable::none;
        action_type type      = action_type::none;

        void clear() noexcept;
    };

    //! \note
    //! 1. \c value_condition stores the bit for input value corresponding to
    //! the user request to satisfy the condition. \c value_mask stores the bit
    //! useful in value_condition. If value_mask equal \c 0x0 then, the
    //! condition is always true. If \c value_mask equals \c 0xff the \c
    //! value_condition bit are mandatory.
    //! 2. \c condition_state_action stores transition or action conditions.
    //! Condition can use input port state or condition on integer a or b.
    struct condition_action
    {
        i32            parameter = 0;
        condition_type type      = condition_type::none;
        u8             port      = 0;
        u8             mask      = 0;

        bool check(u8 port_values, i32 a, i32 b) noexcept;
        void clear() noexcept;
    };

    struct state
    {
        state() noexcept = default;

        state_action enter_action;
        state_action exit_action;

        state_action     if_action;
        state_action     else_action;
        condition_action condition;

        state_id if_transition   = invalid_state_id;
        state_id else_transition = invalid_state_id;

        state_id super_id = invalid_state_id;
        state_id sub_id   = invalid_state_id;
    };

    struct output_message
    {
        u8  port{};
        i32 value{};
    };

    hierarchical_state_machine() noexcept = default;
    hierarchical_state_machine(const hierarchical_state_machine&) noexcept;
    hierarchical_state_machine& operator=(
      const hierarchical_state_machine&) noexcept;

    status start() noexcept;
    void   clear() noexcept;

    /// Dispatches an event.
    /// @return If status is success, return true if the event was
    /// processed, otherwise false. Status can return error if automata is badly
    /// defined.
    std::pair<status, bool> dispatch(const event_type e) noexcept;

    /// Return true if the state machine is currently dispatching an event.
    bool is_dispatching() const noexcept;

    /// Transitions the state machine. This function can not be called from
    /// Enter / Exit events in the state handler.
    void transition(state_id target) noexcept;

    state_id get_current_state() const noexcept;
    state_id get_source_state() const noexcept;

    /// Set a handler for a state ID. This function will overwrite the current
    /// state handler.
    /// \param id state id from 0 to max_number_of_state
    /// \param super_id id of the super state, if invalid_state_id this is a top
    /// state. Only one state can be a top state.
    /// \param sub_id if !=
    /// invalid_state_id this sub state (child state) will be entered after the
    /// state Enter event is executed.
    status set_state(state_id id,
                     state_id super_id = invalid_state_id,
                     state_id sub_id   = invalid_state_id) noexcept;

    /// Resets the state to invalid mode.
    void clear_state(state_id id) noexcept;

    bool is_in_state(state_id id) const noexcept;

    bool handle(const state_id state, const event_type event) noexcept;

    int    steps_to_common_root(state_id source, state_id target) noexcept;
    status on_enter_sub_state() noexcept;

    void affect_action(const state_action& action) noexcept;

    std::array<state, max_number_of_state> states;
    small_vector<output_message, 4>        outputs;

    i32 a      = 0;
    i32 b      = 0;
    u8  values = 0;

    state_id m_current_state        = invalid_state_id;
    state_id m_next_state           = invalid_state_id;
    state_id m_source_state         = invalid_state_id;
    state_id m_current_source_state = invalid_state_id;
    state_id m_top_state            = invalid_state_id;
    bool     m_disallow_transition  = false;
};

enum class hsm_id : u64;

status get_hierarchical_state_machine(simulation&                  sim,
                                      hierarchical_state_machine*& hsm,
                                      hsm_id                       id) noexcept;

struct hsm_wrapper
{
    using hsm      = hierarchical_state_machine;
    using state_id = hsm::state_id;

    input_port  x[4];
    output_port y[4];

    hsm_id   id;
    state_id m_previous_state;
    real     sigma;

    hsm_wrapper() noexcept;
    hsm_wrapper(const hsm_wrapper& other) noexcept;

    status initialize(simulation& sim) noexcept;
    status transition(simulation& sim, time t, time e, time r) noexcept;
    status lambda(simulation& sim) noexcept;
};

template<size_t PortNumber>
struct accumulator
{
    input_port x[2 * PortNumber];
    time       sigma;
    real       number;
    real       numbers[PortNumber];

    accumulator() = default;

    accumulator(const accumulator& other) noexcept
      : sigma(other.sigma)
      , number(other.number)
    {
        std::copy_n(other.numbers, std::size(numbers), numbers);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        number = zero;
        std::fill_n(numbers, PortNumber, zero);

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        for (sz i = 0; i != PortNumber; ++i) {
            if (have_message(x[i + PortNumber])) {
                auto list  = get_message(sim, x[i + PortNumber]);
                numbers[i] = list.front()[0];
            }
        }

        for (sz i = 0; i != PortNumber; ++i) {
            if (have_message(x[i])) {
                auto list = get_message(sim, x[i]);
                if (list.front()[0] != zero)
                    number += numbers[i];
            }
        }

        return status::success;
    }
};

struct cross
{
    input_port  x[4];
    output_port y[2];
    time        sigma;

    real default_threshold = zero;

    real threshold;
    real value;
    real if_value;
    real else_value;
    real result;
    real event;

    cross() noexcept = default;

    cross(const cross& other) noexcept
      : sigma(other.sigma)
      , default_threshold(other.default_threshold)
      , threshold(other.threshold)
      , value(other.value)
      , if_value(other.if_value)
      , else_value(other.else_value)
      , result(other.result)
      , event(other.event)
    {
    }

    enum port_name
    {
        port_value,
        port_if_value,
        port_else_value,
        port_threshold
    };

    status initialize(simulation& /*sim*/) noexcept
    {
        threshold  = default_threshold;
        value      = threshold - one;
        if_value   = zero;
        else_value = zero;
        result     = zero;
        event      = zero;

        sigma = time_domain<time>::zero;

        return status::success;
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        bool have_message       = false;
        bool have_message_value = false;
        event                   = zero;

        auto span_thresholds = get_message(sim, x[port_threshold]);
        for (auto& elem : span_thresholds) {
            threshold    = elem[0];
            have_message = true;
        }

        auto span_value = get_message(sim, x[port_value]);
        for (auto& elem : span_value) {
            value              = elem[0];
            have_message_value = true;
            have_message       = true;
        }

        auto span_if_value = get_message(sim, x[port_if_value]);
        for (auto& elem : span_if_value) {
            if_value     = elem[0];
            have_message = true;
        }

        auto span_else_value = get_message(sim, x[port_else_value]);
        for (auto& elem : span_else_value) {
            else_value   = elem[0];
            have_message = true;
        }

        if (have_message_value) {
            event = zero;
            if (value >= threshold) {
                else_value = if_value;
                event      = one;
            }
        }

        result = else_value;

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        irt_return_if_bad(send_message(sim, y[0], result));
        irt_return_if_bad(send_message(sim, y[1], event));

        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, value, if_value, else_value };
    }
};

template<int QssLevel>
struct abstract_cross
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port  x[4];
    output_port y[3];
    time        sigma;

    real default_threshold = zero;
    bool default_detect_up = true;

    real threshold;
    real if_value[QssLevel];
    real else_value[QssLevel];
    real value[QssLevel];
    real last_reset;
    bool reach_threshold;
    bool detect_up;

    abstract_cross() noexcept = default;

    abstract_cross(const abstract_cross& other) noexcept
      : sigma(other.sigma)
      , default_threshold(other.default_threshold)
      , default_detect_up(other.default_detect_up)
      , threshold(other.threshold)
      , last_reset(other.last_reset)
      , reach_threshold(other.reach_threshold)
      , detect_up(other.detect_up)
    {
        std::copy_n(other.if_value, QssLevel, if_value);
        std::copy_n(other.else_value, QssLevel, else_value);
        std::copy_n(other.value, QssLevel, value);
    }

    enum port_name
    {
        port_value,
        port_if_value,
        port_else_value,
        port_threshold
    };

    enum out_name
    {
        o_if_value,
        o_else_value,
        o_threshold_reached
    };

    status initialize(simulation& /*sim*/) noexcept
    {
        std::fill_n(if_value, QssLevel, zero);
        std::fill_n(else_value, QssLevel, zero);
        std::fill_n(value, QssLevel, zero);

        threshold = default_threshold;
        value[0]  = threshold - one;

        sigma           = time_domain<time>::infinity;
        last_reset      = time_domain<time>::infinity;
        detect_up       = default_detect_up;
        reach_threshold = false;

        return status::success;
    }

    status transition(simulation&           sim,
                      time                  t,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        const auto old_else_value = else_value[0];

        if (have_message(x[port_threshold])) {
            auto span = get_message(sim, x[port_threshold]);
            for (const auto& msg : span)
                threshold = msg[0];
        }

        if (!have_message(x[port_if_value])) {
            if constexpr (QssLevel == 2)
                if_value[0] += if_value[1] * e;
            if constexpr (QssLevel == 3) {
                if_value[0] += if_value[1] * e + if_value[2] * e * e;
                if_value[1] += two * if_value[2] * e;
            }
        } else {
            auto span = get_message(sim, x[port_if_value]);
            for (const auto& msg : span) {
                if_value[0] = msg[0];
                if constexpr (QssLevel >= 2)
                    if_value[1] = msg[1];
                if constexpr (QssLevel == 3)
                    if_value[2] = msg[2];
            }
        }

        if (!have_message(x[port_else_value])) {
            if constexpr (QssLevel == 2)
                else_value[0] += else_value[1] * e;
            if constexpr (QssLevel == 3) {
                else_value[0] += else_value[1] * e + else_value[2] * e * e;
                else_value[1] += two * else_value[2] * e;
            }
        } else {
            auto span = get_message(sim, x[port_else_value]);
            for (const auto& msg : span) {
                else_value[0] = msg[0];
                if constexpr (QssLevel >= 2)
                    else_value[1] = msg[1];
                if constexpr (QssLevel == 3)
                    else_value[2] = msg[2];
            }
        }

        if (!have_message(x[port_value])) {
            if constexpr (QssLevel == 2)
                value[0] += value[1] * e;
            if constexpr (QssLevel == 3) {
                value[0] += value[1] * e + value[2] * e * e;
                value[1] += two * value[2] * e;
            }
        } else {
            auto span = get_message(sim, x[port_value]);
            for (const auto& msg : span) {
                value[0] = msg[0];
                if constexpr (QssLevel >= 2)
                    value[1] = msg[1];
                if constexpr (QssLevel == 3)
                    value[2] = msg[2];
            }
        }

        reach_threshold = false;

        if ((detect_up && value[0] >= threshold) ||
            (!detect_up && value[0] <= threshold)) {
            if (t != last_reset) {
                last_reset      = t;
                reach_threshold = true;
                sigma           = time_domain<time>::zero;
            } else
                sigma = time_domain<time>::infinity;
        } else if (old_else_value != else_value[0]) {
            sigma = time_domain<time>::zero;
        } else {
            if constexpr (QssLevel == 1)
                sigma = time_domain<time>::infinity;
            if constexpr (QssLevel == 2)
                sigma = compute_wake_up(threshold, value[0], value[1]);
            if constexpr (QssLevel == 3)
                sigma =
                  compute_wake_up(threshold, value[0], value[1], value[2]);
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            if (reach_threshold) {
                irt_return_if_bad(
                  send_message(sim, y[o_if_value], if_value[0]));

                irt_return_if_bad(
                  send_message(sim, y[o_threshold_reached], one));
            } else {
                irt_return_if_bad(
                  send_message(sim, y[o_else_value], else_value[0]));

                irt_return_if_bad(
                  send_message(sim, y[o_threshold_reached], zero));
            }

            return status::success;
        }

        if constexpr (QssLevel == 2) {
            if (reach_threshold) {
                irt_return_if_bad(
                  send_message(sim, y[o_if_value], if_value[0], if_value[1]));

                irt_return_if_bad(
                  send_message(sim, y[o_threshold_reached], one));
            } else {
                irt_return_if_bad(send_message(
                  sim, y[o_else_value], else_value[0], else_value[1]));

                irt_return_if_bad(
                  send_message(sim, y[o_threshold_reached], zero));
            }

            return status::success;
        }

        if constexpr (QssLevel == 3) {
            if (reach_threshold) {
                irt_return_if_bad(send_message(
                  sim, y[o_if_value], if_value[0], if_value[1], if_value[2]));

                irt_return_if_bad(
                  send_message(sim, y[o_threshold_reached], one));
            } else {
                irt_return_if_bad(send_message(sim,
                                               y[o_else_value],
                                               else_value[0],
                                               else_value[1],
                                               else_value[2]));

                irt_return_if_bad(
                  send_message(sim, y[o_threshold_reached], zero));
            }

            return status::success;
        }

        return status::success;
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, value[0], if_value[0], else_value[0] };
    }
};

using qss1_cross = abstract_cross<1>;
using qss2_cross = abstract_cross<2>;
using qss3_cross = abstract_cross<3>;

inline real sin_time_function(real t) noexcept
{
    constexpr real f0 = to_real(0.1L);

#if irt_have_numbers == 1
    constexpr real pi = std::numbers::pi_v<real>;
#else
    // std::acos(-1) is not a constexpr in MVSC 2019
    constexpr real pi = 3.141592653589793238462643383279502884;
#endif

    constexpr const real mult = two * pi * f0;

    return std::sin(mult * t);
}

inline real square_time_function(real t) noexcept { return t * t; }

inline real time_function(real t) noexcept { return t; }

struct time_func
{
    output_port y[1];
    time        sigma;

    real default_sigma      = to_real(0.01L);
    real (*default_f)(real) = &time_function;

    real value;
    real (*f)(real) = nullptr;

    time_func() noexcept = default;

    time_func(const time_func& other) noexcept
      : sigma(other.sigma)
      , default_sigma(other.default_sigma)
      , default_f(other.default_f)
      , value(other.value)
      , f(other.f)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        f     = default_f;
        sigma = default_sigma;
        value = 0.0;
        return status::success;
    }

    status transition(simulation& /*sim*/,
                      time t,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        value = (*f)(t);
        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim, y[0], value);
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, value };
    }
};

using adder_2 = adder<2>;
using adder_3 = adder<3>;
using adder_4 = adder<4>;

using mult_2 = mult<2>;
using mult_3 = mult<3>;
using mult_4 = mult<4>;

using accumulator_2 = accumulator<2>;

struct queue
{
    input_port  x[1];
    output_port y[1];
    time        sigma;

    u64 fifo = static_cast<u64>(-1);

    real default_ta = one;

    queue() noexcept = default;

    queue(const queue& other) noexcept
      : sigma(other.sigma)
      , fifo(static_cast<u64>(-1))
      , default_ta(other.default_ta)
    {
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        if (default_ta <= 0)
            irt_bad_return(status::model_queue_bad_ta);

        sigma = time_domain<time>::infinity;
        fifo  = static_cast<u64>(-1);

        return status::success;
    }

    status finalize(simulation& sim) noexcept
    {
        append_dated_message(sim, fifo).clear();
        return status::success;
    }

    status transition(simulation& sim, time t, time /*e*/, time /*r*/) noexcept
    {
        auto list = append_dated_message(sim, fifo);
        while (!list.empty() && list.front().data[0] <= t)
            list.pop_front();

        auto span = get_message(sim, x[0]);
        for (const auto& msg : span) {
            if (!can_alloc_dated_message(sim, 1))
                return status::model_queue_full;

            list.emplace_back(
              irt::real(t + default_ta), msg[0], msg[1], msg[2]);
        }

        if (!list.empty()) {
            sigma = list.front()[0] - t;
            sigma = sigma <= time_domain<time>::zero ? time_domain<time>::zero
                                                     : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if (fifo == static_cast<u64>(-1))
            return status::success;

        auto       list = append_dated_message(sim, fifo);
        auto       it   = list.begin();
        auto       end  = list.end();
        const auto t    = it->data[0];

        for (; it != end && it->data[0] <= t; ++it)
            irt_return_if_bad(
              send_message(sim, y[0], it->data[1], it->data[2], it->data[3]));

        return status::success;
    }
};

struct dynamic_queue
{
    input_port  x[1];
    output_port y[1];
    time        sigma;
    u64         fifo = static_cast<u64>(-1);

    source default_source_ta;
    bool   stop_on_error = false;

    dynamic_queue() noexcept = default;

    dynamic_queue(const dynamic_queue& other) noexcept
      : sigma(other.sigma)
      , fifo(static_cast<u64>(-1))
      , default_source_ta(other.default_source_ta)
      , stop_on_error(other.stop_on_error)
    {
    }

    status initialize(simulation& sim) noexcept
    {
        sigma = time_domain<time>::infinity;
        fifo  = static_cast<u64>(-1);

        if (stop_on_error)
            irt_return_if_bad(initialize_source(sim, default_source_ta));
        else
            (void)initialize_source(sim, default_source_ta);

        return status::success;
    }

    status finalize(simulation& sim) noexcept
    {
        append_dated_message(sim, fifo).clear();
        irt_return_if_bad(finalize_source(sim, default_source_ta));

        return status::success;
    }

    status transition(simulation& sim, time t, time /*e*/, time /*r*/) noexcept
    {
        auto list = append_dated_message(sim, fifo);
        while (!list.empty() && list.front().data[0] <= t)
            list.pop_front();

        auto span = get_message(sim, x[0]);
        for (const auto& msg : span) {
            if (!can_alloc_dated_message(sim, 1))
                return status::model_dynamic_queue_full;

            real ta = zero;
            if (stop_on_error) {
                irt_return_if_bad(update_source(sim, default_source_ta, ta));
                list.emplace_back(
                  t + static_cast<real>(ta), msg[0], msg[1], msg[2]);
            } else {
                if (is_success(update_source(sim, default_source_ta, ta)))
                    list.emplace_back(
                      t + static_cast<real>(ta), msg[0], msg[1], msg[2]);
            }
        }

        if (!list.empty()) {
            sigma = list.front().data[0] - t;
            sigma = sigma <= time_domain<time>::zero ? time_domain<time>::zero
                                                     : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if (fifo == static_cast<u64>(-1))
            return status::success;

        auto       list = append_dated_message(sim, fifo);
        auto       it   = list.begin();
        auto       end  = list.end();
        const auto t    = it->data[0];

        for (; it != end && it->data[0] <= t; ++it)
            irt_return_if_bad(
              send_message(sim, y[0], it->data[1], it->data[2], it->data[3]));

        return status::success;
    }
};

struct priority_queue
{
    input_port  x[1];
    output_port y[1];
    time        sigma;
    u64         fifo       = static_cast<u64>(-1);
    real        default_ta = 1.0;

    source default_source_ta;
    bool   stop_on_error = false;

    priority_queue() noexcept = default;

    priority_queue(const priority_queue& other) noexcept
      : sigma(other.sigma)
      , fifo(static_cast<u64>(-1))
      , default_ta(other.default_ta)
      , default_source_ta(other.default_source_ta)
      , stop_on_error(other.stop_on_error)
    {
    }

private:
    status try_to_insert(simulation&    sim,
                         const time     t,
                         const message& msg) noexcept
    {
        if (!can_alloc_dated_message(sim, 1))
            irt_bad_return(status::model_priority_queue_source_is_null);

        auto list = append_dated_message(sim, fifo);
        if (list.empty() || list.begin()->data[0] > t) {
            list.emplace_front(irt::real(t), msg[0], msg[1], msg[2]);
        } else {
            auto it  = list.begin();
            auto end = list.end();
            ++it;

            for (; it != end; ++it) {
                if (it->data[0] > t) {
                    list.emplace(it, irt::real(t), msg[0], msg[1], msg[2]);
                    return status::success;
                }
            }
        }

        return status::success;
    }

public:
    status initialize(simulation& sim) noexcept
    {
        if (stop_on_error)
            irt_return_if_bad(initialize_source(sim, default_source_ta));
        else
            (void)initialize_source(sim, default_source_ta);

        sigma = time_domain<time>::infinity;
        fifo  = static_cast<u64>(-1);

        return status::success;
    }

    status finalize(simulation& sim) noexcept
    {
        append_dated_message(sim, fifo).clear();
        irt_return_if_bad(finalize_source(sim, default_source_ta));

        return status::success;
    }

    status transition(simulation& sim, time t, time /*e*/, time /*r*/) noexcept
    {
        auto list = append_dated_message(sim, fifo);
        while (!list.empty() && list.front().data[0] <= t)
            list.pop_front();

        auto span = get_message(sim, x[0]);
        for (const auto& msg : span) {
            real value = zero;

            if (stop_on_error) {
                irt_return_if_bad(update_source(sim, default_source_ta, value));

                if (auto ret =
                      try_to_insert(sim, static_cast<real>(value) + t, msg);
                    is_bad(ret))
                    irt_bad_return(status::model_priority_queue_full);
            } else {
                if (is_success(update_source(sim, default_source_ta, value))) {
                    if (auto ret =
                          try_to_insert(sim, static_cast<real>(value) + t, msg);
                        is_bad(ret))
                        irt_bad_return(status::model_priority_queue_full);
                }
            }
        }

        if (!list.empty()) {
            sigma = list.front()[0] - t;
            sigma = sigma <= time_domain<time>::zero ? time_domain<time>::zero
                                                     : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return status::success;
    }

    status lambda(simulation& sim) noexcept
    {
        if (fifo == static_cast<u64>(-1))
            return status::success;

        auto       list = get_dated_message(sim, fifo);
        auto       it   = list.begin();
        auto       end  = list.end();
        const auto t    = it->data[0];

        for (; it != end && it->data[0] <= t; ++it)
            irt_return_if_bad(
              send_message(sim, y[0], it->data[1], it->data[2], it->data[3]));

        return status::success;
    }
};

constexpr sz max(sz a) { return a; }

template<typename... Args>
constexpr sz max(sz a, Args... args)
{
    return std::max(max(args...), a);
}

constexpr sz max_size_in_bytes() noexcept
{
    return max(sizeof(qss1_integrator),
               sizeof(qss1_multiplier),
               sizeof(qss1_cross),
               sizeof(qss1_filter),
               sizeof(qss1_power),
               sizeof(qss1_square),
               sizeof(qss1_sum_2),
               sizeof(qss1_sum_3),
               sizeof(qss1_sum_4),
               sizeof(qss1_wsum_2),
               sizeof(qss1_wsum_3),
               sizeof(qss1_wsum_4),
               sizeof(qss2_integrator),
               sizeof(qss2_multiplier),
               sizeof(qss2_cross),
               sizeof(qss2_filter),
               sizeof(qss2_power),
               sizeof(qss2_square),
               sizeof(qss2_sum_2),
               sizeof(qss2_sum_3),
               sizeof(qss2_sum_4),
               sizeof(qss2_wsum_2),
               sizeof(qss2_wsum_3),
               sizeof(qss2_wsum_4),
               sizeof(qss3_integrator),
               sizeof(qss3_multiplier),
               sizeof(qss3_cross),
               sizeof(qss3_filter),
               sizeof(qss3_power),
               sizeof(qss3_square),
               sizeof(qss3_sum_2),
               sizeof(qss3_sum_3),
               sizeof(qss3_sum_4),
               sizeof(qss3_wsum_2),
               sizeof(qss3_wsum_3),
               sizeof(qss3_wsum_4),
               sizeof(integrator),
               sizeof(quantifier),
               sizeof(adder_2),
               sizeof(adder_3),
               sizeof(adder_4),
               sizeof(mult_2),
               sizeof(mult_3),
               sizeof(mult_4),
               sizeof(counter),
               sizeof(queue),
               sizeof(dynamic_queue),
               sizeof(priority_queue),
               sizeof(generator),
               sizeof(constant),
               sizeof(cross),
               sizeof(time_func),
               sizeof(accumulator_2),
               sizeof(filter),
               sizeof(logical_and_2),
               sizeof(logical_and_3),
               sizeof(logical_or_2),
               sizeof(logical_or_3),
               sizeof(logical_invert),
               sizeof(hsm_wrapper));
}

struct model
{
    real         tl     = 0.0;
    real         tn     = time_domain<time>::infinity;
    heap::handle handle = nullptr;

    observer_id   obs_id = observer_id{ 0 };
    dynamics_type type;

    alignas(8) std::byte dyn[max_size_in_bytes()];
};

template<typename Dynamics>
static constexpr dynamics_type dynamics_typeof() noexcept
{
    if constexpr (std::is_same_v<Dynamics, qss1_integrator>)
        return dynamics_type::qss1_integrator;
    if constexpr (std::is_same_v<Dynamics, qss1_multiplier>)
        return dynamics_type::qss1_multiplier;
    if constexpr (std::is_same_v<Dynamics, qss1_cross>)
        return dynamics_type::qss1_cross;
    if constexpr (std::is_same_v<Dynamics, qss1_filter>)
        return dynamics_type::qss1_filter;
    if constexpr (std::is_same_v<Dynamics, qss1_power>)
        return dynamics_type::qss1_power;
    if constexpr (std::is_same_v<Dynamics, qss1_square>)
        return dynamics_type::qss1_square;
    if constexpr (std::is_same_v<Dynamics, qss1_sum_2>)
        return dynamics_type::qss1_sum_2;
    if constexpr (std::is_same_v<Dynamics, qss1_sum_3>)
        return dynamics_type::qss1_sum_3;
    if constexpr (std::is_same_v<Dynamics, qss1_sum_4>)
        return dynamics_type::qss1_sum_4;
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_2>)
        return dynamics_type::qss1_wsum_2;
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_3>)
        return dynamics_type::qss1_wsum_3;
    if constexpr (std::is_same_v<Dynamics, qss1_wsum_4>)
        return dynamics_type::qss1_wsum_4;

    if constexpr (std::is_same_v<Dynamics, qss2_integrator>)
        return dynamics_type::qss2_integrator;
    if constexpr (std::is_same_v<Dynamics, qss2_multiplier>)
        return dynamics_type::qss2_multiplier;
    if constexpr (std::is_same_v<Dynamics, qss2_cross>)
        return dynamics_type::qss2_cross;
    if constexpr (std::is_same_v<Dynamics, qss2_filter>)
        return dynamics_type::qss2_filter;
    if constexpr (std::is_same_v<Dynamics, qss2_power>)
        return dynamics_type::qss2_power;
    if constexpr (std::is_same_v<Dynamics, qss2_square>)
        return dynamics_type::qss2_square;
    if constexpr (std::is_same_v<Dynamics, qss2_sum_2>)
        return dynamics_type::qss2_sum_2;
    if constexpr (std::is_same_v<Dynamics, qss2_sum_3>)
        return dynamics_type::qss2_sum_3;
    if constexpr (std::is_same_v<Dynamics, qss2_sum_4>)
        return dynamics_type::qss2_sum_4;
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_2>)
        return dynamics_type::qss2_wsum_2;
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_3>)
        return dynamics_type::qss2_wsum_3;
    if constexpr (std::is_same_v<Dynamics, qss2_wsum_4>)
        return dynamics_type::qss2_wsum_4;

    if constexpr (std::is_same_v<Dynamics, qss3_integrator>)
        return dynamics_type::qss3_integrator;
    if constexpr (std::is_same_v<Dynamics, qss3_multiplier>)
        return dynamics_type::qss3_multiplier;
    if constexpr (std::is_same_v<Dynamics, qss3_cross>)
        return dynamics_type::qss3_cross;
    if constexpr (std::is_same_v<Dynamics, qss3_filter>)
        return dynamics_type::qss3_filter;
    if constexpr (std::is_same_v<Dynamics, qss3_power>)
        return dynamics_type::qss3_power;
    if constexpr (std::is_same_v<Dynamics, qss3_square>)
        return dynamics_type::qss3_square;
    if constexpr (std::is_same_v<Dynamics, qss3_sum_2>)
        return dynamics_type::qss3_sum_2;
    if constexpr (std::is_same_v<Dynamics, qss3_sum_3>)
        return dynamics_type::qss3_sum_3;
    if constexpr (std::is_same_v<Dynamics, qss3_sum_4>)
        return dynamics_type::qss3_sum_4;
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_2>)
        return dynamics_type::qss3_wsum_2;
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_3>)
        return dynamics_type::qss3_wsum_3;
    if constexpr (std::is_same_v<Dynamics, qss3_wsum_4>)
        return dynamics_type::qss3_wsum_4;
    if constexpr (std::is_same_v<Dynamics, integrator>)
        return dynamics_type::integrator;
    if constexpr (std::is_same_v<Dynamics, quantifier>)
        return dynamics_type::quantifier;
    if constexpr (std::is_same_v<Dynamics, adder_2>)
        return dynamics_type::adder_2;
    if constexpr (std::is_same_v<Dynamics, adder_3>)
        return dynamics_type::adder_3;
    if constexpr (std::is_same_v<Dynamics, adder_4>)
        return dynamics_type::adder_4;
    if constexpr (std::is_same_v<Dynamics, mult_2>)
        return dynamics_type::mult_2;
    if constexpr (std::is_same_v<Dynamics, mult_3>)
        return dynamics_type::mult_3;
    if constexpr (std::is_same_v<Dynamics, mult_4>)
        return dynamics_type::mult_4;
    if constexpr (std::is_same_v<Dynamics, counter>)
        return dynamics_type::counter;
    if constexpr (std::is_same_v<Dynamics, queue>)
        return dynamics_type::queue;
    if constexpr (std::is_same_v<Dynamics, dynamic_queue>)
        return dynamics_type::dynamic_queue;
    if constexpr (std::is_same_v<Dynamics, priority_queue>)
        return dynamics_type::priority_queue;
    if constexpr (std::is_same_v<Dynamics, generator>)
        return dynamics_type::generator;
    if constexpr (std::is_same_v<Dynamics, constant>)
        return dynamics_type::constant;
    if constexpr (std::is_same_v<Dynamics, cross>)
        return dynamics_type::cross;
    if constexpr (std::is_same_v<Dynamics, time_func>)
        return dynamics_type::time_func;
    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return dynamics_type::accumulator_2;
    if constexpr (std::is_same_v<Dynamics, filter>)
        return dynamics_type::filter;

    if constexpr (std::is_same_v<Dynamics, logical_and_2>)
        return dynamics_type::logical_and_2;
    if constexpr (std::is_same_v<Dynamics, logical_and_3>)
        return dynamics_type::logical_and_3;
    if constexpr (std::is_same_v<Dynamics, logical_or_2>)
        return dynamics_type::logical_or_2;
    if constexpr (std::is_same_v<Dynamics, logical_or_3>)
        return dynamics_type::logical_or_3;
    if constexpr (std::is_same_v<Dynamics, logical_invert>)
        return dynamics_type::logical_invert;

    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        return dynamics_type::hsm_wrapper;

    irt_unreachable();
}

template<typename Function, typename... Args>
constexpr auto dispatch(const model& mdl, Function&& f, Args... args) noexcept
{
    switch (mdl.type) {
    case dynamics_type::qss1_integrator:
        return f(*reinterpret_cast<const qss1_integrator*>(&mdl.dyn), args...);
    case dynamics_type::qss1_multiplier:
        return f(*reinterpret_cast<const qss1_multiplier*>(&mdl.dyn), args...);
    case dynamics_type::qss1_cross:
        return f(*reinterpret_cast<const qss1_cross*>(&mdl.dyn), args...);
    case dynamics_type::qss1_filter:
        return f(*reinterpret_cast<const qss1_filter*>(&mdl.dyn), args...);
    case dynamics_type::qss1_power:
        return f(*reinterpret_cast<const qss1_power*>(&mdl.dyn), args...);
    case dynamics_type::qss1_square:
        return f(*reinterpret_cast<const qss1_square*>(&mdl.dyn), args...);
    case dynamics_type::qss1_sum_2:
        return f(*reinterpret_cast<const qss1_sum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss1_sum_3:
        return f(*reinterpret_cast<const qss1_sum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss1_sum_4:
        return f(*reinterpret_cast<const qss1_sum_4*>(&mdl.dyn), args...);
    case dynamics_type::qss1_wsum_2:
        return f(*reinterpret_cast<const qss1_wsum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss1_wsum_3:
        return f(*reinterpret_cast<const qss1_wsum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss1_wsum_4:
        return f(*reinterpret_cast<const qss1_wsum_4*>(&mdl.dyn), args...);

    case dynamics_type::qss2_integrator:
        return f(*reinterpret_cast<const qss2_integrator*>(&mdl.dyn), args...);
    case dynamics_type::qss2_multiplier:
        return f(*reinterpret_cast<const qss2_multiplier*>(&mdl.dyn), args...);
    case dynamics_type::qss2_cross:
        return f(*reinterpret_cast<const qss2_cross*>(&mdl.dyn), args...);
    case dynamics_type::qss2_filter:
        return f(*reinterpret_cast<const qss2_filter*>(&mdl.dyn), args...);
    case dynamics_type::qss2_power:
        return f(*reinterpret_cast<const qss2_power*>(&mdl.dyn), args...);
    case dynamics_type::qss2_square:
        return f(*reinterpret_cast<const qss2_square*>(&mdl.dyn), args...);
    case dynamics_type::qss2_sum_2:
        return f(*reinterpret_cast<const qss2_sum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss2_sum_3:
        return f(*reinterpret_cast<const qss2_sum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss2_sum_4:
        return f(*reinterpret_cast<const qss2_sum_4*>(&mdl.dyn), args...);
    case dynamics_type::qss2_wsum_2:
        return f(*reinterpret_cast<const qss2_wsum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss2_wsum_3:
        return f(*reinterpret_cast<const qss2_wsum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss2_wsum_4:
        return f(*reinterpret_cast<const qss2_wsum_4*>(&mdl.dyn), args...);

    case dynamics_type::qss3_integrator:
        return f(*reinterpret_cast<const qss3_integrator*>(&mdl.dyn), args...);
    case dynamics_type::qss3_multiplier:
        return f(*reinterpret_cast<const qss3_multiplier*>(&mdl.dyn), args...);
    case dynamics_type::qss3_cross:
        return f(*reinterpret_cast<const qss3_cross*>(&mdl.dyn), args...);
    case dynamics_type::qss3_filter:
        return f(*reinterpret_cast<const qss3_filter*>(&mdl.dyn), args...);
    case dynamics_type::qss3_power:
        return f(*reinterpret_cast<const qss3_power*>(&mdl.dyn), args...);
    case dynamics_type::qss3_square:
        return f(*reinterpret_cast<const qss3_square*>(&mdl.dyn), args...);
    case dynamics_type::qss3_sum_2:
        return f(*reinterpret_cast<const qss3_sum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss3_sum_3:
        return f(*reinterpret_cast<const qss3_sum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss3_sum_4:
        return f(*reinterpret_cast<const qss3_sum_4*>(&mdl.dyn), args...);
    case dynamics_type::qss3_wsum_2:
        return f(*reinterpret_cast<const qss3_wsum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss3_wsum_3:
        return f(*reinterpret_cast<const qss3_wsum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss3_wsum_4:
        return f(*reinterpret_cast<const qss3_wsum_4*>(&mdl.dyn), args...);

    case dynamics_type::integrator:
        return f(*reinterpret_cast<const integrator*>(&mdl.dyn), args...);
    case dynamics_type::quantifier:
        return f(*reinterpret_cast<const quantifier*>(&mdl.dyn), args...);
    case dynamics_type::adder_2:
        return f(*reinterpret_cast<const adder_2*>(&mdl.dyn), args...);
    case dynamics_type::adder_3:
        return f(*reinterpret_cast<const adder_3*>(&mdl.dyn), args...);
    case dynamics_type::adder_4:
        return f(*reinterpret_cast<const adder_4*>(&mdl.dyn), args...);
    case dynamics_type::mult_2:
        return f(*reinterpret_cast<const mult_2*>(&mdl.dyn), args...);
    case dynamics_type::mult_3:
        return f(*reinterpret_cast<const mult_3*>(&mdl.dyn), args...);
    case dynamics_type::mult_4:
        return f(*reinterpret_cast<const mult_4*>(&mdl.dyn), args...);
    case dynamics_type::counter:
        return f(*reinterpret_cast<const counter*>(&mdl.dyn), args...);
    case dynamics_type::queue:
        return f(*reinterpret_cast<const queue*>(&mdl.dyn), args...);
    case dynamics_type::dynamic_queue:
        return f(*reinterpret_cast<const dynamic_queue*>(&mdl.dyn), args...);
    case dynamics_type::priority_queue:
        return f(*reinterpret_cast<const priority_queue*>(&mdl.dyn), args...);
    case dynamics_type::generator:
        return f(*reinterpret_cast<const generator*>(&mdl.dyn), args...);
    case dynamics_type::constant:
        return f(*reinterpret_cast<const constant*>(&mdl.dyn), args...);
    case dynamics_type::cross:
        return f(*reinterpret_cast<const cross*>(&mdl.dyn), args...);
    case dynamics_type::accumulator_2:
        return f(*reinterpret_cast<const accumulator_2*>(&mdl.dyn), args...);
    case dynamics_type::time_func:
        return f(*reinterpret_cast<const time_func*>(&mdl.dyn), args...);
    case dynamics_type::filter:
        return f(*reinterpret_cast<const filter*>(&mdl.dyn), args...);

    case dynamics_type::logical_and_2:
        return f(*reinterpret_cast<const logical_and_2*>(&mdl.dyn), args...);
    case dynamics_type::logical_and_3:
        return f(*reinterpret_cast<const logical_and_3*>(&mdl.dyn), args...);
    case dynamics_type::logical_or_2:
        return f(*reinterpret_cast<const logical_or_2*>(&mdl.dyn), args...);
    case dynamics_type::logical_or_3:
        return f(*reinterpret_cast<const logical_or_3*>(&mdl.dyn), args...);
    case dynamics_type::logical_invert:
        return f(*reinterpret_cast<const logical_invert*>(&mdl.dyn), args...);

    case dynamics_type::hsm_wrapper:
        return f(*reinterpret_cast<const hsm_wrapper*>(&mdl.dyn), args...);
    }

    irt_unreachable();
}

template<typename Function, typename... Args>
constexpr auto dispatch(model& mdl, Function&& f, Args... args) noexcept
{
    switch (mdl.type) {
    case dynamics_type::qss1_integrator:
        return f(*reinterpret_cast<qss1_integrator*>(&mdl.dyn), args...);
    case dynamics_type::qss1_multiplier:
        return f(*reinterpret_cast<qss1_multiplier*>(&mdl.dyn), args...);
    case dynamics_type::qss1_cross:
        return f(*reinterpret_cast<qss1_cross*>(&mdl.dyn), args...);
    case dynamics_type::qss1_filter:
        return f(*reinterpret_cast<qss1_filter*>(&mdl.dyn), args...);
    case dynamics_type::qss1_power:
        return f(*reinterpret_cast<qss1_power*>(&mdl.dyn), args...);
    case dynamics_type::qss1_square:
        return f(*reinterpret_cast<qss1_square*>(&mdl.dyn), args...);
    case dynamics_type::qss1_sum_2:
        return f(*reinterpret_cast<qss1_sum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss1_sum_3:
        return f(*reinterpret_cast<qss1_sum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss1_sum_4:
        return f(*reinterpret_cast<qss1_sum_4*>(&mdl.dyn), args...);
    case dynamics_type::qss1_wsum_2:
        return f(*reinterpret_cast<qss1_wsum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss1_wsum_3:
        return f(*reinterpret_cast<qss1_wsum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss1_wsum_4:
        return f(*reinterpret_cast<qss1_wsum_4*>(&mdl.dyn), args...);

    case dynamics_type::qss2_integrator:
        return f(*reinterpret_cast<qss2_integrator*>(&mdl.dyn), args...);
    case dynamics_type::qss2_multiplier:
        return f(*reinterpret_cast<qss2_multiplier*>(&mdl.dyn), args...);
    case dynamics_type::qss2_cross:
        return f(*reinterpret_cast<qss2_cross*>(&mdl.dyn), args...);
    case dynamics_type::qss2_filter:
        return f(*reinterpret_cast<qss2_filter*>(&mdl.dyn), args...);
    case dynamics_type::qss2_power:
        return f(*reinterpret_cast<qss2_power*>(&mdl.dyn), args...);
    case dynamics_type::qss2_square:
        return f(*reinterpret_cast<qss2_square*>(&mdl.dyn), args...);
    case dynamics_type::qss2_sum_2:
        return f(*reinterpret_cast<qss2_sum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss2_sum_3:
        return f(*reinterpret_cast<qss2_sum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss2_sum_4:
        return f(*reinterpret_cast<qss2_sum_4*>(&mdl.dyn), args...);
    case dynamics_type::qss2_wsum_2:
        return f(*reinterpret_cast<qss2_wsum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss2_wsum_3:
        return f(*reinterpret_cast<qss2_wsum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss2_wsum_4:
        return f(*reinterpret_cast<qss2_wsum_4*>(&mdl.dyn), args...);

    case dynamics_type::qss3_integrator:
        return f(*reinterpret_cast<qss3_integrator*>(&mdl.dyn), args...);
    case dynamics_type::qss3_multiplier:
        return f(*reinterpret_cast<qss3_multiplier*>(&mdl.dyn), args...);
    case dynamics_type::qss3_cross:
        return f(*reinterpret_cast<qss3_cross*>(&mdl.dyn), args...);
    case dynamics_type::qss3_filter:
        return f(*reinterpret_cast<qss3_filter*>(&mdl.dyn), args...);
    case dynamics_type::qss3_power:
        return f(*reinterpret_cast<qss3_power*>(&mdl.dyn), args...);
    case dynamics_type::qss3_square:
        return f(*reinterpret_cast<qss3_square*>(&mdl.dyn), args...);
    case dynamics_type::qss3_sum_2:
        return f(*reinterpret_cast<qss3_sum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss3_sum_3:
        return f(*reinterpret_cast<qss3_sum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss3_sum_4:
        return f(*reinterpret_cast<qss3_sum_4*>(&mdl.dyn), args...);
    case dynamics_type::qss3_wsum_2:
        return f(*reinterpret_cast<qss3_wsum_2*>(&mdl.dyn), args...);
    case dynamics_type::qss3_wsum_3:
        return f(*reinterpret_cast<qss3_wsum_3*>(&mdl.dyn), args...);
    case dynamics_type::qss3_wsum_4:
        return f(*reinterpret_cast<qss3_wsum_4*>(&mdl.dyn), args...);

    case dynamics_type::integrator:
        return f(*reinterpret_cast<integrator*>(&mdl.dyn), args...);
    case dynamics_type::quantifier:
        return f(*reinterpret_cast<quantifier*>(&mdl.dyn), args...);
    case dynamics_type::adder_2:
        return f(*reinterpret_cast<adder_2*>(&mdl.dyn), args...);
    case dynamics_type::adder_3:
        return f(*reinterpret_cast<adder_3*>(&mdl.dyn), args...);
    case dynamics_type::adder_4:
        return f(*reinterpret_cast<adder_4*>(&mdl.dyn), args...);
    case dynamics_type::mult_2:
        return f(*reinterpret_cast<mult_2*>(&mdl.dyn), args...);
    case dynamics_type::mult_3:
        return f(*reinterpret_cast<mult_3*>(&mdl.dyn), args...);
    case dynamics_type::mult_4:
        return f(*reinterpret_cast<mult_4*>(&mdl.dyn), args...);
    case dynamics_type::counter:
        return f(*reinterpret_cast<counter*>(&mdl.dyn), args...);
    case dynamics_type::queue:
        return f(*reinterpret_cast<queue*>(&mdl.dyn), args...);
    case dynamics_type::dynamic_queue:
        return f(*reinterpret_cast<dynamic_queue*>(&mdl.dyn), args...);
    case dynamics_type::priority_queue:
        return f(*reinterpret_cast<priority_queue*>(&mdl.dyn), args...);
    case dynamics_type::generator:
        return f(*reinterpret_cast<generator*>(&mdl.dyn), args...);
    case dynamics_type::constant:
        return f(*reinterpret_cast<constant*>(&mdl.dyn), args...);
    case dynamics_type::cross:
        return f(*reinterpret_cast<cross*>(&mdl.dyn), args...);
    case dynamics_type::accumulator_2:
        return f(*reinterpret_cast<accumulator_2*>(&mdl.dyn), args...);
    case dynamics_type::time_func:
        return f(*reinterpret_cast<time_func*>(&mdl.dyn), args...);
    case dynamics_type::filter:
        return f(*reinterpret_cast<filter*>(&mdl.dyn), args...);

    case dynamics_type::logical_and_2:
        return f(*reinterpret_cast<logical_and_2*>(&mdl.dyn), args...);
    case dynamics_type::logical_and_3:
        return f(*reinterpret_cast<logical_and_3*>(&mdl.dyn), args...);
    case dynamics_type::logical_or_2:
        return f(*reinterpret_cast<logical_or_2*>(&mdl.dyn), args...);
    case dynamics_type::logical_or_3:
        return f(*reinterpret_cast<logical_or_3*>(&mdl.dyn), args...);
    case dynamics_type::logical_invert:
        return f(*reinterpret_cast<logical_invert*>(&mdl.dyn), args...);

    case dynamics_type::hsm_wrapper:
        return f(*reinterpret_cast<hsm_wrapper*>(&mdl.dyn), args...);
    }

    irt_unreachable();
}

template<typename Dynamics>
Dynamics& get_dyn(model& mdl) noexcept
{
    irt_assert(dynamics_typeof<Dynamics>() == mdl.type);

    return *reinterpret_cast<Dynamics*>(&mdl.dyn);
}

template<typename Dynamics>
const Dynamics& get_dyn(const model& mdl) noexcept
{
    irt_assert(dynamics_typeof<Dynamics>() == mdl.type);

    return *reinterpret_cast<const Dynamics*>(&mdl.dyn);
}

template<typename Dynamics>
constexpr const model& get_model(const Dynamics& d) noexcept
{
    const Dynamics* __mptr = &d;
    return *(const model*)((const char*)__mptr - offsetof(model, dyn));
}

template<typename Dynamics>
constexpr model& get_model(Dynamics& d) noexcept
{
    Dynamics* __mptr = &d;
    return *(model*)((char*)__mptr - offsetof(model, dyn));
}

inline status get_input_port(model& src, int port_src, input_port*& p) noexcept
{
    return dispatch(src,
                    [port_src, &p]<typename Dynamics>(Dynamics& dyn) -> status {
                        if constexpr (has_input_port<Dynamics>) {
                            if (port_src >= 0 && port_src < length(dyn.x)) {
                                p = &dyn.x[port_src];
                                return status::success;
                            }
                        }

                        return status::model_connect_output_port_unknown;
                    });
}

inline status get_output_port(model&        dst,
                              int           port_dst,
                              output_port*& p) noexcept
{
    return dispatch(dst,
                    [port_dst, &p]<typename Dynamics>(Dynamics& dyn) -> status {
                        if constexpr (has_output_port<Dynamics>) {
                            if (port_dst >= 0 && port_dst < length(dyn.y)) {
                                p = &dyn.y[port_dst];
                                return status::success;
                            }
                        }

                        return status::model_connect_output_port_unknown;
                    });
}

inline bool is_ports_compatible(const dynamics_type mdl_src,
                                const int           o_port_index,
                                const dynamics_type mdl_dst,
                                const int           i_port_index) noexcept
{
    switch (mdl_src) {
    case dynamics_type::quantifier:
        if (mdl_dst == dynamics_type::integrator &&
            i_port_index ==
              static_cast<int>(integrator::port_name::port_quanta))
            return true;

        return false;

    case dynamics_type::qss1_integrator:
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_power:
    case dynamics_type::qss1_square:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_power:
    case dynamics_type::qss2_square:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss3_integrator:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_power:
    case dynamics_type::qss3_square:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::qss3_wsum_4:
    case dynamics_type::integrator:
    case dynamics_type::adder_2:
    case dynamics_type::adder_3:
    case dynamics_type::adder_4:
    case dynamics_type::mult_2:
    case dynamics_type::mult_3:
    case dynamics_type::mult_4:
    case dynamics_type::counter:
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
    case dynamics_type::generator:
    case dynamics_type::time_func:
    case dynamics_type::filter:
    case dynamics_type::hsm_wrapper:
    case dynamics_type::accumulator_2:
        if (mdl_dst == dynamics_type::integrator &&
            i_port_index ==
              static_cast<int>(integrator::port_name::port_quanta))
            return false;

        if (match(mdl_dst,
                  dynamics_type::logical_and_2,
                  dynamics_type::logical_and_3,
                  dynamics_type::logical_or_2,
                  dynamics_type::logical_or_3,
                  dynamics_type::logical_invert))
            return false;
        return true;

    case dynamics_type::constant:
        return true;

    case dynamics_type::cross:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
    case dynamics_type::qss1_cross:
        if (o_port_index == 2) {
            return match(mdl_dst,
                         dynamics_type::counter,
                         dynamics_type::logical_and_2,
                         dynamics_type::logical_and_3,
                         dynamics_type::logical_or_2,
                         dynamics_type::logical_or_3,
                         dynamics_type::logical_invert);
        } else {
            return !match(mdl_dst,
                          dynamics_type::logical_and_2,
                          dynamics_type::logical_and_3,
                          dynamics_type::logical_or_2,
                          dynamics_type::logical_or_3,
                          dynamics_type::logical_invert);
        }
        return true;

    case dynamics_type::qss2_filter:
    case dynamics_type::qss3_filter:
    case dynamics_type::qss1_filter:
        if (match(o_port_index, 1, 2)) {
            return match(mdl_dst,
                         dynamics_type::counter,
                         dynamics_type::logical_and_2,
                         dynamics_type::logical_and_3,
                         dynamics_type::logical_or_2,
                         dynamics_type::logical_or_3,
                         dynamics_type::logical_invert);
        } else {
            return !match(mdl_dst,
                          dynamics_type::logical_and_2,
                          dynamics_type::logical_and_3,
                          dynamics_type::logical_or_2,
                          dynamics_type::logical_or_3,
                          dynamics_type::logical_invert);
        }
        return true;

    case dynamics_type::logical_and_2:
    case dynamics_type::logical_and_3:
    case dynamics_type::logical_or_2:
    case dynamics_type::logical_or_3:
    case dynamics_type::logical_invert:
        if (match(mdl_dst,
                  dynamics_type::counter,
                  dynamics_type::logical_and_2,
                  dynamics_type::logical_and_3,
                  dynamics_type::logical_or_2,
                  dynamics_type::logical_or_3,
                  dynamics_type::logical_invert))
            return true;
    }

    return false;
}

inline bool is_ports_compatible(const model& mdl_src,
                                const int    o_port_index,
                                const model& mdl_dst,
                                const int    i_port_index) noexcept
{
    if (&mdl_src == &mdl_dst)
        return false;

    return is_ports_compatible(
      mdl_src.type, o_port_index, mdl_dst.type, i_port_index);
}

inline status global_connect(simulation& sim,
                             model&      src,
                             int         port_src,
                             model_id    dst,
                             int         port_dst) noexcept
{
    return dispatch(
      src,
      [&sim, port_src, dst, port_dst]<typename Dynamics>(
        Dynamics& dyn) -> status {
          if constexpr (has_output_port<Dynamics>) {
              auto list = append_node(sim, dyn.y[static_cast<u8>(port_src)]);
              for (const auto& elem : list) {
                  irt_return_if_fail(
                    !(elem.model == dst && elem.port_index == port_dst),
                    status::model_connect_already_exist);
              }

              irt_return_if_fail(can_alloc_node(sim, 1),
                                 status::simulation_not_enough_connection);

              list.emplace_back(dst, static_cast<i8>(port_dst));

              return status::success;
          }

          irt_unreachable();
      });
}

inline status global_disconnect(simulation& sim,
                                model&      src,
                                int         port_src,
                                model_id    dst,
                                int         port_dst) noexcept
{
    return dispatch(
      src,
      [&sim, port_src, dst, port_dst]<typename Dynamics>(
        Dynamics& dyn) -> status {
          if constexpr (has_output_port<Dynamics>) {
              auto list = append_node(sim, dyn.y[port_src]);
              for (auto it = list.begin(), end = list.end(); it != end; ++it) {
                  if (it->model == dst && it->port_index == port_dst) {
                      it = list.erase(it);
                      return status::success;
                  }
              }
          }

          irt_unreachable();
      });
}

/*****************************************************************************
 *
 * scheduller
 *
 ****************************************************************************/

class scheduller
{
private:
    heap m_heap;

public:
    scheduller() = default;

    status init(std::integral auto new_capacity) noexcept
    {
        irt_return_if_fail(is_numeric_castable<u32>(new_capacity),
                           status::head_allocator_bad_capacity);

        irt_return_if_bad(m_heap.init(new_capacity));

        return status::success;
    }

    void clear() { m_heap.clear(); }

    //! @brief Insert a newly model into the scheduller.
    void insert(model& mdl, model_id id, time tn) noexcept
    {
        irt_assert(mdl.handle == nullptr);

        mdl.handle = m_heap.insert(tn, id);
    }

    //! @brief Reintegrate or reinsert an old popped model into the
    //! scheduller.
    void reintegrate(model& mdl, time tn) noexcept
    {
        irt_assert(mdl.handle != nullptr);

        mdl.handle->tn = tn;

        m_heap.insert(mdl.handle);
    }

    void erase(model& mdl) noexcept
    {
        if (mdl.handle) {
            m_heap.remove(mdl.handle);
            m_heap.destroy(mdl.handle);
            mdl.handle = nullptr;
        }
    }

    void update(model& mdl, time tn) noexcept
    {
        irt_assert(mdl.handle != nullptr);

        mdl.handle->tn = tn;

        irt_assert(tn <= mdl.tn);

        if (tn < mdl.tn)
            m_heap.decrease(mdl.handle);
        else if (tn > mdl.tn)
            m_heap.increase(mdl.handle);
    }

    void pop(vector<model_id>& out) noexcept;

    // void pop(vector<model_id>& out) noexcept
    // {
    //     time t = tn();

    //     out.clear();
    //     out.emplace_back(m_heap.pop()->id);

    //     while (!m_heap.empty() && t == tn())
    //         out.emplace_back(m_heap.pop()->id);
    // }

    time tn() const noexcept { return m_heap.top()->tn; }

    bool empty() const noexcept { return m_heap.empty(); }

    unsigned size() const noexcept { return m_heap.size(); }
    int      ssize() const noexcept { return static_cast<int>(m_heap.size()); }
};

/*****************************************************************************
 *
 * simulation
 *
 ****************************************************************************/

inline void copy(const model& src, model& dst) noexcept
{
    dst.type   = src.type;
    dst.handle = nullptr;

    dispatch(dst, [&src]<typename Dynamics>(Dynamics& dst_dyn) -> void {
        const auto& src_dyn = get_dyn<Dynamics>(src);
        std::construct_at(&dst_dyn, src_dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dst_dyn.x); i != e; ++i)
                dst_dyn.x[i] = static_cast<u64>(-1);

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dst_dyn.y); i != e; ++i)
                dst_dyn.y[i] = static_cast<u64>(-1);
    });
}

struct simulation
{
    block_allocator<list_view_node<message>>       message_alloc;
    block_allocator<list_view_node<node>>          node_alloc;
    block_allocator<list_view_node<record>>        record_alloc;
    block_allocator<list_view_node<dated_message>> dated_message_alloc;
    vector<output_message>                         emitting_output_ports;
    vector<model_id>                               immediate_models;
    vector<observer_id>                            immediate_observers;
    data_array<model, model_id>                    models;
    data_array<hierarchical_state_machine, hsm_id> hsms;
    data_array<observer, observer_id>              observers;

    external_source srcs;
    scheduller      sched;

    //! @brief Use initialize, generate or finalize data from a source.
    //!
    //! See the @c external_source class for an implementation.

    model_id get_id(const model& mdl) const { return models.get_id(mdl); }

    template<typename Dynamics>
    model_id get_id(const Dynamics& dyn) const
    {
        return models.get_id(get_model(dyn));
    }

public:
    status init(std::integral auto model_capacity,
                std::integral auto messages_capacity)
    {
        constexpr size_t ten = 10u;

        irt_return_if_fail(is_numeric_castable<u32>(model_capacity),
                           status::simulation_not_enough_model);

        irt_return_if_fail(is_numeric_castable<u32>(messages_capacity),
                           status::simulation_not_enough_model);

        size_t max_hsms = (model_capacity / 10) <= 0
                            ? 1u
                            : static_cast<unsigned>(model_capacity) / 10u;

        size_t max_srcs = (model_capacity / 10) <= 10
                            ? 10u
                            : static_cast<unsigned>(model_capacity) / 10u;

        irt_return_if_bad(message_alloc.init(messages_capacity));
        irt_return_if_bad(node_alloc.init(model_capacity * ten));
        irt_return_if_bad(record_alloc.init(model_capacity * ten));
        irt_return_if_bad(dated_message_alloc.init(model_capacity));
        irt_return_if_bad(models.init(model_capacity));
        irt_return_if_bad(hsms.init(max_hsms));
        irt_return_if_bad(observers.init(model_capacity));
        irt_return_if_bad(sched.init(model_capacity));
        irt_return_if_bad(srcs.init(max_srcs));

        emitting_output_ports.reserve(model_capacity);
        immediate_models.reserve(model_capacity);
        immediate_observers.reserve(model_capacity);

        return status::success;
    }

    bool can_alloc(std::integral auto place) const noexcept;
    bool can_alloc(dynamics_type type, std::integral auto place) const noexcept;

    template<typename Dynamics>
    bool can_alloc_dynamics(std::integral auto place) const noexcept;

    //! @brief cleanup simulation object
    //!
    //! Clean scheduller and input/output port from message.
    void clean() noexcept
    {
        sched.clear();

        message_alloc.reset();
        record_alloc.reset();
        dated_message_alloc.reset();

        emitting_output_ports.clear();
        immediate_models.clear();
        immediate_observers.clear();
    }

    //! @brief cleanup simulation and destroy all models and connections
    void clear() noexcept
    {
        clean();

        node_alloc.reset();

        models.clear();
        observers.clear();
    }

    //! @brief This function allocates dynamics and models.
    template<typename Dynamics>
    Dynamics& alloc() noexcept;

    //! @brief This function allocates dynamics and models.
    model& clone(const model& mdl) noexcept;

    //! @brief This function allocates dynamics and models.
    model& alloc(dynamics_type type) noexcept;

    void observe(model& mdl, observer& obs) noexcept
    {
        mdl.obs_id = observers.get_id(obs);
        obs.model  = models.get_id(mdl);
        obs.type   = mdl.type;
    }

    void unobserve(model& mdl) noexcept
    {
        if (auto* obs = observers.try_to_get(mdl.obs_id); obs) {
            obs->model = undefined<model_id>();
            mdl.obs_id = undefined<observer_id>();
            observers.free(*obs);
        }

        mdl.obs_id = undefined<observer_id>();
    }

    status deallocate(model_id id)
    {
        auto* mdl = models.try_to_get(id);
        irt_return_if_fail(mdl, status::unknown_dynamics);

        unobserve(*mdl);

        dispatch(*mdl, [this]<typename Dynamics>(Dynamics& dyn) {
            this->do_deallocate<Dynamics>(dyn);
        });

        sched.erase(*mdl);
        models.free(*mdl);

        return status::success;
    }

    template<typename Dynamics>
    void do_deallocate(Dynamics& dyn) noexcept
    {
        if constexpr (has_output_port<Dynamics>) {
            for (auto& elem : dyn.y)
                append_node(*this, elem).clear();
        }

        if constexpr (has_input_port<Dynamics>) {
            for (auto& elem : dyn.x)
                append_message(*this, elem).clear();
        }

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            if (auto* machine = hsms.try_to_get(dyn.id); machine)
                hsms.free(dyn.id);
        }

        std::destroy_at(&dyn);
    }

    bool can_connect(int number) const noexcept
    {
        return can_alloc_node(*this, number);
    }

    status connect(model& src, int port_src, model& dst, int port_dst) noexcept
    {
        irt_return_if_fail(is_ports_compatible(src, port_src, dst, port_dst),
                           status::model_connect_bad_dynamics);

        const auto dst_id = get_id(dst);

        return global_connect(*this, src, port_src, dst_id, port_dst);
    }

    template<typename DynamicsSrc, typename DynamicsDst>
    status connect(DynamicsSrc& src,
                   int          port_src,
                   DynamicsDst& dst,
                   int          port_dst) noexcept
    {
        model&   src_model    = get_model(src);
        model&   dst_model    = get_model(dst);
        model_id model_dst_id = get_id(dst);

        irt_return_if_fail(
          is_ports_compatible(src_model, port_src, dst_model, port_dst),
          status::model_connect_bad_dynamics);

        return global_connect(
          *this, src_model, port_src, model_dst_id, port_dst);
    }

    status disconnect(model& src,
                      int    port_src,
                      model& dst,
                      int    port_dst) noexcept
    {
        return global_disconnect(*this, src, port_src, get_id(dst), port_dst);
    }

    status initialize(time t) noexcept
    {
        clean();

        irt::model* mdl = nullptr;
        while (models.next(mdl))
            irt_return_if_bad(make_initialize(*mdl, t));

        irt::observer* obs = nullptr;
        while (observers.next(obs)) {
            obs->reset();

            if (auto* mdl = models.try_to_get(obs->model); mdl) {
                dispatch(*mdl,
                         [mdl, &obs, t]<typename Dynamics>(Dynamics& dyn) {
                             if constexpr (has_observation_function<Dynamics>) {
                                 obs->update(dyn.observation(t, t - mdl->tl));
                             }
                         });
            }
        }

        return status::success;
    }

    status run(time& t) noexcept;

    // status run(time& t) noexcept
    // {
    //     if (sched.empty()) {
    //         t = time_domain<time>::infinity;
    //         return status::success;
    //     }

    //     if (t = sched.tn(); time_domain<time>::is_infinity(t))
    //         return status::success;

    //     immediate_models.clear();
    //     sched.pop(immediate_models);

    //     emitting_output_ports.clear();
    //     for (const auto id : immediate_models)
    //         if (auto* mdl = models.try_to_get(id); mdl)
    //             irt_return_if_bad(make_transition(*mdl, t));

    //     for (int i = 0, e = length(emitting_output_ports); i != e; ++i) {
    //         auto* mdl =
    //         models.try_to_get(emitting_output_ports[i].model); if (!mdl)
    //             continue

    //         sched.update(*mdl, t);

    //         irt_return_if_fail(can_alloc_message(*this, 1),
    //                            status::simulation_not_enough_message);

    //         auto  port = emitting_output_ports[i].port;
    //         auto& msg  = emitting_output_ports[i].msg;

    //         dispatch(
    //           *mdl, [this, port, &msg]<typename Dynamics>(Dynamics& dyn)
    //           {
    //               if constexpr (is_detected_v<has_input_port_t,
    //               Dynamics>) {
    //                   auto list = append_message(*this, dyn.x[port]);
    //                   list.push_back(msg);
    //               }
    //           });
    //     }

    //     return status::success;
    // }

    template<typename Dynamics>
    status make_initialize(model& mdl, Dynamics& dyn, time t) noexcept
    {
        if constexpr (has_input_port<Dynamics>) {
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = static_cast<u64>(-1);
        }

        if constexpr (has_initialize_function<Dynamics>)
            irt_return_if_bad(dyn.initialize(*this));

        mdl.tl     = t;
        mdl.tn     = t + dyn.sigma;
        mdl.handle = nullptr;

        sched.insert(mdl, models.get_id(mdl), mdl.tn);

        return status::success;
    }

    status make_initialize(model& mdl, time t) noexcept
    {
        return dispatch(mdl, [this, &mdl, t]<typename Dynamics>(Dynamics& dyn) {
            return this->make_initialize(mdl, dyn, t);
        });
    }

    template<typename Dynamics>
    status make_transition(model& mdl, Dynamics& dyn, time t) noexcept
    {
        if constexpr (has_observation_function<Dynamics>) {
            if (mdl.obs_id != undefined<observer_id>()) {
                if (auto* obs = observers.try_to_get(mdl.obs_id); obs) {
                    obs->update(dyn.observation(t, t - mdl.tl));

                    if (obs->full())
                        immediate_observers.emplace_back(mdl.obs_id);
                }
            } else {
                mdl.obs_id = static_cast<observer_id>(0);
            }
        }

        if (mdl.tn == mdl.handle->tn) {
            if constexpr (has_lambda_function<Dynamics>)
                if constexpr (has_output_port<Dynamics>)
                    irt_return_if_bad(dyn.lambda(*this));
        }

        if constexpr (has_transition_function<Dynamics>)
            irt_return_if_bad(dyn.transition(*this, t, t - mdl.tl, mdl.tn - t));

        if constexpr (has_input_port<Dynamics>) {
            for (auto& elem : dyn.x)
                append_message(*this, elem).clear();
        }

        irt_assert(mdl.tn >= t);

        mdl.tl = t;
        mdl.tn = t + dyn.sigma;
        if (dyn.sigma != 0 && mdl.tn == t)
            mdl.tn = std::nextafter(t, t + irt::one);

        sched.reintegrate(mdl, mdl.tn);

        return status::success;
    }

    status make_transition(model& mdl, time t) noexcept
    {
        return dispatch(mdl, [this, &mdl, t]<typename Dynamics>(Dynamics& dyn) {
            return this->make_transition(mdl, dyn, t);
        });
    }

    template<typename Dynamics>
    status make_finalize(Dynamics& dyn, observer* obs, time t) noexcept
    {
        if constexpr (has_observation_function<Dynamics>) {
            if (obs) {
                obs->update(dyn.observation(t, t - get_model(dyn).tl));
            }
        }

        if constexpr (has_finalize_function<Dynamics>) {
            irt_return_if_bad(dyn.finalize(*this));
        }

        return status::success;
    }

    /**
     * @brief Finalize and cleanup simulation objects.
     *
     * Clean:
     * - the scheduller nodes
     * - all input/output remaining messages
     * - call the observers' callback to finalize observation
     *
     * This function must be call at the end of the simulation.
     */
    status finalize(time t) noexcept
    {
        model* mdl = nullptr;
        while (models.next(mdl)) {
            observer* obs = nullptr;
            if (is_defined(mdl->obs_id))
                obs = observers.try_to_get(mdl->obs_id);

            auto ret =
              dispatch(*mdl, [this, obs, t]<typename Dynamics>(Dynamics& dyn) {
                  return this->make_finalize(dyn, obs, t);
              });

            irt_return_if_bad(ret);
        }

        return status::success;
    }
};

inline status initialize_source(simulation& sim, source& src) noexcept
{
    return sim.srcs.dispatch(src, source::operation_type::initialize);
}

inline status update_source(simulation& sim, source& src, double& val) noexcept
{
    if (src.next(val))
        return status::success;

    if (auto ret = sim.srcs.dispatch(src, source::operation_type::update);
        is_bad(ret))
        return ret;

    return src.next(val) ? status::success : status::source_empty;
}

inline status finalize_source(simulation& sim, source& src) noexcept
{
    return sim.srcs.dispatch(src, source::operation_type::finalize);
}

inline bool can_alloc_message(const simulation& sim, int alloc_number) noexcept
{
    return sim.message_alloc.can_alloc(alloc_number);
}

inline list_view<message> append_message(simulation& sim,
                                         input_port& port) noexcept
{
    return list_view<message>(sim.message_alloc, port);
}

inline list_view_const<message> get_message(const simulation& sim,
                                            const input_port  port) noexcept
{
    return list_view_const<message>(sim.message_alloc, port);
}

inline list_view<node> append_node(simulation& sim, output_port& port) noexcept
{
    return list_view<node>(sim.node_alloc, port);
}

inline list_view_const<node> get_node(const simulation& sim,
                                      const output_port port) noexcept
{
    return list_view_const<node>(sim.node_alloc, port);
}

inline list_view_const<record> get_archive(const simulation& sim,
                                           const u64         id) noexcept
{
    return list_view_const<record>(sim.record_alloc, id);
}

inline list_view<record> append_archive(simulation& sim, u64& id) noexcept
{
    return list_view<record>(sim.record_alloc, id);
}

inline bool can_alloc_node(const simulation& sim, int alloc_number) noexcept
{
    return sim.node_alloc.can_alloc(alloc_number);
}

inline bool can_alloc_dated_message(const simulation& sim,
                                    int               alloc_number) noexcept
{
    return sim.dated_message_alloc.can_alloc(alloc_number);
}

inline list_view<dated_message> append_dated_message(simulation& sim,
                                                     u64&        id) noexcept
{
    return list_view<dated_message>(sim.dated_message_alloc, id);
}

inline list_view_const<dated_message> get_dated_message(const simulation& sim,
                                                        const u64 id) noexcept
{
    return list_view_const<dated_message>(sim.dated_message_alloc, id);
}

/*****************************************************************************
 *
 * Containers implementation
 *
 ****************************************************************************/

// template<typeanem T, typename Identifier>
// class data_array;

template<typename T, typename Identifier>
constexpr typename data_array<T, Identifier>::identifier_type
data_array<T, Identifier>::make_id(index_type key, index_type index) noexcept
{
    if constexpr (std::is_same_v<u16, index_type>)
        return static_cast<identifier_type>((static_cast<u32>(key) << 16) |
                                            static_cast<u32>(index));
    else
        return static_cast<identifier_type>((static_cast<u64>(key) << 32) |
                                            static_cast<u64>(index));
}

template<typename T, typename Identifier>
constexpr typename data_array<T, Identifier>::index_type
data_array<T, Identifier>::make_next_key(index_type key) noexcept
{
    if constexpr (std::is_same_v<u16, index_type>)
        return key == 0xffffffff ? 1u : key + 1u;
    else
        return key == 0xffffffffffffffff ? 1u : key + 1;
}

template<typename T, typename Identifier>
constexpr typename data_array<T, Identifier>::index_type
data_array<T, Identifier>::get_key(Identifier id) noexcept
{
    if constexpr (std::is_same_v<u16, index_type>)
        return static_cast<u16>((static_cast<u32>(id) >> 16) & 0xffff);
    else
        return static_cast<u32>((static_cast<u64>(id) >> 32) & 0xffffffff);
}

template<typename T, typename Identifier>
constexpr typename data_array<T, Identifier>::index_type
data_array<T, Identifier>::get_index(Identifier id) noexcept
{
    if constexpr (std::is_same_v<u16, index_type>)
        return static_cast<u16>(static_cast<u32>(id) & 0xffff);
    else
        return static_cast<u32>(static_cast<u64>(id) & 0xffffffff);
}

template<typename T, typename Identifier>
data_array<T, Identifier>::~data_array() noexcept
{
    clear();

    if (m_items)
        g_free_fn(m_items);
}

template<typename T, typename Identifier>
status data_array<T, Identifier>::init(std::integral auto capacity) noexcept
{
    if (std::cmp_greater_equal(capacity,
                               std::numeric_limits<index_type>::max()))
        return status::data_array_init_capacity_error;

    auto* buffer = g_alloc_fn(static_cast<sz>(capacity) * sizeof(item));
    irt_return_if_fail(buffer, status::data_array_not_enough_memory);

    clear();

    m_items     = reinterpret_cast<item*>(buffer);
    m_max_size  = 0;
    m_max_used  = 0;
    m_capacity  = static_cast<index_type>(capacity);
    m_next_key  = 1;
    m_free_head = none;

    return status::success;
}

template<typename T, typename Identifier>
status data_array<T, Identifier>::resize(std::integral auto capacity) noexcept
{
    if (std::cmp_greater_equal(m_capacity, capacity))
        return status::success;

    auto* buffer = g_alloc_fn(static_cast<sz>(capacity) * sizeof(item));
    irt_return_if_fail(buffer, status::data_array_not_enough_memory);

    auto* new_items = reinterpret_cast<item*>(buffer);
    std::copy_n(m_items, static_cast<sz>(m_max_used), new_items);

    g_free_fn(m_items);
    m_items = new_items;

    return status::success;
}

template<typename T, typename Identifier>
void data_array<T, Identifier>::clear() noexcept
{
    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (index_type i = 0; i != m_max_used; ++i) {
            if (is_valid(m_items[i].id)) {
                std::destroy_at(&m_items[i].item);
                m_items[i].id = static_cast<identifier_type>(0);
            }
        }
    } else {
        auto* void_ptr = reinterpret_cast<std::byte*>(m_items);
        auto  size     = m_max_used * sizeof(item);
        std::uninitialized_fill_n(void_ptr, size, std::byte{});
    }

    m_max_size  = 0;
    m_max_used  = 0;
    m_next_key  = 1;
    m_free_head = none;
}

template<typename T, typename Identifier>
template<typename... Args>
typename data_array<T, Identifier>::value_type&
data_array<T, Identifier>::alloc(Args&&... args) noexcept
{
    irt_assert(can_alloc(1) && "check alloc() with full() before using use.");

    index_type new_index;

    if (m_free_head != none) {
        new_index = m_free_head;
        if (is_valid(m_items[m_free_head].id))
            m_free_head = none;
        else
            m_free_head = get_index(m_items[m_free_head].id);
    } else {
        new_index = m_max_used++;
    }

    std::construct_at(&m_items[new_index].item, std::forward<Args>(args)...);

    m_items[new_index].id = make_id(m_next_key, new_index);
    m_next_key            = make_next_key(m_next_key);

    ++m_max_size;

    return m_items[new_index].item;
}

template<typename T, typename Identifier>
template<typename... Args>
std::pair<bool, T*> data_array<T, Identifier>::try_alloc(
  Args&&... args) noexcept
{
    if (!can_alloc(1))
        return { false, nullptr };

    index_type new_index;

    if (m_free_head != none) {
        new_index = m_free_head;
        if (is_valid(m_items[m_free_head].id))
            m_free_head = none;
        else
            m_free_head = get_index(m_items[m_free_head].id);
    } else {
        new_index = m_max_used++;
    }

    std::construct_at(&m_items[new_index].item, std::forward<Args>(args)...);

    m_items[new_index].id = make_id(m_next_key, new_index);
    m_next_key            = make_next_key(m_next_key);

    ++m_max_size;

    return { true, &m_items[new_index].item };
}

template<typename T, typename Identifier>
void data_array<T, Identifier>::free(T& t) noexcept
{
    auto id    = get_id(t);
    auto index = get_index(id);

    irt_assert(&m_items[index] == static_cast<void*>(&t));
    irt_assert(m_items[index].id == id);
    irt_assert(is_valid(id));

    std::destroy_at(&m_items[index].item);

    m_items[index].id = static_cast<Identifier>(m_free_head);
    m_free_head       = static_cast<index_type>(index);

    --m_max_size;
}

template<typename T, typename Identifier>
void data_array<T, Identifier>::free(Identifier id) noexcept
{
    auto index = get_index(id);

    if (m_items[index].id == id && is_valid(id)) {
        std::destroy_at(&m_items[index].item);

        m_items[index].id = static_cast<Identifier>(m_free_head);
        m_free_head       = static_cast<index_type>(index);

        --m_max_size;
    }
}

template<typename T, typename Identifier>
Identifier data_array<T, Identifier>::get_id(const T* t) const noexcept
{
    irt_assert(t != nullptr);

    auto* ptr = reinterpret_cast<const item*>(t);
    return ptr->id;
}

template<typename T, typename Identifier>
Identifier data_array<T, Identifier>::get_id(const T& t) const noexcept
{
    auto* ptr = reinterpret_cast<const item*>(&t);
    return ptr->id;
}

template<typename T, typename Identifier>
T& data_array<T, Identifier>::get(Identifier id) noexcept
{
    return m_items[get_index(id)].item;
}

template<typename T, typename Identifier>
const T& data_array<T, Identifier>::get(Identifier id) const noexcept
{
    return m_items[get_index(id)].item;
}

template<typename T, typename Identifier>
T* data_array<T, Identifier>::try_to_get(Identifier id) const noexcept
{
    if (get_key(id)) {
        auto index = get_index(id);
        if (std::cmp_greater_equal(index, 0) &&
            std::cmp_less(index, m_max_used) && m_items[index].id == id)
            return &m_items[index].item;
    }

    return nullptr;
}

template<typename T, typename Identifier>
T* data_array<T, Identifier>::try_to_get(
  std::integral auto index) const noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_max_used));

    if (is_valid(m_items[index].id))
        return &m_items[index].item;

    return nullptr;
}

template<typename T, typename Identifier>
bool data_array<T, Identifier>::next(T*& t) noexcept
{
    index_type index;

    if (t) {
        auto id = get_id(*t);
        index   = get_index(id);
        ++index;

        for (; index < m_max_used; ++index) {
            if (is_valid(m_items[index].id)) {
                t = &m_items[index].item;
                return true;
            }
        }
    } else {
        for (index = 0; index < m_max_used; ++index) {
            if (is_valid(m_items[index].id)) {
                t = &m_items[index].item;
                return true;
            }
        }
    }

    return false;
}

template<typename T, typename Identifier>
bool data_array<T, Identifier>::next(const T*& t) const noexcept
{
    index_type index;

    if (t) {
        auto id = get_id(*t);
        index   = get_index(id);
        ++index;

        for (; index < m_max_used; ++index) {
            if (is_valid(m_items[index].id)) {
                t = &m_items[index].item;
                return true;
            }
        }
    } else {
        for (index = 0; index < m_max_used; ++index) {
            if (is_valid(m_items[index].id)) {
                t = &m_items[index].item;
                return true;
            }
        }
    }

    return false;
}

template<typename T, typename Identifier>
constexpr bool data_array<T, Identifier>::full() const noexcept
{
    return m_free_head == none && m_max_used == m_capacity;
}

template<typename T, typename Identifier>
constexpr unsigned data_array<T, Identifier>::size() const noexcept
{
    return static_cast<unsigned>(m_max_size);
}

template<typename T, typename Identifier>
constexpr int data_array<T, Identifier>::ssize() const noexcept
{
    return static_cast<int>(m_max_size);
}

template<typename T, typename Identifier>
constexpr bool data_array<T, Identifier>::can_alloc(
  std::integral auto nb) const noexcept
{
    const u64 capacity = m_capacity;
    const u64 max_size = m_max_size;

    return std::cmp_greater_equal(capacity - max_size, nb);
}

template<typename T, typename Identifier>
constexpr bool data_array<T, Identifier>::can_alloc() const noexcept
{
    return std::cmp_greater(m_capacity, m_max_size);
}

template<typename T, typename Identifier>
constexpr int data_array<T, Identifier>::max_size() const noexcept
{
    return static_cast<int>(m_max_size);
}

template<typename T, typename Identifier>
constexpr int data_array<T, Identifier>::max_used() const noexcept
{
    return static_cast<int>(m_max_used);
}

template<typename T, typename Identifier>
constexpr int data_array<T, Identifier>::capacity() const noexcept
{
    return static_cast<int>(m_capacity);
}

template<typename T, typename Identifier>
constexpr typename data_array<T, Identifier>::index_type
data_array<T, Identifier>::next_key() const noexcept
{
    return m_next_key;
}

template<typename T, typename Identifier>
constexpr bool data_array<T, Identifier>::is_free_list_empty() const noexcept
{
    return m_free_head == none;
}

// template<typename T>
// class vector;

template<typename T>
inline vector<T>::vector(int capacity) noexcept
{
    if (capacity > 0) {
        m_data     = reinterpret_cast<T*>(g_alloc_fn(capacity * sizeof(T)));
        m_capacity = capacity;
        m_size     = 0;
    }
}

template<typename T>
inline vector<T>::vector(int capacity, int size) noexcept
{
    static_assert(std::is_constructible_v<T>,
                  "T must be nothrow or trivially default constructible");

    capacity = std::max(capacity, size);

    if (capacity > 0) {
        m_data = reinterpret_cast<T*>(g_alloc_fn(capacity * sizeof(T)));

        std::uninitialized_value_construct_n(m_data, size);
        // std::uninitialized_default_construct_n(m_data, size);

        m_capacity = capacity;
        m_size     = size;
    }
}

template<typename T>
inline vector<T>::vector(int      capacity,
                         int      size,
                         const T& default_value) noexcept
{
    static_assert(std::is_copy_constructible_v<T>,
                  "T must be nothrow or trivially default constructible");

    capacity = std::max(capacity, size);

    if (capacity > 0) {
        m_data = reinterpret_cast<T*>(g_alloc_fn(capacity * sizeof(T)));

        std::uninitialized_fill_n(m_data, size, default_value);

        m_capacity = capacity;
        m_size     = size;
    }
}

template<typename T>
inline vector<T>::~vector() noexcept
{
    destroy();
}

template<typename T>
inline constexpr vector<T>::vector(const vector& other) noexcept
  : m_data(nullptr)
  , m_size(0)
  , m_capacity(0)
{
    operator=(other);
}

template<typename T>
inline constexpr vector<T>& vector<T>::operator=(const vector& other) noexcept
{
    clear();

    resize(other.m_size);
    std::copy_n(other.m_data, other.m_size, m_data);
    return *this;
}

template<typename T>
inline constexpr vector<T>::vector(vector&& other) noexcept
  : m_data(other.m_data)
  , m_size(other.m_size)
  , m_capacity(other.m_capacity)
{
    other.m_data     = nullptr;
    other.m_size     = 0;
    other.m_capacity = 0;
}

template<typename T>
inline constexpr vector<T>& vector<T>::operator=(vector&& other) noexcept
{
    destroy();

    m_data           = other.m_data;
    m_size           = other.m_size;
    m_capacity       = other.m_capacity;
    other.m_data     = nullptr;
    other.m_size     = 0;
    other.m_capacity = 0;

    return *this;
}

template<typename T>
inline void vector<T>::destroy() noexcept
{
    clear();

    if (m_data)
        g_free_fn(m_data);

    m_data     = nullptr;
    m_size     = 0;
    m_capacity = 0;
}

template<typename T>
inline constexpr void vector<T>::clear() noexcept
{
    std::destroy_n(data(), m_size);

    m_size = 0;
}

template<typename T>
void vector<T>::resize(std::integral auto size) noexcept
{
    static_assert(std::is_default_constructible_v<T> ||
                    std::is_trivially_default_constructible_v<T>,
                  "T must be default or trivially default constructible to use "
                  "resize() function");

    irt_assert(
      std::cmp_less(size, std::numeric_limits<decltype(m_capacity)>::max()));

    if (std::cmp_greater(size, m_capacity))
        reserve(compute_new_capacity(static_cast<decltype(m_capacity)>(size)));

    std::uninitialized_value_construct_n(data() + m_size, size - m_size);
    // std::uninitialized_default_construct_n(data() + m_size, size);

    m_size = static_cast<decltype(m_size)>(size);
}

template<typename T>
void vector<T>::reserve(std::integral auto new_capacity) noexcept
{
    irt_assert(is_numeric_castable<u32>(new_capacity));

    if (std::cmp_greater(new_capacity, m_capacity)) {
        T* new_data = reinterpret_cast<T*>(
          g_alloc_fn(static_cast<sz>(new_capacity) * sizeof(T)));

        if (m_data) {
            if constexpr (std::is_move_constructible_v<T>)
                std::uninitialized_move_n(data(), m_size, new_data);
            else
                std::uninitialized_copy_n(data(), m_size, new_data);

            g_free_fn(m_data);
        }

        m_data     = new_data;
        m_capacity = static_cast<decltype(m_capacity)>(new_capacity);
    }
}

template<typename T>
inline constexpr T* vector<T>::data() noexcept
{
    return m_data;
}

template<typename T>
inline constexpr const T* vector<T>::data() const noexcept
{
    return m_data;
}

template<typename T>
inline constexpr typename vector<T>::reference vector<T>::front() noexcept
{
    irt_assert(m_size > 0);
    return m_data[0];
}

template<typename T>
inline constexpr typename vector<T>::const_reference vector<T>::front()
  const noexcept
{
    irt_assert(m_size > 0);
    return m_data[0];
}

template<typename T>
inline constexpr typename vector<T>::reference vector<T>::back() noexcept
{
    irt_assert(m_size > 0);
    return m_data[m_size - 1];
}

template<typename T>
inline constexpr typename vector<T>::const_reference vector<T>::back()
  const noexcept
{
    irt_assert(m_size > 0);
    return m_data[m_size - 1];
}

template<typename T>
inline constexpr typename vector<T>::reference vector<T>::operator[](
  std::integral auto index) noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T>
inline constexpr typename vector<T>::const_reference vector<T>::operator[](
  std::integral auto index) const noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T>
inline constexpr typename vector<T>::iterator vector<T>::begin() noexcept
{
    return data();
}

template<typename T>
inline constexpr typename vector<T>::const_iterator vector<T>::begin()
  const noexcept
{
    return data();
}

template<typename T>
inline constexpr typename vector<T>::iterator vector<T>::end() noexcept
{
    return data() + m_size;
}

template<typename T>
inline constexpr typename vector<T>::const_iterator vector<T>::end()
  const noexcept
{
    return data() + m_size;
}

template<typename T>
inline constexpr unsigned vector<T>::size() const noexcept
{
    return static_cast<unsigned>(m_size);
}

template<typename T>
inline constexpr int vector<T>::ssize() const noexcept
{
    return m_size;
}

template<typename T>
inline constexpr int vector<T>::capacity() const noexcept
{
    return static_cast<int>(m_capacity);
}

template<typename T>
inline constexpr bool vector<T>::empty() const noexcept
{
    return m_size == 0;
}

template<typename T>
inline constexpr bool vector<T>::full() const noexcept
{
    return m_size >= m_capacity;
}

template<typename T>
inline constexpr bool vector<T>::can_alloc(
  std::integral auto number) const noexcept
{
    return std::cmp_greater_equal(m_capacity - m_size, number);
}

template<typename T>
inline constexpr int vector<T>::find(const T& t) const noexcept
{
    for (auto i = 0, e = ssize(); i != e; ++i)
        if (m_data[i] == t)
            return i;

    return m_size;
}

template<typename T>
template<typename... Args>
inline constexpr typename vector<T>::reference vector<T>::emplace_back(
  Args&&... args) noexcept
{
    static_assert(std::is_trivially_constructible_v<T, Args...> ||
                    std::is_nothrow_constructible_v<T, Args...>,
                  "T must but trivially or nothrow constructible from this "
                  "argument(s)");

    if (m_size >= m_capacity)
        reserve(compute_new_capacity(m_size + 1));

    std::construct_at(data() + m_size, std::forward<Args>(args)...);

    ++m_size;

    return data()[m_size - 1];
}

template<typename T>
inline constexpr void vector<T>::pop_back() noexcept
{
    static_assert(std::is_nothrow_destructible_v<T> ||
                    std::is_trivially_destructible_v<T>,
                  "T must be nothrow or trivially destructible to use "
                  "pop_back() function");

    irt_assert(m_size);

    if (m_size) {
        std::destroy_at(data() + m_size - 1);
        --m_size;
    }
}

template<typename T>
inline constexpr void vector<T>::swap_pop_back(
  std::integral auto index) noexcept
{
    irt_assert(std::cmp_less(index, m_size));

    if (index == m_size - 1) {
        pop_back();
    } else {
        auto to_delete = data() + index;
        auto last      = data() + m_size - 1;

        std::destroy_at(to_delete);

        if constexpr (std::is_move_constructible_v<T>) {
            std::uninitialized_move_n(last, 1, to_delete);
        } else {
            std::uninitialized_copy_n(last, 1, to_delete);
            std::destroy_at(last);
        }

        --m_size;
    }
}

template<typename T>
inline constexpr void vector<T>::erase(iterator it) noexcept
{
    irt_assert(it >= data() && it < data() + m_size);

    if (it == end())
        return;

    std::destroy_at(it);

    auto last = data() + m_size - 1;

    if (auto next = it + 1; next != end()) {
        if constexpr (std::is_move_constructible_v<T>) {
            std::uninitialized_move(next, end(), it);
        } else {
            std::uninitialized_copy(next, end(), it);
            std::destroy_at(last);
        }
    }

    --m_size;
}

template<typename T>
inline constexpr void vector<T>::erase(iterator first, iterator last) noexcept
{
    irt_assert(first >= data() && first < data() + m_size && last > first &&
               last <= data() + m_size);

    std::destroy(first, last);
    const ptrdiff_t count = last - first;

    if (count > 0) {
        if constexpr (std::is_move_constructible_v<T>) {
            std::uninitialized_move(last, end(), first);
        } else {
            std::uninitialized_copy(last, end(), first);
            std::destroy(last + count, end());
        }
    }

    m_size -= static_cast<i32>(count);
}

template<typename T>
i32 vector<T>::compute_new_capacity(i32 size) const
{
    i32 new_capacity = m_capacity ? (m_capacity + m_capacity / 2) : 8;
    return new_capacity > size ? new_capacity : size;
}

// template<typename T, size_type length>
// class small_vector;

template<typename T, int length>
constexpr small_vector<T, length>::small_vector() noexcept
{
    m_size = 0;
}

template<typename T, int length>
constexpr small_vector<T, length>::small_vector(
  const small_vector<T, length>& other) noexcept
{
    std::uninitialized_copy_n(other.data(), other.m_size, data());

    m_size = other.m_size;
}

template<typename T, int length>
constexpr small_vector<T, length>::~small_vector() noexcept
{
    std::destroy_n(data(), m_size);
}

template<typename T, int length>
constexpr small_vector<T, length>& small_vector<T, length>::operator=(
  const small_vector<T, length>& other) noexcept
{
    if (&other != this) {
        std::destroy_n(data(), m_size);
        std::uninitialized_copy_n(other.data(), other.m_size, data());

        m_size = other.m_size;
    }

    return *this;
}

template<typename T, int length>
constexpr status small_vector<T, length>::resize(
  std::integral auto default_size) noexcept
{
    static_assert(std::is_nothrow_default_constructible_v<T>,
                  "T must be nothrow default constructible to use "
                  "init() function");

    irt_return_if_fail(std::cmp_greater(default_size, 0) &&
                         std::cmp_less(default_size, length),
                       status::vector_init_capacity_error);

    const auto new_default_size = static_cast<size_type>(default_size);

    if (new_default_size > m_size)
        std::uninitialized_value_construct_n(data() + m_size,
                                             new_default_size - m_size);
    else
        std::destroy_n(data() + new_default_size, m_size - new_default_size);

    m_size = new_default_size;

    return status::success;
}

template<typename T, int length>
constexpr T* small_vector<T, length>::data() noexcept
{
    return reinterpret_cast<T*>(&m_buffer[0]);
}

template<typename T, int length>
constexpr const T* small_vector<T, length>::data() const noexcept
{
    return reinterpret_cast<const T*>(&m_buffer[0]);
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::front() noexcept
{
    irt_assert(m_size > 0);
    return data();
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::front() const noexcept
{
    irt_assert(m_size > 0);
    return data();
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::back() noexcept
{
    irt_assert(m_size > 0);
    return data()[m_size - 1];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::back() const noexcept
{
    irt_assert(m_size > 0);
    return data()[m_size - 1];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::operator[](std::integral auto index) noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::operator[](std::integral auto index) const noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::iterator
small_vector<T, length>::begin() noexcept
{
    return data();
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_iterator
small_vector<T, length>::begin() const noexcept
{
    return data();
}

template<typename T, int length>
constexpr typename small_vector<T, length>::iterator
small_vector<T, length>::end() noexcept
{
    return data() + m_size;
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_iterator
small_vector<T, length>::end() const noexcept
{
    return data() + m_size;
}

template<typename T, int length>
constexpr unsigned small_vector<T, length>::size() const noexcept
{
    return m_size;
}

template<typename T, int length>
constexpr int small_vector<T, length>::ssize() const noexcept
{
    return m_size;
}

template<typename T, int length>
constexpr int small_vector<T, length>::capacity() const noexcept
{
    return length;
}

template<typename T, int length>
constexpr bool small_vector<T, length>::empty() const noexcept
{
    return m_size == 0;
}

template<typename T, int length>
constexpr bool small_vector<T, length>::full() const noexcept
{
    return m_size >= length;
}

template<typename T, int length>
constexpr void small_vector<T, length>::clear() noexcept
{
    std::destroy_n(data(), m_size);
    m_size = 0;
}

template<typename T, int length>
constexpr bool small_vector<T, length>::can_alloc(
  std::integral auto number) noexcept
{
    const auto remaining = length - static_cast<int>(m_size);

    return std::cmp_greater_equal(remaining, number);
}

template<typename T, int length>
template<typename... Args>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::emplace_back(Args&&... args) noexcept
{
    static_assert(std::is_nothrow_constructible_v<T, Args...>,
                  "T must but trivially constructible from this "
                  "argument(s)");

    assert(can_alloc(1) && "check alloc() with full() before using use.");

    std::construct_at(&(data()[m_size]), std::forward<Args>(args)...);
    ++m_size;

    return data()[m_size - 1];
}

template<typename T, int length>
constexpr void small_vector<T, length>::pop_back() noexcept
{
    static_assert(std::is_nothrow_destructible_v<T>,
                  "T must be nothrow destructible to use "
                  "pop_back() function");

    if (m_size) {
        std::destroy_at(data() + m_size - 1);
        --m_size;
    }
}

template<typename T, int length>
constexpr void small_vector<T, length>::swap_pop_back(
  std::integral auto index) noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0) &&
               std::cmp_less(index, m_size));

    const auto new_index = static_cast<size_type>(index);

    if (new_index == m_size - 1) {
        pop_back();
    } else {
        auto to_delete = data() + new_index;
        auto last      = data() + m_size - 1;

        std::destroy_at(to_delete);

        if constexpr (std::is_move_constructible_v<T>) {
            std::uninitialized_move_n(last, 1, to_delete);
        } else {
            std::uninitialized_copy_n(last, 1, to_delete);
            std::destroy_at(last);
        }

        --m_size;
    }
}

// template<typename T>
// class ring_buffer;

template<class T>
constexpr i32 ring_buffer<T>::advance(i32 position) const noexcept
{
    return (position + 1) % m_capacity;
}

template<class T>
constexpr i32 ring_buffer<T>::back(i32 position) const noexcept
{
    return (((position - 1) % m_capacity) + m_capacity) % m_capacity;
}

template<class T>
constexpr ring_buffer<T>::ring_buffer(std::integral auto capacity) noexcept
{
    irt_assert(std::cmp_less(capacity,
                             std::numeric_limits<decltype(m_capacity)>::max()));

    if (std::cmp_greater(capacity, 0)) {
        const auto size = static_cast<sz>(capacity) * sizeof(T);
        buffer          = reinterpret_cast<T*>(g_alloc_fn(size));
        m_capacity      = static_cast<decltype(m_capacity)>(capacity);
    }
}

template<class T>
constexpr ring_buffer<T>::ring_buffer(const ring_buffer& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        if (m_capacity != rhs.m_capacity) {
            g_free_fn(buffer);

            if (rhs.m_capacity) {
                buffer =
                  reinterpret_cast<T*>(g_alloc_fn(rhs.m_capacity * sizeof(T)));
                m_capacity = rhs.m_capacity;
            }
        }

        for (auto it = rhs.begin(), et = rhs.end(); it != et; ++it)
            push_back(*it);
    }
}

template<class T>
constexpr ring_buffer<T>& ring_buffer<T>::operator=(
  const ring_buffer& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        if (m_capacity != rhs.m_capacity) {
            g_free_fn(buffer);

            if (rhs.m_capacity > 0) {
                buffer =
                  reinterpret_cast<T*>(g_alloc_fn(rhs.m_capacity * sizeof(T)));
                m_capacity = rhs.m_capacity;
            }
        }

        for (auto it = rhs.begin(), et = rhs.end(); it != et; ++it)
            push_back(*it);
    }

    return *this;
}

template<class T>
constexpr ring_buffer<T>::ring_buffer(ring_buffer&& rhs) noexcept
{
    buffer     = rhs.buffer;
    m_head     = rhs.m_head;
    m_tail     = rhs.m_tail;
    m_capacity = rhs.m_capacity;

    rhs.buffer     = nullptr;
    rhs.m_head     = 0;
    rhs.m_tail     = 0;
    rhs.m_capacity = 0;
}

template<class T>
constexpr ring_buffer<T>& ring_buffer<T>::operator=(ring_buffer&& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        buffer     = rhs.buffer;
        m_head     = rhs.m_head;
        m_tail     = rhs.m_tail;
        m_capacity = rhs.m_capacity;

        rhs.buffer     = nullptr;
        rhs.m_head     = 0;
        rhs.m_tail     = 0;
        rhs.m_capacity = 0;
    }

    return *this;
}

template<class T>
constexpr ring_buffer<T>::~ring_buffer() noexcept
{
    clear();

    g_free_fn(buffer);
}

template<class T>
constexpr void ring_buffer<T>::swap(ring_buffer& rhs) noexcept
{
    std::swap(buffer, rhs.buffer);
    std::swap(m_head, rhs.m_head);
    std::swap(m_tail, rhs.m_tail);
    std::swap(m_capacity, rhs.m_capacity);
}

template<class T>
constexpr void ring_buffer<T>::clear() noexcept
{
    if (!std::is_trivially_destructible_v<T>) {
        while (!empty())
            dequeue();
    }

    m_head = 0;
    m_tail = 0;
}

template<class T>
template<typename... Args>
constexpr bool ring_buffer<T>::emplace_front(Args&&... args) noexcept
{
    if (full())
        return false;

    m_head = back(m_head);
    std::construct_at(&buffer[m_head], std::forward<Args>(args)...);

    return true;
}

template<class T>
template<typename... Args>
constexpr bool ring_buffer<T>::emplace_back(Args&&... args) noexcept
{
    if (full())
        return false;

    std::construct_at(&buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);

    return true;
}

template<class T>
constexpr bool ring_buffer<T>::push_front(const T& item) noexcept
{
    if (full())
        return false;

    m_head = back(m_head);
    std::construct_at(&buffer[m_head], item);

    return true;
}

template<class T>
constexpr void ring_buffer<T>::pop_front() noexcept
{
    if (!empty()) {
        std::destroy_at(&buffer[m_head]);
        m_head = advance(m_head);
    }
}

template<class T>
constexpr bool ring_buffer<T>::push_back(const T& item) noexcept
{
    if (full())
        return false;

    std::construct_at(&buffer[m_tail], item);
    m_tail = advance(m_tail);

    return true;
}

template<class T>
constexpr void ring_buffer<T>::pop_back() noexcept
{
    if (!empty()) {
        m_tail = back(m_tail);
        std::destroy_at(&buffer[m_tail]);
    }
}

template<class T>
constexpr void ring_buffer<T>::erase_after(iterator this_it) noexcept
{
    irt_assert(this_it.ring != nullptr);

    if (this_it == tail())
        return;

    while (this_it != tail())
        pop_back();
}

template<class T>
constexpr void ring_buffer<T>::erase_before(iterator this_it) noexcept
{
    irt_assert(this_it.ring != nullptr);

    if (this_it == head())
        return;

    auto it = head();
    while (it != head())
        pop_front();
}

template<class T>
template<typename... Args>
constexpr bool ring_buffer<T>::emplace_enqueue(Args&&... args) noexcept
{
    if (full())
        return false;

    std::construct_at(&buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);

    return true;
}

template<class T>
template<typename... Args>
constexpr void ring_buffer<T>::force_emplace_enqueue(Args&&... args) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);
}

template<class T>
constexpr bool ring_buffer<T>::enqueue(const T& item) noexcept
{
    if (full())
        return false;

    std::construct_at(&buffer[m_tail], item);
    m_tail = advance(m_tail);

    return true;
}

template<class T>
constexpr void ring_buffer<T>::force_enqueue(const T& item) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&buffer[m_tail], item);
    m_tail = advance(m_tail);
}

template<class T>
constexpr void ring_buffer<T>::dequeue() noexcept
{
    if (!empty()) {
        std::destroy_at(&buffer[m_head]);
        m_head = advance(m_head);
    }
}

template<class T>
constexpr typename ring_buffer<T>::iterator ring_buffer<T>::head() noexcept
{
    return empty() ? iterator{ nullptr, 0 } : iterator{ this, m_head };
}

template<class T>
constexpr typename ring_buffer<T>::const_iterator ring_buffer<T>::head()
  const noexcept
{
    return empty() ? const_iterator{ nullptr, 0 }
                   : const_iterator{ this, m_head };
}

template<class T>
constexpr typename ring_buffer<T>::iterator ring_buffer<T>::tail() noexcept
{
    return empty() ? iterator{ nullptr, 0 } : iterator{ this, back(m_tail) };
}

template<class T>
constexpr typename ring_buffer<T>::const_iterator ring_buffer<T>::tail()
  const noexcept
{
    return empty() ? const_iterator{ nullptr, 0 }
                   : const_iterator{ this, back(m_tail) };
}

template<class T>
constexpr typename ring_buffer<T>::iterator ring_buffer<T>::begin() noexcept
{
    return empty() ? iterator{ nullptr, 0 } : iterator{ this, m_head };
}

template<class T>
constexpr typename ring_buffer<T>::const_iterator ring_buffer<T>::begin()
  const noexcept
{
    return empty() ? const_iterator{ nullptr, 0 }
                   : const_iterator{ this, m_head };
}

template<class T>
constexpr typename ring_buffer<T>::iterator ring_buffer<T>::end() noexcept
{
    return iterator{ nullptr, 0 };
}

template<class T>
constexpr typename ring_buffer<T>::const_iterator ring_buffer<T>::end()
  const noexcept
{
    return const_iterator{ nullptr, 0 };
}

template<class T>
constexpr T* ring_buffer<T>::data() noexcept
{
    return &buffer[0];
}

template<class T>
constexpr const T* ring_buffer<T>::data() const noexcept
{
    return &buffer[0];
}

template<class T>
constexpr T& ring_buffer<T>::operator[](std::integral auto index) noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_capacity));

    return buffer[index];
}

template<class T>
constexpr const T& ring_buffer<T>::operator[](
  std::integral auto index) const noexcept
{
    irt_assert(std::cmp_greater_equal(index, 0));
    irt_assert(std::cmp_less(index, m_capacity));

    return buffer[index];
}

template<class T>
constexpr T& ring_buffer<T>::front() noexcept
{
    irt_assert(!empty());

    return buffer[m_head];
}

template<class T>
constexpr const T& ring_buffer<T>::front() const noexcept
{
    irt_assert(!empty());

    return buffer[m_head];
}

template<class T>
constexpr T& ring_buffer<T>::back() noexcept
{
    irt_assert(!empty());

    return buffer[back(m_tail)];
}

template<class T>
constexpr const T& ring_buffer<T>::back() const noexcept
{
    irt_assert(!empty());

    return buffer[back(m_tail)];
}

template<class T>
constexpr bool ring_buffer<T>::empty() const noexcept
{
    return m_head == m_tail;
}

template<class T>
constexpr bool ring_buffer<T>::full() const noexcept
{
    return advance(m_tail) == m_head;
}

template<class T>
constexpr unsigned ring_buffer<T>::size() const noexcept
{
    return static_cast<sz>(ssize());
}

template<class T>
constexpr int ring_buffer<T>::ssize() const noexcept
{
    return (m_tail >= m_head) ? m_tail - m_head
                              : m_capacity - (m_head - m_tail);
}

template<class T>
constexpr int ring_buffer<T>::available() const noexcept
{
    return capacity() - size();
}

template<class T>
constexpr int ring_buffer<T>::capacity() const noexcept
{
    return m_capacity;
}

template<class T>
constexpr i32 ring_buffer<T>::index_from_begin(i32 idx) const noexcept
{
    return (m_head + idx) % m_capacity;
}

template<class T>
constexpr i32 ring_buffer<T>::head_index() const noexcept
{
    return m_head;
}

template<class T>
constexpr i32 ring_buffer<T>::tail_index() const noexcept
{
    return m_tail;
}

// template<size_t length = 8>
// class small_string

template<int length>
inline constexpr small_string<length>::small_string() noexcept
{
    clear();
}

template<int length>
inline constexpr small_string<length>::small_string(
  const small_string<length>& str) noexcept
{
    std::copy_n(str.m_buffer, str.m_size, m_buffer);
    m_buffer[str.m_size] = '\0';
    m_size               = str.m_size;
}

template<int length>
inline constexpr small_string<length>::small_string(
  small_string<length>&& str) noexcept
{
    std::copy_n(str.m_buffer, str.m_size, m_buffer);
    m_buffer[str.m_size] = '\0';
    m_size               = str.m_size;
    str.clear();
}

template<int length>
inline constexpr small_string<length>& small_string<length>::operator=(
  const small_string<length>& str) noexcept
{
    if (&str != this) {
        std::copy_n(str.m_buffer, str.m_size, m_buffer);
        m_buffer[str.m_size] = '\0';
        m_size               = str.m_size;
    }

    return *this;
}

template<int length>
inline constexpr small_string<length>& small_string<length>::operator=(
  small_string<length>&& str) noexcept
{
    if (&str != this) {
        std::copy_n(str.m_buffer, str.m_size, m_buffer);
        m_buffer[str.m_size] = '\0';
        m_size               = str.m_size;
    }

    return *this;
}

template<int length>
inline constexpr small_string<length>& small_string<length>::operator=(
  const char* str) noexcept
{
    if (m_buffer != str) {
        std::strncpy(m_buffer, str, length - 1);
        m_buffer[length - 1] = '\0';
        m_size               = static_cast<u8>(std::strlen(m_buffer));
    }
    return *this;
}

template<int length>
inline constexpr small_string<length>& small_string<length>::operator=(
  const std::string_view str) noexcept
{
    assign(str);

    return *this;
}

template<int length>
inline constexpr small_string<length>::small_string(const char* str) noexcept
{
    std::strncpy(m_buffer, str, length - 1);
    m_buffer[length - 1] = '\0';
    m_size               = static_cast<u8>(std::strlen(m_buffer));
}

template<int length>
inline constexpr small_string<length>::small_string(
  const std::string_view str) noexcept
{
    assign(str);
}

template<int length>
void small_string<length>::resize(std::integral auto size) noexcept
{
    if (size < 0) {
        m_size = 0;
    } else if (std::cmp_greater_equal(size, length - 1)) {
        m_size = length - 1;
    } else {
        m_size = static_cast<size_type>(size);
    }

    m_buffer[m_size] = '\0';
}

template<int length>
inline constexpr bool small_string<length>::empty() const noexcept
{
    return std::cmp_equal(m_size, 0);
}

template<int length>
inline constexpr unsigned small_string<length>::size() const noexcept
{
    return m_size;
}

template<int length>
inline constexpr int small_string<length>::ssize() const noexcept
{
    return m_size;
}

template<int length>
inline constexpr int small_string<length>::capacity() const noexcept
{
    return length;
}

template<int length>
inline constexpr void small_string<length>::assign(
  const std::string_view str) noexcept
{
    m_size = std::cmp_less(str.size(), length - 1)
               ? static_cast<size_type>(str.size())
               : static_cast<size_type>(length - 1);

    std::copy_n(str.data(), m_size, &m_buffer[0]);
    m_buffer[m_size] = '\0';
}

template<int length>
inline constexpr std::string_view small_string<length>::sv() const noexcept
{
    return { &m_buffer[0], m_size };
}

template<int length>
inline constexpr std::u8string_view small_string<length>::u8sv() const noexcept
{
    return { reinterpret_cast<const char8_t*>(&m_buffer[0]), m_size };
}

template<int length>
inline constexpr void small_string<length>::clear() noexcept
{
    std::fill_n(m_buffer, length, '\0');
    m_size = 0;
}

template<int length>
inline constexpr typename small_string<length>::reference
small_string<length>::operator[](std::integral auto index) noexcept
{
    irt_assert(std::cmp_less(index, length));

    return m_buffer[index];
}

template<int length>
inline constexpr typename small_string<length>::const_reference
small_string<length>::operator[](std::integral auto index) const noexcept
{
    irt_assert(std::cmp_less(index, m_size));

    return m_buffer[index];
}

template<int length>
inline constexpr const char* small_string<length>::c_str() const noexcept
{
    return m_buffer;
}

template<int length>
inline constexpr typename small_string<length>::iterator
small_string<length>::begin() noexcept
{
    return m_buffer;
}

template<int length>
inline constexpr typename small_string<length>::iterator
small_string<length>::end() noexcept
{
    return m_buffer + m_size;
}

template<int length>
inline constexpr typename small_string<length>::const_iterator
small_string<length>::begin() const noexcept
{
    return m_buffer;
}

template<int length>
inline constexpr typename small_string<length>::const_iterator
small_string<length>::end() const noexcept
{
    return m_buffer + m_size;
}

template<int length>
inline constexpr bool small_string<length>::operator==(
  const small_string<length>& rhs) const noexcept
{
    return std::strncmp(m_buffer, rhs.m_buffer, length) == 0;
}

template<int length>
inline constexpr bool small_string<length>::operator!=(
  const small_string<length>& rhs) const noexcept
{
    return std::strncmp(m_buffer, rhs.m_buffer, length) != 0;
}

template<int length>
inline constexpr bool small_string<length>::operator>(
  const small_string<length>& rhs) const noexcept
{
    return std::strncmp(m_buffer, rhs.m_buffer, length) > 0;
}

template<int length>
inline constexpr bool small_string<length>::operator<(
  const small_string<length>& rhs) const noexcept
{
    return std::strncmp(m_buffer, rhs.m_buffer, length) < 0;
}

template<int length>
inline constexpr bool small_string<length>::operator==(
  const char* rhs) const noexcept
{
    return std::strncmp(m_buffer, rhs, length) == 0;
}

template<int length>
inline constexpr bool small_string<length>::operator!=(
  const char* rhs) const noexcept
{
    return std::strncmp(m_buffer, rhs, length) != 0;
}

template<int length>
inline constexpr bool small_string<length>::operator>(
  const char* rhs) const noexcept
{
    return std::strncmp(m_buffer, rhs, length) > 0;
}

template<int length>
inline constexpr bool small_string<length>::operator<(
  const char* rhs) const noexcept
{
    return std::strncmp(m_buffer, rhs, length) < 0;
}

//! observer

inline observer::observer(std::string_view name_,
                          u64              user_id_,
                          i32              user_type_) noexcept
  : buffer{ 64 }
  , user_id{ user_id_ }
  , user_type{ user_type_ }
  , name{ name_ }
  , flags{ flags_none }
{
}

inline void observer::reset() noexcept
{
    buffer.clear();
    flags = flags_none;
}

inline void observer::clear() noexcept
{
    buffer.clear();
    flags = (flags & flags_data_lost) ? flags_data_lost : flags_none;
}

inline void observer::update(observation_message msg) noexcept
{
    u8 new_flags = flags_data_available | (flags & flags_data_lost);

    if (flags & flags_buffer_full)
        new_flags |= flags_data_lost;

    if (!buffer.empty() && buffer.tail()->data[0] == msg[0])
        *(buffer.tail()) = msg;
    else
        buffer.force_enqueue(msg);

    if (buffer.full())
        new_flags |= flags_buffer_full;

    flags = new_flags;
}

inline bool observer::full() const noexcept
{
    return flags & flags_buffer_full;
}

//! scheduller

inline void scheduller::pop(vector<model_id>& out) noexcept
{
    time t = tn();

    out.clear();
    out.emplace_back(m_heap.pop()->id);

    while (!m_heap.empty() && t == tn())
        out.emplace_back(m_heap.pop()->id);
}

inline status send_message(simulation&  sim,
                           output_port& p,
                           real         r1,
                           real         r2,
                           real         r3) noexcept
{
    auto list = append_node(sim, p);
    auto it   = list.begin();
    auto end  = list.end();

    while (it != end) {
        auto* mdl = sim.models.try_to_get(it->model);
        if (!mdl) {
            it = list.erase(it);
        } else {
            irt_return_if_fail(sim.emitting_output_ports.can_alloc(1),
                               status::simulation_not_enough_message);

            auto& output_message  = sim.emitting_output_ports.emplace_back();
            output_message.msg[0] = r1;
            output_message.msg[1] = r2;
            output_message.msg[2] = r3;
            output_message.model  = it->model;
            output_message.port   = it->port_index;

            ++it;
        }
    }

    return status::success;
}

inline status get_hierarchical_state_machine(simulation&                  sim,
                                             hierarchical_state_machine*& hsm,
                                             const hsm_id id) noexcept
{
    if (hsm = sim.hsms.try_to_get(id); hsm)
        return status::success;

    irt_bad_return(status::unknown_dynamics); // @TODO new message
}

inline status simulation::run(time& t) noexcept
{
    immediate_models.clear();
    immediate_observers.clear();

    if (sched.empty()) {
        t = time_domain<time>::infinity;
        return status::success;
    }

    if (t = sched.tn(); time_domain<time>::is_infinity(t))
        return status::success;

    sched.pop(immediate_models);

    emitting_output_ports.clear();
    for (const auto id : immediate_models)
        if (auto* mdl = models.try_to_get(id); mdl)
            irt_return_if_bad(make_transition(*mdl, t));

    for (int i = 0, e = length(emitting_output_ports); i != e; ++i) {
        auto* mdl = models.try_to_get(emitting_output_ports[i].model);
        if (!mdl)
            continue;

        sched.update(*mdl, t);

        irt_return_if_fail(can_alloc_message(*this, 1),
                           status::simulation_not_enough_message);

        auto  port = emitting_output_ports[i].port;
        auto& msg  = emitting_output_ports[i].msg;

        dispatch(*mdl, [this, port, &msg]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (has_input_port<Dynamics>) {
                auto list = append_message(*this, dyn.x[port]);
                list.push_back(msg);
            }
        });
    }

    return status::success;
}

//
// class HSM
//

inline hsm_wrapper::hsm_wrapper() noexcept = default;

inline hsm_wrapper::hsm_wrapper(const hsm_wrapper& other) noexcept
  : id{ undefined<hsm_id>() }
  , m_previous_state{ other.m_previous_state }
  , sigma{ other.sigma }
{
}

inline status hsm_wrapper::initialize(simulation& sim) noexcept
{
    hierarchical_state_machine* machine = nullptr;
    irt_return_if_bad(get_hierarchical_state_machine(sim, machine, id));

    m_previous_state = hierarchical_state_machine::invalid_state_id;
    irt_return_if_bad(machine->start());

    sigma = time_domain<time>::infinity;

    return status::success;
}

inline status hsm_wrapper::transition(simulation& sim,
                                      time /*t*/,
                                      time /*e*/,
                                      time /*r*/) noexcept
{
    hierarchical_state_machine* machine = nullptr;
    irt_return_if_bad(get_hierarchical_state_machine(sim, machine, id));

    machine->outputs.clear();
    sigma                       = time_domain<time>::infinity;
    const auto old_values_state = machine->values;

    for (int i = 0, e = length(x); i != e; ++i) {
        if (have_message(x[i]))
            machine->values |= static_cast<u8>(1u << i);

        // notice for clear a bit if negative value ? or 0 value ?
        // for (const auto& msg : span) {
        //     if (msg[0] != 0)
        //         machine->values |= 1u << i;
        //     else
        //         machine->values &= ~(1u << n);
        // }
        //
        // Setting a bit number |= 1UL << n;
        // clearing a bit bit number &= ~(1UL << n);
        // Toggling a bit number ^= 1UL << n;
        // Checking a bit bit = (number >> n) & 1U;
        // Changing the nth bit to x
        //  number = (number & ~(1UL << n)) | (x << n);
    }

    if (old_values_state != machine->values) {
        m_previous_state = machine->get_current_state();

        auto ret = machine->dispatch(
          hierarchical_state_machine::event_type::input_changed);

        if (is_bad(ret.first))
            irt_bad_return(ret.first);

        if (ret.second && !machine->outputs.empty())
            sigma = time_domain<time>::zero;
    }

    return status::success;
}

inline status hsm_wrapper::lambda(simulation& sim) noexcept
{
    hierarchical_state_machine* machine = nullptr;
    irt_return_if_bad(get_hierarchical_state_machine(sim, machine, id));

    if (m_previous_state != hierarchical_state_machine::invalid_state_id &&
        !machine->outputs.empty()) {
        for (int i = 0, e = machine->outputs.ssize(); i != e; ++i) {
            const u8  port  = machine->outputs[i].port;
            const i32 value = machine->outputs[i].value;

            irt_assert(port >= 0 && port < machine->outputs.size());

            irt_return_if_bad(send_message(sim, y[port], to_real(value)));
        }
    }

    return status::success;
}

inline bool simulation::can_alloc(std::integral auto place) const noexcept
{
    return models.can_alloc(place);
}

inline bool simulation::can_alloc(dynamics_type      type,
                                  std::integral auto place) const noexcept
{
    if (type == dynamics_type::hsm_wrapper)
        return models.can_alloc(place) && hsms.can_alloc(place);
    else
        return models.can_alloc(place);
}

template<typename Dynamics>
inline bool simulation::can_alloc_dynamics(
  std::integral auto place) const noexcept
{
    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        return models.can_alloc(place) && hsms.can_alloc(place);
    else
        return models.can_alloc(place);
}

template<typename Dynamics>
Dynamics& simulation::alloc() noexcept
{
    irt_assert(!models.full());

    auto& mdl  = models.alloc();
    mdl.type   = dynamics_typeof<Dynamics>();
    mdl.handle = nullptr;

    std::construct_at(reinterpret_cast<Dynamics*>(&mdl.dyn));
    auto& dyn = get_dyn<Dynamics>(mdl);

    if constexpr (has_input_port<Dynamics>)
        for (int i = 0, e = length(dyn.x); i != e; ++i)
            dyn.x[i] = static_cast<u64>(-1);

    if constexpr (has_output_port<Dynamics>)
        for (int i = 0, e = length(dyn.y); i != e; ++i)
            dyn.y[i] = static_cast<u64>(-1);

    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
        auto& hsm = hsms.alloc();
        auto  id  = hsms.get_id(hsm);
        dyn.id    = id;
    }

    return dyn;
}

//! @brief This function allocates dynamics and models.
inline model& simulation::clone(const model& mdl) noexcept
{
    /* Use can_alloc before using this function. */
    irt_assert(!models.full());

    auto& new_mdl  = models.alloc();
    new_mdl.type   = mdl.type;
    new_mdl.handle = nullptr;

    dispatch(new_mdl, [this, &mdl]<typename Dynamics>(Dynamics& dyn) -> void {
        const auto& src_dyn = get_dyn<Dynamics>(mdl);
        std::construct_at(&dyn, src_dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = static_cast<u64>(-1);

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = static_cast<u64>(-1);

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            if (auto* hsm_src = hsms.try_to_get(src_dyn.id); hsm_src) {
                auto& hsm = hsms.alloc(*hsm_src);
                auto  id  = hsms.get_id(hsm);
                dyn.id    = id;
            } else {
                auto& hsm = hsms.alloc();
                auto  id  = hsms.get_id(hsm);
                dyn.id    = id;
            }
        }
    });

    return new_mdl;
}

//! @brief This function allocates dynamics and models.
inline model& simulation::alloc(dynamics_type type) noexcept
{
    irt_assert(!models.full());

    auto& mdl  = models.alloc();
    mdl.type   = type;
    mdl.handle = nullptr;

    dispatch(mdl, [this]<typename Dynamics>(Dynamics& dyn) -> void {
        std::construct_at(&dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = static_cast<u64>(-1);

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = static_cast<u64>(-1);

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            auto& hsm = hsms.alloc();
            auto  id  = hsms.get_id(hsm);
            dyn.id    = id;
        }
    });

    return mdl;
}

} // namespace irt

#endif
