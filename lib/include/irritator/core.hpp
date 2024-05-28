// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2020
#define ORG_VLEPROJECT_IRRITATOR_2020

#include <irritator/container.hpp>
#include <irritator/error.hpp>
#include <irritator/macros.hpp>

#include <array>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#ifdef __has_include
#if __has_include(<numbers>)
#include <numbers>
#define irt_have_numbers 1
#else
#define irt_have_numbers 0
#endif
#endif

#include <cmath>
#include <cstdint>
#include <cstring>

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

//! @brief An helper function to initialize floating point number and
//! disable warnings the IRRITATOR_REAL_TYPE_F64 is defined.
//!
//! @param v The floating point number to convert to float, double or long
//! double.
//! @return A real.
inline constexpr real to_real(long double v) noexcept
{
    return static_cast<real>(v);
}

namespace literals {

//! @brief An helper literal function to initialize floating point number
//! and disable warnings the IRRITATOR_REAL_TYPE_F64 is defined.
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

inline constexpr u16 make_halfword(u8 a, u8 b) noexcept
{
    return static_cast<u16>((a << 8) | b);
}

inline constexpr void unpack_halfword(u16 halfword, u8* a, u8* b) noexcept
{
    *a = static_cast<u8>((halfword >> 8) & 0xff);
    *b = static_cast<u8>(halfword & 0xff);
}

inline constexpr auto unpack_halfword(u16 halfword) noexcept
  -> std::pair<u8, u8>
{
    return std::make_pair(static_cast<u8>((halfword >> 8) & 0xff),
                          static_cast<u8>(halfword & 0xff));
}

inline constexpr u32 make_word(u16 a, u16 b) noexcept
{
    return (static_cast<u32>(a) << 16) | static_cast<u32>(b);
}

inline constexpr void unpack_word(u32 word, u16* a, u16* b) noexcept
{
    *a = static_cast<u16>((word >> 16) & 0xffff);
    *b = static_cast<u16>(word & 0xffff);
}

inline constexpr auto unpack_word(u32 word) noexcept -> std::pair<u16, u16>
{
    return std::make_pair(static_cast<u16>((word >> 16) & 0xffff),
                          static_cast<u16>(word & 0xffff));
}

inline constexpr u64 make_doubleword(u32 a, u32 b) noexcept
{
    return (static_cast<u64>(a) << 32) | static_cast<u64>(b);
}

inline constexpr void unpack_doubleword(u64 doubleword, u32* a, u32* b) noexcept
{
    *a = static_cast<u32>((doubleword >> 32) & 0xffffffff);
    *b = static_cast<u32>(doubleword & 0xffffffff);
}

inline constexpr auto unpack_doubleword(u64 doubleword) noexcept
  -> std::pair<u32, u32>
{
    return std::make_pair(static_cast<u32>((doubleword >> 32) & 0xffffffff),
                          static_cast<u32>(doubleword & 0xffffffff));
}

inline constexpr u32 unpack_doubleword_left(u64 doubleword) noexcept
{
    return static_cast<u32>((doubleword >> 32) & 0xffffffff);
}

inline constexpr u32 unpack_doubleword_right(u64 doubleword) noexcept
{
    return static_cast<u32>(doubleword & 0xffffffff);
}

template<typename Integer>
constexpr typename std::make_unsigned<Integer>::type to_unsigned(Integer value)
{
    debug::ensure(value >= 0);
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
    debug::ensure(is_castable);

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
    return (!(begin == end) and !(value < *begin)) ? begin : end;
}

//! @brief returns an iterator to the result or end if not found
//!
//! Binary search function which returns an iterator to the result or end if
//! not found using the lower_bound standard function. Compare functor must
//! use interoperable *begin type as first argument and T as second
//! argument.
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
enum class log_level {
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

template<typename T, typename... Args>
constexpr bool any_equal(const T& s, Args... args) noexcept
{
    return ((s == args) || ... || false);
}

template<class T, class... Rest>
constexpr bool all_same_type() noexcept
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
 * Definition of Time
 *
 * TODO:
 * - enable template definition of float or [float|double,bool absolute]
 *   representation of time?
 *
 ****************************************************************************/

using time = real;

template<typename T>
struct time_domain {};

template<>
struct time_domain<time> {
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

using message             = std::array<real, 3>;
using dated_message       = std::array<real, 4>;
using observation_message = std::array<real, 5>;

struct model;
class simulation;
class hierarchical_state_machine;
class source;

enum class hsm_id : u64;
enum class model_id : u64;
enum class dynamics_id : u64;
enum class message_id : u64;
enum class observer_id : u64;
enum class node_id : u64;
enum class message_id : u64;
enum class dated_message_id : u64;
enum class constant_source_id : u64;
enum class binary_file_source_id : u64;
enum class text_file_source_id : u64;
enum class random_source_id : u64;

/*****************************************************************************
 *
 * @c source and @c source_id are data from files or random generators.
 *
 ****************************************************************************/

static constexpr int external_source_chunk_size = 512;
static constexpr int default_max_client_number  = 32;

using chunk_type = std::array<double, external_source_chunk_size>;

enum class distribution_type {
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
class constant_source
{
public:
    small_string<23> name;
    chunk_type       buffer;
    u32              length = 0u;

    constant_source() noexcept = default;
    constant_source(const constant_source& src) noexcept;
    constant_source(constant_source&& src) noexcept = delete;
    constant_source& operator=(const constant_source& src) noexcept;
    constant_source& operator=(constant_source&& src) noexcept = delete;

    status init() noexcept;
    status init(source& src) noexcept;
    status update(source& src) noexcept;
    status restore(source& src) noexcept;
    status finalize(source& src) noexcept;
};

//! Use a file with a set of double real in binary mode (little endian) to
//! produce external data. This external source can be shared between @c
//! max_clients sources. Each client read a @c external_source_chunk_size
//! real of the set.
//!
//! source::chunk_id[0] is used to store the client identifier.
//! source::chunk_id[1] is used to store the current position in the file.
class binary_file_source
{
public:
    struct access_file_error {};    //<! Add an e_file_name
    struct too_small_file_error {}; //<! Add an e_file_name
    struct open_file_error {};      //<! Add an e_file_name
    struct eof_file_error {};       //<! Add an e_file_name

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
    binary_file_source& operator=(const binary_file_source& other) noexcept;

    void swap(binary_file_source& other) noexcept;

    status init() noexcept;
    void   finalize() noexcept;
    status init(source& src) noexcept;
    status update(source& src) noexcept;
    status restore(source& src) noexcept;
    status finalize(source& src) noexcept;

    bool seekg(const long to_seek) noexcept;
    bool read(source& src, const int length) noexcept;
    int  tellg() noexcept;
};

//! Use a file with a set of double real in ascii text file to produce
//! external data. This external source can not be shared between @c source.
//!
//! source::chunk_id[0] is used to store the current position in the file to
//! simplify restore operation.
class text_file_source
{
public:
    struct open_file_error {}; //<! Add an e_file_name
    struct eof_file_error {};  //<! Add an e_file_name

    small_string<23> name;
    chunk_type       buffer;
    u64              offset;

    std::filesystem::path file_path;
    std::ifstream         ifs;

    text_file_source() noexcept = default;
    text_file_source(const text_file_source& other) noexcept;
    text_file_source& operator=(const text_file_source& other) noexcept;

    void swap(text_file_source& other) noexcept;

    status init() noexcept;
    void   finalize() noexcept;
    status init(source& src) noexcept;
    status update(source& src) noexcept;
    status restore(source& src) noexcept;
    status finalize(source& src) noexcept;

    bool read_chunk() noexcept;
};

//! Use a prng to produce set of double real. This external source can be
//! shared between @c max_clients sources. Each client read a @c
//! external_source_chunk_size real of the set.
//!
//! The source::chunk_id[0-5] array is used to store the prng state.
class random_source
{
public:
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
    random_source(random_source&& other) noexcept = delete;
    random_source& operator=(const random_source& other) noexcept;
    random_source& operator=(random_source&& other) noexcept = delete;

    void swap(random_source& other) noexcept;

    status init() noexcept;
    void   finalize() noexcept;
    status init(source& src) noexcept;
    status update(source& src) noexcept;
    status restore(source& src) noexcept;
    status finalize(source& src) noexcept;
};

//! @brief Reference external source from a model.
//!
//! @details A @c source references a external source (file, PRNG, etc.).
//! Model auses the source to get data external to the simulation.
class source
{
public:
    enum class source_type : i16 {
        none,
        binary_file, /* Best solution to reproductible simulation. Each
                        client take a part of the stream (substream). */
        constant,    /* Just an easy source to use mode. */
        random,      /* How to retrieve old position in debug mode? */
        text_file    /* How to retreive old position in debug mode? */
    };

    static inline constexpr int source_type_count = 5;

    enum class operation_type {
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

    //! Check if the source is empty and required a filling.
    bool is_empty() const noexcept
    {
        return std::cmp_greater_equal(index, buffer.size());
    }

    //! Get the next double in the buffer. Abort if the buffer is empty. Use
    //! the
    //! @c is_empty() function first.
    //!
    //! @return true if success, false otherwise.
    double next() noexcept
    {
        debug::ensure(!is_empty());

        const auto old_index = index++;
        return buffer[static_cast<sz>(old_index)];
    }
};

class external_source
{
public:
    enum class part {
        constant_source,
        binary_file_source,
        text_file_source,
        random_source,
    };

    data_array<constant_source, constant_source_id, freelist_allocator>
      constant_sources;
    data_array<binary_file_source, binary_file_source_id, freelist_allocator>
      binary_file_sources;
    data_array<text_file_source, text_file_source_id, freelist_allocator>
      text_file_sources;
    data_array<random_source, random_source_id, freelist_allocator>
      random_sources;

    u64 seed[2] = { 0xdeadbeef12345678U, 0xdeadbeef12345678U };

    //! Call the @c init function for all sources (@c constant_source, @c
    //! binary_file_source etc.).
    status prepare() noexcept;

    //! Call the @c finalize function for all sources (@c constant_source,
    //! @c binary_file_source etc.). Usefull to close opened files.
    void finalize() noexcept;

    status import_from(const external_source& srcs);

    status dispatch(source& src, const source::operation_type op) noexcept;

    //! Call the @c data_array<T, Id>::clear() function for all sources.
    void clear() noexcept;

    //! Call `clear()` and release memory.
    void destroy() noexcept;

    //! An example of error handlers to catch all error from the external
    //! source class and friend (@c binary_file_source, @c text_file_source,
    //! @c random_source and @c constant_source).
    // constexpr auto make_error_handlers() const noexcept;
};

//! Assign a value constrained by the template parameters.
//!
//! @code
//! static int v = 0;
//! if (ImGui::InputInt("test", &v)) {
//!     f(v);
//! }
//!
//! void f(constrained_value<int, 0, 100> v)
//! {
//!     assert(0 <= *v && *v <= 100);
//! }
//! @endcode
template<typename T = int, T Lower = 0, T Upper = 100>
class constrained_value
{
public:
    using value_type = T;

private:
    static_assert(std::is_trivial_v<T>,
                  "T must be a trivial type in ratio_parameter");
    static_assert(Lower < Upper);

    T m_value;

public:
    constexpr constrained_value(const T value_) noexcept
      : m_value(value_ < Lower   ? Lower
                : value_ < Upper ? value_
                                 : Upper)
    {}

    constexpr explicit   operator T() const noexcept { return m_value; }
    constexpr value_type operator*() const noexcept { return m_value; }
    constexpr value_type value() const noexcept { return m_value; }
};

//! To be used in model declaration to initialize a source instance
//! according to the type of the external source.
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

enum class dynamics_type : i32 {
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
    counter,
    queue,
    dynamic_queue,
    priority_queue,
    generator,
    constant,
    time_func,
    accumulator_2,
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

/*****************************************************************************
 *
 * data-array
 *
 ****************************************************************************/

struct observation {
    observation() noexcept = default;
    observation(const real x_, const real y_) noexcept
      : x{ x_ }
      , y{ y_ }
    {}

    real x{};
    real y{};
};

enum class observer_flags : u8 {
    none              = 0,
    data_available    = 1,
    buffer_full       = 2,
    data_lost         = 4,
    use_linear_buffer = 8,
    Count
};

struct observer {
    using value_type = observation;

    observer(std::string_view name_) noexcept;

    void init(const i32   raw_buffer_size,
              const i32   linearizer_buffer_size,
              const float time_step_) noexcept;
    void reset() noexcept;
    void clear() noexcept;
    void update(observation_message msg) noexcept;
    bool full() const noexcept;
    void push_back(const observation& vec) noexcept;

    ring_buffer<observation_message> buffer;
    ring_buffer<observation>         linearized_buffer;

    model_id              model     = undefined<model_id>();
    dynamics_type         type      = dynamics_type::qss1_integrator;
    float                 time_step = 0.1f;
    std::pair<real, real> limits;

    small_string<14>         name;
    bitflags<observer_flags> states;
};

struct node {
    node() = default;

    node(const model_id model_, const i8 port_index_) noexcept
      : model(model_)
      , port_index(port_index_)
    {}

    model_id model      = undefined<model_id>();
    i8       port_index = 0;
};

struct output_message {
    output_message() noexcept  = default;
    ~output_message() noexcept = default;

    message  msg;
    model_id model = undefined<model_id>();
    i8       port  = 0;
};

static inline constexpr u32 invalid_heap_handle = 0xffffffff;

//! @brief Pairing heap implementation.
//!
//! A pairing heap is a type of heap data structure with relatively simple
//! implementation and excellent practical amortized performance, introduced
//! by Michael Fredman, Robert Sedgewick, Daniel Sleator, and Robert Tarjan
//! in 1986.
//!
//! https://en.wikipedia.org/wiki/Pairing_heap
template<typename A = default_allocator>
class heap
{
public:
    using this_container  = heap<A>;
    using allocator_type  = A;
    using memory_resource = typename A::memory_resource_t;
    using index_type      = u32;

    struct node {
        time     tn;
        model_id id;

        u32 prev;
        u32 next;
        u32 child;
    };

private:
    node* nodes{ nullptr };
    u32   m_size{ 0 };
    u32   max_size{ 0 };
    u32   capacity{ 0 };
    u32   free_list{ invalid_heap_handle };
    u32   root{ invalid_heap_handle };

    [[no_unique_address]] allocator_type m_alloc;

public:
    using handle = u32;

    constexpr heap() noexcept = default;

    constexpr heap(memory_resource* mem) noexcept
        requires(!std::is_empty_v<A>);

    constexpr ~heap() noexcept;

    //! clear then destroy buffer.
    constexpr void destroy() noexcept;

    //! Clear the buffer.
    constexpr void clear() noexcept;

    //! Call @c destroy() then allocate the new_capacity.
    constexpr bool reserve(std::integral auto new_capacity) noexcept;

    constexpr handle insert(time tn, model_id id) noexcept;

    constexpr void   destroy(handle elem) noexcept;
    constexpr void   insert(handle elem) noexcept;
    constexpr void   reintegrate(time tn, handle elem) noexcept;
    constexpr void   remove(handle elem) noexcept;
    constexpr handle pop() noexcept;
    constexpr void   decrease(time tn, handle elem) noexcept;
    constexpr void   increase(time tn, handle elem) noexcept;

    constexpr time tn(handle elem) noexcept;

    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;

    constexpr bool full() const noexcept;
    constexpr bool empty() const noexcept;

    constexpr handle top() const noexcept;

    constexpr void merge(heap& src) noexcept;

    constexpr const node& operator[](handle h) const noexcept;
    constexpr node&       operator[](handle h) noexcept;

private:
    constexpr handle merge(handle a, handle b) noexcept;
    constexpr handle merge_right(handle a) noexcept;
    constexpr handle merge_left(handle a) noexcept;
    constexpr handle merge_subheaps(handle a) noexcept;
    constexpr void   detach_subheap(handle elem) noexcept;
};

/*****************************************************************************
 *
 * scheduler
 *
 ****************************************************************************/

template<typename A = default_allocator>
class scheduller
{
public:
    using this_container  = heap<A>;
    using allocator_type  = A;
    using memory_resource = typename A::memory_resource_t;

private:
    heap<A> m_heap;

public:
    using internal_value_type = heap<A>;
    using handle              = u32;

    constexpr scheduller() = default;

    constexpr scheduller(memory_resource* mem) noexcept
        requires(!std::is_empty_v<A>);

    bool reserve(std::integral auto new_capacity) noexcept;

    void clear() noexcept;

    void destroy() noexcept;

    void insert(model& mdl, model_id id, time tn) noexcept;

    void reintegrate(model& mdl, time tn) noexcept;

    void erase(model& mdl) noexcept;

    void update(model& mdl, time tn) noexcept;

    void pop(vector<model_id, freelist_allocator>& out) noexcept;

    //! Returns the top event @c time-next in the heap.
    time tn() const noexcept;

    //! Returns the @c time-next event for handle.
    time tn(handle h) const noexcept;

    bool empty() const noexcept;

    unsigned size() const noexcept;
    int      ssize() const noexcept;
};

//! Helps to calculate the sizes of the `vectors` and `data_array` from a
//! number of bytes. Compute for each `source` the same number and adjust
//! the `max_client` variables for both random and binary source.
struct external_source_memory_requirement {
    int constant_nb            = 4;
    int text_file_nb           = 4;
    int binary_file_nb         = 4;
    int random_nb              = 4;
    int binary_file_max_client = 8;
    int random_max_client      = 8;

    //! Assign a default size for each external source subobject.
    constexpr external_source_memory_requirement() noexcept = default;

    //! Compute the size of each source.
    //!
    //! @param bytes The numbers of bytes availables.
    //! @param source_client_ratio A integer in the range `[0..100]` defines
    //! the ratio between `source` and `max_client` variables. `50` mean 50%
    //! sources and 50% max_clients, `0` means no source, `100` means no
    //! max-client.
    constexpr external_source_memory_requirement(
      const std::size_t                   bytes,
      const constrained_value<int, 5, 95> source_client_ratio) noexcept;

    constexpr bool valid() const;

    constexpr size_t in_bytes() const noexcept;
};

//! Helps to calculate the sizes of the `vectors` and `data_array` from a
//! number of bytes.
struct simulation_memory_requirement {
    std::size_t connections_b     = 0;
    std::size_t dated_messages_b  = 0;
    std::size_t external_source_b = 0;
    std::size_t simulation_b      = 0;

    int model_nb = 32;
    int hsms_nb  = 4;

    external_source_memory_requirement srcs;

    //! Assign a default size for each simulation subobject.
    constexpr simulation_memory_requirement() noexcept = default;

    //! Compute the size of each sub-objects.
    //!
    //! @param bytes The numbers of bytes availables.
    //! @param external_source Percentage of memory to use in external
    //! source.
    //! @param hsms_ratio Percentage of hsms model vs model number.
    //! @param source_client_ratio
    //! @param connections Percentage of simulation memory dedicated to
    //! connections.
    //! @param dated_messages Percentage of siulation memory dedicated to fifo,
    //! lifo history.
    constexpr simulation_memory_requirement(
      const std::size_t                   bytes,
      const constrained_value<int, 5, 90> connections     = 5,
      const constrained_value<int, 0, 50> dated_messages  = 5,
      const constrained_value<int, 0, 90> hsms            = 10,
      const constrained_value<int, 1, 90> external_source = 10,
      const constrained_value<int, 1, 95> source_client   = 50) noexcept;

    constexpr bool valid() const noexcept;

    //! Compute an estimate to store a model in simulation memory.
    constexpr static size_t estimate_model() noexcept;
};

class simulation
{
public:
    //! Used to report which part of the @c project have a problem with the
    //! @c new_error function.
    enum class part {
        messages,
        nodes,
        dated_messages,
        models,
        hsms,
        observers,
        scheduler,
        external_sources
    };

    mr_allocator<memory_resource> m_alloc;

    freelist_memory_resource shared;
    freelist_memory_resource nodes_alloc;
    freelist_memory_resource dated_messages_alloc;
    freelist_memory_resource external_source_alloc;

    vector<output_message, freelist_allocator> emitting_output_ports;
    vector<model_id, freelist_allocator>       immediate_models;
    vector<observer_id, freelist_allocator>    immediate_observers;

    data_array<model, model_id, freelist_allocator>                    models;
    data_array<hierarchical_state_machine, hsm_id, freelist_allocator> hsms;
    data_array<observer, observer_id, freelist_allocator> observers;
    data_array<vector<node, freelist_allocator>, node_id, freelist_allocator>
      nodes;
    data_array<small_vector<message, 8>, message_id, freelist_allocator>
      messages;

    data_array<ring_buffer<dated_message, freelist_allocator>,
               dated_message_id,
               freelist_allocator>
      dated_messages;

    scheduller<freelist_allocator> sched;

    external_source srcs;

    model_id get_id(const model& mdl) const;

    template<typename Dynamics>
    model_id get_id(const Dynamics& dyn) const;

public:
    //! Use the default malloc memory resource to allocate all memory need
    //! by sub-containers.
    simulation(const simulation_memory_requirement& init =
                 simulation_memory_requirement()) noexcept;

    //! Use the memory resource to allocate all memory need by
    //! sub-containers. This constructor alloc to allocate in heap or in
    //! stack for example.
    //!
    //! @param mem Must be no-null.
    simulation(memory_resource*                     mem,
               const simulation_memory_requirement& init =
                 simulation_memory_requirement()) noexcept;

    ~simulation() noexcept;

    bool can_alloc(std::integral auto place) const noexcept;
    bool can_alloc(dynamics_type type, std::integral auto place) const noexcept;

    template<typename Dynamics>
    bool can_alloc_dynamics(std::integral auto place) const noexcept;

    //! @brief cleanup simulation object
    //!
    //! Clean scheduler and input/output port from message.
    void clean() noexcept;

    //! @brief cleanup simulation and destroy all models and connections
    void clear() noexcept;

    //! @brief This function allocates dynamics and models.
    template<typename Dynamics>
    Dynamics& alloc() noexcept;

    //! @brief This function allocates dynamics and models.
    model& clone(const model& mdl) noexcept;

    //! @brief This function allocates dynamics and models.
    model& alloc(dynamics_type type) noexcept;

    void observe(model& mdl, observer& obs) noexcept;

    void unobserve(model& mdl) noexcept;

    void deallocate(model_id id) noexcept;

    template<typename Dynamics>
    void do_deallocate(Dynamics& dyn) noexcept;

    bool can_connect(int number) const noexcept;

    status connect(model& src, int port_src, model& dst, int port_dst) noexcept;
    bool   can_connect(const model& src,
                       int          port_src,
                       const model& dst,
                       int          port_dst) const noexcept;

    template<typename DynamicsSrc, typename DynamicsDst>
    status connect(DynamicsSrc& src,
                   int          port_src,
                   DynamicsDst& dst,
                   int          port_dst) noexcept;

    status disconnect(model& src,
                      int    port_src,
                      model& dst,
                      int    port_dst) noexcept;

    status initialize(time t) noexcept;

    //!
    status run(time& t) noexcept;

    template<typename Dynamics>
    status make_initialize(model& mdl, Dynamics& dyn, time t) noexcept;

    status make_initialize(model& mdl, time t) noexcept;

    template<typename Dynamics>
    status make_transition(model& mdl, Dynamics& dyn, time t) noexcept;

    status make_transition(model& mdl, time t) noexcept;

    template<typename Dynamics>
    status make_finalize(Dynamics& dyn, observer* obs, time t) noexcept;

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
    status finalize(time t) noexcept;
};

/////////////////////////////////////////////////////////////////////////////
//
// Some template type-trait to detect function and attribute in DEVS model.
//
/////////////////////////////////////////////////////////////////////////////

template<typename T>
concept has_lambda_function = requires(T t, simulation& sim) {
    {
        t.lambda(sim)
    } -> std::same_as<status>;
};

template<typename T>
concept has_transition_function =
  requires(T t, simulation& sim, time s, time e, time r) {
      {
          t.transition(sim, s, e, r)
      } -> std::same_as<status>;
  };

template<typename T>
concept has_observation_function = requires(T t, time s, time e) {
    {
        t.observation(s, e)
    } -> std::same_as<observation_message>;
};

template<typename T>
concept has_initialize_function = requires(T t, simulation& sim) {
    {
        t.initialize(sim)
    } -> std::same_as<status>;
};

template<typename T>
concept has_finalize_function = requires(T t, simulation& sim) {
    {
        t.finalize(sim)
    } -> std::same_as<status>;
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

inline status send_message(simulation& sim,
                           node_id&    output_port,
                           real        r1,
                           real        r2 = 0.0,
                           real        r3 = 0.0) noexcept;

/*****************************************************************************
 *
 * Qss1 part
 *
 ****************************************************************************/

template<int QssLevel>
struct abstract_integrator;

template<>
struct abstract_integrator<1> {
    message_id x[2];
    node_id    y[1];
    real       default_X  = zero;
    real       default_dQ = irt::real(0.01);
    real       X;
    real       q;
    real       u;
    time       sigma = time_domain<time>::zero;

    enum port_name { port_x_dot, port_reset };

    struct X_error {};
    struct dQ_error {};

    abstract_integrator() = default;

    abstract_integrator(const abstract_integrator& other) noexcept
      : default_X(other.default_X)
      , default_dQ(other.default_dQ)
      , X(other.X)
      , q(other.q)
      , u(other.u)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        if (!std::isfinite(default_X))
            return new_error(X_error{});

        if (!(std::isfinite(default_dQ) && default_dQ > zero))
            return new_error(dQ_error{});

        X = default_X;
        q = std::floor(X / default_dQ) * default_dQ;
        u = zero;

        sigma = time_domain<time>::zero;

        return success();
    }

    status external(const time e, const message& msg) noexcept
    {
        X += e * u;
        u = msg[0];

        if (sigma != zero) {
            if (u == zero)
                sigma = time_domain<time>::infinity;
            else if (u > zero)
                sigma = (q + default_dQ - X) / u;
            else
                sigma = (q - default_dQ - X) / u;
        }

        return success();
    }

    status reset(const message& msg) noexcept
    {
        X     = msg[0];
        q     = X;
        sigma = time_domain<time>::zero;
        return success();
    }

    status internal() noexcept
    {
        X += sigma * u;
        q = X;

        sigma =
          u == zero ? time_domain<time>::infinity : default_dQ / std::abs(u);

        return success();
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        auto* lst_x_dot = sim.messages.try_to_get(x[port_x_dot]);
        auto* lst_reset = sim.messages.try_to_get(x[port_reset]);

        if ((not lst_x_dot or lst_x_dot->empty()) and
            (not lst_reset or lst_reset->empty())) {
            if (auto ret = internal(); !ret)
                return ret.error();
        } else {
            if (lst_reset and not lst_reset->empty()) {
                if (auto ret = reset(lst_reset->front()); !ret)
                    return ret.error();
            } else if (lst_x_dot and not lst_x_dot->empty()) {
                if (auto ret = external(e, lst_x_dot->front()); !ret)
                    return ret.error();
            }
        }

        return success();
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
struct abstract_integrator<2> {
    message_id x[2];
    node_id    y[1];
    real       default_X  = zero;
    real       default_dQ = irt::real(0.01);
    real       X;
    real       u;
    real       mu;
    real       q;
    real       mq;
    time       sigma = time_domain<time>::zero;

    enum port_name { port_x_dot, port_reset };

    struct X_error {};
    struct dQ_error {};

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
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        if (!std::isfinite(default_X))
            return new_error(X_error{});

        if (!std::isfinite(default_dQ) && default_dQ > zero)
            return new_error(dQ_error{});

        X = default_X;

        u  = zero;
        mu = zero;
        q  = X;
        mq = zero;

        sigma = time_domain<time>::zero;

        return success();
    }

    status external(const time e, const message& msg) noexcept
    {
        const real value_x     = msg[0];
        const real value_slope = msg[1];

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

        return success();
    }

    status internal() noexcept
    {
        X += u * sigma + mu / two * sigma * sigma;
        q = X;
        u += mu * sigma;
        mq = u;

        sigma = mu == zero ? time_domain<time>::infinity
                           : std::sqrt(two * default_dQ / std::abs(mu));

        return success();
    }

    status reset(const message& msg) noexcept
    {
        X     = msg[0];
        q     = X;
        sigma = time_domain<time>::zero;

        return success();
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        auto* lst_x_dot = sim.messages.try_to_get(x[port_x_dot]);
        auto* lst_reset = sim.messages.try_to_get(x[port_reset]);

        if ((not lst_x_dot or lst_x_dot->empty()) and
            (not lst_reset or lst_reset->empty())) {
            if (auto ret = internal(); !ret)
                return ret.error();
        } else {
            if (lst_reset and not lst_reset->empty()) {
                if (auto ret = reset(lst_reset->front()); !ret)
                    return ret.error();
            } else if (lst_x_dot and not lst_x_dot->empty()) {
                if (auto ret = external(e, lst_x_dot->front()); !ret)
                    return ret.error();
            }
        }

        return success();
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
struct abstract_integrator<3> {
    message_id x[2];
    node_id    y[1];
    real       default_X  = zero;
    real       default_dQ = irt::real(0.01);
    real       X;
    real       u;
    real       mu;
    real       pu;
    real       q;
    real       mq;
    real       pq;
    time       sigma = time_domain<time>::zero;

    enum port_name { port_x_dot, port_reset };

    struct X_error {};
    struct dQ_error {};

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
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        if (!std::isfinite(default_X))
            return new_error(X_error{});

        if (!(std::isfinite(default_dQ) && default_dQ > zero))
            return new_error(dQ_error{});

        X     = default_X;
        u     = zero;
        mu    = zero;
        pu    = zero;
        q     = default_X;
        mq    = zero;
        pq    = zero;
        sigma = time_domain<time>::zero;

        return success();
    }

    status external(const time e, const message& msg) noexcept
    {
        const real value_x          = msg[0];
        const real value_slope      = msg[1];
        const real value_derivative = msg[2];

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

        return success();
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

        return success();
    }

    status reset(const message& msg) noexcept
    {
        X     = msg[0];
        q     = X;
        sigma = time_domain<time>::zero;

        return success();
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        auto* lst_x_dot = sim.messages.try_to_get(x[port_x_dot]);
        auto* lst_reset = sim.messages.try_to_get(x[port_reset]);

        if ((not lst_x_dot or lst_x_dot->empty()) and
            (not lst_reset or lst_reset->empty())) {
            if (auto ret = internal(); !ret)
                return ret.error();
        } else {
            if (lst_reset and not lst_reset->empty()) {
                if (auto ret = reset(lst_reset->front()); !ret)
                    return ret.error();
            } else if (lst_x_dot and not lst_x_dot->empty()) {
                if (auto ret = external(e, lst_x_dot->front()); !ret)
                    return ret.error();
            }
        }

        return success();
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
struct abstract_power {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    message_id x[1];
    node_id    y[1];
    time       sigma;

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

        return success();
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

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        sigma     = time_domain<time>::infinity;
        auto* lst = sim.messages.try_to_get(x[0]);

        if (lst and not lst->empty()) {
            const auto& msg = lst->front();

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

        return success();
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
struct abstract_square {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    message_id x[1];
    node_id    y[1];
    time       sigma;

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

        return success();
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

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        sigma     = time_domain<time>::infinity;
        auto* lst = sim.messages.try_to_get(x[0]);

        if (lst and not lst->empty()) {
            const auto& msg = lst->front();

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

        return success();
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
struct abstract_sum {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");
    static_assert(PortNumber > 1, "sum model need at least two input port");

    message_id x[PortNumber];
    node_id    y[1];
    time       sigma;

    real default_values[PortNumber]    = { 0 };
    real values[QssLevel * PortNumber] = { 0 };

    abstract_sum() noexcept = default;

    abstract_sum(const abstract_sum& other) noexcept
      : sigma(other.sigma)
    {
        std::copy_n(other.default_values,
                    std::size(other.default_values),
                    default_values);

        std::copy_n(other.values, std::size(other.values), values);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::copy_n(default_values, std::size(default_values), values);

        if constexpr (QssLevel >= 2) {
            std::fill_n(values + std::size(default_values),
                        std::size(values) - std::size(default_values),
                        zero);
        }

        sigma = time_domain<time>::infinity;

        return success();
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

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        bool message = false;

        if constexpr (QssLevel == 1) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (auto* lst = sim.messages.try_to_get(x[i]); lst) {
                    for (const auto& msg : *lst) {
                        values[i] = msg[0];
                        message   = true;
                    }
                }
            }
        }

        if constexpr (QssLevel == 2) {
            for (size_t i = 0; i != PortNumber; ++i) {
                auto* lst = sim.messages.try_to_get(x[i]);

                if (!lst or lst->empty()) {
                    values[i] += values[i + PortNumber] * e;
                } else {
                    for (const auto& msg : *lst) {
                        values[i]              = msg[0];
                        values[i + PortNumber] = msg[1];
                        message                = true;
                    }
                }
            }
        }

        if constexpr (QssLevel == 3) {
            for (size_t i = 0; i != PortNumber; ++i) {
                auto* lst = sim.messages.try_to_get(x[i]);

                if (!lst or lst->empty()) {
                    values[i] += values[i + PortNumber] * e +
                                 values[i + PortNumber + PortNumber] * e * e;
                    values[i + PortNumber] +=
                      2 * values[i + PortNumber + PortNumber] * e;
                } else {
                    for (const auto& msg : *lst) {
                        values[i]                           = msg[0];
                        values[i + PortNumber]              = msg[1];
                        values[i + PortNumber + PortNumber] = msg[2];
                        message                             = true;
                    }
                }
            }
        }

        sigma = message ? time_domain<time>::zero : time_domain<time>::infinity;

        return success();
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
struct abstract_wsum {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");
    static_assert(PortNumber > 1, "sum model need at least two input port");

    message_id x[PortNumber];
    node_id    y[1];
    time       sigma;

    real default_values[PortNumber]       = { 0 };
    real default_input_coeffs[PortNumber] = { 0 };
    real values[QssLevel * PortNumber]    = { 0 };

    abstract_wsum() noexcept = default;

    abstract_wsum(const abstract_wsum& other) noexcept
      : sigma(other.sigma)
    {
        std::copy_n(other.default_values,
                    std::size(other.default_values),
                    default_values);

        std::copy_n(other.default_input_coeffs,
                    std::size(other.default_input_coeffs),
                    default_input_coeffs);

        std::copy_n(other.values, std::size(other.values), values);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        std::copy_n(default_values, std::size(default_values), values);

        if constexpr (QssLevel >= 2) {
            std::fill_n(std::begin(values) + std::size(default_values),
                        std::size(values) - std::size(default_values),
                        zero);
        }

        sigma = time_domain<time>::infinity;

        return success();
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

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        bool message = false;

        if constexpr (QssLevel == 1) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (auto* lst = sim.messages.try_to_get(x[i]); lst) {
                    if (not lst->empty()) {
                        for (const auto& msg : *lst) {
                            values[i] = msg[0];
                            message   = true;
                        }
                    }
                }
            }
        }

        if constexpr (QssLevel == 2) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (auto* lst = sim.messages.try_to_get(x[i]); lst) {
                    if (lst->empty()) {
                        values[i] += values[i + PortNumber] * e;
                    } else {
                        for (const auto& msg : *lst) {
                            values[i]              = msg[0];
                            values[i + PortNumber] = msg[1];
                            message                = true;
                        }
                    }
                }
            }
        }

        if constexpr (QssLevel == 3) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (auto* lst = sim.messages.try_to_get(x[i]); lst) {
                    if (lst->empty()) {
                        values[i] +=
                          values[i + PortNumber] * e +
                          values[i + PortNumber + PortNumber] * e * e;
                        values[i + PortNumber] +=
                          2 * values[i + PortNumber + PortNumber] * e;
                    } else {
                        for (const auto& msg : *lst) {
                            values[i]                           = msg[0];
                            values[i + PortNumber]              = msg[1];
                            values[i + PortNumber + PortNumber] = msg[2];
                            message                             = true;
                        }
                    }
                }
            }
        }

        sigma = message ? time_domain<time>::zero : time_domain<time>::infinity;

        return success();
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
struct abstract_multiplier {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    message_id x[2];
    node_id    y[1];
    time       sigma;

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

        return success();
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

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        auto* lst_0 = sim.messages.try_to_get(x[0]);
        auto* lst_1 = sim.messages.try_to_get(x[1]);

        if (!lst_0 or !lst_1)
            return success();

        bool message_port_0 = not lst_0->empty();
        bool message_port_1 = not lst_1->empty();
        sigma               = time_domain<time>::infinity;

        for (const auto& msg : *lst_0) {
            sigma     = time_domain<time>::zero;
            values[0] = msg[0];

            if constexpr (QssLevel >= 2)
                values[2 + 0] = msg[1];

            if constexpr (QssLevel == 3)
                values[2 + 2 + 0] = msg[2];
        }

        for (const auto& msg : *lst_1) {
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

        return success();
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

struct counter {
    message_id x[1];
    time       sigma;
    i64        number;

    counter() noexcept = default;

    counter(const counter& other) noexcept
      : sigma(other.sigma)
      , number(other.number)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        number = { 0 };
        sigma  = time_domain<time>::infinity;

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        if (auto* lst = sim.messages.try_to_get(x[0]); lst)
            number += lst->ssize();

        return success();
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, static_cast<real>(number) };
    }
};

struct generator {
    node_id y[1];
    time    sigma;
    real    value;

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
    {}

    status initialize(simulation& sim) noexcept
    {
        sigma = default_offset;

        if (stop_on_error) {
            irt_check(initialize_source(sim, default_source_ta));
            irt_check(initialize_source(sim, default_source_value));
        } else {
            (void)initialize_source(sim, default_source_ta);
            (void)initialize_source(sim, default_source_value);
        }

        return success();
    }

    status finalize(simulation& sim) noexcept
    {
        irt_check(finalize_source(sim, default_source_ta));
        irt_check(finalize_source(sim, default_source_value));

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        double local_sigma = 0;
        double local_value = 0;

        if (stop_on_error) {
            irt_check(update_source(sim, default_source_ta, local_sigma));
            irt_check(update_source(sim, default_source_value, local_value));
            sigma = static_cast<real>(local_sigma);
            value = static_cast<real>(local_value);
        } else {
            if (auto ret = update_source(sim, default_source_ta, local_sigma);
                !ret)
                sigma = time_domain<time>::infinity;
            else
                sigma = static_cast<real>(local_sigma);

            if (auto ret =
                  update_source(sim, default_source_value, local_value);
                !ret)
                value = 0;
            else
                value = static_cast<real>(local_value);
        }

        return success();
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

struct constant {
    node_id y[1];
    time    sigma;

    real default_value  = 0.0;
    time default_offset = time_domain<time>::zero;

    enum class init_type : i8 {
        // A constant value initialized at startup of the simulation. Use
        // the @c default_value.
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
        // component. Use the @c port attribute to specify the identifier of
        // the port. The @c default_value is filled via the component to
        // simulation algorithm. Otherwise, the default value is unmodified.
        incoming_component_n,

        // The number of incoming connections on the nth output ports of the
        // component. Use the @c port attribute to specify the identifier of
        // the port. The @c default_value is filled via the component to
        // simulation algorithm. Otherwise, the default value is unmodified.
        outcoming_component_n,
    };

    static inline constexpr int init_type_count = 5;

    real      value = 0.0;
    init_type type  = init_type::constant;
    u64       port  = 0;

    constant() noexcept = default;

    constant(const constant& other) noexcept
      : sigma(other.sigma)
      , default_value(other.default_value)
      , default_offset(other.default_offset)
      , value(other.value)
      , type(other.type)
      , port(other.port)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        sigma = default_offset;
        value = default_value;

        return success();
    }

    status transition(simulation& /*sim*/,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        sigma = time_domain<time>::infinity;

        return success();
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
struct abstract_filter {
    message_id x[1];
    node_id    y[3];

    time sigma;
    real default_lower_threshold = -std::numeric_limits<real>::infinity();
    real default_upper_threshold = +std::numeric_limits<real>::infinity();
    real lower_threshold;
    real upper_threshold;
    real value[QssLevel];
    bool reach_lower_threshold = false;
    bool reach_upper_threshold = false;

    struct threshold_condition_error {};

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
        if (default_lower_threshold >= default_upper_threshold)
            return new_error(threshold_condition_error{});

        lower_threshold       = default_lower_threshold;
        upper_threshold       = default_upper_threshold;
        reach_lower_threshold = false;
        reach_upper_threshold = false;
        std::fill_n(value, QssLevel, zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        auto* lst = sim.messages.try_to_get(x[0]);

        if (not lst or lst->empty()) {
            if constexpr (QssLevel == 2)
                value[0] += value[1] * e;
            if constexpr (QssLevel == 3) {
                value[0] += value[1] * e + value[2] * e * e;
                value[1] += two * value[2] * e;
            }
        } else {
            for (const auto& msg : *lst) {
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

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            if (reach_upper_threshold) {
                irt_check(send_message(sim, y[0], upper_threshold));
                irt_check(send_message(sim, y[1], one));
            } else {
                irt_check(send_message(sim, y[0], value[0]));
                irt_check(send_message(sim, y[1], zero));
            }

            if (reach_lower_threshold) {
                irt_check(send_message(sim, y[0], lower_threshold));
                irt_check(send_message(sim, y[2], one));
            } else {
                irt_check(send_message(sim, y[0], value[0]));
                irt_check(send_message(sim, y[2], zero));
            }
        }

        if constexpr (QssLevel == 2) {
            if (reach_upper_threshold) {
                irt_check(send_message(sim, y[0], upper_threshold));
                irt_check(send_message(sim, y[1], one));
            } else {
                irt_check(send_message(sim, y[0], value[0], value[1]));
                irt_check(send_message(sim, y[1], zero));
            }

            if (reach_lower_threshold) {
                irt_check(send_message(sim, y[0], lower_threshold));
                irt_check(send_message(sim, y[2], one));
            } else {
                irt_check(send_message(sim, y[0], value[0], value[1]));
                irt_check(send_message(sim, y[2], zero));
            }

            return success();
        }

        if constexpr (QssLevel == 3) {
            if (reach_upper_threshold) {
                irt_check(send_message(sim, y[0], upper_threshold));
                irt_check(send_message(sim, y[1], one));
            } else {
                irt_check(
                  send_message(sim, y[0], value[0], value[1], value[2]));
                irt_check(send_message(sim, y[1], zero));
            }

            if (reach_lower_threshold) {
                irt_check(send_message(sim, y[0], lower_threshold));
                irt_check(send_message(sim, y[2], one));
            } else {
                irt_check(
                  send_message(sim, y[0], value[0], value[1], value[2]));
                irt_check(send_message(sim, y[2], zero));
            }

            return success();
        }

        return success();
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

struct abstract_and_check {
    template<typename Iterator>
    bool operator()(Iterator begin, Iterator end) const noexcept
    {
        return std::all_of(
          begin, end, [](const bool value) noexcept { return value == true; });
    }
};

struct abstract_or_check {
    template<typename Iterator>
    bool operator()(Iterator begin, Iterator end) const noexcept
    {
        return std::any_of(
          begin, end, [](const bool value) noexcept { return value == true; });
    }
};

template<typename AbstractLogicalTester, int PortNumber>
struct abstract_logical {
    message_id x[PortNumber];
    node_id    y[1];
    time       sigma = time_domain<time>::infinity;

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

        AbstractLogicalTester tester{};
        is_valid      = tester(std::begin(values), std::end(values));
        sigma         = time_domain<time>::zero;
        value_changed = true;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if (value_changed)
            return send_message(sim, y[0], is_valid ? one : zero);

        return success();
    }

    status transition(simulation& sim, time, time, time) noexcept
    {
        const bool old_is_value = is_valid;

        for (int i = 0; i < PortNumber; ++i) {
            if (auto* lst = sim.messages.try_to_get(x[i]);
                lst and not lst->empty()) {
                if (lst->front()[0] != zero) {
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

        AbstractLogicalTester tester{};
        is_valid = tester(std::begin(values), std::end(values));

        if (is_valid != old_is_value) {
            value_changed = true;
            sigma         = time_domain<time>::zero;
        } else {
            value_changed = false;
            sigma         = time_domain<time>::infinity;
        }

        return success();
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

struct logical_invert {
    message_id x[1];
    node_id    y[1];
    time       sigma;

    bool default_value = false;
    bool value;
    bool value_changed;

    logical_invert() noexcept { sigma = time_domain<time>::infinity; }

    status initialize() noexcept
    {
        value         = default_value;
        sigma         = time_domain<time>::infinity;
        value_changed = false;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if (value_changed)
            return send_message(sim, y[0], value ? zero : one);

        return success();
    }

    status transition(simulation& sim, time, time, time) noexcept
    {
        auto  value_changed = false;
        auto* lst           = sim.messages.try_to_get(x[0]);

        if (lst and not lst->empty()) {
            const auto& msg = lst->front();

            if ((msg[0] != zero && !value) || (msg[0] == zero && value))
                value_changed = true;
        }

        sigma =
          value_changed ? time_domain<time>::zero : time_domain<time>::infinity;
        return success();
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
 * 2. You are not allowed to dispatch an event from within an event
 * dispatch. You should queue events if you want such behavior. This
 * restriction is imposed only to prevent the user from creating extremely
 * complicated to trace state machines (which is what we are trying to
 * avoid).
 */
class hierarchical_state_machine
{
public:
    using state_id                      = u8;
    static const u8 max_number_of_state = 254;
    static const u8 invalid_state_id    = 255;

    enum class event_type : u8 { enter = 0, exit, input_changed };

    struct top_state_error {};
    struct next_state_error {};

    constexpr static int event_type_count     = 3;
    constexpr static int variable_count       = 8;
    constexpr static int action_type_count    = 16;
    constexpr static int condition_type_count = 20;

    enum class variable : u8 {
        none = 0,
        port_0,
        port_1,
        port_2,
        port_3,
        var_a,
        var_b,
        constant
    };

    enum class action_type : u8 {
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

    enum class condition_type : u8 {
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

    //! Action available when state is processed during enter, exit or
    //! condition event. \note Only on action (value set/unset, devs output,
    //! etc.) by action. To perform more action, use several states.
    struct state_action {
        i32         parameter = 0;
        variable    var1      = variable::none;
        variable    var2      = variable::none;
        action_type type      = action_type::none;

        void clear() noexcept;
    };

    //! \note
    //! 1. \c value_condition stores the bit for input value corresponding
    //! to the user request to satisfy the condition. \c value_mask stores
    //! the bit useful in value_condition. If value_mask equal \c 0x0 then,
    //! the condition is always true. If \c value_mask equals \c 0xff the \c
    //! value_condition bit are mandatory.
    //! 2. \c condition_state_action stores transition or action conditions.
    //! Condition can use input port state or condition on integer a or b.
    struct condition_action {
        i32            parameter = 0;
        condition_type type      = condition_type::none;
        u8             port      = 0;
        u8             mask      = 0;

        bool check(u8 port_values, i32 a, i32 b) noexcept;
        void clear() noexcept;
    };

    struct state {
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

    struct output_message {
        i32 value{};
        u8  port{};
    };

    struct execution {
        i32 a      = 0;
        i32 b      = 0;
        u8  values = 0;

        state_id current_state        = invalid_state_id;
        state_id next_state           = invalid_state_id;
        state_id source_state         = invalid_state_id;
        state_id current_source_state = invalid_state_id;
        state_id previous_state       = invalid_state_id;
        bool     disallow_transition  = false;

        small_vector<output_message, 4> outputs;

        void clear() noexcept
        {
            values = 0u;
            outputs.clear();

            current_state        = invalid_state_id;
            next_state           = invalid_state_id;
            source_state         = invalid_state_id;
            current_source_state = invalid_state_id;
            previous_state       = invalid_state_id;
            disallow_transition  = false;
        }
    };

    hierarchical_state_machine() noexcept = default;
    hierarchical_state_machine(const hierarchical_state_machine&) noexcept;
    hierarchical_state_machine& operator=(
      const hierarchical_state_machine&) noexcept;

    status start(execution& state) noexcept;

    void clear() noexcept
    {
        states    = {};
        top_state = invalid_state_id;
    }

    /// Dispatches an event.
    /// @return return true if the event was processed, otherwise false. If
    /// the automata is badly defined, return an modeling error.
    result<bool> dispatch(const event_type e, execution& exec) noexcept;

    /// Return true if the state machine is currently dispatching an event.
    bool is_dispatching(execution& state) const noexcept;

    /// Transitions the state machine. This function can not be called from
    /// Enter / Exit events in the state handler.
    void transition(state_id target, execution& exec) noexcept;

    /// Set a handler for a state ID. This function will overwrite the
    /// current state handler. \param id state id from 0 to
    /// max_number_of_state \param super_id id of the super state, if
    /// invalid_state_id this is a top state. Only one state can be a top
    /// state. \param sub_id if != invalid_state_id this sub state (child
    /// state) will be entered after the state Enter event is executed.
    status set_state(state_id id,
                     state_id super_id = invalid_state_id,
                     state_id sub_id   = invalid_state_id) noexcept;

    /// Resets the state to invalid mode.
    void clear_state(state_id id) noexcept;

    bool is_in_state(execution& state, state_id id) const noexcept;

    bool handle(const state_id   state,
                const event_type event,
                execution&       exec) noexcept;

    int    steps_to_common_root(state_id source, state_id target) noexcept;
    status on_enter_sub_state(execution& state) noexcept;

    void affect_action(const state_action& action, execution& exec) noexcept;

    std::array<state, max_number_of_state> states;
    state_id                               top_state = invalid_state_id;
};

status get_hierarchical_state_machine(simulation&                  sim,
                                      hierarchical_state_machine*& hsm,
                                      hsm_id                       id) noexcept;

struct hsm_wrapper {
    using hsm      = hierarchical_state_machine;
    using state_id = hsm::state_id;

    message_id x[4];
    node_id    y[4];

    hsm_id         id;
    u64            compo_id;
    hsm::execution exec;

    real sigma;

    hsm_wrapper() noexcept;
    hsm_wrapper(const hsm_wrapper& other) noexcept;

    status initialize(simulation& sim) noexcept;
    status transition(simulation& sim, time t, time e, time r) noexcept;
    status lambda(simulation& sim) noexcept;
};

template<int PortNumber>
struct accumulator {
    message_id x[2 * PortNumber];
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

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        for (sz i = 0; i != PortNumber; ++i) {
            if (auto* lst = sim.messages.try_to_get(x[i + PortNumber]);
                lst and not lst->empty()) {
                numbers[i] = lst->front()[0];
            }
        }

        for (sz i = 0; i != PortNumber; ++i) {
            if (auto* lst = sim.messages.try_to_get(x[i]);
                lst and not lst->empty()) {
                if (lst->front()[0] != zero)
                    number += numbers[i];
            }
        }

        return success();
    }
};

template<int QssLevel>
struct abstract_cross {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    message_id x[4];
    node_id    y[3];
    time       sigma;

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

    enum port_name {
        port_value,
        port_if_value,
        port_else_value,
        port_threshold
    };

    enum out_name { o_if_value, o_else_value, o_threshold_reached };

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

        return success();
    }

    status transition(simulation&           sim,
                      time                  t,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        const auto old_else_value = else_value[0];

        if (is_defined(x[port_threshold])) {
            if (auto* lst = sim.messages.try_to_get(x[port_threshold]); lst) {
                for (const auto& msg : *lst)
                    threshold = msg[0];
            }
        }

        if (!is_defined(x[port_if_value])) {
            if constexpr (QssLevel == 2)
                if_value[0] += if_value[1] * e;
            if constexpr (QssLevel == 3) {
                if_value[0] += if_value[1] * e + if_value[2] * e * e;
                if_value[1] += two * if_value[2] * e;
            }
        } else {
            if (auto* lst = sim.messages.try_to_get(x[port_if_value]); lst) {
                for (const auto& msg : *lst) {
                    if_value[0] = msg[0];
                    if constexpr (QssLevel >= 2)
                        if_value[1] = msg[1];
                    if constexpr (QssLevel == 3)
                        if_value[2] = msg[2];
                }
            }
        }

        if (!is_defined(x[port_else_value])) {
            if constexpr (QssLevel == 2)
                else_value[0] += else_value[1] * e;
            if constexpr (QssLevel == 3) {
                else_value[0] += else_value[1] * e + else_value[2] * e * e;
                else_value[1] += two * else_value[2] * e;
            }
        } else {
            if (auto* lst = sim.messages.try_to_get(x[port_else_value]); lst) {
                for (const auto& msg : *lst) {
                    else_value[0] = msg[0];
                    if constexpr (QssLevel >= 2)
                        else_value[1] = msg[1];
                    if constexpr (QssLevel == 3)
                        else_value[2] = msg[2];
                }
            }
        }

        if (!is_defined(x[port_value])) {
            if constexpr (QssLevel == 2)
                value[0] += value[1] * e;
            if constexpr (QssLevel == 3) {
                value[0] += value[1] * e + value[2] * e * e;
                value[1] += two * value[2] * e;
            }
        } else {
            if (auto* lst = sim.messages.try_to_get(x[port_value]); lst) {
                for (const auto& msg : *lst) {
                    value[0] = msg[0];
                    if constexpr (QssLevel >= 2)
                        value[1] = msg[1];
                    if constexpr (QssLevel == 3)
                        value[2] = msg[2];
                }
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

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            if (reach_threshold) {
                irt_check(send_message(sim, y[o_if_value], if_value[0]));

                irt_check(send_message(sim, y[o_threshold_reached], one));
            } else {
                irt_check(send_message(sim, y[o_else_value], else_value[0]));

                irt_check(send_message(sim, y[o_threshold_reached], zero));
            }

            return success();
        }

        if constexpr (QssLevel == 2) {
            if (reach_threshold) {
                irt_check(
                  send_message(sim, y[o_if_value], if_value[0], if_value[1]));

                irt_check(send_message(sim, y[o_threshold_reached], one));
            } else {
                irt_check(send_message(
                  sim, y[o_else_value], else_value[0], else_value[1]));

                irt_check(send_message(sim, y[o_threshold_reached], zero));
            }

            return success();
        }

        if constexpr (QssLevel == 3) {
            if (reach_threshold) {
                irt_check(send_message(
                  sim, y[o_if_value], if_value[0], if_value[1], if_value[2]));

                irt_check(send_message(sim, y[o_threshold_reached], one));
            } else {
                irt_check(send_message(sim,
                                       y[o_else_value],
                                       else_value[0],
                                       else_value[1],
                                       else_value[2]));

                irt_check(send_message(sim, y[o_threshold_reached], zero));
            }

            return success();
        }

        return success();
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

struct time_func {
    node_id y[1];
    time    sigma;

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
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        f     = default_f;
        sigma = default_sigma;
        value = 0.0;
        return success();
    }

    status transition(simulation& /*sim*/,
                      time t,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        value = (*f)(t);
        return success();
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

using accumulator_2 = accumulator<2>;

struct queue {
    message_id x[1];
    node_id    y[1];
    time       sigma;

    dated_message_id fifo = undefined<dated_message_id>();

    real default_ta = one;

    struct ta_error {};

    queue() noexcept = default;

    queue(const queue& other) noexcept
      : sigma(other.sigma)
      , fifo(undefined<dated_message_id>())
      , default_ta(other.default_ta)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        if (default_ta <= 0)
            return new_error(ta_error{});

        sigma = time_domain<time>::infinity;
        fifo  = undefined<dated_message_id>();

        return success();
    }

    status finalize(simulation& sim) noexcept
    {
        if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
            ar->clear();
            sim.dated_messages.free(*ar);
            fifo = undefined<dated_message_id>();
        }

        return success();
    }

    status transition(simulation& sim, time t, time /*e*/, time /*r*/) noexcept;

    status lambda(simulation& sim) noexcept
    {
        if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
            const auto t = ar->head()->data()[0];

            for (const auto& elem : *ar)
                if (elem[0] <= t)
                    irt_check(
                      send_message(sim, y[0], elem[1], elem[2], elem[3]));
        }

        return success();
    }
};

struct dynamic_queue {
    message_id       x[1];
    node_id          y[1];
    time             sigma;
    dated_message_id fifo = undefined<dated_message_id>();

    source default_source_ta;
    bool   stop_on_error = false;

    dynamic_queue() noexcept = default;

    dynamic_queue(const dynamic_queue& other) noexcept
      : sigma(other.sigma)
      , fifo(undefined<dated_message_id>())
      , default_source_ta(other.default_source_ta)
      , stop_on_error(other.stop_on_error)
    {}

    status initialize(simulation& sim) noexcept
    {
        sigma = time_domain<time>::infinity;
        fifo  = undefined<dated_message_id>();

        if (stop_on_error) {
            irt_check(initialize_source(sim, default_source_ta));
        } else {
            (void)initialize_source(sim, default_source_ta);
        }

        return success();
    }

    status finalize(simulation& sim) noexcept
    {
        if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
            ar->clear();
            sim.dated_messages.free(*ar);
            fifo = undefined<dated_message_id>();
        }

        irt_check(finalize_source(sim, default_source_ta));

        return success();
    }

    status transition(simulation& sim, time t, time /*e*/, time /*r*/) noexcept;

    status lambda(simulation& sim) noexcept
    {
        if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
            auto       it  = ar->begin();
            auto       end = ar->end();
            const auto t   = it->data()[0];

            for (; it != end && it->data()[0] <= t; ++it)
                irt_check(send_message(
                  sim, y[0], it->data()[1], it->data()[2], it->data()[3]));
        }

        return success();
    }
};

struct priority_queue {
    message_id       x[1];
    node_id          y[1];
    time             sigma;
    dated_message_id fifo       = undefined<dated_message_id>();
    real             default_ta = 1.0;

    source default_source_ta;
    bool   stop_on_error = false;

    priority_queue() noexcept = default;

    priority_queue(const priority_queue& other) noexcept
      : sigma(other.sigma)
      , fifo(undefined<dated_message_id>())
      , default_ta(other.default_ta)
      , default_source_ta(other.default_source_ta)
      , stop_on_error(other.stop_on_error)
    {}

private:
    status try_to_insert(simulation&    sim,
                         const time     t,
                         const message& msg) noexcept;

public:
    status initialize(simulation& sim) noexcept
    {
        if (stop_on_error) {
            irt_check(initialize_source(sim, default_source_ta));
        } else {
            (void)initialize_source(sim, default_source_ta);
        }

        sigma = time_domain<time>::infinity;
        fifo  = undefined<dated_message_id>();

        return success();
    }

    status finalize(simulation& sim) noexcept
    {
        if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
            ar->clear();
            sim.dated_messages.free(*ar);
            fifo = undefined<dated_message_id>();
        }

        irt_check(finalize_source(sim, default_source_ta));

        return success();
    }

    status transition(simulation& sim, time t, time /*e*/, time /*r*/) noexcept;

    status lambda(simulation& sim) noexcept
    {
        if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
            auto       it  = ar->begin();
            auto       end = ar->end();
            const auto t   = it->data()[0];

            for (; it != end && it->data()[0] <= t; ++it)
                irt_check(send_message(
                  sim, y[0], it->data()[1], it->data()[2], it->data()[3]));
        }

        return success();
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
               sizeof(counter),
               sizeof(queue),
               sizeof(dynamic_queue),
               sizeof(priority_queue),
               sizeof(generator),
               sizeof(constant),
               sizeof(time_func),
               sizeof(accumulator_2),
               sizeof(logical_and_2),
               sizeof(logical_and_3),
               sizeof(logical_or_2),
               sizeof(logical_or_3),
               sizeof(logical_invert),
               sizeof(hsm_wrapper));
}

struct model {
    real tl     = 0.0;
    real tn     = time_domain<time>::infinity;
    u32  handle = invalid_heap_handle;

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

    if constexpr (std::is_same_v<Dynamics, time_func>)
        return dynamics_type::time_func;
    if constexpr (std::is_same_v<Dynamics, accumulator_2>)
        return dynamics_type::accumulator_2;

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

    unreachable();
}

template<typename Function>
constexpr auto dispatch(const model& mdl, Function&& f) noexcept
{
    switch (mdl.type) {
    case dynamics_type::qss1_integrator:
        return f(*reinterpret_cast<const qss1_integrator*>(&mdl.dyn));
    case dynamics_type::qss1_multiplier:
        return f(*reinterpret_cast<const qss1_multiplier*>(&mdl.dyn));
    case dynamics_type::qss1_cross:
        return f(*reinterpret_cast<const qss1_cross*>(&mdl.dyn));
    case dynamics_type::qss1_filter:
        return f(*reinterpret_cast<const qss1_filter*>(&mdl.dyn));
    case dynamics_type::qss1_power:
        return f(*reinterpret_cast<const qss1_power*>(&mdl.dyn));
    case dynamics_type::qss1_square:
        return f(*reinterpret_cast<const qss1_square*>(&mdl.dyn));
    case dynamics_type::qss1_sum_2:
        return f(*reinterpret_cast<const qss1_sum_2*>(&mdl.dyn));
    case dynamics_type::qss1_sum_3:
        return f(*reinterpret_cast<const qss1_sum_3*>(&mdl.dyn));
    case dynamics_type::qss1_sum_4:
        return f(*reinterpret_cast<const qss1_sum_4*>(&mdl.dyn));
    case dynamics_type::qss1_wsum_2:
        return f(*reinterpret_cast<const qss1_wsum_2*>(&mdl.dyn));
    case dynamics_type::qss1_wsum_3:
        return f(*reinterpret_cast<const qss1_wsum_3*>(&mdl.dyn));
    case dynamics_type::qss1_wsum_4:
        return f(*reinterpret_cast<const qss1_wsum_4*>(&mdl.dyn));

    case dynamics_type::qss2_integrator:
        return f(*reinterpret_cast<const qss2_integrator*>(&mdl.dyn));
    case dynamics_type::qss2_multiplier:
        return f(*reinterpret_cast<const qss2_multiplier*>(&mdl.dyn));
    case dynamics_type::qss2_cross:
        return f(*reinterpret_cast<const qss2_cross*>(&mdl.dyn));
    case dynamics_type::qss2_filter:
        return f(*reinterpret_cast<const qss2_filter*>(&mdl.dyn));
    case dynamics_type::qss2_power:
        return f(*reinterpret_cast<const qss2_power*>(&mdl.dyn));
    case dynamics_type::qss2_square:
        return f(*reinterpret_cast<const qss2_square*>(&mdl.dyn));
    case dynamics_type::qss2_sum_2:
        return f(*reinterpret_cast<const qss2_sum_2*>(&mdl.dyn));
    case dynamics_type::qss2_sum_3:
        return f(*reinterpret_cast<const qss2_sum_3*>(&mdl.dyn));
    case dynamics_type::qss2_sum_4:
        return f(*reinterpret_cast<const qss2_sum_4*>(&mdl.dyn));
    case dynamics_type::qss2_wsum_2:
        return f(*reinterpret_cast<const qss2_wsum_2*>(&mdl.dyn));
    case dynamics_type::qss2_wsum_3:
        return f(*reinterpret_cast<const qss2_wsum_3*>(&mdl.dyn));
    case dynamics_type::qss2_wsum_4:
        return f(*reinterpret_cast<const qss2_wsum_4*>(&mdl.dyn));

    case dynamics_type::qss3_integrator:
        return f(*reinterpret_cast<const qss3_integrator*>(&mdl.dyn));
    case dynamics_type::qss3_multiplier:
        return f(*reinterpret_cast<const qss3_multiplier*>(&mdl.dyn));
    case dynamics_type::qss3_cross:
        return f(*reinterpret_cast<const qss3_cross*>(&mdl.dyn));
    case dynamics_type::qss3_filter:
        return f(*reinterpret_cast<const qss3_filter*>(&mdl.dyn));
    case dynamics_type::qss3_power:
        return f(*reinterpret_cast<const qss3_power*>(&mdl.dyn));
    case dynamics_type::qss3_square:
        return f(*reinterpret_cast<const qss3_square*>(&mdl.dyn));
    case dynamics_type::qss3_sum_2:
        return f(*reinterpret_cast<const qss3_sum_2*>(&mdl.dyn));
    case dynamics_type::qss3_sum_3:
        return f(*reinterpret_cast<const qss3_sum_3*>(&mdl.dyn));
    case dynamics_type::qss3_sum_4:
        return f(*reinterpret_cast<const qss3_sum_4*>(&mdl.dyn));
    case dynamics_type::qss3_wsum_2:
        return f(*reinterpret_cast<const qss3_wsum_2*>(&mdl.dyn));
    case dynamics_type::qss3_wsum_3:
        return f(*reinterpret_cast<const qss3_wsum_3*>(&mdl.dyn));
    case dynamics_type::qss3_wsum_4:
        return f(*reinterpret_cast<const qss3_wsum_4*>(&mdl.dyn));

    case dynamics_type::counter:
        return f(*reinterpret_cast<const counter*>(&mdl.dyn));
    case dynamics_type::queue:
        return f(*reinterpret_cast<const queue*>(&mdl.dyn));
    case dynamics_type::dynamic_queue:
        return f(*reinterpret_cast<const dynamic_queue*>(&mdl.dyn));
    case dynamics_type::priority_queue:
        return f(*reinterpret_cast<const priority_queue*>(&mdl.dyn));
    case dynamics_type::generator:
        return f(*reinterpret_cast<const generator*>(&mdl.dyn));
    case dynamics_type::constant:
        return f(*reinterpret_cast<const constant*>(&mdl.dyn));
    case dynamics_type::accumulator_2:
        return f(*reinterpret_cast<const accumulator_2*>(&mdl.dyn));
    case dynamics_type::time_func:
        return f(*reinterpret_cast<const time_func*>(&mdl.dyn));

    case dynamics_type::logical_and_2:
        return f(*reinterpret_cast<const logical_and_2*>(&mdl.dyn));
    case dynamics_type::logical_and_3:
        return f(*reinterpret_cast<const logical_and_3*>(&mdl.dyn));
    case dynamics_type::logical_or_2:
        return f(*reinterpret_cast<const logical_or_2*>(&mdl.dyn));
    case dynamics_type::logical_or_3:
        return f(*reinterpret_cast<const logical_or_3*>(&mdl.dyn));
    case dynamics_type::logical_invert:
        return f(*reinterpret_cast<const logical_invert*>(&mdl.dyn));

    case dynamics_type::hsm_wrapper:
        return f(*reinterpret_cast<const hsm_wrapper*>(&mdl.dyn));
    }

    unreachable();
}

template<typename Function>
constexpr auto dispatch(model& mdl, Function&& f) noexcept
{
    switch (mdl.type) {
    case dynamics_type::qss1_integrator:
        return f(*reinterpret_cast<qss1_integrator*>(&mdl.dyn));
    case dynamics_type::qss1_multiplier:
        return f(*reinterpret_cast<qss1_multiplier*>(&mdl.dyn));
    case dynamics_type::qss1_cross:
        return f(*reinterpret_cast<qss1_cross*>(&mdl.dyn));
    case dynamics_type::qss1_filter:
        return f(*reinterpret_cast<qss1_filter*>(&mdl.dyn));
    case dynamics_type::qss1_power:
        return f(*reinterpret_cast<qss1_power*>(&mdl.dyn));
    case dynamics_type::qss1_square:
        return f(*reinterpret_cast<qss1_square*>(&mdl.dyn));
    case dynamics_type::qss1_sum_2:
        return f(*reinterpret_cast<qss1_sum_2*>(&mdl.dyn));
    case dynamics_type::qss1_sum_3:
        return f(*reinterpret_cast<qss1_sum_3*>(&mdl.dyn));
    case dynamics_type::qss1_sum_4:
        return f(*reinterpret_cast<qss1_sum_4*>(&mdl.dyn));
    case dynamics_type::qss1_wsum_2:
        return f(*reinterpret_cast<qss1_wsum_2*>(&mdl.dyn));
    case dynamics_type::qss1_wsum_3:
        return f(*reinterpret_cast<qss1_wsum_3*>(&mdl.dyn));
    case dynamics_type::qss1_wsum_4:
        return f(*reinterpret_cast<qss1_wsum_4*>(&mdl.dyn));

    case dynamics_type::qss2_integrator:
        return f(*reinterpret_cast<qss2_integrator*>(&mdl.dyn));
    case dynamics_type::qss2_multiplier:
        return f(*reinterpret_cast<qss2_multiplier*>(&mdl.dyn));
    case dynamics_type::qss2_cross:
        return f(*reinterpret_cast<qss2_cross*>(&mdl.dyn));
    case dynamics_type::qss2_filter:
        return f(*reinterpret_cast<qss2_filter*>(&mdl.dyn));
    case dynamics_type::qss2_power:
        return f(*reinterpret_cast<qss2_power*>(&mdl.dyn));
    case dynamics_type::qss2_square:
        return f(*reinterpret_cast<qss2_square*>(&mdl.dyn));
    case dynamics_type::qss2_sum_2:
        return f(*reinterpret_cast<qss2_sum_2*>(&mdl.dyn));
    case dynamics_type::qss2_sum_3:
        return f(*reinterpret_cast<qss2_sum_3*>(&mdl.dyn));
    case dynamics_type::qss2_sum_4:
        return f(*reinterpret_cast<qss2_sum_4*>(&mdl.dyn));
    case dynamics_type::qss2_wsum_2:
        return f(*reinterpret_cast<qss2_wsum_2*>(&mdl.dyn));
    case dynamics_type::qss2_wsum_3:
        return f(*reinterpret_cast<qss2_wsum_3*>(&mdl.dyn));
    case dynamics_type::qss2_wsum_4:
        return f(*reinterpret_cast<qss2_wsum_4*>(&mdl.dyn));

    case dynamics_type::qss3_integrator:
        return f(*reinterpret_cast<qss3_integrator*>(&mdl.dyn));
    case dynamics_type::qss3_multiplier:
        return f(*reinterpret_cast<qss3_multiplier*>(&mdl.dyn));
    case dynamics_type::qss3_cross:
        return f(*reinterpret_cast<qss3_cross*>(&mdl.dyn));
    case dynamics_type::qss3_filter:
        return f(*reinterpret_cast<qss3_filter*>(&mdl.dyn));
    case dynamics_type::qss3_power:
        return f(*reinterpret_cast<qss3_power*>(&mdl.dyn));
    case dynamics_type::qss3_square:
        return f(*reinterpret_cast<qss3_square*>(&mdl.dyn));
    case dynamics_type::qss3_sum_2:
        return f(*reinterpret_cast<qss3_sum_2*>(&mdl.dyn));
    case dynamics_type::qss3_sum_3:
        return f(*reinterpret_cast<qss3_sum_3*>(&mdl.dyn));
    case dynamics_type::qss3_sum_4:
        return f(*reinterpret_cast<qss3_sum_4*>(&mdl.dyn));
    case dynamics_type::qss3_wsum_2:
        return f(*reinterpret_cast<qss3_wsum_2*>(&mdl.dyn));
    case dynamics_type::qss3_wsum_3:
        return f(*reinterpret_cast<qss3_wsum_3*>(&mdl.dyn));
    case dynamics_type::qss3_wsum_4:
        return f(*reinterpret_cast<qss3_wsum_4*>(&mdl.dyn));

    case dynamics_type::counter:
        return f(*reinterpret_cast<counter*>(&mdl.dyn));
    case dynamics_type::queue:
        return f(*reinterpret_cast<queue*>(&mdl.dyn));
    case dynamics_type::dynamic_queue:
        return f(*reinterpret_cast<dynamic_queue*>(&mdl.dyn));
    case dynamics_type::priority_queue:
        return f(*reinterpret_cast<priority_queue*>(&mdl.dyn));
    case dynamics_type::generator:
        return f(*reinterpret_cast<generator*>(&mdl.dyn));
    case dynamics_type::constant:
        return f(*reinterpret_cast<constant*>(&mdl.dyn));
    case dynamics_type::accumulator_2:
        return f(*reinterpret_cast<accumulator_2*>(&mdl.dyn));
    case dynamics_type::time_func:
        return f(*reinterpret_cast<time_func*>(&mdl.dyn));

    case dynamics_type::logical_and_2:
        return f(*reinterpret_cast<logical_and_2*>(&mdl.dyn));
    case dynamics_type::logical_and_3:
        return f(*reinterpret_cast<logical_and_3*>(&mdl.dyn));
    case dynamics_type::logical_or_2:
        return f(*reinterpret_cast<logical_or_2*>(&mdl.dyn));
    case dynamics_type::logical_or_3:
        return f(*reinterpret_cast<logical_or_3*>(&mdl.dyn));
    case dynamics_type::logical_invert:
        return f(*reinterpret_cast<logical_invert*>(&mdl.dyn));

    case dynamics_type::hsm_wrapper:
        return f(*reinterpret_cast<hsm_wrapper*>(&mdl.dyn));
    }

    unreachable();
}

template<typename Dynamics>
Dynamics& get_dyn(model& mdl) noexcept
{
    debug::ensure(dynamics_typeof<Dynamics>() == mdl.type);

    return *reinterpret_cast<Dynamics*>(&mdl.dyn);
}

template<typename Dynamics>
const Dynamics& get_dyn(const model& mdl) noexcept
{
    debug::ensure(dynamics_typeof<Dynamics>() == mdl.type);

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

inline result<message_id> get_input_port(model& src, int port_src) noexcept;

inline status get_input_port(model& src, int port_src, message_id*& p) noexcept;
inline result<node_id> get_output_port(model& dst, int port_dst) noexcept;
inline status get_output_port(model& dst, int port_dst, node_id*& p) noexcept;

inline bool is_ports_compatible(const dynamics_type mdl_src,
                                const int           o_port_index,
                                const dynamics_type mdl_dst,
                                const int /*i_port_index*/) noexcept
{
    switch (mdl_src) {
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
    case dynamics_type::counter:
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
    case dynamics_type::generator:
    case dynamics_type::time_func:
    case dynamics_type::hsm_wrapper:
    case dynamics_type::accumulator_2:
        if (any_equal(mdl_dst,
                      dynamics_type::logical_and_2,
                      dynamics_type::logical_and_3,
                      dynamics_type::logical_or_2,
                      dynamics_type::logical_or_3,
                      dynamics_type::logical_invert))
            return false;
        return true;

    case dynamics_type::constant:
        return true;

    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
    case dynamics_type::qss1_cross:
        if (o_port_index == 2) {
            return any_equal(mdl_dst,
                             dynamics_type::counter,
                             dynamics_type::logical_and_2,
                             dynamics_type::logical_and_3,
                             dynamics_type::logical_or_2,
                             dynamics_type::logical_or_3,
                             dynamics_type::logical_invert);
        } else {
            return !any_equal(mdl_dst,
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
        if (any_equal(o_port_index, 1, 2)) {
            return any_equal(mdl_dst,
                             dynamics_type::counter,
                             dynamics_type::logical_and_2,
                             dynamics_type::logical_and_3,
                             dynamics_type::logical_or_2,
                             dynamics_type::logical_or_3,
                             dynamics_type::logical_invert);
        } else {
            return !any_equal(mdl_dst,
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
        if (any_equal(mdl_dst,
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
                                const int    i_port_index) noexcept;

inline status global_connect(simulation& sim,
                             model&      src,
                             int         port_src,
                             model_id    dst,
                             int         port_dst) noexcept;

inline status global_disconnect(simulation& sim,
                                model&      src,
                                int         port_src,
                                model_id    dst,
                                int         port_dst) noexcept;

/*****************************************************************************
 *
 * simulation
 *
 ****************************************************************************/

inline void copy(const model& src, model& dst) noexcept
{
    dst.type   = src.type;
    dst.handle = invalid_heap_handle;

    dispatch(dst, [&src]<typename Dynamics>(Dynamics& dst_dyn) -> void {
        const auto& src_dyn = get_dyn<Dynamics>(src);
        std::construct_at(&dst_dyn, src_dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dst_dyn.x); i != e; ++i)
                dst_dyn.x[i] = undefined<message_id>();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dst_dyn.y); i != e; ++i)
                dst_dyn.y[i] = undefined<node_id>();
    });
}

inline status initialize_source(simulation& sim, source& src) noexcept
{
    return sim.srcs.dispatch(src, source::operation_type::initialize);
}

inline status update_source(simulation& sim, source& src, double& val) noexcept
{
    if (src.is_empty())
        irt_check(sim.srcs.dispatch(src, source::operation_type::update));

    val = src.next();
    return success();
}

inline status finalize_source(simulation& sim, source& src) noexcept
{
    return sim.srcs.dispatch(src, source::operation_type::finalize);
}

//! observer

inline observer::observer(std::string_view name_) noexcept
  : buffer{ 64 }
  , name{ name_ }
  , states{ observer_flags::none }
{}

inline void observer::init(const i32   raw_buffer_size,
                           const i32   linearizer_buffer_size,
                           const float time_step_) noexcept
{
    debug::ensure(time_step > 0.f);
    debug::ensure(raw_buffer_size > 0);
    debug::ensure(linearizer_buffer_size > 0);

    buffer.clear();
    buffer.reserve(raw_buffer_size);
    linearized_buffer.clear();
    linearized_buffer.reserve(linearizer_buffer_size);
    time_step = time_step_;
}

inline void observer::reset() noexcept
{
    buffer.clear();
    linearized_buffer.clear();
    limits = std::make_pair(-std::numeric_limits<real>::infinity(),
                            +std::numeric_limits<real>::infinity());
    states.reset();
}

inline void observer::clear() noexcept
{
    buffer.clear();
    linearized_buffer.clear();
    limits = std::make_pair(-std::numeric_limits<real>::infinity(),
                            +std::numeric_limits<real>::infinity());

    const auto have_data_lost = states[observer_flags::data_lost];
    states.reset();
    if (have_data_lost)
        states.set(observer_flags::data_lost);
}

inline void observer::update(observation_message msg) noexcept
{
    bitflags<observer_flags> new_states(observer_flags::data_available);
    if (states[observer_flags::data_lost])
        new_states.set(observer_flags::data_lost);

    if (states[observer_flags::buffer_full])
        states.set(observer_flags::data_lost);

    if (!buffer.empty() && buffer.tail()->data()[0] == msg[0])
        *(buffer.tail()) = msg;
    else
        buffer.force_enqueue(msg);

    if (states[observer_flags::use_linear_buffer]) {
        if (linearized_buffer.ssize() >= 1) {
            limits.first  = linearized_buffer.front().x;
            limits.second = linearized_buffer.back().y;
        }
    }

    if (buffer.full())
        new_states.set(observer_flags::buffer_full);

    states = new_states;
}

inline void observer::push_back(const observation& vec) noexcept
{
    linearized_buffer.push_tail(vec);
}

inline bool observer::full() const noexcept
{
    return states[observer_flags::buffer_full];
}

inline status send_message(simulation& sim,
                           node_id&    output_port,
                           real        r1,
                           real        r2,
                           real        r3) noexcept
{
    if (auto* lst = sim.nodes.try_to_get(output_port); lst) {
        auto it  = lst->begin();
        auto end = lst->end();

        while (it != end) {
            auto* mdl = sim.models.try_to_get(it->model);
            if (!mdl) {
                it = lst->erase(it);
            } else {
                if (!sim.emitting_output_ports.can_alloc(1))
                    return new_error(simulation::part::messages,
                                     container_full_error{});

                auto& output_message = sim.emitting_output_ports.emplace_back();
                output_message.msg[0] = r1;
                output_message.msg[1] = r2;
                output_message.msg[2] = r3;
                output_message.model  = it->model;
                output_message.port   = it->port_index;

                ++it;
            }
        }
    }

    return success();
}

//! Get an @c hierarchical_state_machine from the ID.
//!
//! @return @c success() or a new error @c simulation::hsms_error{} @c
//! unknown_error{} and e_ulong_id{ ordinal(id).
inline status get_hierarchical_state_machine(simulation&                  sim,
                                             hierarchical_state_machine*& hsm,
                                             const hsm_id id) noexcept
{
    if (hsm = sim.hsms.try_to_get(id); hsm)
        return success();

    return new_error(
      simulation::part::hsms, unknown_error{}, e_ulong_id{ ordinal(id) });
}

//
// template<typename A>
// heap<A>
//

template<typename A>
constexpr heap<A>::heap(memory_resource* mem) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc{ mem }
{}

template<typename A>
constexpr heap<A>::~heap() noexcept
{
    destroy();
}

template<typename A>
constexpr void heap<A>::destroy() noexcept
{
    if (nodes)
        m_alloc.template deallocate<node>(nodes, capacity);

    nodes     = nullptr;
    m_size    = 0;
    max_size  = 0;
    capacity  = 0;
    free_list = invalid_heap_handle;
    root      = invalid_heap_handle;
}

template<typename A>
constexpr void heap<A>::clear() noexcept
{
    m_size    = 0;
    max_size  = 0;
    free_list = invalid_heap_handle;
    root      = invalid_heap_handle;
}

template<typename A>
constexpr bool heap<A>::reserve(std::integral auto new_capacity) noexcept
{
    debug::ensure(
      std::cmp_less(new_capacity, std::numeric_limits<index_type>::max()));

    if (std::cmp_less_equal(new_capacity, capacity))
        return true;

    auto* new_data = m_alloc.template allocate<node>(new_capacity);
    if (!new_data)
        return false;

    std::uninitialized_copy_n(nodes, max_size, new_data);
    if (nodes)
        m_alloc.template deallocate<node>(nodes, capacity);

    nodes    = new_data;
    capacity = static_cast<index_type>(new_capacity);

    return true;
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::insert(time     tn,
                                                   model_id id) noexcept
{
    u32 new_node;

    if (free_list != invalid_heap_handle) {
        new_node  = free_list;
        free_list = nodes[free_list].next;
    } else {
        new_node = max_size++;
    }

    nodes[new_node].tn    = tn;
    nodes[new_node].id    = id;
    nodes[new_node].prev  = invalid_heap_handle;
    nodes[new_node].next  = invalid_heap_handle;
    nodes[new_node].child = invalid_heap_handle;

    insert(new_node);

    return new_node;
}

template<typename A>
constexpr void heap<A>::destroy(handle elem) noexcept
{
    debug::ensure(elem != invalid_heap_handle);

    if (m_size == 0) {
        clear();
    } else {
        nodes[elem].prev  = invalid_heap_handle;
        nodes[elem].child = invalid_heap_handle;
        nodes[elem].id    = static_cast<model_id>(0);
        nodes[elem].next  = free_list;

        free_list = elem;
    }
}

template<typename A>
constexpr void heap<A>::reintegrate(time tn, handle elem) noexcept
{
    debug::ensure(elem != invalid_heap_handle);

    nodes[elem].tn = tn;

    insert(elem);
}

template<typename A>
constexpr void heap<A>::insert(handle elem) noexcept
{
    nodes[elem].prev  = invalid_heap_handle;
    nodes[elem].next  = invalid_heap_handle;
    nodes[elem].child = invalid_heap_handle;

    ++m_size;

    if (root == invalid_heap_handle)
        root = elem;
    else
        root = merge(elem, root);
}

template<typename A>
constexpr void heap<A>::remove(handle elem) noexcept
{
    debug::ensure(elem != invalid_heap_handle);

    if (elem == root) {
        pop();
        return;
    }

    debug::ensure(m_size > 0);

    m_size--;
    detach_subheap(elem);

    if (nodes[elem].prev) { /* Not use pop() before. Use in interactive code */
        elem = merge_subheaps(elem);
        root = merge(root, elem);
    }
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::pop() noexcept
{
    debug::ensure(m_size > 0);

    m_size--;

    auto top = root;

    if (nodes[top].child == invalid_heap_handle)
        root = invalid_heap_handle;
    else
        root = merge_subheaps(top);

    nodes[top].child = invalid_heap_handle;
    nodes[top].next  = invalid_heap_handle;
    nodes[top].prev  = invalid_heap_handle;

    return top;
}

template<typename A>
constexpr void heap<A>::decrease(time tn, handle elem) noexcept
{
    nodes[elem].tn = tn;

    if (nodes[elem].prev == invalid_heap_handle)
        return;

    detach_subheap(elem);
    root = merge(root, elem);
}

template<typename A>
constexpr void heap<A>::increase(time tn, handle elem) noexcept
{
    nodes[elem].tn = tn;

    remove(elem);
    insert(elem);
}

template<typename A>
constexpr time heap<A>::tn(handle elem) noexcept
{
    return nodes[elem].tn;
}

template<typename A>
constexpr unsigned heap<A>::size() const noexcept
{
    return static_cast<unsigned>(m_size);
}

template<typename A>
constexpr int heap<A>::ssize() const noexcept
{
    return static_cast<int>(m_size);
}

template<typename A>
constexpr bool heap<A>::full() const noexcept
{
    return m_size == capacity;
}

template<typename A>
constexpr bool heap<A>::empty() const noexcept
{
    return root == invalid_heap_handle;
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::top() const noexcept
{
    return root;
}

template<typename A>
constexpr void heap<A>::merge(heap& src) noexcept
{
    if (this == &src)
        return;

    if (root == invalid_heap_handle) {
        root = src.root;
        return;
    }

    root = merge(root, src.root);
    m_size += src.m_size;
}

template<typename A>
constexpr const typename heap<A>::node& heap<A>::operator[](
  handle h) const noexcept
{
    return nodes[h];
}

template<typename A>
constexpr typename heap<A>::node& heap<A>::operator[](handle h) noexcept
{
    return nodes[h];
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::merge(handle a, handle b) noexcept
{
    if (nodes[a].tn < nodes[b].tn) {
        if (nodes[a].child != invalid_heap_handle)
            nodes[nodes[a].child].prev = b;

        if (nodes[b].next != invalid_heap_handle)
            nodes[nodes[b].next].prev = a;

        nodes[a].next  = nodes[b].next;
        nodes[b].next  = nodes[a].child;
        nodes[a].child = b;
        nodes[b].prev  = a;

        return a;
    }

    if (nodes[b].child != invalid_heap_handle)
        nodes[nodes[b].child].prev = a;

    if (nodes[a].prev != invalid_heap_handle && nodes[nodes[a].prev].child != a)
        nodes[nodes[a].prev].next = b;

    nodes[b].prev  = nodes[a].prev;
    nodes[a].prev  = b;
    nodes[a].next  = nodes[b].child;
    nodes[b].child = a;

    return b;
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::merge_right(handle a) noexcept
{
    u32 b = invalid_heap_handle;

    for (; a != invalid_heap_handle; a = nodes[b].next) {
        if ((b = nodes[a].next) == invalid_heap_handle)
            return a;

        b = merge(a, b);
    }

    return b;
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::merge_left(handle a) noexcept
{
    u32 b = nodes[a].prev;

    for (; b != invalid_heap_handle; b = nodes[a].prev)
        a = merge(b, a);

    return a;
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::merge_subheaps(handle a) noexcept
{
    nodes[nodes[a].child].prev = invalid_heap_handle;

    const u32 e = merge_right(nodes[a].child);

    return merge_left(e);
}

template<typename A>
constexpr void heap<A>::detach_subheap(handle elem) noexcept
{
    if (nodes[nodes[elem].prev].child == elem)
        nodes[nodes[elem].prev].child = nodes[elem].next;
    else
        nodes[nodes[elem].prev].next = nodes[elem].next;

    if (nodes[elem].next != invalid_heap_handle)
        nodes[nodes[elem].next].prev = nodes[elem].prev;

    nodes[elem].prev = invalid_heap_handle;
    nodes[elem].next = invalid_heap_handle;
}

//
// template<typename A>
// scheduller<A>
//

template<typename A>
constexpr scheduller<A>::scheduller(memory_resource* mem) noexcept
    requires(!std::is_empty_v<A>)
  : m_heap{ mem }
{}

template<typename A>
inline bool scheduller<A>::reserve(std::integral auto new_capacity) noexcept
{
    return m_heap.reserve(new_capacity);
}

template<typename A>
inline void scheduller<A>::clear() noexcept
{
    m_heap.clear();
}

template<typename A>
inline void scheduller<A>::destroy() noexcept
{
    m_heap.destroy();
}

template<typename A>
inline void scheduller<A>::insert(model& mdl, model_id id, time tn) noexcept
{
    debug::ensure(mdl.handle == invalid_heap_handle);

    mdl.handle = m_heap.insert(tn, id);
}

template<typename A>
inline void scheduller<A>::reintegrate(model& mdl, time tn) noexcept
{
    debug::ensure(mdl.handle != invalid_heap_handle);

    m_heap.reintegrate(tn, mdl.handle);
}

template<typename A>
inline void scheduller<A>::erase(model& mdl) noexcept
{
    if (mdl.handle) {
        m_heap.remove(mdl.handle);
        m_heap.destroy(mdl.handle);
        mdl.handle = invalid_heap_handle;
    }
}

template<typename A>
inline void scheduller<A>::update(model& mdl, time tn) noexcept
{
    debug::ensure(mdl.handle != invalid_heap_handle);
    debug::ensure(tn <= mdl.tn);

    if (tn < mdl.tn)
        m_heap.decrease(tn, mdl.handle);
    else if (tn > mdl.tn)
        m_heap.increase(tn, mdl.handle);
}

template<typename A>
inline void scheduller<A>::pop(
  vector<model_id, freelist_allocator>& out) noexcept
{
    time t = tn();

    out.clear();
    out.emplace_back(m_heap[m_heap.pop()].id);

    while (!m_heap.empty() && t == tn())
        out.emplace_back(m_heap[m_heap.pop()].id);
}

template<typename A>
inline time scheduller<A>::tn() const noexcept
{
    return m_heap[m_heap.top()].tn;
}

template<typename A>
inline time scheduller<A>::tn(handle h) const noexcept
{
    return m_heap[h].tn;
}

template<typename A>
inline bool scheduller<A>::empty() const noexcept
{
    return m_heap.empty();
}

template<typename A>
inline unsigned scheduller<A>::size() const noexcept
{
    return m_heap.size();
}

template<typename A>
inline int scheduller<A>::ssize() const noexcept
{
    return static_cast<int>(m_heap.size());
}

//
// simulation
//

inline simulation::simulation(
  const simulation_memory_requirement& init) noexcept
  : simulation::simulation(get_malloc_memory_resource(), init)
{}

inline simulation::simulation(
  memory_resource*                     mem,
  const simulation_memory_requirement& init) noexcept
  : m_alloc(mem)
  , emitting_output_ports(&shared)
  , immediate_models(&shared)
  , immediate_observers(&shared)
  , models(&shared)
  , hsms(&shared)
  , observers(&shared)
  , nodes(&shared)
  , messages(&shared)
  , dated_messages(&shared)
  , sched(&shared)
  , srcs{ .constant_sources{ &external_source_alloc },
          .binary_file_sources{ &external_source_alloc },
          .text_file_sources{ &external_source_alloc },
          .random_sources{ &external_source_alloc } }
{
    debug::ensure(mem);
    debug::ensure(init.valid());

    shared.reset(m_alloc.allocate_bytes(init.simulation_b), init.simulation_b);
    nodes_alloc.reset(m_alloc.allocate_bytes(init.connections_b),
                      init.connections_b);
    dated_messages_alloc.reset(m_alloc.allocate_bytes(init.dated_messages_b),
                               init.dated_messages_b);
    external_source_alloc.reset(m_alloc.allocate_bytes(init.external_source_b),
                                init.external_source_b);

    if (init.model_nb > 0) {
        models.reserve(init.model_nb);
        observers.reserve(init.model_nb);
        nodes.reserve(init.model_nb * 4); // Max 4 output port by models
        messages.reserve(init.model_nb);
        dated_messages.reserve(init.model_nb);

        emitting_output_ports.reserve(init.model_nb);
        immediate_models.reserve(init.model_nb);
        immediate_observers.reserve(init.model_nb);

        sched.reserve(init.model_nb);
    }

    if (init.hsms_nb > 0)
        hsms.reserve(init.hsms_nb);

    if (init.srcs.constant_nb > 0)
        srcs.constant_sources.reserve(init.srcs.constant_nb);

    if (init.srcs.binary_file_nb > 0)
        srcs.binary_file_sources.reserve(init.srcs.binary_file_nb);

    if (init.srcs.text_file_nb > 0)
        srcs.text_file_sources.reserve(init.srcs.text_file_nb);

    if (init.srcs.random_nb > 0)
        srcs.random_sources.reserve(init.srcs.random_nb);
}

inline simulation::~simulation() noexcept
{
    models.destroy();
    observers.destroy();
    nodes.destroy();
    messages.destroy();
    dated_messages.destroy();
    emitting_output_ports.destroy();
    immediate_models.destroy();
    immediate_observers.destroy();
    sched.destroy();
    hsms.destroy();
    srcs.destroy();

    shared.reset();
    nodes_alloc.reset();
    dated_messages_alloc.reset();
    external_source_alloc.reset();

    m_alloc.deallocate_bytes(shared.head(), shared.capacity());
    m_alloc.deallocate_bytes(nodes_alloc.head(), nodes_alloc.capacity());
    m_alloc.deallocate_bytes(dated_messages_alloc.head(),
                             dated_messages_alloc.capacity());
    m_alloc.deallocate_bytes(external_source_alloc.head(),
                             external_source_alloc.capacity());
}

inline model_id simulation::get_id(const model& mdl) const
{
    return models.get_id(mdl);
}

template<typename Dynamics>
model_id simulation::get_id(const Dynamics& dyn) const
{
    return models.get_id(get_model(dyn));
}

inline void simulation::clean() noexcept
{
    sched.clear();

    messages.clear();
    dated_messages.clear();

    emitting_output_ports.clear();
    immediate_models.clear();
    immediate_observers.clear();
}

//! @brief cleanup simulation and destroy all models and connections
inline void simulation::clear() noexcept
{
    clean();

    nodes.clear();
    models.clear();
    observers.clear();
}

inline void simulation::observe(model& mdl, observer& obs) noexcept
{
    mdl.obs_id = observers.get_id(obs);
    obs.model  = models.get_id(mdl);
    obs.type   = mdl.type;
}

inline void simulation::unobserve(model& mdl) noexcept
{
    if (auto* obs = observers.try_to_get(mdl.obs_id); obs) {
        obs->model = undefined<model_id>();
        mdl.obs_id = undefined<observer_id>();
        observers.free(*obs);
    }

    mdl.obs_id = undefined<observer_id>();
}

inline void simulation::deallocate(model_id id) noexcept
{
    auto* mdl = models.try_to_get(id);
    debug::ensure(mdl);

    unobserve(*mdl);

    dispatch(*mdl, [this]<typename Dynamics>(Dynamics& dyn) {
        this->do_deallocate<Dynamics>(dyn);
    });

    sched.erase(*mdl);
    models.free(*mdl);
}

template<typename Dynamics>
void simulation::do_deallocate(Dynamics& dyn) noexcept
{
    if constexpr (has_output_port<Dynamics>) {
        for (auto& elem : dyn.y) {
            nodes.free(elem);
            elem = undefined<node_id>();
        }
    }

    if constexpr (has_input_port<Dynamics>) {
        for (auto& elem : dyn.x) {
            messages.free(elem);
            elem = undefined<message_id>();
        }
    }

    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
        if (auto* machine = hsms.try_to_get(dyn.id); machine)
            hsms.free(dyn.id);
    }

    std::destroy_at(&dyn);
}

inline bool simulation::can_connect(int number) const noexcept
{
    return nodes.can_alloc(number);
}

inline status simulation::connect(model& src,
                                  int    port_src,
                                  model& dst,
                                  int    port_dst) noexcept
{
    if (!is_ports_compatible(src, port_src, dst, port_dst))
        return new_error(simulation::part::nodes, incompatibility_error{});

    const auto dst_id = get_id(dst);

    return global_connect(*this, src, port_src, dst_id, port_dst);
}

inline bool simulation::can_connect(const model& src,
                                    int          port_src,
                                    const model& dst,
                                    int          port_dst) const noexcept
{
    return dispatch(src, [&]<typename Dynamics>(Dynamics& dyn) -> bool {
        if constexpr (has_output_port<Dynamics>) {
            if (auto* lst = nodes.try_to_get(dyn.y[port_src]); lst) {
                for (const auto& elem : *lst)
                    if (elem.model == models.get_id(dst) &&
                        elem.port_index == port_dst)
                        return false;
            }

            return true;
        }

        return false;
    });
}

template<typename DynamicsSrc, typename DynamicsDst>
status simulation::connect(DynamicsSrc& src,
                           int          port_src,
                           DynamicsDst& dst,
                           int          port_dst) noexcept
{
    model&   src_model    = get_model(src);
    model&   dst_model    = get_model(dst);
    model_id model_dst_id = get_id(dst);

    if (!is_ports_compatible(src_model, port_src, dst_model, port_dst))
        return new_error(simulation::part::nodes, incompatibility_error{});

    return global_connect(*this, src_model, port_src, model_dst_id, port_dst);
}

inline status simulation::disconnect(model& src,
                                     int    port_src,
                                     model& dst,
                                     int    port_dst) noexcept
{
    return global_disconnect(*this, src, port_src, get_id(dst), port_dst);
}

inline status simulation::initialize(time t) noexcept
{
    clean();

    irt::model* mdl = nullptr;
    while (models.next(mdl))
        irt_check(make_initialize(*mdl, t));

    irt::observer* obs = nullptr;
    while (observers.next(obs)) {
        obs->reset();

        if (auto* mdl = models.try_to_get(obs->model); mdl) {
            dispatch(*mdl, [mdl, &obs, t]<typename Dynamics>(Dynamics& dyn) {
                if constexpr (has_observation_function<Dynamics>) {
                    obs->update(dyn.observation(t, t - mdl->tl));
                }
            });
        }
    }

    return success();
}

template<typename Dynamics>
status simulation::make_initialize(model& mdl, Dynamics& dyn, time t) noexcept
{
    if constexpr (has_input_port<Dynamics>) {
        for (int i = 0, e = length(dyn.x); i != e; ++i) {
            if (not messages.try_to_get(dyn.x[i]))
                dyn.x[i] = undefined<message_id>();
        }
    }

    if constexpr (has_initialize_function<Dynamics>)
        irt_check(dyn.initialize(*this));

    mdl.tl     = t;
    mdl.tn     = t + dyn.sigma;
    mdl.handle = invalid_heap_handle;

    sched.insert(mdl, models.get_id(mdl), mdl.tn);

    return success();
}

inline status simulation::make_initialize(model& mdl, time t) noexcept
{
    return dispatch(
      mdl, [this, &mdl, t]<typename Dynamics>(Dynamics& dyn) -> status {
          return this->make_initialize(mdl, dyn, t);
      });
}

template<typename Dynamics>
status simulation::make_transition(model& mdl, Dynamics& dyn, time t) noexcept
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

    if (mdl.tn == sched.tn(mdl.handle)) {
        if constexpr (has_lambda_function<Dynamics>)
            if constexpr (has_output_port<Dynamics>)
                irt_check(dyn.lambda(*this));
    }

    if constexpr (has_transition_function<Dynamics>)
        irt_check(dyn.transition(*this, t, t - mdl.tl, mdl.tn - t));

    if constexpr (has_input_port<Dynamics>) {
        for (auto& elem : dyn.x) {
            if (auto* lst = messages.try_to_get(elem); lst)
                lst->clear();
        }
    }

    debug::ensure(mdl.tn >= t);

    mdl.tl = t;
    mdl.tn = t + dyn.sigma;
    if (dyn.sigma != 0 && mdl.tn == t)
        mdl.tn = std::nextafter(t, t + irt::one);

    sched.reintegrate(mdl, mdl.tn);

    return success();
}

inline status simulation::make_transition(model& mdl, time t) noexcept
{
    return dispatch(mdl, [this, &mdl, t]<typename Dynamics>(Dynamics& dyn) {
        return this->make_transition(mdl, dyn, t);
    });
}

template<typename Dynamics>
status simulation::make_finalize(Dynamics& dyn, observer* obs, time t) noexcept
{
    if constexpr (has_observation_function<Dynamics>) {
        if (obs) {
            obs->update(dyn.observation(t, t - get_model(dyn).tl));
        }
    }

    if constexpr (has_finalize_function<Dynamics>) {
        irt_check(dyn.finalize(*this));
    }

    return success();
}

inline status simulation::finalize(time t) noexcept
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

        irt_check(ret);
    }

    return success();
}

inline status simulation::run(time& t) noexcept
{
    immediate_models.clear();
    immediate_observers.clear();

    if (sched.empty()) {
        t = time_domain<time>::infinity;
        return success();
    }

    if (t = sched.tn(); time_domain<time>::is_infinity(t))
        return success();

    sched.pop(immediate_models);

    emitting_output_ports.clear();
    for (const auto id : immediate_models)
        if (auto* mdl = models.try_to_get(id); mdl)
            irt_check(make_transition(*mdl, t));

    for (int i = 0, e = length(emitting_output_ports); i != e; ++i) {
        auto* mdl = models.try_to_get(emitting_output_ports[i].model);
        if (!mdl)
            continue;

        sched.update(*mdl, t);

        if (!messages.can_alloc(1))
            return new_error(simulation::part::messages,
                             container_full_error{});

        auto  port = emitting_output_ports[i].port;
        auto& msg  = emitting_output_ports[i].msg;

        dispatch(*mdl, [this, port, &msg]<typename Dynamics>(Dynamics& dyn) {
            if constexpr (has_input_port<Dynamics>) {
                auto* list = messages.try_to_get(dyn.x[port]);
                if (not list) {
                    auto& new_list = messages.alloc();
                    dyn.x[port]    = messages.get_id(new_list);
                    list           = &new_list;
                }

                list->push_back({ msg[0], msg[1], msg[2] });
            }
        });
    }

    return success();
}

//
// class HSM
//

inline hsm_wrapper::hsm_wrapper() noexcept = default;

inline hsm_wrapper::hsm_wrapper(const hsm_wrapper& other) noexcept
  : id{ other.id }
  , exec{ other.exec }
  , sigma{ other.sigma }
{}

inline status hsm_wrapper::initialize(simulation& sim) noexcept
{
    hierarchical_state_machine* machine = nullptr;
    irt_check(get_hierarchical_state_machine(sim, machine, id));

    irt_check(machine->start(exec));

    sigma = time_domain<time>::infinity;

    return success();
}

inline status hsm_wrapper::transition(simulation& sim,
                                      time /*t*/,
                                      time /*e*/,
                                      time /*r*/) noexcept
{
    hierarchical_state_machine* machine = nullptr;
    irt_check(get_hierarchical_state_machine(sim, machine, id));

    exec.outputs.clear();
    sigma                       = time_domain<time>::infinity;
    const auto old_values_state = exec.values;

    for (int i = 0, e = length(x); i != e; ++i) {
        if (is_defined(x[i]))
            exec.values |= static_cast<u8>(1u << i);

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

    if (old_values_state != exec.values) {
        exec.previous_state = exec.current_state;

        auto ret = machine->dispatch(
          hierarchical_state_machine::event_type::input_changed, exec);

        if (!ret)
            return ret.error();

        if (*ret == true && !exec.outputs.empty())
            sigma = time_domain<time>::zero;
    }

    return success();
}

inline status hsm_wrapper::lambda(simulation& sim) noexcept
{
    hierarchical_state_machine* machine = nullptr;
    irt_check(get_hierarchical_state_machine(sim, machine, id));

    if (exec.previous_state != hierarchical_state_machine::invalid_state_id &&
        !exec.outputs.empty()) {
        for (int i = 0, e = exec.outputs.ssize(); i != e; ++i) {
            const u8  port  = exec.outputs[i].port;
            const i32 value = exec.outputs[i].value;

            debug::ensure(port < exec.outputs.size());

            irt_check(send_message(sim, y[port], to_real(value)));
        }
    }

    return success();
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
    debug::ensure(!models.full());

    auto& mdl  = models.alloc();
    mdl.type   = dynamics_typeof<Dynamics>();
    mdl.handle = invalid_heap_handle;

    std::construct_at(reinterpret_cast<Dynamics*>(&mdl.dyn));
    auto& dyn = get_dyn<Dynamics>(mdl);

    if constexpr (has_input_port<Dynamics>)
        for (int i = 0, e = length(dyn.x); i != e; ++i)
            dyn.x[i] = undefined<message_id>();

    if constexpr (has_output_port<Dynamics>)
        for (int i = 0, e = length(dyn.y); i != e; ++i)
            dyn.y[i] = undefined<node_id>();

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
    debug::ensure(!models.full());

    auto& new_mdl  = models.alloc();
    new_mdl.type   = mdl.type;
    new_mdl.handle = invalid_heap_handle;

    dispatch(new_mdl, [this, &mdl]<typename Dynamics>(Dynamics& dyn) -> void {
        const auto& src_dyn = get_dyn<Dynamics>(mdl);
        std::construct_at(&dyn, src_dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = undefined<message_id>();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = undefined<node_id>();

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
    debug::ensure(!models.full());

    auto& mdl  = models.alloc();
    mdl.type   = type;
    mdl.handle = invalid_heap_handle;

    dispatch(mdl, [this]<typename Dynamics>(Dynamics& dyn) -> void {
        std::construct_at(&dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i] = undefined<message_id>();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = undefined<node_id>();

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            auto& hsm = hsms.alloc();
            auto  id  = hsms.get_id(hsm);
            dyn.id    = id;
        }
    });

    return mdl;
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
              auto* list = sim.nodes.try_to_get(dyn.y[port_src]);

              if (not list) {
                  auto& new_node  = sim.nodes.alloc(&sim.nodes_alloc, 4);
                  dyn.y[port_src] = sim.nodes.get_id(new_node);
                  list            = &new_node;
              }

              for (const auto& elem : *list) {
                  if (elem.model == dst && elem.port_index == port_dst)
                      return new_error(simulation::part::nodes,
                                       already_exist_error{});
              }

              // if (not list->can_alloc(1))
              //     return new_error(simulation::part::nodes,
              //                      container_full_error{});

              list->emplace_back(dst, static_cast<i8>(port_dst));

              return success();
          }

          unreachable();
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
              if (auto* list = sim.nodes.try_to_get(dyn.y[port_src]); list) {
                  for (auto it = list->begin(), end = list->end(); it != end;
                       ++it) {
                      if (it->model == dst && it->port_index == port_dst) {
                          it = list->erase(it);
                          return success();
                      }
                  }
              }
          }

          unreachable();
      });
}

inline result<message_id> get_input_port(model& src, int port_src) noexcept
{
    return dispatch(
      src, [&]<typename Dynamics>(Dynamics& dyn) -> result<message_id> {
          if constexpr (has_input_port<Dynamics>) {
              if (port_src >= 0 && port_src < length(dyn.x)) {
                  return dyn.x[port_src];
              }
          }

          return new_error(simulation::part::nodes, unknown_error{});
      });
}

inline status get_input_port(model& src, int port_src, message_id*& p) noexcept
{
    return dispatch(
      src, [port_src, &p]<typename Dynamics>(Dynamics& dyn) -> status {
          if constexpr (has_input_port<Dynamics>) {
              if (port_src >= 0 && port_src < length(dyn.x)) {
                  p = &dyn.x[port_src];
                  return success();
              }
          }

          return new_error(simulation::part::nodes, unknown_error{});
      });
}

inline result<node_id> get_output_port(model& dst, int port_dst) noexcept
{
    return dispatch(
      dst, [&]<typename Dynamics>(Dynamics& dyn) -> result<node_id> {
          if constexpr (has_output_port<Dynamics>) {
              if (port_dst >= 0 && port_dst < length(dyn.y))
                  return dyn.y[port_dst];
          }

          return new_error(simulation::part::nodes, unknown_error{});
      });
}

inline status get_output_port(model& dst, int port_dst, node_id*& p) noexcept
{
    return dispatch(
      dst, [port_dst, &p]<typename Dynamics>(Dynamics& dyn) -> status {
          if constexpr (has_output_port<Dynamics>) {
              if (port_dst >= 0 && port_dst < length(dyn.y)) {
                  p = &dyn.y[port_dst];
                  return success();
              }
          }

          return new_error(simulation::part::nodes, unknown_error{});
      });
}

/////////////////////////////////////////////////////////////////////
///
/// Model implementation

inline status queue::transition(simulation& sim,
                                time        t,
                                time /*e*/,
                                time /*r*/) noexcept
{
    if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
        while (!ar->empty() and ar->tail()->data()[0] <= t)
            ar->pop_tail();

        auto* lst = sim.messages.try_to_get(x[0]);
        if (lst) {
            for (const auto& msg : *lst) {
                if (!sim.dated_messages.can_alloc(1))
                    return new_error(simulation::part::dated_messages,
                                     container_full_error{});

                ar->push_head(
                  { irt::real(t + default_ta), msg[0], msg[1], msg[2] });
            }
        }

        if (lst and not !lst->empty()) {
            sigma = lst->front()[0] - t;
            sigma = sigma <= time_domain<time>::zero ? time_domain<time>::zero
                                                     : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }
    } else {
        sigma = time_domain<time>::infinity;
    }

    return success();
}

inline status dynamic_queue::transition(simulation& sim,
                                        time        t,
                                        time /*e*/,
                                        time /*r*/) noexcept
{
    if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
        while (not ar->empty() and ar->tail()->data()[0] <= t)
            ar->pop_tail();

        auto* lst = sim.messages.try_to_get(x[0]);

        if (lst) {
            for (const auto& msg : *lst) {
                if (!sim.dated_messages.can_alloc(1))
                    return new_error(simulation::part::dated_messages,
                                     container_full_error{});
                real ta = zero;
                if (stop_on_error) {
                    irt_check(update_source(sim, default_source_ta, ta));
                    ar->push_head(
                      { t + static_cast<real>(ta), msg[0], msg[1], msg[2] });
                } else {
                    if (auto ret = update_source(sim, default_source_ta, ta);
                        !ret)
                        ar->push_head({ t + static_cast<real>(ta),
                                        msg[0],
                                        msg[1],
                                        msg[2] });
                }
            }
        }

        if (lst and not lst->empty()) {
            sigma = lst->front().data()[0] - t;
            sigma = sigma <= time_domain<time>::zero ? time_domain<time>::zero
                                                     : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }
    } else {
        sigma = time_domain<time>::infinity;
    }

    return success();
}

inline status priority_queue::try_to_insert(simulation&    sim,
                                            const time     t,
                                            const message& msg) noexcept
{
    if (not sim.dated_messages.can_alloc(1))
        return new_error(simulation::part::dated_messages,
                         container_full_error{});

    if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
        ar->push_head({ irt::real(t), msg[0], msg[1], msg[2] });
        ar->sort([](auto& l, auto& r) noexcept -> bool {
            return l.data()[0] < r.data()[0];
        });
    }

    return success();
}

inline status priority_queue::transition(simulation& sim,
                                         time        t,
                                         time /*e*/,
                                         time /*r*/) noexcept
{
    if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
        while (not ar->empty() and ar->tail()->data()[0] <= t)
            ar->pop_tail();

        auto* lst = sim.messages.try_to_get(x[0]);

        if (lst) {
            for (const auto& msg : *lst) {
                real value = zero;

                if (stop_on_error) {
                    irt_check(update_source(sim, default_source_ta, value));

                    if (auto ret =
                          try_to_insert(sim, static_cast<real>(value) + t, msg);
                        !ret)
                        return new_error(simulation::part::dated_messages,
                                         container_full_error{});
                } else {
                    if (auto ret = update_source(sim, default_source_ta, value);
                        !ret) {
                        if (auto ret = try_to_insert(
                              sim, static_cast<real>(value) + t, msg);
                            !ret)
                            return new_error(simulation::part::dated_messages,
                                             container_full_error{});
                    }
                }
            }
        }

        if (lst and not lst->empty()) {
            sigma = lst->front()[0] - t;
            sigma = sigma <= time_domain<time>::zero ? time_domain<time>::zero
                                                     : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }
    } else {
        sigma = time_domain<time>::infinity;
    }

    return success();
}

// simulation memory requirement

inline constexpr simulation_memory_requirement::simulation_memory_requirement(
  const std::size_t                   bytes,
  const constrained_value<int, 5, 90> connections,
  const constrained_value<int, 0, 50> dated_messages,
  const constrained_value<int, 0, 90> hsms_model,
  const constrained_value<int, 1, 90> external_source,
  const constrained_value<int, 1, 95> source_client) noexcept
{
    constexpr size_t alg = alignof(std::max_align_t);

    connections_b     = make_divisible_to(bytes * *connections / 100, alg);
    dated_messages_b  = make_divisible_to(bytes * *dated_messages / 100, alg);
    external_source_b = make_divisible_to(bytes * *external_source / 100, alg);
    simulation_b      = make_divisible_to(
      bytes - (connections_b + dated_messages_b + external_source_b), alg);

    debug::ensure(connections_b + dated_messages_b + external_source_b < bytes);
    debug::ensure(simulation_b > 0);
    debug::ensure(connections_b + dated_messages_b + external_source_b +
                    simulation_b <=
                  bytes);

    const auto model_size   = estimate_model();
    const auto max_model_nb = static_cast<int>(simulation_b / model_size);

    hsms_nb  = max_model_nb * *hsms_model / 100;
    model_nb = max_model_nb - hsms_nb;
    srcs =
      external_source_memory_requirement(external_source_b, *source_client);
}

inline constexpr bool simulation_memory_requirement::valid() const noexcept
{
    return model_nb > 0 and hsms_nb >= 0 and srcs.valid();
}

inline constexpr size_t simulation_memory_requirement::estimate_model() noexcept
{
    return sizeof(output_message) + sizeof(model_id) + sizeof(observer_id) +
           sizeof(data_array<model, model_id, freelist_allocator>::
                    internal_value_type) +
           sizeof(data_array<observer, observer_id, freelist_allocator>::
                    internal_value_type) +
           sizeof(data_array<vector<node, freelist_allocator>,
                             node_id,
                             freelist_allocator>::internal_value_type) +
           sizeof(node) * 8 +
           sizeof(data_array<small_vector<message, 8>,
                             message_id,
                             freelist_allocator>::internal_value_type) *
             8 +
           sizeof(data_array<ring_buffer<dated_message, freelist_allocator>,
                             dated_message_id,
                             freelist_allocator>::internal_value_type) +
           sizeof(heap<mr_allocator<freelist_memory_resource>>::node);
}

inline constexpr external_source_memory_requirement::
  external_source_memory_requirement(
    const std::size_t                   bytes,
    const constrained_value<int, 5, 95> source_client_ratio) noexcept
{
    const auto srcs = make_divisible_to(bytes * *source_client_ratio / 100);
    const auto max_client = make_divisible_to(bytes - srcs);
    const auto block_size = sizeof(chunk_type);
    const auto client     = static_cast<int>((max_client / 2) / block_size);

    const auto sources = static_cast<int>(srcs / block_size);
    const auto source  = sources / 4;

    constant_nb            = source;
    text_file_nb           = source;
    binary_file_nb         = source;
    random_nb              = source;
    binary_file_max_client = client;
    random_max_client      = client;
}

inline constexpr bool external_source_memory_requirement::valid() const
{
    return constant_nb >= 0 and text_file_nb >= 0 and binary_file_nb >= 0 and
           random_nb >= 0 and binary_file_max_client >= 0 and
           random_max_client >= 0;
}

inline constexpr size_t external_source_memory_requirement::in_bytes()
  const noexcept
{
    return sizeof(data_array<constant_source,
                             constant_source_id,
                             freelist_allocator>::internal_value_type) *
             constant_nb +

           sizeof(data_array<binary_file_source,
                             binary_file_source_id,
                             freelist_allocator>::internal_value_type) *
             text_file_nb +

           (sizeof(data_array<text_file_source,
                              text_file_source_id,
                              freelist_allocator>::internal_value_type) +
            (sizeof(chunk_type) + sizeof(u64)) * binary_file_max_client) *
             binary_file_nb +

           (sizeof(data_array<random_source,
                              random_source_id,
                              freelist_allocator>::internal_value_type) +
            (sizeof(chunk_type) + sizeof(u64) * 4) * random_max_client) *
             random_nb;
}

} // namespace irt

#endif
