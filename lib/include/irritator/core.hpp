// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2020
#define ORG_VLEPROJECT_IRRITATOR_2020

#include <irritator/container.hpp>
#include <irritator/error.hpp>
#include <irritator/macros.hpp>

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include <cmath>
#include <cstring>

namespace irt {

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
inline constexpr real operator""_r(long double v) noexcept
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

template<std::signed_integral Integer>
constexpr typename std::make_unsigned_t<Integer> to_unsigned(
  Integer value) noexcept
{
    using dest_type = typename std::make_unsigned_t<Integer>;

    debug::ensure(value >= 0);
    return static_cast<dest_type>(value);
}

template<std::unsigned_integral Integer>
constexpr typename std::make_signed_t<Integer> to_signed(Integer value) noexcept
{
    using dest_type = typename std::make_signed_t<Integer>;

    debug::ensure(std::cmp_less(value, std::numeric_limits<dest_type>::max()));
    return static_cast<dest_type>(value);
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
    return (!(begin == end) and !comp(value, *begin)) ? begin : end;
}

/*****************************************************************************
 *
 * Return status of many function
 *
 ****************************************************************************/

template<typename T, typename... Args>
constexpr bool any_equal(const T& s, Args&&... args) noexcept
{
    return ((s == std::forward<Args>(args)) || ... || false);
}

template<class T, class... Rest>
constexpr bool all_same_type() noexcept
{
    return (std::is_same_v<T, Rest> && ...);
}

/** Checks if the given two numbers are relatively equal according to a relative
 * epsilon. */
template<std::floating_point Real>
constexpr bool almost_equal(Real a, Real b, Real relative_epsilon) noexcept
{
    const auto diff    = std::abs(a - b);
    const auto abs_a   = std::abs(a);
    const auto abs_b   = std::abs(b);
    const auto largest = abs_b > abs_a ? abs_b : abs_a;

    return diff <= largest * relative_epsilon;
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

    static bool is_infinity(time t) noexcept
    {
        return std::fpclassify(t) == FP_INFINITE;
    }

    static bool is_zero(time t) noexcept
    {
        return std::fpclassify(t) == FP_ZERO;
    }
};

inline bool is_infinity(const std::floating_point auto x) noexcept
{
    return std::fpclassify(x) == FP_INFINITE;
}

inline bool is_zero(const std::floating_point auto x) noexcept
{
    return std::fpclassify(x) == FP_ZERO;
}

/*****************************************************************************
 *
 * Containers
 *
 ****************************************************************************/

/// @brief A message is a simple array of 3 real numbers.
/// A three real value to store in the worst case a piiecewise parabolic input
/// trajectory of the quantized integrator.
using message = std::array<real, 3>;

/// @brief A dated-message is a simple array of 4 real numbers.
/// A first real to store a date (wakeup date in queue) and a three real value
/// to store in the worst case a piiecewise parabolic input trajectory of the
/// quantized integrator.
using dated_message = std::array<real, 4>;

/// @brief A observation-message is a simple array of 5 real numbers.
/// The first three real value to store in the worst case a piiecewise parabolic
/// input trajectory of the quantized integrator, the 4th stores the current
/// date and the 5th stores the elapsed time since last transition.
using observation_message = std::array<real, 5>;

struct parameter;
struct model;
class simulation;
class hierarchical_state_machine;
class source;

enum class registred_path_id : u32;
enum class dir_path_id : u32;
enum class file_path_id : u32;

enum class hsm_id : u32;
enum class graph_id : u32;
enum class model_id : u64;
enum class dynamics_id : u64;
enum class observer_id : u64;
enum class block_node_id : u64;
enum class message_id : u64;
enum class dated_message_id : u64;
enum class constant_source_id : u32;
enum class binary_file_source_id : u32;
enum class text_file_source_id : u32;
enum class random_source_id : u32;

/*****************************************************************************
 *
 * @c source and @c source_id are data from files or random generators.
 *
 ****************************************************************************/

static constexpr int external_source_chunk_size = 512;
static constexpr int default_max_client_number  = 32;
static constexpr int default_name_string_size   = 32 - 1; // -1 for length;

using name_str = small_string<default_name_string_size>;

using chunk_type = std::array<double, external_source_chunk_size>;

enum class source_type : u8 {
    constant,    /**< Just an easy source to use mode. */
    binary_file, /**< Best solution to reproductible simulation. Each
                    client take a part of the stream (substream). */
    text_file,   /**< How to retreive old position in debug mode? */
    random,      /**< How to retrieve old position in debug mode? */
};

enum class source_operation_type : u8 {
    initialize, /**< Initialize the buffer at simulation init step. */
    update,     /**< Update the buffer when all values are read. */
    restore,    /**< Restore the buffer when debug mode activated. */
    finalize    /**< Clear the buffer at simulation finalize step. */
};

/** The identifier of the external source. */
union source_any_id {
    constexpr source_any_id() noexcept
      : constant_id(undefined<constant_source_id>())
    {}

    constexpr source_any_id(const constant_source_id id) noexcept
      : constant_id(id)
    {}

    constexpr source_any_id(const binary_file_source_id id) noexcept
      : binary_file_id(id)
    {}

    constexpr source_any_id(const text_file_source_id id) noexcept
      : text_file_id(id)
    {}

    constexpr source_any_id(const random_source_id id) noexcept
      : random_id(id)
    {}

    constexpr source_any_id& operator=(const constant_source_id id) noexcept
    {
        constant_id = id;
        return *this;
    }

    constexpr source_any_id& operator=(const binary_file_source_id id) noexcept
    {
        binary_file_id = id;
        return *this;
    }

    constexpr source_any_id& operator=(const text_file_source_id id) noexcept
    {
        text_file_id = id;
        return *this;
    }

    constexpr source_any_id& operator=(const random_source_id id) noexcept
    {
        random_id = id;
        return *this;
    }

    constant_source_id    constant_id;
    binary_file_source_id binary_file_id;
    text_file_source_id   text_file_id;
    random_source_id      random_id;
};

enum class distribution_type : u8 {
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

struct external_source_definition {
    enum class id : u32;

    struct constant_source {
        vector<real> data;
    };

    struct binary_source {
        dir_path_id  dir  = undefined<dir_path_id>();
        file_path_id file = undefined<file_path_id>();
    };

    struct text_source {
        dir_path_id  dir  = undefined<dir_path_id>();
        file_path_id file = undefined<file_path_id>();
    };

    struct random_source {
        std::array<real, 2> reals;
        std::array<i32, 2>  ints;
        distribution_type   type;
    };

    using source_element =
      std::variant<constant_source, binary_source, text_source, random_source>;

    id_data_array<void,
                  id,
                  allocator<new_delete_memory_resource>,
                  source_element,
                  name_str>
      data;

    template<typename T>
    T& emplace(const external_source_definition::id id,
               std::string_view                     name = "") noexcept
    {
        debug::ensure(data.exists(id));

        data.get<name_str>(id) = name;
        return data.get<external_source_definition::source_element>(id)
          .emplace<T>();
    }

    constant_source& alloc_constant_source(std::string_view name = "") noexcept;
    binary_source&   alloc_binary_source(std::string_view name = "") noexcept;
    text_source&     alloc_text_source(std::string_view name = "") noexcept;
    random_source&   alloc_random_source(std::string_view name = "") noexcept;
};

//! Use a buffer with a set of double real to produce external data. This
//! external source can be shared between @c undefined number of  @c source.
class constant_source
{
public:
    static constexpr u32 default_length = 8u; // default length of the buffer.

    name_str   name;
    chunk_type buffer;
    u32        length = default_length;

    constant_source() noexcept = default;
    explicit constant_source(std::span<const real> src) noexcept;

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
    name_str           name;
    vector<chunk_type> buffers; // buffer, size is defined by max_clients
    vector<u64>        offsets; // offset, size is defined by max_clients
    u32                max_clients = 1; // number of source max (must be >= 1).
    u64                max_reals   = 0; // number of real in the file.

    std::filesystem::path file_path;
    std::ifstream         ifs;
    u32                   next_client = 0;
    u64                   next_offset = 0;

    binary_file_source() noexcept = default;
    explicit binary_file_source(const std::filesystem::path& p) noexcept;

    binary_file_source(const binary_file_source& other) noexcept;
    binary_file_source(binary_file_source&& other) noexcept = delete;
    binary_file_source& operator=(const binary_file_source& other) noexcept;
    binary_file_source& operator=(binary_file_source&& other) noexcept = delete;

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
    name_str   name;
    chunk_type buffer;
    u64        offset = 0u;

    std::filesystem::path file_path;
    std::ifstream         ifs;

    text_file_source() noexcept = default;
    explicit text_file_source(const std::filesystem::path& p) noexcept;

    text_file_source(const text_file_source& other) noexcept;
    text_file_source(text_file_source&& other) noexcept = delete;
    text_file_source& operator=(const text_file_source& other) noexcept;
    text_file_source& operator=(text_file_source&& other) noexcept = delete;

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
//! shared between multiple sources. Each client read a @c
//! external_source_chunk_size real of the set.
//!
//! The source::chunk_id[0-5] array is used to store the prng state.
class random_source
{
public:
    name_str name;

    std::array<real, 2> reals; // reals parameters for distribution
    std::array<i32, 2>  ints;  // and integers part.

    distribution_type distribution = distribution_type::uniform_real;

    random_source() noexcept = default;
    explicit random_source(const distribution_type  type,
                           std::span<const real, 2> reals_,
                           std::span<const i32, 2>  ints_) noexcept;

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

/**  @brief Reference external source from a model.
 * A @c source references a external data (files, PRNG, etc.). Model uses the
 * source to get data external to the simulation. */
class source
{
public:
    /** A view on external-source buffers provided by binary or text files,
     * random or constant vectors. */
    std::span<double> buffer;

    /** Stores external data for text, binary and random external sources
     * to enable restore operation in past (for instance position in the
     * text or binary files and seed parameters for random source). */
    std::array<u64, 6> chunk_id{};

    /** Stores random source to enable restore operation
     * in past (for instance parameters of the random distribution). */
    std::array<real, 2> chunk_real{};

    source_any_id id   = undefined<constant_source_id>();
    source_type   type = source_type::constant;

    /** Index of the next double to read the @a buffer or in the @c chunk_real
     */
    u16 index = 0u;

    source() noexcept = default;

    explicit source(const constant_source_id id_) noexcept;
    explicit source(const binary_file_source_id id_) noexcept;
    explicit source(const text_file_source_id id_) noexcept;
    explicit source(const random_source_id id_) noexcept;

    source(const source_type type_, const source_any_id id_) noexcept;
    explicit source(const source& src) noexcept;
    explicit source(source&& src) noexcept;

    source& operator=(const source& src) noexcept;
    source& operator=(source&& src) noexcept;

    /** Reset the buffer and assign a new type/id. */
    void reset(const source_type type, const source_any_id id) noexcept;

    /** Reset the buffer and assign a new type/id from the @a parameter. */
    void reset(const i64 param) noexcept;

    /** Reset the position in the @a buffer. */
    void reset() noexcept;

    /** Clear the source, buffer is released, id, type and index are zero
     * initialized. */
    void clear() noexcept;

    /** Check if the source is empty and required a filling.
     *  @return true if all data in the buffer are read, false otherwise. */
    bool is_empty() const noexcept;

    /** Get the next double in the buffer. Use the @c is_empty() function first
     * before calling @c next() otherwise, the index returns to the initial
     * index.
     * @return The current chunk value or zero if buffer is empty. */
    double next() noexcept;
};

struct external_source_reserve_definition {
    constrained_value<int, 0, INT_MAX> constant_nb{};
    constrained_value<int, 0, INT_MAX> text_file_nb{};
    constrained_value<int, 0, INT_MAX> binary_file_nb{};
    constrained_value<int, 0, INT_MAX> random_nb{};
    constrained_value<int, 8, INT_MAX> binary_file_max_client{};
    constrained_value<int, 8, INT_MAX> random_max_client{};
};

struct simulation_reserve_definition {
    constrained_value<int, 512, INT_MAX>  models{};
    constrained_value<int, 1024, INT_MAX> connections{};
    constrained_value<int, 16, INT_MAX>   hsms{};
    constrained_value<int, 256, INT_MAX>  dated_messages{};
};

class external_source
{
public:
    data_array<constant_source, constant_source_id>       constant_sources;
    data_array<binary_file_source, binary_file_source_id> binary_file_sources;
    data_array<text_file_source, text_file_source_id>     text_file_sources;
    data_array<random_source, random_source_id>           random_sources;
    int binary_file_max_client = 8;
    int random_max_client      = 8;

    /** Build empty data-array. Use the @c realloc function after this
     * constructor to allocate memory. */
    external_source(const external_source_reserve_definition& res =
                      external_source_reserve_definition()) noexcept;

    //! Call `clear()` and release memory.
    void destroy() noexcept;

    u64 seed[2] = { 0xdeadbeef12345678U, 0xdeadbeef12345678U };

    //! Call the @c init function for all sources (@c constant_source, @c
    //! binary_file_source etc.).
    status prepare() noexcept;

    //! Call the @c finalize function for all sources (@c constant_source,
    //! @c binary_file_source etc.). Usefull to close opened files.
    void finalize() noexcept;

    status import_from(const external_source& srcs);

    status dispatch(source& src, const source_operation_type op) noexcept;

    //! Call the @c data_array<T, Id>::clear() function for all sources.
    void clear() noexcept;
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

enum class dynamics_type : u8 {
    qss1_integrator,
    qss1_multiplier,
    qss1_cross,
    qss1_flipflop,
    qss1_filter,
    qss1_power,
    qss1_square,
    qss1_sum_2,
    qss1_sum_3,
    qss1_sum_4,
    qss1_wsum_2,
    qss1_wsum_3,
    qss1_wsum_4,
    qss1_inverse,
    qss1_integer,
    qss1_compare,
    qss1_gain,
    qss1_sin,
    qss1_cos,
    qss1_log,
    qss1_exp,
    qss2_integrator,
    qss2_multiplier,
    qss2_cross,
    qss2_flipflop,
    qss2_filter,
    qss2_power,
    qss2_square,
    qss2_sum_2,
    qss2_sum_3,
    qss2_sum_4,
    qss2_wsum_2,
    qss2_wsum_3,
    qss2_wsum_4,
    qss2_inverse,
    qss2_integer,
    qss2_compare,
    qss2_gain,
    qss2_sin,
    qss2_cos,
    qss2_log,
    qss2_exp,
    qss3_integrator,
    qss3_multiplier,
    qss3_cross,
    qss3_flipflop,
    qss3_filter,
    qss3_power,
    qss3_square,
    qss3_sum_2,
    qss3_sum_3,
    qss3_sum_4,
    qss3_wsum_2,
    qss3_wsum_3,
    qss3_wsum_4,
    qss3_inverse,
    qss3_integer,
    qss3_compare,
    qss3_gain,
    qss3_sin,
    qss3_cos,
    qss3_log,
    qss3_exp,
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

/**
   Stores dynamics model parameters for each @c irt::dynamics.

   Two vertors of real and integer are available to initialize each type of
   @c irt::dynamics.
 */
struct parameter {
    parameter() noexcept = default;

    //! Import values from the model @c mdl according to the underlying @c
    //! irt::dynamics_type.
    explicit parameter(const model& mdl) noexcept;

    //! Initialize values from the default dynamics type.
    explicit parameter(const dynamics_type type) noexcept;

    //! Copy data from the vectors to the simulation model.
    void copy_to(model& mdl) const noexcept;

    //! Copy data from model to the vectors of this parameter.
    void copy_from(const model& mdl) noexcept;

    //! Initialize data from dynamics type default values.
    void init_from(const dynamics_type type) noexcept;

    //! Assign @c 0 to reals and integers arrays.
    parameter& clear() noexcept;

    parameter& set_constant(real value, real offset) noexcept;
    parameter& set_cross(real threshold,
                         real up_value   = one,
                         real down_value = one) noexcept;
    parameter& set_filter(real lower_bound, real upper_bound) noexcept;
    parameter& set_compare(real equal, real not_equal) noexcept;
    parameter& set_gain(real k) noexcept;

    parameter& set_power(real expoonent) noexcept;
    parameter& set_integrator(real X, real dQ) noexcept;
    parameter& set_time_func(real offset, real timestep, int type) noexcept;
    parameter& set_wsum2(real coeff1, real coeff2) noexcept;
    parameter& set_wsum3(real coeff1, real coeff2, real coeff3) noexcept;
    parameter& set_wsum4(real coeff1,
                         real coeff2,
                         real coeff3,
                         real coeff4) noexcept;
    parameter& set_hsm_wrapper(const u32 id) noexcept;
    parameter& set_hsm_wrapper(i64  i1,
                               i64  i2,
                               real r1,
                               real r2,
                               real timer) noexcept;

    parameter& set_queue(real sigma) noexcept;
    parameter& set_priority_queue(real sigma) noexcept;

    parameter& set_generator_ta(const source_type   type,
                                const source_any_id id) noexcept;
    parameter& set_generator_value(const source_type   type,
                                   const source_any_id id) noexcept;
    parameter& set_dynamic_queue_ta(const source_type   type,
                                    const source_any_id id) noexcept;
    parameter& set_priority_queue_ta(const source_type   type,
                                     const source_any_id id) noexcept;
    parameter& set_hsm_wrapper_value(const source_type   type,
                                     const source_any_id id) noexcept;
    parameter& set_generator_ta(
      const external_source_definition::id id) noexcept;
    parameter& set_generator_value(
      const external_source_definition::id id) noexcept;
    parameter& set_dynamic_queue_ta(
      const external_source_definition::id id) noexcept;
    parameter& set_priority_queue_ta(
      const external_source_definition::id id) noexcept;
    parameter& set_hsm_wrapper_value(
      const external_source_definition::id id) noexcept;

    std::array<real, 4> reals{};
    std::array<i64, 4>  integers{};
};

struct observation {
    observation() noexcept = default;
    observation(const real x_, const real y_) noexcept
      : x{ x_ }
      , y{ y_ }
    {}

    real x{};
    real y{};
};

enum class observer_flags : u8 { buffer_full, data_lost, use_linear_buffer };

//! How to use @c observation_message and interpolate functions.
enum class interpolate_type : u8 {
    none,
    qss1,
    qss2,
    qss3,
};

struct observer {
    static inline constexpr auto default_buffer_size            = 4;
    static inline constexpr auto default_linearized_buffer_size = 256;

    using buffer_size_t            = constrained_value<int, 4, 64>;
    using linearized_buffer_size_t = constrained_value<int, 64, 32768>;

    using value_type = observation;

    /// Allocate raw and liearized buffers with default sizes.
    observer() noexcept;

    /// Change the raw and linearized buffers with specified constrained
    /// sizes and change the time-step.
    void init(const buffer_size_t            buffer_size,
              const linearized_buffer_size_t linearized_buffer_size,
              const float                    ts) noexcept;

    void reset() noexcept;
    void clear() noexcept;
    void update(const observation_message& msg) noexcept;
    bool full() const noexcept;

    shared_buffer<ring_buffer<observation_message>> buffer;
    shared_buffer<ring_buffer<observation>>         linearized_buffer;

    model_id         model     = undefined<model_id>();
    interpolate_type type      = interpolate_type::none;
    float            time_step = 1e-2f;

    bitflags<observer_flags> states;
};

static inline constexpr u32 invalid_heap_handle = 0xffffffff;

/**
   A pairing heap is a type of heap data structure with relatively simple
   implementation and excellent practical amortized performance, introduced
   by Michael Fredman, Robert Sedgewick, Daniel Sleator, and Robert Tarjan
   in 1986.
 */
template<typename A = allocator<new_delete_memory_resource>>
class heap
{
public:
    using this_container = heap<A>;
    using allocator_type = A;
    using index_type     = u32;

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

    explicit constexpr heap(
      constrained_value<int, 512, INT_MAX> pcapacity) noexcept;

    constexpr heap(const heap& other) noexcept;
    constexpr heap(heap&& other) noexcept;
    constexpr heap& operator=(const heap& other) noexcept;
    constexpr heap& operator=(heap&& other) noexcept;

    constexpr ~heap() noexcept;

    /** Clear and free the allocated buffer. */
    constexpr void destroy() noexcept;

    /** Clear the buffer. Root equals null. */
    constexpr void clear() noexcept;

    /**
       Try to allocate a new buffer, copy the old buffer into the new one
       assign the new buffer.

       This function returns false and to nothing on the buffer if the @c
       new_capacity can hold the current capacity and if a new buffer can be
       allocate.
     */
    constexpr bool reserve(std::integral auto new_capacity) noexcept;

    /**
       Allocate a new node into the heap and insert the @c model_id and the
       @c time into the tree.
     */
    constexpr handle alloc(time tn, model_id id) noexcept;

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
    constexpr bool is_in_tree(handle h) const noexcept;

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

/**
   The heap nodes are only allocated and freed using the @c heap::alloc()
   and @c heap::destroy() functions. These functions make a link between @c
   model and
   @c heap::node via the @c heap::node::id and the @c model::handle
   attributes. Make sure you check model and node at the same time. If a
   model is allocate, you must allocate a node before using the scheduler.
   If a model is delete, you must free the node to free memory.

   A node is detached from the heap if the handle `node::child`,
   `node::next` and `node::prev` are null (`irt::invalid_heap_handle`). To
   detach a node, you can use the `heap::pop()` or `heap::remove()`
   functions.
 */
template<typename A = allocator<new_delete_memory_resource>>
class scheduller
{
public:
    using this_container = heap<A>;
    using allocator_type = A;

private:
    heap<A> m_heap;

public:
    using internal_value_type = heap<A>;
    using handle              = u32;

    explicit scheduller(constrained_value<int, 512, INT_MAX> capacity) noexcept;

    bool reserve(std::integral auto new_capacity) noexcept;
    void clear() noexcept;
    void destroy() noexcept;

    /**
       Allocate a new @c heap::node and makes a link between @c heap::node
       and
       @c model (@c heap::node::id equal @c id and @c mdl.handle equal to
       the newly @c handle.

       This function @c abort if the scheduller fail to allocate more nodes.
       Use the @c can_alloc() before use.
     */
    void alloc(model& mdl, model_id id, time tn) noexcept;

    /**
       Unlink the @c model and the @c heap::node if it exists and free
       memory used by the @c heap::node.
     */
    void free(model& mdl) noexcept;

    /**
       Insert or reinsert the node into the @c heap. Use this function only
       and only if the node is detached via the @c remove() or the @c pop()
       functions.
     */
    void reintegrate(model& mdl, time tn) noexcept;

    /**
       Remove the @c node from the tree of the heap. The @c node can be
       reuse with the @c reintegrate() function.
     */
    void remove(model& mdl) noexcept;

    /**
       According to the @c tn parameter, @c update function call the @c
       increase or @c decrease function. You can you this function only if
       the node is in the tree.
     */
    void update(model& mdl, time tn) noexcept;
    void increase(model& mdl, time tn) noexcept;
    void decrease(model& mdl, time tn) noexcept;

    /**
       Remove all @c node with the same @c tn from the tree of the heap. The
       @c node can be reuse with the @c reintegrate() function.
     */
    void pop(vector<model_id>& out) noexcept;

    /** Returns the top event @c time-next in the heap. */
    time tn() const noexcept;

    /** Returns the @c time-next event for handle. */
    time tn(handle h) const noexcept;

    /**
       Return true if the node @c h is a valid heap handle and if `h` is
       equal to `root` or if the node prev, next or child handle are not
       null.
     */
    bool is_in_tree(handle h) const noexcept;

    bool empty() const noexcept;

    inline unsigned size() const noexcept;
    int             ssize() const noexcept;
};

/// Stores two simulation time values, the begin `]-oo, +oo[` and the end
/// value
/// `]begin, +oo]`. This function take care to keep @c begin less than @c
/// end value.
class time_limit
{
private:
    time m_begin = 0;
    time m_end   = 100;

public:
    constexpr void set_bound(const double begin_, const double end_) noexcept;
    constexpr void set_duration(const double begin_,
                                const double end_) noexcept;

    /// Restore @c begin and @c end to @c 0 and @c 100 as in default ctor.
    constexpr void clear() noexcept;

    /// @return true if @c value if the value exceeds the @c m_end value.
    constexpr bool expired(const double value) const noexcept;

    constexpr time duration() const noexcept;
    constexpr time begin() const noexcept;
    constexpr time end() const noexcept;
};

enum class output_port_id : u64;

struct node {
    constexpr node() noexcept = default;
    constexpr node(const model_id id, const i8 port) noexcept;

    model_id model      = undefined<model_id>();
    i8       port_index = 0;
};

/** Stores a list of @c node to make connection between models. Each output port
 * of atomic models stores a static list of @c node and a linked list of @c
 * block_node.
 */
struct block_node {
    small_vector<node, 4> nodes;

    block_node_id next = undefined<block_node_id>();
};

struct input_port {
    u32 position = 0; /**< The current read position in the
                                message buffer. */
    u16 size = 0;     /**< The current size of the message
                                buffer. */
    u16 capacity = 0; /**< The capacity of the message list. */

    /** Assign zero to @c position @c size and @c capacity variables. */
    constexpr void reset() noexcept;
};

struct output_port {
    message msg; /**< A single message.Multiple call to @c send_message override
                    previous message. */

    small_vector<node, 4> connections; /**< A list of connected nodes. */

    block_node_id next = undefined<block_node_id>(); /**< The next block
                                                node in the linked
                                                list of output port
                                                connections. */

    /** Call the @c Function for each node from the @c output_port::connection
     * and/or from the @c output_port::next linked list. */
    template<typename Function, typename... Args>
    void for_each(const data_array<model, model_id>&           models,
                  const data_array<block_node, block_node_id>& nodes,
                  Function&&                                   fn,
                  Args&&... args) const noexcept;

    /** Call the @c Function for each node from the @c output_port::connection
     * and/or from the @c output_port::next linked list. All nodes without valid
     * model are remove, all @c block_node empty are remove and if @c
     * output_port::connection have free space, we merge nodes from @c
     * output_port::next linked list. */
    template<typename Function, typename... Args>
    void for_each(data_array<model, model_id>&           models,
                  data_array<block_node, block_node_id>& nodes,
                  Function&&                             fn,
                  Args&&... args) noexcept;
};

class simulation
{
public:
    vector<model_id>       immediate_models;
    vector<observer_id>    immediate_observers;
    vector<output_port_id> active_output_ports;
    vector<message>        message_buffer;
    vector<parameter>      parameters;

    data_array<model, model_id>                              models;
    data_array<hierarchical_state_machine, hsm_id>           hsms;
    data_array<observer, observer_id>                        observers;
    data_array<block_node, block_node_id>                    nodes;
    data_array<output_port, output_port_id>                  output_ports;
    data_array<ring_buffer<dated_message>, dated_message_id> dated_messages;

    scheduller<allocator<new_delete_memory_resource>> sched;

    external_source srcs;

    time_limit limits;

private:
    time t = time_domain<time>::infinity;

    /**
     * The latest not infinity simulation time. At begin of the simulation,
     * @c last_valid_t equals the begin date. During simulation @c
     * last_valid_t stores the latest valid simulation time (neither
     * infinity.
     */
    time last_valid_t = 0.0;

public:
    time last_time() const noexcept { return last_valid_t; }
    time current_time() const noexcept { return t; }
    void current_time(const time new_t) noexcept
    {
        if (limits.begin() <= new_t and new_t < limits.end())
            t = new_t;
    }

    model_id get_id(const model& mdl) const;

    template<typename Dynamics>
    model_id get_id(const Dynamics& dyn) const;

public:
    //! Use the default malloc memory resource to allocate all memory need
    //! by sub-containers.
    simulation(const simulation_reserve_definition& res =
                 simulation_reserve_definition(),
               const external_source_reserve_definition& psrcs =
                 external_source_reserve_definition()) noexcept;

    /** Call the @C destroy function to free allocated memory */
    ~simulation() noexcept;

    /** Grow models dependant data-array and vectors according to the factor
     * Num Denum.
     * @return true if success.
     */
    template<int Num, int Denum>
    bool grow_models() noexcept;

    /** Grow models dependant data-array and vectors according to @a
     * capacity.
     * @return true if success.
     */
    bool grow_models_to(std::integral auto capacity) noexcept;

    /** Grow connections dependant data-array and vectors according to the
     * factor Num Denum.
     * @return true if success.
     */
    template<int Num, int Denum>
    bool grow_connections() noexcept;

    /** Grow connections dependant data-array and vectors according to the
     * capacity.
     * @return true if success.
     */
    bool grow_connections_to(std::integral auto capacity) noexcept;

    /** Clear, delete or destroy any buffer allocated in constructor
     * or in @c realloc() function. Use the @c realloc() function to
     * allocate new buffer and use simulation again. */
    void destroy() noexcept;

    bool can_alloc(std::integral auto place) const noexcept;
    bool can_alloc(dynamics_type type, std::integral auto place) const noexcept;

    template<typename Dynamics>
    bool can_alloc_dynamics(std::integral auto place) const noexcept;

    //! @brief cleanup simulation object
    //!
    //! Clean scheduler and input/output port from message.
    void clean() noexcept;

    //! @brief cleanup simulation and destroy all models and
    //! connections
    void clear() noexcept;

    //! @brief This function allocates dynamics and models.
    template<typename Dynamics>
    Dynamics& alloc() noexcept;

    //! @brief This function allocates dynamics and models.
    model& clone(const model& mdl) noexcept;

    //! @brief This function allocates dynamics and models.
    model& alloc(dynamics_type type) noexcept;

    void observe(model& mdl, observer& obs) const noexcept;

    void unobserve(model& mdl) noexcept;

    void deallocate(model_id id) noexcept;

    template<typename Dynamics>
    void do_deallocate(Dynamics& dyn) noexcept;

    bool can_connect(int number) const noexcept;

    status connect(model&       src,
                   int          port_src,
                   const model& dst,
                   int          port_dst) noexcept;

    bool can_connect(const model& src,
                     int          port_src,
                     const model& dst,
                     int          port_dst) const noexcept;
    void disconnect(model& src,
                    int    port_src,
                    model& dst,
                    int    port_dst) noexcept;

    status connect(output_port& port, model_id dst, int port_dst) noexcept;
    status connect(output_port_id& port, model_id dst, int port_dst) noexcept;

    template<typename Function, typename... Args>
    void for_each(output_port& port, Function&& fn, Args&&... args) noexcept
    {
        for (const auto& elem : port.connections)
            if (auto* mdl = models.try_to_get(elem.model))
                std::invoke(std::forward<Function>(fn),
                            *mdl,
                            elem.port_index,
                            std::forward<Args>(args)...);

        block_node* prev = nullptr;
        for (auto* block = nodes.try_to_get(port.next); block;
             block       = nodes.try_to_get(block->next)) {

            for (auto it = block->nodes.begin(); it != block->nodes.end();) {
                if (auto* mdl = models.try_to_get(it->model)) {
                    std::invoke(std::forward<Function>(fn),
                                *mdl,
                                it->port_index,
                                std::forward<Args>(args)...);
                    ++it;
                } else {
                    block->nodes.swap_pop_back(it);
                }

                if (block->nodes.empty()) {
                    if (prev != nullptr) {
                        prev->next = block->next;
                        nodes.free(*block);
                        block = prev;
                    } else {
                        if (auto* next_block = nodes.try_to_get(block->next)) {
                            block->nodes = next_block->nodes;
                            block->next  = next_block->next;
                            nodes.free(*next_block);
                        } else {
                            nodes.free(*block);
                            port.next = undefined<block_node_id>();
                            break;
                        }
                    }
                } else {
                    prev = block;
                }
            }
        }
    }

    template<typename Function, typename... Args>
    void for_each(const output_port_id port,
                  Function&&           fn,
                  Args&&... args) noexcept
    {
        if (auto* y = output_ports.try_to_get(port))
            for_each(
              *y, std::forward<Function>(fn), std::forward<Args>(args)...);
    }

    template<typename Function, typename... Args>
    void for_each(const output_port_id& port,
                  Function&&            fn,
                  Args&&... args) const noexcept
    {
        if (auto* y = output_ports.try_to_get(port)) {
            for (const auto& elem : y->connections)
                if (auto* mdl = models.try_to_get(elem.model))
                    std::invoke(std::forward<Function>(fn),
                                *mdl,
                                elem.port_index,
                                std::forward<Args>(args)...);

            for (const auto* block = nodes.try_to_get(y->next); block;
                 block             = nodes.try_to_get(block->next))
                for (const auto& elem : block->nodes)
                    if (auto* mdl = models.try_to_get(elem.model))
                        std::invoke(std::forward<Function>(fn),
                                    *mdl,
                                    elem.port_index,
                                    std::forward<Args>(args)...);
        }
    }

    template<typename DynamicsSrc, typename DynamicsDst>
    status connect_dynamics(DynamicsSrc& src,
                            int          port_src,
                            DynamicsDst& dst,
                            int          port_dst) noexcept;

    /** Call the initialize member function for each model of the
     * simulation an prepare the simulation class to call the `run`
     * function. */
    status initialize() noexcept;

    status run() noexcept;

    template<typename Fn, typename... Args>
    status run_with_cb(Fn&& fn, Args&&... args) noexcept;

    template<typename Dynamics>
    status make_initialize(model& mdl, Dynamics& dyn, time t) noexcept;

    status make_initialize(model& mdl, time t) noexcept;

    template<typename Dynamics>
    status make_transition(model& mdl, Dynamics& dyn, time t) noexcept;

    status make_transition(model& mdl, time t) noexcept;

    template<typename Dynamics>
    status make_finalize(Dynamics& dyn, observer* obs, time t) noexcept;

    bool current_time_expired() const noexcept { return limits.expired(t); }

    /** Finalize and cleanup simulation objects.
     *
     * Clean:
     * - the scheduller nodes
     * - all input/output remaining messages
     * - call the observers' callback to finalize observation
     *
     * This function must be call at the end of the simulation.
     */
    status finalize() noexcept;
};

/////////////////////////////////////////////////////////////////////////////
//
// Some template type-trait to detect function and attribute in
// DEVS model.
//
/////////////////////////////////////////////////////////////////////////////

template<typename T>
concept has_lambda_function = requires(T t, simulation& sim) {
    { t.lambda(sim) } -> std::same_as<status>;
};

template<typename T>
concept has_transition_function =
  requires(T t, simulation& sim, time s, time e, time r) {
      { t.transition(sim, s, e, r) } -> std::same_as<status>;
  };

template<typename T>
concept has_observation_function = requires(T t, time s, time e) {
    { t.observation(s, e) } -> std::same_as<observation_message>;
};

template<typename T>
concept has_initialize_function = requires(T t, simulation& sim) {
    { t.initialize(sim) } -> std::same_as<status>;
};

template<typename T>
concept has_finalize_function = requires(T t, simulation& sim) {
    { t.finalize(sim) } -> std::same_as<status>;
};

template<typename T>
concept has_input_port = requires(T t) {
    { t.x };
};

template<typename T>
concept has_output_port = requires(T t) {
    { t.y };
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

template<std::size_t QssLevel>
constexpr void update([[maybe_unused]] std::span<real, QssLevel> values,
                      [[maybe_unused]] time                      e) noexcept
{
    if constexpr (QssLevel == 2) {
        values[0] += values[1] * e;
    }

    if constexpr (QssLevel == 3) {
        values[0] += values[1] * e + values[2] * e * e;
        values[1] += two * values[2] * e;
    }
}

template<std::size_t QssLevel>
constexpr void update(std::span<real, QssLevel> values,
                      std::span<const real, 3>  msg) noexcept
{
    values[0] = msg[0];

    if constexpr (QssLevel >= 2) {
        values[1] = msg[1];
    }

    if constexpr (QssLevel == 3) {
        values[2] = msg[2];
    }
}

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
                const auto sqrt_d = std::sqrt(d);
                const auto x1     = (-b + sqrt_d) / (two * a);
                const auto x2     = (-b - sqrt_d) / (two * a);

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
            } else if (is_zero(d)) {
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

inline constexpr auto get_message(simulation&       sim,
                                  const input_port& port) noexcept
  -> std::span<message>;

/** Get a message from the list of messages according to the QSS level. If the
 * lists is empty(), this function returns a message with zero values, if the
 * list contains one value, is returns it finally, if the list contains more
 * messages, the message with the maximum value, slope and derivative is
 * returned.
 */
template<size_t QssLevel>
inline auto get_qss_message(std::span<const message> msgs) noexcept
  -> const message&;

inline status send_message(simulation&    sim,
                           output_port_id output_port,
                           real           r1,
                           real           r2 = 0.0,
                           real           r3 = 0.0) noexcept;

/*****************************************************************************
 *
 * Qss1 part
 *
 ****************************************************************************/

template<std::size_t QssLevel>
struct abstract_integrator;

template<>
struct abstract_integrator<1> {
    input_port     x[2] = {};
    output_port_id y[1] = {};

    real dQ;
    real X;
    real q;
    real u;
    time sigma;

    enum port_name : u8 { port_x_dot, port_reset };

    abstract_integrator() = default;

    abstract_integrator(const abstract_integrator& other) noexcept
      : dQ(other.dQ)
      , X(other.X)
      , q(other.q)
      , u(other.u)
      , sigma(other.sigma)
    {}

    /** @c X and @c dQ are initialized from @c parameters. */
    status initialize(simulation& /*sim*/) noexcept
    {
        if (!std::isfinite(X))
            return new_error(simulation_errc::abstract_integrator_x_error);

        if (!(std::isfinite(dQ) && dQ > zero))
            return new_error(simulation_errc::abstract_integrator_dq_error);

        q = std::floor(X / dQ) * dQ;
        u = zero;

        sigma = time_domain<time>::zero;

        return success();
    }

    status external(const time e, const message& msg) noexcept
    {
        X += e * u;
        u = msg[0];

        if (not is_zero(sigma)) {
            if (is_zero(u))
                sigma = time_domain<time>::infinity;
            else if (u > zero)
                sigma = (q + dQ - X) / u;
            else
                sigma = (q - dQ - X) / u;
        }

        return success();
    }

    status reset(const message& msg) noexcept
    {
        X     = msg[0];
        q     = std::floor(X / dQ) * dQ;
        u     = zero;
        sigma = time_domain<time>::zero;

        return success();
    }

    status internal() noexcept
    {
        X += sigma * u;
        q     = X;
        sigma = is_zero(u) ? time_domain<time>::infinity : dQ / std::abs(u);

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time                  e,
                      [[maybe_unused]] time r) noexcept
    {
        const auto lst_x_dot      = get_message(sim, x[port_x_dot]);
        const auto lst_reset      = get_message(sim, x[port_reset]);
        const auto have_x_dot_msg = not lst_x_dot.empty();
        const auto have_reset_msg = not lst_reset.empty();
        const auto have_msg       = have_x_dot_msg or have_reset_msg;

        if (not have_msg)
            return internal();

        if (have_reset_msg)
            return reset(get_qss_message<1>(lst_reset));

        if (have_x_dot_msg)
            return external(e, get_qss_message<1>(lst_x_dot));

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(
          sim, y[0], is_zero(u) ? q : q + dQ * u / std::abs(u));
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
    input_port     x[2] = {};
    output_port_id y[1] = {};

    real dQ;
    real X;
    real u;
    real mu;
    real q;
    real mq;
    time sigma;

    enum port_name : u8 { port_x_dot, port_reset };

    abstract_integrator() = default;

    abstract_integrator(const abstract_integrator& other) noexcept
      : dQ(other.dQ)
      , X(other.X)
      , u(other.u)
      , mu(other.mu)
      , q(other.q)
      , mq(other.mq)
      , sigma(other.sigma)
    {}

    /** @c X and @c dQ are initialized from @c parameters. */
    status initialize(simulation& /*sim*/) noexcept
    {
        if (!std::isfinite(X))
            return new_error(simulation_errc::abstract_integrator_x_error);

        if (!(std::isfinite(dQ) && dQ > zero))
            return new_error(simulation_errc::abstract_integrator_dq_error);

        u  = zero;
        mu = zero;
        q  = X;
        mq = zero;

        sigma = time_domain<time>::zero;

        return success();
    }

    status external(const time e, const message& msg) noexcept
    {
        X += (u * e) + (mu / two) * (e * e);
        u  = msg[0];
        mu = msg[1];

        if (not is_zero(sigma)) {
            q            = q + mq * e;
            const real a = mu / two;
            const real b = u - mq;
            auto       c = X - q + dQ;

            sigma = time_domain<time>::infinity;

            if (is_zero(a)) {
                if (not is_zero(b)) {
                    auto s = -c / b;
                    if (s > zero)
                        sigma = s;

                    c = X - q - dQ;
                    s = -c / b;
                    if ((s > zero) && (s < sigma))
                        sigma = s;
                }
            } else {
                auto sq = std::sqrt(b * b - four * a * c);

                auto s = (-b + sq) / two / a;
                if (s > zero)
                    sigma = s;

                s = (-b - sq) / two / a;
                if ((s > zero) && (s < sigma))
                    sigma = s;

                c  = X - q - dQ;
                sq = std::sqrt(b * b - four * a * c);

                s = (-b + sq) / two / a;
                if ((s > zero) && (s < sigma))
                    sigma = s;

                s = (-b - sq) / two / a;
                if ((s > zero) && (s < sigma))
                    sigma = s;
            }

            if (((X - q) > dQ) || ((q - X) > dQ))
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

        sigma = is_zero(mu) ? time_domain<time>::infinity
                            : std::sqrt(two * dQ / std::abs(mu));

        return success();
    }

    status reset(const message& msg) noexcept
    {
        X     = msg[0];
        u     = zero;
        mu    = zero;
        q     = X;
        mq    = zero;
        sigma = time_domain<time>::zero;

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time                  e,
                      [[maybe_unused]] time r) noexcept
    {
        const auto lst_x_dot      = get_message(sim, x[port_x_dot]);
        const auto lst_reset      = get_message(sim, x[port_reset]);
        const auto have_x_dot_msg = not lst_x_dot.empty();
        const auto have_reset_msg = not lst_reset.empty();
        const auto have_msg       = have_x_dot_msg or have_reset_msg;

        if (not have_msg)
            return internal();

        if (have_reset_msg)
            return reset(get_qss_message<2>(lst_reset));

        if (have_x_dot_msg)
            return external(e, get_qss_message<2>(lst_x_dot));

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
    input_port     x[2] = {};
    output_port_id y[1] = {};

    real dQ;
    real X;
    real u;
    real mu;
    real pu;
    real q;
    real mq;
    real pq;
    time sigma;

    enum port_name : u8 { port_x_dot, port_reset };

    abstract_integrator() = default;

    abstract_integrator(const abstract_integrator& other) noexcept
      : dQ(other.dQ)
      , X(other.X)
      , u(other.u)
      , mu(other.mu)
      , pu(other.pu)
      , q(other.q)
      , mq(other.mq)
      , pq(other.pq)
      , sigma(other.sigma)
    {}

    /** @c X and @c dQ are initialized from @c parameters. */
    status initialize(simulation& /*sim*/) noexcept
    {
        if (!std::isfinite(X))
            return new_error(simulation_errc::abstract_integrator_x_error);

        if (!(std::isfinite(dQ) && dQ > zero))
            return new_error(simulation_errc::abstract_integrator_dq_error);

        u     = zero;
        mu    = zero;
        pu    = zero;
        q     = X;
        mq    = zero;
        pq    = zero;
        sigma = time_domain<time>::zero;

        return success();
    }

    status external(const time e, const message& msg) noexcept
    {
        constexpr real pi_div_3 = 1.0471975511965976;
        const auto     e_2      = e * e;
        const auto     e_3      = e_2 * e;

        X += u * e + (mu * e_2) / two + (pu * e_3) / three;
        u  = msg[0];
        mu = msg[1];
        pu = msg[2];

        if (not is_zero(sigma)) {
            q += mq * e + pq * e_2;
            mq += two * pq * e;
            auto a = mu / two - pq;
            auto b = u - mq;
            auto c = X - q - dQ;

            if (not is_zero(pu)) {
                a       = three * a / pu;
                b       = three * b / pu;
                c       = three * c / pu;
                auto s  = zero;
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
                } else if (is_zero(i2)) {
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
                    } else if (x2 < zero)
                        s = x1;
                    else if (x1 < x2)
                        s = x1;
                    else
                        s = x2;
                } else {
                    auto arg = w * std::sqrt(real(27) / (-v)) / (two * v);
                    arg      = std::acos(arg) / three;
                    auto y1  = two * std::sqrt(-v / three);
                    auto y2  = -y1 * std::cos(pi_div_3 - arg) - a / three;
                    auto y3  = -y1 * std::cos(pi_div_3 + arg) - a / three;
                    y1       = y1 * std::cos(arg) - a / three;
                    if (y1 < zero)
                        s = time_domain<time>::infinity;
                    else if (y3 < zero)
                        s = y1;
                    else if (y2 < zero)
                        s = y3;
                    else
                        s = y2;
                }
                c += real(6) * dQ / pu;
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

                    if (s < sigma || sigma < zero)
                        sigma = s;
                } else if (is_zero(i2)) {
                    auto A = i1;
                    if (A > zero)
                        A = std::pow(A, one / three);
                    else
                        A = -std::pow(std::abs(A), one / three);
                    auto x1 = two * A - a / three;
                    auto x2 = -(A + a / three);
                    if (x1 < zero) {
                        if (x2 < zero)
                            sigma = time_domain<time>::infinity;
                        else
                            sigma = x2;
                    } else if (x2 < zero)
                        sigma = x1;
                    else if (x1 < x2)
                        sigma = x1;
                    else
                        sigma = x2;

                    if (s < sigma)
                        sigma = s;
                } else {
                    auto arg = w * std::sqrt(real(27) / (-v)) / (two * v);
                    arg      = std::acos(arg) / three;
                    auto y1  = two * std::sqrt(-v / three);
                    auto y2  = -y1 * std::cos(pi_div_3 - arg) - a / three;
                    auto y3  = -y1 * std::cos(pi_div_3 + arg) - a / three;
                    y1       = y1 * std::cos(arg) - a / three;
                    if (y1 < zero)
                        sigma = time_domain<time>::infinity;
                    else if (y3 < zero)
                        sigma = y1;
                    else if (y2 < zero)
                        sigma = y3;
                    else
                        sigma = y2;

                    if (s < sigma)
                        sigma = s;
                }
            } else {
                auto s = zero;

                if (not is_zero(a)) {
                    auto x1 = b * b - four * a * c;
                    if (x1 < zero) {
                        s = time_domain<time>::infinity;
                    } else {
                        x1      = std::sqrt(x1);
                        auto x2 = (-b - x1) / two / a;
                        x1      = (-b + x1) / two / a;
                        if (x1 < zero) {
                            if (x2 < zero)
                                s = time_domain<time>::infinity;
                            else
                                s = x2;
                        } else if (x2 < zero)
                            s = x1;
                        else if (x1 < x2)
                            s = x1;
                        else
                            s = x2;
                    }

                    c  = c + two * dQ;
                    x1 = b * b - four * a * c;
                    if (x1 < zero) {
                        sigma = time_domain<time>::infinity;
                    } else {
                        x1      = std::sqrt(x1);
                        auto x2 = (-b - x1) / two / a;
                        x1      = (-b + x1) / two / a;
                        if (x1 < zero) {
                            if (x2 < zero)
                                sigma = time_domain<time>::infinity;
                            else
                                sigma = x2;
                        } else if (x2 < zero)
                            sigma = x1;
                        else if (x1 < x2)
                            sigma = x1;
                        else
                            sigma = x2;
                    }

                    if (s < sigma)
                        sigma = s;
                } else {
                    if (not is_zero(b)) {
                        auto x1 = -c / b;
                        auto x2 = x1 - two * dQ / b;
                        if (x1 < zero)
                            x1 = time_domain<time>::infinity;
                        if (x2 < zero)
                            x2 = time_domain<time>::infinity;
                        if (x1 < x2)
                            sigma = x1;
                        else
                            sigma = x2;
                    }
                }
            }

            if ((std::abs(X - q)) > dQ)
                sigma = time_domain<time>::zero;
        }

        return success();
    }

    status internal() noexcept
    {
        const auto sigma_2 = sigma * sigma;
        const auto sigma_3 = sigma_2 * sigma;

        X += u * sigma + (mu * sigma_2) / two + (pu * sigma_3) / three;
        q = X;
        u += mu * sigma + pu * sigma_2;
        mq = u;
        mu += two * pu * sigma;
        pq = mu / two;

        sigma = is_zero(pu) ? time_domain<time>::infinity
                            : std::pow(std::abs(three * dQ / pu), one / three);

        return success();
    }

    /** Reset the integrator if and only if only if the value change and all
     * state are not zero. */
    status reset(const message& msg) noexcept
    {
        X     = msg[0];
        u     = zero;
        mu    = zero;
        pu    = zero;
        q     = X;
        mq    = zero;
        pq    = zero;
        sigma = time_domain<time>::zero;

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time                  e,
                      [[maybe_unused]] time r) noexcept
    {
        const auto lst_x_dot      = get_message(sim, x[port_x_dot]);
        const auto lst_reset      = get_message(sim, x[port_reset]);
        const auto have_x_dot_msg = not lst_x_dot.empty();
        const auto have_reset_msg = not lst_reset.empty();
        const auto have_msg       = have_x_dot_msg or have_reset_msg;

        if (not have_msg)
            return internal();

        if (have_reset_msg)
            return reset(get_qss_message<3>(lst_reset));

        if (have_x_dot_msg)
            return external(e, get_qss_message<3>(lst_x_dot));
        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        const auto sigma_2 = sigma * sigma;
        const auto sigma_3 = sigma_2 * sigma;

        return send_message(sim,
                            y[0],
                            X + u * sigma + (mu * sigma_2) / two +
                              (pu * sigma_3) / three,
                            u + mu * sigma + pu * sigma_2,
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

template<std::size_t QssLevel>
struct abstract_power {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    real                       n;
    time                       sigma;

    abstract_power() noexcept = default;

    abstract_power(const abstract_power& other) noexcept
      : value(other.value)
      , n(other.n)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        if (not std::isfinite(n))
            return new_error(simulation_errc::abstract_power_n_error);

        value.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1)
            return send_message(sim, y[0], std::pow(value[0], n));

        if constexpr (QssLevel == 2)
            return send_message(sim,
                                y[0],
                                std::pow(value[0], n),
                                n * std::pow(value[0], n - 1) * value[1]);

        if constexpr (QssLevel == 3)
            return send_message(sim,
                                y[0],
                                std::pow(value[0], n),
                                n * std::pow(value[0], n - 1) * value[1],
                                n * (n - 1) * std::pow(value[0], n - 2) *
                                    (value[1] * value[1]) +
                                  n * std::pow(value[0], n - 1) * value[2]);

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            update<QssLevel>(value, get_qss_message<QssLevel>(lst));
            sigma = time_domain<time>::zero;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return success();
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            auto X = std::pow(value[0], n);
            return { t, X };
        }

        if constexpr (QssLevel == 2) {
            auto X = std::pow(value[0], n);
            auto u = n * std::pow(value[0], n - 1) * value[1];

            return qss_observation(X, u, t, e);
        }

        if constexpr (QssLevel == 3) {
            auto X = std::pow(value[0], n);
            auto u = n * std::pow(value[0], n - 1) * value[1];
            auto mu =
              n * (n - 1) * std::pow(value[0], n - 2) * (value[1] * value[1]) +
              n * std::pow(value[0], n - 1) * value[2];

            return qss_observation(X, u, mu, t, e);
        }
    }
};

using qss1_power = abstract_power<1>;
using qss2_power = abstract_power<2>;
using qss3_power = abstract_power<3>;

template<std::size_t QssLevel>
struct abstract_square {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    time                       sigma;

    abstract_square() noexcept = default;

    abstract_square(const abstract_square& other) noexcept
      : value(other.value)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);

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
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            update<QssLevel>(value, get_qss_message<QssLevel>(lst));
            sigma = time_domain<time>::zero;
        } else {
            sigma = time_domain<time>::infinity;
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

template<std::size_t QssLevel, std::size_t PortNumber>
struct abstract_sum {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");
    static_assert(PortNumber > 1, "sum model need at least two input port");

    input_port     x[PortNumber] = {};
    output_port_id y[1]          = {};

    std::array<real, QssLevel * PortNumber> values;
    time                                    sigma;

    abstract_sum() noexcept = default;

    abstract_sum(const abstract_sum& other) noexcept
      : values(other.values)
      , sigma(other.sigma)
    {}

    /// Initialize all values to zero except the first @c PortNumber ones
    /// which are inialized in with parameters.
    status initialize(simulation& /*sim*/) noexcept
    {
        values.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            real value = 0.;
            for (size_t i = 0; i != PortNumber; ++i)
                value += values[i];

            return send_message(sim, y[0], value);
        }

        if constexpr (QssLevel == 2) {
            real value = 0.;
            real slope = 0.;

            for (size_t i = 0; i != PortNumber; ++i) {
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
                if (const auto lst = get_message(sim, x[i]); not lst.empty()) {
                    const auto& msg = get_qss_message<QssLevel>(lst);
                    values[i]       = msg[0];
                    message         = true;
                }
            }
        }

        if constexpr (QssLevel == 2) {
            for (size_t i = 0; i != PortNumber; ++i) {
                const auto lst = get_message(sim, x[i]);

                if (lst.empty()) {
                    values[i] += values[i + PortNumber] * e;
                } else {
                    const auto& msg        = get_qss_message<QssLevel>(lst);
                    values[i]              = msg[0];
                    values[i + PortNumber] = msg[1];
                    message                = true;
                }
            }
        }

        if constexpr (QssLevel == 3) {
            for (size_t i = 0; i != PortNumber; ++i) {
                const auto lst = get_message(sim, x[i]);

                if (lst.empty()) {
                    values[i] += values[i + PortNumber] * e +
                                 values[i + PortNumber + PortNumber] * e * e;
                    values[i + PortNumber] +=
                      2 * values[i + PortNumber + PortNumber] * e;
                } else {
                    const auto& msg        = get_qss_message<QssLevel>(lst);
                    values[i]              = msg[0];
                    values[i + PortNumber] = msg[1];
                    values[i + PortNumber + PortNumber] = msg[2];
                    message                             = true;
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

template<std::size_t QssLevel, std::size_t PortNumber>
struct abstract_wsum {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");
    static_assert(PortNumber > 1, "sum model need at least two input port");

    input_port     x[PortNumber] = {};
    output_port_id y[1]          = {};

    std::array<real, PortNumber>            input_coeffs;
    std::array<real, QssLevel * PortNumber> values;
    time                                    sigma;

    abstract_wsum() noexcept = default;

    abstract_wsum(const abstract_wsum& other) noexcept
      : input_coeffs(other.input_coeffs)
      , values(other.values)
      , sigma(other.sigma)
    {}

    /// Initialize all values to zero except the first @c PortNumber ones
    /// which are inialized with parameters.
    status initialize(simulation& /*sim*/) noexcept
    {
        for (const auto elem : input_coeffs)
            if (not std::isfinite(elem))
                return new_error(simulation_errc::abstract_wsum_coeff_error);

        values.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1) {
            real value = zero;

            for (size_t i = 0; i != PortNumber; ++i)
                value += input_coeffs[i] * values[i];

            return send_message(sim, y[0], value);
        }

        if constexpr (QssLevel == 2) {
            real value = zero;
            real slope = zero;

            for (size_t i = 0; i != PortNumber; ++i) {
                value += input_coeffs[i] * values[i];
                slope += input_coeffs[i] * values[i + PortNumber];
            }

            return send_message(sim, y[0], value, slope);
        }

        if constexpr (QssLevel == 3) {
            real value      = zero;
            real slope      = zero;
            real derivative = zero;

            for (size_t i = 0; i != PortNumber; ++i) {
                value += input_coeffs[i] * values[i];
                slope += input_coeffs[i] * values[i + PortNumber];
                derivative +=
                  input_coeffs[i] * values[i + PortNumber + PortNumber];
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
                const auto lst = get_message(sim, x[i]);
                if (not lst.empty()) {
                    const auto& msg = get_qss_message<QssLevel>(lst);
                    values[i]       = msg[0];
                    message         = true;
                }
            }
        }

        if constexpr (QssLevel == 2) {
            for (size_t i = 0; i != PortNumber; ++i) {
                const auto lst = get_message(sim, x[i]);
                if (lst.empty()) {
                    values[i] += values[i + PortNumber] * e;
                } else {
                    const auto& msg        = get_qss_message<QssLevel>(lst);
                    values[i]              = msg[0];
                    values[i + PortNumber] = msg[1];
                    message                = true;
                }
            }
        }

        if constexpr (QssLevel == 3) {
            for (size_t i = 0; i != PortNumber; ++i) {
                const auto lst = get_message(sim, x[i]);
                if (lst.empty()) {
                    values[i] += values[i + PortNumber] * e +
                                 values[i + PortNumber + PortNumber] * e * e;
                    values[i + PortNumber] +=
                      2 * values[i + PortNumber + PortNumber] * e;
                } else {
                    const auto& msg        = get_qss_message<QssLevel>(lst);
                    values[i]              = msg[0];
                    values[i + PortNumber] = msg[1];
                    values[i + PortNumber + PortNumber] = msg[2];
                    message                             = true;
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
                value += input_coeffs[i] * values[i];

            return { t, value };
        }

        if constexpr (QssLevel == 2) {
            real value = zero;
            real slope = zero;

            for (size_t i = 0; i != PortNumber; ++i) {
                value += input_coeffs[i] * values[i];
                slope += input_coeffs[i] * values[i + PortNumber];
            }

            return qss_observation(value, slope, t, e);
        }

        if constexpr (QssLevel == 3) {
            real value      = zero;
            real slope      = zero;
            real derivative = zero;

            for (size_t i = 0; i != PortNumber; ++i) {
                value += input_coeffs[i] * values[i];
                slope += input_coeffs[i] * values[i + PortNumber];
                derivative +=
                  input_coeffs[i] * values[i + PortNumber + PortNumber];
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

template<std::size_t QssLevel>
struct abstract_inverse {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> values;
    time                       sigma;

    abstract_inverse() noexcept = default;

    abstract_inverse(const abstract_inverse& other) noexcept
      : values(other.values)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        values.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            update<QssLevel>(values, get_qss_message<QssLevel>(lst));
            sigma = time_domain<time>::zero;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if (is_zero(values[0]))
            return new_error(simulation_errc::abstract_log_input_error);

        if constexpr (QssLevel == 1) {
            return send_message(sim, y[0], one / values[0]);
        }

        if constexpr (QssLevel == 2) {
            return send_message(
              sim, y[0], one / values[0], -values[1] / (values[0] * values[0]));
        }

        if constexpr (QssLevel == 3) {
            return send_message(sim,
                                y[0],
                                one / values[0],
                                -values[1] / (values[0] * values[0]),
                                -(values[2] / (values[0] * values[0])) +
                                  ((two * values[1] * values[1]) /
                                   (values[0] * values[0] * values[0])));
        }

        return success();
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            return { t,
                     is_zero(values[0]) ? std::numeric_limits<real>::infinity()
                                        : one / values[0] };
        }

        if constexpr (QssLevel == 2) {
            return is_zero(values[0])
                     ? observation_message{ t,
                                            std::numeric_limits<
                                              real>::infinity(),
                                            std::numeric_limits<
                                              real>::infinity(),
                                            std::numeric_limits<
                                              real>::infinity() }
                     : qss_observation(one / values[0],
                                       -values[1] / (values[0] * values[0]),
                                       t,
                                       e);
        }

        if constexpr (QssLevel == 3) {
            return is_zero(values[0])
                     ? observation_message{ t,
                                            std::numeric_limits<
                                              real>::infinity(),
                                            std::numeric_limits<
                                              real>::infinity(),
                                            std::numeric_limits<
                                              real>::infinity() }
                     : qss_observation(one / values[0],
                                       -values[1] / (values[0] * values[0]),
                                       -(values[2] / (values[0] * values[0])) +
                                         ((two * values[1] * values[1]) /
                                          (values[0] * values[0] * values[0])),
                                       t,
                                       e);
        }
    }
};

using qss1_inverse = abstract_inverse<1>;
using qss2_inverse = abstract_inverse<2>;
using qss3_inverse = abstract_inverse<3>;

template<std::size_t QssLevel>
struct abstract_multiplier {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[2] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel * 2> values;
    time                           sigma;

    abstract_multiplier() noexcept = default;

    abstract_multiplier(const abstract_multiplier& other) noexcept
      : values(other.values)
      , sigma(other.sigma)
    {}

    /// Initialize all values to zero except the first @c PortNumber ones
    /// which are inialized with the parameters.
    status initialize(simulation& /*sim*/) noexcept
    {
        values.fill(0);

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
            return send_message(sim,
                                y[0],
                                values[0] * values[1],
                                values[2 + 0] * values[1] +
                                  values[2 + 1] * values[0],
                                values[0] * values[2 + 2 + 1] +
                                  two * values[2 + 0] * values[2 + 1] +
                                  values[2 + 2 + 0] * values[1]);
        }

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      [[maybe_unused]] time e,
                      time /*r*/) noexcept
    {
        const auto lst_0          = get_message(sim, x[0]);
        const auto lst_1          = get_message(sim, x[1]);
        const auto message_port_0 = not lst_0.empty();
        const auto message_port_1 = not lst_1.empty();
        sigma                     = time_domain<time>::infinity;

        if (message_port_0) {
            const auto& msg = get_qss_message<QssLevel>(lst_0);
            sigma           = time_domain<time>::zero;
            values[0]       = msg[0];

            if constexpr (QssLevel >= 2)
                values[2 + 0] = msg[1];

            if constexpr (QssLevel == 3)
                values[2 + 2 + 0] = msg[2];
        }

        if (message_port_1) {
            const auto& msg = get_qss_message<QssLevel>(lst_1);
            sigma           = time_domain<time>::zero;
            values[1]       = msg[0];

            if constexpr (QssLevel >= 2)
                values[2 + 1] = msg[1];

            if constexpr (QssLevel == 3)
                values[2 + 2 + 1] = msg[2];
        }

        if constexpr (QssLevel == 2) {
            if (not message_port_0)
                values[0] += e * values[2 + 0];

            if (not message_port_1)
                values[1] += e * values[2 + 1];
        }

        if constexpr (QssLevel == 3) {
            if (not message_port_0) {
                values[0] += e * values[2 + 0] + values[2 + 2 + 0] * e * e;
                values[2 + 0] += 2 * values[2 + 2 + 0] * e;
            }

            if (not message_port_1) {
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
            return qss_observation(values[0] * values[1],
                                   values[2 + 0] * values[1] +
                                     values[2 + 1] * values[0],
                                   values[0] * values[2 + 2 + 1] +
                                     two * values[2 + 0] * values[2 + 1] +
                                     values[2 + 2 + 0] * values[1],
                                   t,
                                   e);
        }
    }
};

using qss1_multiplier = abstract_multiplier<1>;
using qss2_multiplier = abstract_multiplier<2>;
using qss3_multiplier = abstract_multiplier<3>;

template<typename std::size_t QssLevel>
struct abstract_integer {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    time                       sigma   = time_domain<time>::infinity;
    real                       upper   = std::numeric_limits<real>::infinity();
    real                       lower   = -std::numeric_limits<real>::infinity();
    real                       to_send = zero;
    real last_send_value               = std::numeric_limits<real>::infinity();

    bool reach_upper = false;
    bool reach_lower = false;

    abstract_integer() noexcept = default;

    abstract_integer(const abstract_integer& other) noexcept
      : value(other.value)
      , sigma(other.sigma)
      , upper(other.upper)
      , lower(other.lower)
      , to_send(other.to_send)
      , last_send_value(other.last_send_value)
      , reach_upper(other.reach_upper)
      , reach_lower(other.reach_lower)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);

        sigma           = time_domain<time>::infinity;
        upper           = std::numeric_limits<real>::infinity();
        lower           = -std::numeric_limits<real>::infinity();
        reach_upper     = false;
        reach_lower     = false;
        to_send         = zero;
        last_send_value = std::numeric_limits<real>::infinity();

        return success();
    }

    void compute_next_cross(const real val) noexcept
    {
        if (val < 0) {
            upper = std::trunc(val);
            lower = upper - 1.0;
        } else {
            lower = std::trunc(val);
            upper = lower + 1.0;
        }
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        const auto lst            = get_message(sim, x[0]);
        const auto have_msg       = not lst.empty();
        auto       external_cross = false;

        if (not have_msg) {
            last_send_value = to_send;

            if constexpr (QssLevel == 2)
                value[0] += value[1] * e;
            if constexpr (QssLevel == 3) {
                value[0] += value[1] * e + value[2] * e * e;
                value[1] += two * value[2] * e;
            }
        } else {
            const auto& msg = get_qss_message<QssLevel>(lst);

            if (last_send_value != std::trunc(msg[0])) {
                external_cross = true;
            }

            value[0] = msg[0];
            if constexpr (QssLevel >= 2)
                value[1] = msg[1];
            if constexpr (QssLevel == 3)
                value[2] = msg[2];
        }

        compute_next_cross(value[0]);

        if (external_cross) {
            to_send = value[0];
            sigma   = 0.0;
        } else {
            if constexpr (QssLevel == 1) {
                sigma   = time_domain<time>::infinity;
                to_send = value[0];
            } else if constexpr (QssLevel == 2) {
                sigma   = std::min(compute_wake_up(upper, value[0], value[1]),
                                 compute_wake_up(lower, value[0], value[1]));
                to_send = value[0] + value[1] * sigma;
            } else if constexpr (QssLevel == 3) {
                sigma = std::min(
                  compute_wake_up(upper, value[0], value[1], value[2]),
                  compute_wake_up(lower, value[0], value[1], value[2]));
                to_send =
                  value[0] + value[1] * sigma + value[2] * sigma * sigma;
            }
        }

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim, y[0], std::trunc(to_send));
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1)
            return { t, std::trunc(value[0]) };
        else if constexpr (QssLevel == 2)
            return { t, std::trunc(value[0] + value[1] * e) };
        else if constexpr (QssLevel == 3)
            return { t,
                     std::trunc(value[0] + value[1] * e + value[2] * e * e) };
    }
};

using qss1_integer = abstract_integer<1>;
using qss2_integer = abstract_integer<2>;
using qss3_integer = abstract_integer<3>;

template<typename std::size_t QssLevel>
struct abstract_compare {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[2] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> a;
    std::array<real, QssLevel> b;
    std::array<real, 2>        output; /**< output[0] for is_a_less_b false and
                                        output[1] for is_a_less_b true. */
    time sigma       = time_domain<time>::infinity;
    bool is_a_less_b = false;

    abstract_compare() noexcept = default;

    abstract_compare(const abstract_compare& other) noexcept
      : a(other.a)
      , b(other.b)
      , output(other.output)
      , sigma(other.sigma)
      , is_a_less_b(other.is_a_less_b)
    {}

    /// Initialize all values to zero except the first element in @c a and
    /// @c b and all in @c output which are copy parameter.
    status initialize(simulation& /*sim*/) noexcept
    {
        if (not std::isfinite(output[0]) or not std::isfinite(output[1]))
            return new_error(
              simulation_errc::abstract_compare_output_value_error);

        a.fill(zero);
        b.fill(zero);

        sigma       = time_domain<time>::infinity;
        is_a_less_b = false;

        return success();
    }

    time compute_next_cross() const noexcept
    {
        if constexpr (QssLevel == 2) {
            const auto y = a[1] - b[1];
            const auto x = a[0] - b[0];
            const auto s1 =
              not is_zero(y) ? -x / y : time_domain<time>::infinity;

            return s1 > 0 ? s1 : time_domain<time>::infinity;
        }

        if constexpr (QssLevel == 3) {
            const auto z  = a[2] - b[2];
            const auto y  = a[1] - b[1];
            const auto x  = a[0] - b[0];
            auto       s1 = time_domain<time>::infinity;
            auto       s2 = time_domain<time>::infinity;

            if (is_zero(z)) {
                if (not is_zero(y))
                    s1 = -x / y;
            } else {
                s1 = (-y + std::sqrt(y * y - four * z * x)) / two / z;
                s2 = (-y - std::sqrt(y * y - four * z * x)) / two / z;
            }

            if ((s1 > zero) and ((s1 < s2) or (s2 < zero)))
                return s1;

            if (s2 > zero)
                return s2;
        }

        return time_domain<time>::infinity;
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        const auto lst_a     = get_message(sim, x[0]);
        const auto lst_b     = get_message(sim, x[1]);
        const auto message_a = not lst_a.empty();
        const auto message_b = not lst_b.empty();

        if (not message_a and not message_b) {
            if constexpr (QssLevel == 2) {
                a[0] += a[1] * e;
                b[0] += b[1] * e;
            } else if constexpr (QssLevel == 3) {
                a[0] += a[1] * e + a[2] * e * e;
                a[1] += two * a[2] * e;
                b[0] += b[1] * e + b[2] * e * e;
                b[1] += two * b[2] * e;
            }
        } else {
            if (message_a) {
                const auto& msg = get_qss_message<QssLevel>(lst_a);

                a[0] = msg[0];
                if constexpr (QssLevel >= 2)
                    a[1] = msg[1];
                if constexpr (QssLevel == 3)
                    a[2] = msg[2];
            } else {
                if constexpr (QssLevel == 2) {
                    a[0] += a[1] * e;
                } else if constexpr (QssLevel == 3) {
                    a[0] += a[1] * e + a[2] * e * e;
                    a[1] += two * a[2] * e;
                }
            }

            if (message_b) {
                const auto& msg = get_qss_message<QssLevel>(lst_b);

                b[0] = msg[0];
                if constexpr (QssLevel >= 2)
                    b[1] = msg[1];
                if constexpr (QssLevel == 3)
                    b[2] = msg[2];
            } else {
                if constexpr (QssLevel == 2) {
                    b[0] += b[1] * e;
                } else if constexpr (QssLevel == 3) {
                    b[0] += b[1] * e + b[2] * e * e;
                    b[1] += two * b[2] * e;
                }
            }
        }

        const auto cross = compute_next_cross();
        if (a[0] - b[0] > 0 and is_a_less_b) {
            is_a_less_b = false;
            sigma       = zero;
        } else if (a[0] - b[0] < 0 and not is_a_less_b) {
            is_a_less_b = true;
            sigma       = zero;
        } else {
            sigma = cross;
        }

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        return send_message(sim, y[0], output[is_a_less_b]);
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, output[is_a_less_b] };
    }
};

using qss1_compare = abstract_compare<1>;
using qss2_compare = abstract_compare<2>;
using qss3_compare = abstract_compare<3>;

template<std::size_t QssLevel>
struct abstract_gain {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    real                       k = one;
    time                       sigma;

    abstract_gain() noexcept = default;

    abstract_gain(const abstract_gain& other) noexcept
      : value(other.value)
      , k(other.k)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1)
            return send_message(sim, y[0], k * value[0]);

        if constexpr (QssLevel == 2)
            return send_message(sim, y[0], k * value[0], k * value[1]);

        if constexpr (QssLevel == 3)
            return send_message(
              sim, y[0], k * value[0], k * value[1], k * value[2]);

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            update<QssLevel>(value, get_qss_message<QssLevel>(lst));
            sigma = time_domain<time>::zero;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return success();
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            return { t, k * value[0] };
        }

        if constexpr (QssLevel == 2) {
            const auto X = k * value[0];
            const auto u = k * value[1];

            return qss_observation(X, u, t, e);
        }

        if constexpr (QssLevel == 3) {
            const auto X  = k * value[0];
            const auto u  = k * value[1];
            const auto mu = k * value[2];

            return qss_observation(X, u, mu, t, e);
        }
    }
};

using qss1_gain = abstract_gain<1>;
using qss2_gain = abstract_gain<2>;
using qss3_gain = abstract_gain<3>;

template<std::size_t QssLevel>
struct abstract_log {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    time                       sigma;

    abstract_log() noexcept = default;

    abstract_log(const abstract_log& other) noexcept
      : value(other.value)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if (is_zero(value[0]) or value[0] < 0)
            return new_error(simulation_errc::abstract_log_input_error);

        if constexpr (QssLevel == 1)
            return send_message(sim, y[0], std::log(value[0]));

        if constexpr (QssLevel == 2)
            return send_message(
              sim, y[0], std::log(value[0]), value[1] / value[0]);

        if constexpr (QssLevel == 3)
            return send_message(sim,
                                y[0],
                                std::log(value[0]),
                                value[1] / value[0],
                                -(value[1] * value[1]) / (value[0] * value[0]) +
                                  value[2] / value[0]);

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            update<QssLevel>(value, get_qss_message<QssLevel>(lst));
            sigma = time_domain<time>::zero;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return success();
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            return { t, std::log(value[0]) };
        }

        if constexpr (QssLevel == 2) {
            const auto X = std::log(value[0]);
            const auto u = value[1] / value[0];

            return qss_observation(X, u, t, e);
        }

        if constexpr (QssLevel == 3) {
            const auto X  = std::log(value[0]);
            const auto u  = value[1] / value[0];
            const auto mu = -(value[1] * value[1]) / (value[0] * value[0]) +
                            value[2] / value[0];

            return qss_observation(X, u, mu, t, e);
        }
    }
};

using qss1_log = abstract_log<1>;
using qss2_log = abstract_log<2>;
using qss3_log = abstract_log<3>;

template<std::size_t QssLevel>
struct abstract_exp {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    time                       sigma;

    abstract_exp() noexcept = default;

    abstract_exp(const abstract_exp& other) noexcept
      : value(other.value)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1)
            return send_message(sim, y[0], std::exp(value[0]));

        if constexpr (QssLevel == 2)
            return send_message(
              sim, y[0], std::exp(value[0]), std::exp(value[0]) * value[1]);

        if constexpr (QssLevel == 3)
            return send_message(sim,
                                y[0],
                                std::exp(value[0]),
                                std::exp(value[0]) * value[1],
                                std::exp(value[0]) *
                                  (value[1] * value[1] + value[2]));

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            update<QssLevel>(value, get_qss_message<QssLevel>(lst));
            sigma = time_domain<time>::zero;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return success();
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            return { t, std::exp(value[0]) };
        }

        if constexpr (QssLevel == 2) {
            const auto X = std::exp(value[0]);
            const auto u = std::exp(value[0]) * value[1];

            return qss_observation(X, u, t, e);
        }

        if constexpr (QssLevel == 3) {
            const auto X = std::exp(value[0]);
            const auto u = std::exp(value[0]) * value[1];
            const auto mu =
              std::exp(value[0]) * (value[1] * value[1] + value[2]);

            return qss_observation(X, u, mu, t, e);
        }
    }
};

using qss1_exp = abstract_exp<1>;
using qss2_exp = abstract_exp<2>;
using qss3_exp = abstract_exp<3>;

template<std::size_t QssLevel>
struct abstract_sin {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    time                       sigma;

    abstract_sin() noexcept = default;

    abstract_sin(const abstract_sin& other) noexcept
      : value(other.value)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1)
            return send_message(sim, y[0], std::sin(value[0]));

        if constexpr (QssLevel == 2)
            return send_message(
              sim, y[0], std::sin(value[0]), std::cos(value[0]) * value[1]);

        if constexpr (QssLevel == 3)
            return send_message(sim,
                                y[0],
                                std::sin(value[0]),
                                std::cos(value[0]) * value[1],
                                -std::sin(value[0]) * value[1] * value[1] +
                                  std::cos(value[0]) * value[2]);

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            update<QssLevel>(value, get_qss_message<QssLevel>(lst));
            sigma = time_domain<time>::zero;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return success();
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            return { t, std::sin(value[0]) };
        }

        if constexpr (QssLevel == 2) {
            const auto X = std::sin(value[0]);
            const auto u = std::cos(value[0]) * value[1];

            return qss_observation(X, u, t, e);
        }

        if constexpr (QssLevel == 3) {
            const auto X  = std::sin(value[0]);
            const auto u  = std::cos(value[0]) * value[1];
            const auto mu = -std::sin(value[0]) * value[1] * value[1] +
                            std::cos(value[0]) * value[2];

            return qss_observation(X, u, mu, t, e);
        }
    }
};

using qss1_sin = abstract_sin<1>;
using qss2_sin = abstract_sin<2>;
using qss3_sin = abstract_sin<3>;

template<std::size_t QssLevel>
struct abstract_cos {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    time                       sigma;

    abstract_cos() noexcept = default;

    abstract_cos(const abstract_cos& other) noexcept
      : value(other.value)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status lambda(simulation& sim) noexcept
    {
        if constexpr (QssLevel == 1)
            return send_message(sim, y[0], std::cos(value[0]));

        if constexpr (QssLevel == 2)
            return send_message(
              sim, y[0], std::cos(value[0]), -std::sin(value[0]) * value[1]);

        if constexpr (QssLevel == 3)
            return send_message(sim,
                                y[0],
                                std::cos(value[0]),
                                -std::sin(value[0]) * value[1],
                                -std::cos(value[0]) * value[1] * value[1] -
                                  std::sin(value[0]) * value[2]);

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            update<QssLevel>(value, get_qss_message<QssLevel>(lst));
            sigma = time_domain<time>::zero;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return success();
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1) {
            return { t, std::cos(value[0]) };
        }

        if constexpr (QssLevel == 2) {
            const auto X = std::cos(value[0]);
            const auto u = -std::sin(value[0]) * value[1];

            return qss_observation(X, u, t, e);
        }

        if constexpr (QssLevel == 3) {
            const auto X  = std::cos(value[0]);
            const auto u  = -std::sin(value[0]) * value[1];
            const auto mu = -std::cos(value[0]) * value[1] * value[1] -
                            std::sin(value[0]) * value[2];
            ;

            return qss_observation(X, u, mu, t, e);
        }
    }
};

using qss1_cos = abstract_cos<1>;
using qss2_cos = abstract_cos<2>;
using qss3_cos = abstract_cos<3>;

struct counter {
    input_port x[1] = {};

    i64  number     = 0;
    real last_value = 0;
    time sigma;

    counter() noexcept = default;

    counter(const counter& other) noexcept
      : number(other.number)
      , last_value(other.last_value)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        number     = 0;
        last_value = zero;
        sigma      = time_domain<time>::infinity;

        return success();
    }

    status transition(simulation& sim,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        if (const auto lst = get_message(sim, x[0]); not lst.empty()) {
            number += numeric_cast<i64>(lst.size());
            last_value = get_qss_message<1>(lst)[0];
        }

        return success();
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, static_cast<real>(number) };
    }
};

struct generator {
    enum : u8 { x_value, x_t, x_add_tr, x_mult_tr };

    enum class option : u8 { ta_use_source, value_use_source };

    input_port     x[4] = {};
    output_port_id y[1] = {};

    time sigma;
    real value;

    source           source_ta;
    source           source_value;
    bitflags<option> flags;

    generator() noexcept = default;

    generator(const generator& other) noexcept
      : sigma(other.sigma)
      , value(other.value)
      , source_ta(other.source_ta)
      , source_value(other.source_value)
      , flags(option::ta_use_source, option::value_use_source)
    {}

    /// Before calling this function, the @c sigma member is initialized
    /// with
    /// @c params.reals[0] and the @c value member is initialized with
    /// @c params.reals[1].
    status initialize(simulation& sim) noexcept
    {
        sigma = time_domain<time>::infinity;

        if (flags[option::ta_use_source]) {
            if (initialize_source(sim, source_ta).has_error())
                return new_error(
                  simulation_errc::generator_ta_initialization_error);

            sigma = source_ta.next();
        }

        value = zero;
        if (flags[option::value_use_source]) {
            if (initialize_source(sim, source_value).has_error())
                return new_error(
                  simulation_errc::generator_source_initialization_error);

            value = source_value.next();
        }

        return success();
    }

    status finalize(simulation& sim) noexcept
    {
        if (flags[option::ta_use_source])
            irt_check(finalize_source(sim, source_ta));

        if (flags[option::value_use_source])
            irt_check(finalize_source(sim, source_value));

        return success();
    }

    status transition(simulation& sim, time /*t*/, time /*e*/, time r) noexcept
    {
        auto update_source_by_input_port = false;

        if (const auto lst_value = get_message(sim, x[x_value]);
            not lst_value.empty()) {
            for (const auto& msg : lst_value)
                value = msg[0];

            sigma                       = r;
            update_source_by_input_port = true;
        }

        if (is_zero(r)) {
            if (flags[option::value_use_source] and
                not update_source_by_input_port) {
                if (const auto ret = update_source(sim, source_value, value);
                    ret.has_error())
                    return ret.error();
            }

            if (flags[option::ta_use_source]) {
                if (const auto ret = update_source(sim, source_ta, sigma);
                    ret.has_error())
                    return ret.error();

                if (not std::isfinite(sigma) or std::signbit(sigma))
                    return new_error(simulation_errc::ta_abnormal);
            }
        }

        const auto lst_t = get_message(sim, x[x_t]);
        real       t     = -1.0;
        if (not lst_t.empty())
            for (const auto& msg : lst_t)
                t = std::min(msg[0], t);

        const auto lst_add_tr = get_message(sim, x[x_add_tr]);
        real       add_tr     = time_domain<time>::infinity;
        if (not lst_add_tr.empty())
            for (const auto& msg : lst_add_tr)
                add_tr = std::min(msg[0], add_tr);

        const auto lst_mult_tr = get_message(sim, x[x_mult_tr]);
        real       mult_tr     = zero;
        if (not lst_mult_tr.empty())
            for (const auto& msg : lst_mult_tr)
                mult_tr = std::max(msg[0], mult_tr);

        if (not(lst_t.empty() and lst_add_tr.empty() and lst_mult_tr.empty())) {
            if (t >= zero) {
                sigma = t;
            } else {
                if (std::isfinite(add_tr))
                    sigma = r + add_tr;

                if (std::isnormal(mult_tr))
                    sigma = r * mult_tr;
            }
        }

        if (sigma < 0.0)
            sigma = 0.0;

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
    output_port_id y[1] = {};
    time           sigma;

    enum class init_type : u8 {
        /// A constant value initialized at startup of the simulation. Use
        /// the
        /// @c default_value.
        constant,

        /// The numbers of incoming connections on all input ports of the
        /// component. The @c default_value is filled via the component to
        /// simulation algorithm. Otherwise, the default value is
        /// unmodified.
        incoming_component_all,

        /// The number of outcoming connections on all output ports of the
        /// component. The @c default_value is filled via the component to
        /// simulation algorithm. Otherwise, the default value is
        /// unmodified.
        outcoming_component_all,

        /// The number of incoming connections on the nth input port of the
        /// component. Use the @c port attribute to specify the identifier
        /// of
        /// the port. The @c default_value is filled via the component to
        /// simulation algorithm. Otherwise, the default value is
        /// unmodified.
        incoming_component_n,

        /// The number of incoming connections on the nth output ports of
        /// the
        /// component. Use the @c port attribute to specify the identifier
        /// of
        /// the port. The @c default_value is filled via the component to
        /// simulation algorithm. Otherwise, the default value is
        /// unmodified.
        outcoming_component_n,
    };

    static inline constexpr int init_type_count = 5;

    time      offset = 0.0;
    real      value  = 0.0;
    init_type type   = init_type::constant;
    u64       port   = 0;

    constant() noexcept = default;

    constant(const constant& other) noexcept
      : sigma(other.sigma)
      , offset(other.offset)
      , value(other.value)
      , type(other.type)
      , port(other.port)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        if (not std::isfinite(value))
            return new_error(simulation_errc::constant_value_error);

        if (not std::isfinite(offset) or offset < zero)
            return new_error(simulation_errc::constant_offset_error);

        sigma = offset;

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

template<std::size_t QssLevel>
struct abstract_filter {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[1] = {};
    output_port_id y[3] = {};

    time                       sigma;
    real                       lower_threshold;
    real                       upper_threshold;
    std::array<real, QssLevel> value;

    bool reach_lower_threshold = false;
    bool reach_upper_threshold = false;

    abstract_filter() noexcept = default;

    abstract_filter(const abstract_filter& other) noexcept
      : sigma(other.sigma)
      , lower_threshold(other.lower_threshold)
      , upper_threshold(other.upper_threshold)
      , value(other.value)
      , reach_lower_threshold(other.reach_lower_threshold)
      , reach_upper_threshold(other.reach_upper_threshold)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        if (lower_threshold >= upper_threshold)
            return new_error(
              simulation_errc::abstract_filter_threshold_condition_error);

        reach_lower_threshold = false;
        reach_upper_threshold = false;

        value.fill(zero);

        sigma = time_domain<time>::infinity;

        return success();
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        const auto lst = get_message(sim, x[0]);

        if (lst.empty()) {
            if constexpr (QssLevel == 2)
                value[0] += value[1] * e;
            if constexpr (QssLevel == 3) {
                value[0] += value[1] * e + value[2] * e * e;
                value[1] += two * value[2] * e;
            }
        } else {
            const auto& msg = get_qss_message<QssLevel>(lst);
            value[0]        = msg[0];
            if constexpr (QssLevel >= 2)
                value[1] = msg[1];
            if constexpr (QssLevel == 3)
                value[2] = msg[2];
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

template<typename AbstractLogicalTester, std::size_t PortNumber>
struct abstract_logical {
    input_port     x[PortNumber];
    output_port_id y[1];

    std::array<bool, PortNumber> values;
    time                         sigma = time_domain<time>::infinity;

    bool is_valid      = true;
    bool value_changed = false;

    abstract_logical() noexcept = default;

    status initialize(simulation& /*sim*/) noexcept
    {
        values.fill(false);

        sigma         = time_domain<time>::infinity;
        is_valid      = false;
        value_changed = false;

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

        for (sz i = 0; i < PortNumber; ++i) {
            if (const auto lst = get_message(sim, x[i]); not lst.empty()) {
                if (not is_zero(lst.front()[0])) {
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
    input_port     x[1] = {};
    output_port_id y[1] = {};

    time sigma         = time_domain<time>::infinity;
    bool value         = false;
    bool value_changed = false;

    logical_invert() noexcept = default;

    status initialize(simulation& /*sim*/) noexcept
    {
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
        value_changed  = false;
        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            const auto& msg = lst.front();

            if ((not is_zero(msg[0]) && !value) || (is_zero(msg[0]) && value))
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

/** Hierarchical state machine.
 *
 * @note This implementation have the standard restriction for
 * HSM:
 * 1. You can not call Transition from HSM::event_type::enter and
 * HSM::event_type::exit! These event are provided to execute
 * your construction/destruction.
 * 2. You are not allowed to dispatch an event from within an
 * event dispatch. You should queue events if you want such
 * behavior. This restriction is imposed only to prevent the user
 * from creating extremely complicated to trace state machines
 * (which is what we are trying to avoid).
 */
class hierarchical_state_machine
{
public:
    using state_id = u8;
    struct execution;

    static constexpr const u8 max_number_of_state = 254;
    static constexpr const u8 invalid_state_id    = 255;
    static constexpr const u8 max_constants       = 8;

    struct top_state_error {};
    struct next_state_error {};
    struct empty_value_error {};

    constexpr static int event_type_count     = 5;
    constexpr static int variable_count       = 21;
    constexpr static int action_type_count    = 16;
    constexpr static int condition_type_count = 9;

    enum class option : u8 {
        use_source, /**< HSM can use external data in the action part. */
    };

    enum class event_type : u8 {
        enter,
        exit,
        input_changed, /**< HSM receives an expected input
                          message. */
        internal,      /**< HSM move to the next state if @c check()
                          is valid. */
        wake_up,       /**< HSM receives an end of a timer message with
                          or without input message (priority to timer
                          event). */

    };

    enum class variable : u8 {
        none,
        port_0,
        port_1,
        port_2,
        port_3,
        var_i1,
        var_i2,
        var_r1,
        var_r2,
        var_timer,
        constant_i,
        constant_r,
        hsm_constant_0, /**< Real from the HSM component not HSM
                           wrapper. */
        hsm_constant_1,
        hsm_constant_2,
        hsm_constant_3,
        hsm_constant_4,
        hsm_constant_5,
        hsm_constant_6,
        hsm_constant_7,
        source, /**< A value read from external source. The
                    hsm_t::option::use_value must be defined to
                   receives external data. */
    };

    enum class action_type : u8 {
        none,       //!< do nothing.
        set,        //!< port identifer + a variable.
        unset,      //!< port identifier to clear.
        reset,      //!< all port to reset
        output,     //!< port identifier + a variable.
        affect,     //!< x = y
        plus,       //!< x = x + y
        minus,      //!< x = x - y
        negate,     //!< x = -y
        multiplies, //!< x = x * y
        divides,    //!< x = x / y (if y is nul, x equals infinity)
        modulus,    //!< x = x % y (if y is nul, x equals infinity)
        bit_and,    //!< x = x & y (only if y is integer)
        bit_or,     //!< x = x | y (only if y is integer)
        bit_not,    //!< x = !y (only if y is integer)
        bit_xor     //!< x = x ^ y (only if y is integer)
    };

    enum class condition_type : u8 {
        none,  // No condition (always true)
        port,  // Wait a message on port.
        sigma, // Wait a wakup after a `ta(sigma)`.
        equal_to,
        not_equal_to,
        greater,
        greater_equal,
        less,
        less_equal,
    };

    /**
       Action available when state is processed during enter,
       exit or DEVS condition event. @note Only one action (value
       set/unset, devs output, etc.) by action. To perform more
       action, use several states.
     */
    struct state_action {
        variable    var1 = variable::none;
        variable    var2 = variable::none;
        action_type type = action_type::none;

        union {
            i32   i;
            float f;
        } constant;

        /**
         * Assign the @a action_type @a t to the @a this state_action and
         * set the @a var1, @a var2 and @a constant union to the default @a
         * action_type.
         *
         * @param t The type of action to assign.
         */
        void set_default(const action_type t) noexcept;

        void set_setport(variable v1) noexcept;
        void set_unsetport(variable v1) noexcept;
        void set_reset() noexcept;
        void set_output(variable v1, variable v2) noexcept;
        void set_output(variable v1, i32 i) noexcept;
        void set_output(variable v1, float f) noexcept;

        void set_affect(variable v1, variable v2) noexcept;
        void set_affect(variable v1, i32 i) noexcept;
        void set_affect(variable v1, float f) noexcept;
        void set_plus(variable v1, variable v2) noexcept;
        void set_plus(variable v1, i32 i) noexcept;
        void set_plus(variable v1, float f) noexcept;
        void set_minus(variable v1, variable v2) noexcept;
        void set_minus(variable v1, i32 i) noexcept;
        void set_minus(variable v1, float f) noexcept;
        void set_negate(variable v1, variable v2) noexcept;
        void set_multiplies(variable v1, variable v2) noexcept;
        void set_multiplies(variable v1, i32 i) noexcept;
        void set_multiplies(variable v1, float f) noexcept;
        void set_divides(variable v1, variable v2) noexcept;
        void set_divides(variable v1, i32 i) noexcept;
        void set_divides(variable v1, float f) noexcept;
        void set_modulus(variable v1, variable v2) noexcept;
        void set_modulus(variable v1, i32 i) noexcept;
        void set_modulus(variable v1, float f) noexcept;
        void set_bit_and(variable v1, variable v2) noexcept;
        void set_bit_and(variable v1, i32 i) noexcept;
        void set_bit_or(variable v1, variable v2) noexcept;
        void set_bit_or(variable v1, i32 i) noexcept;
        void set_bit_not(variable v1, variable v2) noexcept;
        void set_bit_not(variable v1, i32 i) noexcept;
        void set_bit_xor(variable v1, variable v2) noexcept;
        void set_bit_xor(variable v1, i32 i) noexcept;

        void clear() noexcept;
    };

    /**
       1. @c value_condition stores the bit for input value
       corresponding to the user request to satisfy the
       condition. @c value_mask stores the bit useful in
       value_condition. If value_mask equal @c 0x0 then, the
       condition is always true. If @c value_mask equals @c 0xff
       the @c value_condition bit are mandatory.
       2. @c condition_state_action stores transition or action
       conditions. Condition can use input port state or
       condition on integer a or b.
     */
    struct condition_action {
        variable       var1 = variable::none;
        variable       var2 = variable::none;
        condition_type type = condition_type::none;

        union {
            i32   i;
            u32   u;
            float f;
        } constant;

        void set(u8 port, u8 mask) noexcept;
        void get(u8& port, u8& mask) const noexcept;

        void set(const std::bitset<4> port, const std::bitset<4> mask) noexcept;
        std::pair<std::bitset<4>, std::bitset<4>> get_bitset() const noexcept;

        void set_timer() noexcept;
        void set_equal_to(variable v1, variable v2) noexcept;
        void set_equal_to(variable v1, i32 i) noexcept;
        void set_equal_to(variable v1, float i) noexcept;
        void set_not_equal_to(variable v1, variable v2) noexcept;
        void set_not_equal_to(variable v1, i32 i) noexcept;
        void set_not_equal_to(variable v1, float i) noexcept;
        void set_greater(variable v1, variable v2) noexcept;
        void set_greater(variable v1, i32 i) noexcept;
        void set_greater(variable v1, float i) noexcept;
        void set_greater_equal(variable v1, variable v2) noexcept;
        void set_greater_equal(variable v1, i32 i) noexcept;
        void set_greater_equal(variable v1, float i) noexcept;
        void set_less(variable v1, variable v2) noexcept;
        void set_less(variable v1, i32 i) noexcept;
        void set_less(variable v1, float i) noexcept;
        void set_less_equal(variable v1, variable v2) noexcept;
        void set_less_equal(variable v1, i32 i) noexcept;
        void set_less_equal(variable v1, float i) noexcept;

        bool check(const std::span<const real, max_constants> c,
                   execution&                                 e) const noexcept;
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

        bool is_terminal() const noexcept
        {
            return if_transition == invalid_state_id and
                   else_transition == invalid_state_id;
        }
    };

    struct execution {
        i32  i1    = 0;
        i32  i2    = 0;
        real r1    = 0.0;
        real r2    = 0.0;
        time timer = time_domain<time>::infinity;

        std::array<real, 4> ports; //<! Stores value of input messages in big
                                   //<! endian order. ports[0] is port_3 etc..

        std::array<real, 4> message_values;
        std::array<u8, 4>   message_ports;
        int                 messages = 0;

        source source_value;

        std::bitset<4> values; //<! Bit storage message available on X port
                               // in big endian.
                               // values[0] stores value of input port_3.

        state_id current_state        = invalid_state_id;
        state_id next_state           = invalid_state_id;
        state_id source_state         = invalid_state_id;
        state_id current_source_state = invalid_state_id;
        state_id previous_state       = invalid_state_id;
        bool     disallow_transition  = false;

        constexpr void push_message(real               value,
                                    std::integral auto port) noexcept
        {
            debug::ensure(not(messages >= 4 or std::cmp_less(port, 0) or
                              std::cmp_greater(port, 4)));

            if (messages >= 4 or std::cmp_less(port, 0) or
                std::cmp_greater(port, 4))
                return;

            message_values[messages] = value;
            message_ports[messages]  = port;
            ++messages;
        }

        void clear() noexcept
        {
            i1    = 0;
            i2    = 0;
            r1    = 0;
            r2    = 0;
            timer = time_domain<time>::infinity;

            ports.fill(0.0);
            values.reset();

            current_state        = invalid_state_id;
            next_state           = invalid_state_id;
            source_state         = invalid_state_id;
            current_source_state = invalid_state_id;
            previous_state       = invalid_state_id;
            disallow_transition  = false;

            messages = 0;
        }
    };

    hierarchical_state_machine() noexcept = default;
    hierarchical_state_machine(const hierarchical_state_machine&) noexcept =
      default;

    //! Initialize the @c execution object from the HSM and start
    //! the automate. During the handle of the automate, @c srcs
    //! may be use to read value from external source buffer.
    //!
    //! @c return @c empty_value_error{} if the external source
    //! fail to update the buffer.
    status start(execution& state, external_source& srcs) noexcept;

    void clear() noexcept
    {
        states = {};

        top_state = invalid_state_id;
    }

    //! Dispatches an event.
    //!
    //! @return return true if the event was processed, otherwise
    //! false. If the automata is badly defined, return an
    //! modeling error or @c empty_value_error if the external
    //! source fail to update the buffer.
    expected<bool> dispatch(const event_type e,
                            execution&       exec,
                            external_source& srcs) noexcept;

    /// Return true if the state machine is currently dispatching
    /// an event.
    bool is_dispatching(execution& state) const noexcept;

    //! Transitions the state machine. This function can not be
    //! called from Enter / Exit events in the state handler.
    //!
    //! @c return @c empty_value_error if the external source
    //! fail to update the buffer.
    status transition(state_id         target,
                      execution&       exec,
                      external_source& srcs) noexcept;

    /// Set a handler for a state ID. This function will
    /// overwrite the current state handler. \param id state id
    /// from 0 to max_number_of_state \param super_id id of the
    /// super state, if invalid_state_id this is a top state.
    /// Only one state can be a top state. \param sub_id if !=
    /// invalid_state_id this sub state (child state) will be
    /// entered after the state Enter event is executed.
    status set_state(state_id id,
                     state_id super_id = invalid_state_id,
                     state_id sub_id   = invalid_state_id) noexcept;

    /// Resets the state to invalid mode.
    void clear_state(state_id id) noexcept;

    bool is_in_state(execution& state, state_id id) const noexcept;

    //! Handle the @c event_type @event for the @c state_id @c
    //! state. If a underlying action use external source, @c
    //! external_source functions may be use to update the
    //! buffer.
    //!
    //! @c return empty_value_error if the external source fail
    //! to update the buffer.
    expected<bool> handle(const state_id   state,
                          const event_type event,
                          execution&       exec,
                          external_source& srcs) noexcept;

    int    steps_to_common_root(state_id source, state_id target) noexcept;
    status on_enter_sub_state(execution& state, external_source& srcs) noexcept;

    void affect_action(const state_action& action, execution& exec) noexcept;

    std::array<state, max_number_of_state> states;

    constexpr bool is_using_source() const noexcept
    {
        return flags[option::use_source];
    }

    //! Return true if at least one action use the
    //! variable::source.
    //!
    //! Linear traversal of all valid states to detect if @c
    //! if_action,
    //! @c else_action, @c enter_action or @c exit_action actions
    //! use a variable::source.
    bool compute_is_using_source() const noexcept;

    //! Return the biggest state index used in the HSM.
    int compute_max_state_used() const noexcept;

    /** @c constants array are real and can be use in the @c
     * state_action or @c condition_action to perform easy
     * initilization and quick test. */
    std::array<real, 8> constants;

    /// The ordinal value of the hsm_component identifier. If defined,
    /// simulation can get state names from component (in modeling layer).
    u64 parent_id = 0;

    state_id         top_state = invalid_state_id;
    bitflags<option> flags;
};

auto get_hierarchical_state_machine(simulation& sim, hsm_id id) noexcept
  -> expected<hierarchical_state_machine*>;

struct hsm_wrapper {
    using hsm      = hierarchical_state_machine;
    using state_id = hsm::state_id;

    input_port     x[4] = {};
    output_port_id y[4] = {};

    hsm::execution exec;

    real   sigma;
    hsm_id id = undefined<hsm_id>();

    hsm_wrapper() noexcept;
    hsm_wrapper(const hsm_wrapper& other) noexcept;

    status initialize(simulation& sim) noexcept;
    status transition(simulation& sim, time t, time e, time r) noexcept;
    status lambda(simulation& sim) noexcept;
    status finalize(simulation& sim) noexcept;
    observation_message observation(time t, time e) const noexcept;
};

template<std::size_t PortNumber>
struct accumulator {
    input_port x[2u * PortNumber] = {};
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
                      time /*r`*/) noexcept
    {
        for (size_t i = 0; i != PortNumber; ++i) {
            if (const auto lst = get_message(sim, x[i + PortNumber]);
                not lst.empty()) {
                numbers[i] = lst.front()[0];
            }
        }

        for (size_t i = 0; i != PortNumber; ++i) {
            if (const auto lst = get_message(sim, x[i]); not lst.empty()) {
                if (not is_zero(lst.front()[0]))
                    number += numbers[i];
            }
        }

        return success();
    }

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, number };
    }
};

template<std::size_t QssLevel>
struct abstract_cross {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    enum class zone_type : u8 { undefined, up, down };

    input_port     x[2] = {};
    output_port_id y[2] = {};

    std::array<real, 2>        output_values;
    std::array<real, QssLevel> value;
    real                       threshold = zero;
    time                       sigma;
    zone_type                  zone = zone_type::undefined;

    abstract_cross() noexcept = default;

    abstract_cross(const abstract_cross& other) noexcept
      : value(other.value)
      , threshold(other.threshold)
      , sigma(other.sigma)
      , zone(other.zone)
    {}

    enum o_port_name : u8 { port_value, port_threshold };
    enum i_port_name : u8 { port_up, port_down };

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);

        sigma = time_domain<time>::infinity;
        zone  = zone_type::undefined;

        return success();
    }

    constexpr zone_type compute_zone(
      [[maybe_unused]] const real old_value) noexcept
    {
        // The value is too close to threshold, for QSS2 and 3 we use the slope
        // or derivative to decide the zone_type otherwise for QSS1 we use the
        // previous value.

        if (std::abs(value[0] - threshold) < 0x1p-30) {
            if constexpr (QssLevel == 1) {
                if (old_value > value[0])
                    return zone_type::down;
                else
                    return zone_type::up;
            }

            if constexpr (QssLevel >= 2) {
                if (value[1] >= 0)
                    return zone_type::up;
                else
                    return zone_type::down;
            }
        } else {
            if (value[0] >= threshold)
                return zone_type::up;
            else
                return zone_type::down;
        }
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        const auto p_threshold   = get_message(sim, x[port_threshold]);
        const auto p_value       = get_message(sim, x[port_value]);
        const auto msg_threshold = not p_threshold.empty();
        const auto msg_value     = not p_value.empty();

        if (msg_threshold)
            threshold = get_qss_message<QssLevel>(p_threshold)[0];

        const auto old_value = value[0];
        msg_value ? update<QssLevel>(value, get_qss_message<QssLevel>(p_value))
                  : update<QssLevel>(value, e);

        const auto new_zone = compute_zone(old_value);
        if (new_zone != zone) {
            zone  = new_zone;
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
        return zone == zone_type::up
                 ? send_message(sim, y[port_up], output_values[0])
                 : send_message(sim, y[port_down], output_values[1]);
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1)
            return { t, value[0] };

        if constexpr (QssLevel == 2)
            return qss_observation(value[0], value[1], t, e);

        if constexpr (QssLevel == 3)
            return qss_observation(value[0], value[1], value[2], t, e);

        unreachable();
    }
};

using qss1_cross = abstract_cross<1>;
using qss2_cross = abstract_cross<2>;
using qss3_cross = abstract_cross<3>;

template<std::size_t QssLevel>
struct abstract_flipflop {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    input_port     x[2] = {};
    output_port_id y[1] = {};

    std::array<real, QssLevel> value;
    time                       sigma = zero;

    enum o_port_name : u8 { port_in, port_event };
    enum i_port_name : u8 { port_out };

    abstract_flipflop() noexcept = default;

    abstract_flipflop(const abstract_flipflop& other) noexcept
      : value(other.value)
      , sigma(other.sigma)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        value.fill(zero);
        value[0] = std::numeric_limits<real>::infinity();

        sigma = time_domain<time>::infinity;

        return success();
    }

    status transition(simulation& sim, time /*t*/, time e, time /*r*/) noexcept
    {
        const auto lst_value   = get_message(sim, x[port_in]);
        const auto lst_event   = get_message(sim, x[port_event]);
        const auto have_values = not lst_value.empty();
        const auto have_event  = not lst_event.empty();

        have_values
          ? update<QssLevel>(value, get_qss_message<QssLevel>(lst_value))
          : update<QssLevel>(value, e);

        sigma = have_event ? zero : time_domain<time>::infinity;

        return success();
    }

    /** flipflop send QSS value, slope and derivative if and only if a
     * message was received on @c x[port_in] input port. */
    status lambda(simulation& sim) noexcept
    {

        if (value[0] != std::numeric_limits<real>::infinity()) {
            if constexpr (QssLevel == 1)
                return send_message(sim, y[port_out], value[0]);

            if constexpr (QssLevel == 2)
                return send_message(sim, y[port_out], value[0], value[1]);

            if constexpr (QssLevel == 3)
                return send_message(
                  sim, y[port_out], value[0], value[1], value[2]);
        }

        return success();
    }

    observation_message observation(time t, time e) const noexcept
    {
        if constexpr (QssLevel == 1)
            return { t, value[0] };

        if constexpr (QssLevel == 2)
            return qss_observation(value[0], value[1], t, e);

        if constexpr (QssLevel == 3)
            return qss_observation(value[0], value[1], value[2], t, e);

        unreachable();
    }
};

using qss1_flipflop = abstract_flipflop<1>;
using qss2_flipflop = abstract_flipflop<2>;
using qss3_flipflop = abstract_flipflop<3>;

struct time_func {
    output_port_id y[1] = {};

    enum class function_type : u8 { sine, square, linear };
    constexpr static u8 function_type_count = ordinal(function_type::linear);

    time offset   = 0;
    time timestep = 0.01;
    real value    = 0;
    time sigma;

    function_type function = function_type::linear;

    time_func() noexcept = default;

    time_func(const time_func& other) noexcept
      : offset(other.offset)
      , timestep(other.timestep)
      , value(other.value)
      , function(other.function)
    {}

    real call_function(real t) const noexcept
    {
        constexpr real pi = 3.141592653589793238462643383279502884;

        switch (function) {
        case function_type::sine:
            return std::sin(two * 0.1 * pi * t);
        case function_type::square:
            return t * t;
        case function_type::linear:
            return t;
        default:
            unreachable();
        }
    }

    status initialize(simulation& sim) noexcept
    {
        if (not std::isfinite(offset) or offset < zero)
            return new_error(simulation_errc::time_func_offset_error);

        if (not std::isfinite(timestep) or timestep <= zero)
            return new_error(simulation_errc::time_func_timestep_error);

        sigma = offset;
        value = call_function(sim.current_time());

        return success();
    }

    status transition(simulation& /*sim*/,
                      time t,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        value = call_function(t);
        sigma = timestep;
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
    input_port     x[1] = {};
    output_port_id y[1] = {};
    time           sigma;

    dated_message_id fifo = undefined<dated_message_id>();

    real ta = one;

    queue() noexcept = default;

    queue(const queue& other) noexcept
      : sigma(other.sigma)
      , fifo(undefined<dated_message_id>())
      , ta(other.ta)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        if (ta <= 0)
            return new_error(simulation_errc::queue_ta_error);

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
    input_port       x[1] = {};
    output_port_id   y[1] = {};
    time             sigma;
    dated_message_id fifo = undefined<dated_message_id>();

    source source_ta;

    dynamic_queue() noexcept = default;

    dynamic_queue(const dynamic_queue& other) noexcept
      : sigma(other.sigma)
      , fifo(undefined<dated_message_id>())
      , source_ta(other.source_ta)
    {}

    status initialize(simulation& sim) noexcept
    {
        sigma = time_domain<time>::infinity;
        fifo  = undefined<dated_message_id>();

        irt_check(initialize_source(sim, source_ta));

        return success();
    }

    status finalize(simulation& sim) noexcept
    {
        if (auto* ar = sim.dated_messages.try_to_get(fifo); ar) {
            ar->clear();
            sim.dated_messages.free(*ar);
            fifo = undefined<dated_message_id>();
        }

        irt_check(finalize_source(sim, source_ta));

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
    input_port       x[1] = {};
    output_port_id   y[1] = {};
    time             sigma;
    dated_message_id fifo = undefined<dated_message_id>();
    real             ta   = 1.0;

    source source_ta;

    priority_queue() noexcept = default;

    priority_queue(const priority_queue& other) noexcept
      : sigma(other.sigma)
      , fifo(undefined<dated_message_id>())
      , ta(other.ta)
      , source_ta(other.source_ta)
    {}

private:
    status try_to_insert(simulation&    sim,
                         const time     t,
                         const message& msg) noexcept;

public:
    status initialize(simulation& sim) noexcept
    {
        irt_check(initialize_source(sim, source_ta));

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

        irt_check(finalize_source(sim, source_ta));

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
constexpr sz max(sz a, Args&&... args)
{
    return std::max(max(std::forward<Args>(args)...), a);
}

constexpr sz max_size_in_bytes() noexcept
{
    return max(sizeof(qss1_integrator),
               sizeof(qss1_multiplier),
               sizeof(qss1_cross),
               sizeof(qss1_flipflop),
               sizeof(qss1_filter),
               sizeof(qss1_power),
               sizeof(qss1_square),
               sizeof(qss1_sum_2),
               sizeof(qss1_sum_3),
               sizeof(qss1_sum_4),
               sizeof(qss1_wsum_2),
               sizeof(qss1_wsum_3),
               sizeof(qss1_wsum_4),
               sizeof(qss1_inverse),
               sizeof(qss1_integer),
               sizeof(qss1_compare),
               sizeof(qss1_gain),
               sizeof(qss1_sin),
               sizeof(qss1_cos),
               sizeof(qss1_log),
               sizeof(qss1_exp),
               sizeof(qss2_integrator),
               sizeof(qss2_multiplier),
               sizeof(qss2_cross),
               sizeof(qss2_flipflop),
               sizeof(qss2_filter),
               sizeof(qss2_power),
               sizeof(qss2_square),
               sizeof(qss2_sum_2),
               sizeof(qss2_sum_3),
               sizeof(qss2_sum_4),
               sizeof(qss2_wsum_2),
               sizeof(qss2_wsum_3),
               sizeof(qss2_wsum_4),
               sizeof(qss2_inverse),
               sizeof(qss2_integer),
               sizeof(qss2_compare),
               sizeof(qss2_gain),
               sizeof(qss2_sin),
               sizeof(qss2_cos),
               sizeof(qss2_log),
               sizeof(qss2_exp),
               sizeof(qss3_integrator),
               sizeof(qss3_multiplier),
               sizeof(qss3_cross),
               sizeof(qss3_flipflop),
               sizeof(qss3_filter),
               sizeof(qss3_power),
               sizeof(qss3_square),
               sizeof(qss3_sum_2),
               sizeof(qss3_sum_3),
               sizeof(qss3_sum_4),
               sizeof(qss3_wsum_2),
               sizeof(qss3_wsum_3),
               sizeof(qss3_wsum_4),
               sizeof(qss3_inverse),
               sizeof(qss3_integer),
               sizeof(qss3_compare),
               sizeof(qss3_gain),
               sizeof(qss3_sin),
               sizeof(qss3_cos),
               sizeof(qss3_log),
               sizeof(qss3_exp),
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

template<typename Dynamics>
concept dynamics =
  std::is_same_v<Dynamics, qss1_integrator> or
  std::is_same_v<Dynamics, qss1_multiplier> or
  std::is_same_v<Dynamics, qss1_cross> or
  std::is_same_v<Dynamics, qss1_flipflop> or
  std::is_same_v<Dynamics, qss1_filter> or
  std::is_same_v<Dynamics, qss1_power> or
  std::is_same_v<Dynamics, qss1_square> or
  std::is_same_v<Dynamics, qss1_sum_2> or
  std::is_same_v<Dynamics, qss1_sum_3> or
  std::is_same_v<Dynamics, qss1_sum_4> or
  std::is_same_v<Dynamics, qss1_wsum_2> or
  std::is_same_v<Dynamics, qss1_wsum_3> or
  std::is_same_v<Dynamics, qss1_wsum_4> or
  std::is_same_v<Dynamics, qss1_inverse> or
  std::is_same_v<Dynamics, qss1_integer> or
  std::is_same_v<Dynamics, qss1_compare> or
  std::is_same_v<Dynamics, qss1_gain> or std::is_same_v<Dynamics, qss1_sin> or
  std::is_same_v<Dynamics, qss1_cos> or std::is_same_v<Dynamics, qss1_log> or
  std::is_same_v<Dynamics, qss1_exp> or
  std::is_same_v<Dynamics, qss2_integrator> or
  std::is_same_v<Dynamics, qss2_multiplier> or
  std::is_same_v<Dynamics, qss2_cross> or
  std::is_same_v<Dynamics, qss2_flipflop> or
  std::is_same_v<Dynamics, qss2_filter> or
  std::is_same_v<Dynamics, qss2_power> or
  std::is_same_v<Dynamics, qss2_square> or
  std::is_same_v<Dynamics, qss2_sum_2> or
  std::is_same_v<Dynamics, qss2_sum_3> or
  std::is_same_v<Dynamics, qss2_sum_4> or
  std::is_same_v<Dynamics, qss2_wsum_2> or
  std::is_same_v<Dynamics, qss2_wsum_3> or
  std::is_same_v<Dynamics, qss2_wsum_4> or
  std::is_same_v<Dynamics, qss2_inverse> or
  std::is_same_v<Dynamics, qss2_integer> or
  std::is_same_v<Dynamics, qss2_compare> or
  std::is_same_v<Dynamics, qss2_gain> or std::is_same_v<Dynamics, qss2_sin> or
  std::is_same_v<Dynamics, qss2_cos> or std::is_same_v<Dynamics, qss2_log> or
  std::is_same_v<Dynamics, qss2_exp> or
  std::is_same_v<Dynamics, qss3_integrator> or
  std::is_same_v<Dynamics, qss3_multiplier> or
  std::is_same_v<Dynamics, qss3_cross> or
  std::is_same_v<Dynamics, qss3_flipflop> or
  std::is_same_v<Dynamics, qss3_filter> or
  std::is_same_v<Dynamics, qss3_power> or
  std::is_same_v<Dynamics, qss3_square> or
  std::is_same_v<Dynamics, qss3_sum_2> or
  std::is_same_v<Dynamics, qss3_sum_3> or
  std::is_same_v<Dynamics, qss3_sum_4> or
  std::is_same_v<Dynamics, qss3_wsum_2> or
  std::is_same_v<Dynamics, qss3_wsum_3> or
  std::is_same_v<Dynamics, qss3_wsum_4> or
  std::is_same_v<Dynamics, qss3_inverse> or
  std::is_same_v<Dynamics, qss3_integer> or
  std::is_same_v<Dynamics, qss3_compare> or
  std::is_same_v<Dynamics, qss3_gain> or std::is_same_v<Dynamics, qss3_sin> or
  std::is_same_v<Dynamics, qss3_cos> or std::is_same_v<Dynamics, qss3_log> or
  std::is_same_v<Dynamics, qss3_exp> or std::is_same_v<Dynamics, counter> or
  std::is_same_v<Dynamics, queue> or std::is_same_v<Dynamics, dynamic_queue> or
  std::is_same_v<Dynamics, priority_queue> or
  std::is_same_v<Dynamics, generator> or std::is_same_v<Dynamics, constant> or
  std::is_same_v<Dynamics, accumulator_2> or
  std::is_same_v<Dynamics, time_func> or
  std::is_same_v<Dynamics, logical_and_2> or
  std::is_same_v<Dynamics, logical_and_3> or
  std::is_same_v<Dynamics, logical_or_2> or
  std::is_same_v<Dynamics, logical_or_3> or
  std::is_same_v<Dynamics, logical_invert> or
  std::is_same_v<Dynamics, hsm_wrapper>;

struct model {
    real tl     = zero;
    real tn     = time_domain<time>::infinity;
    u32  handle = invalid_heap_handle;

    dynamics_type type;
    observer_id   obs_id = observer_id{ 0 };

    alignas(8) std::byte dyn[max_size_in_bytes()];
};

template<dynamics Dynamics>
static constexpr dynamics_type dynamics_typeof() noexcept
{
    if constexpr (std::is_same_v<Dynamics, qss1_integrator>)
        return dynamics_type::qss1_integrator;
    if constexpr (std::is_same_v<Dynamics, qss1_multiplier>)
        return dynamics_type::qss1_multiplier;
    if constexpr (std::is_same_v<Dynamics, qss1_cross>)
        return dynamics_type::qss1_cross;
    if constexpr (std::is_same_v<Dynamics, qss1_flipflop>)
        return dynamics_type::qss1_flipflop;
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
    if constexpr (std::is_same_v<Dynamics, qss1_inverse>)
        return dynamics_type::qss1_inverse;
    if constexpr (std::is_same_v<Dynamics, qss1_integer>)
        return dynamics_type::qss1_integer;
    if constexpr (std::is_same_v<Dynamics, qss1_compare>)
        return dynamics_type::qss1_compare;
    if constexpr (std::is_same_v<Dynamics, qss1_gain>)
        return dynamics_type::qss1_gain;
    if constexpr (std::is_same_v<Dynamics, qss1_sin>)
        return dynamics_type::qss1_sin;
    if constexpr (std::is_same_v<Dynamics, qss1_cos>)
        return dynamics_type::qss1_cos;
    if constexpr (std::is_same_v<Dynamics, qss1_log>)
        return dynamics_type::qss1_log;
    if constexpr (std::is_same_v<Dynamics, qss1_exp>)
        return dynamics_type::qss1_exp;

    if constexpr (std::is_same_v<Dynamics, qss2_integrator>)
        return dynamics_type::qss2_integrator;
    if constexpr (std::is_same_v<Dynamics, qss2_multiplier>)
        return dynamics_type::qss2_multiplier;
    if constexpr (std::is_same_v<Dynamics, qss2_cross>)
        return dynamics_type::qss2_cross;
    if constexpr (std::is_same_v<Dynamics, qss2_flipflop>)
        return dynamics_type::qss2_flipflop;
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
    if constexpr (std::is_same_v<Dynamics, qss2_inverse>)
        return dynamics_type::qss2_inverse;
    if constexpr (std::is_same_v<Dynamics, qss2_integer>)
        return dynamics_type::qss2_integer;
    if constexpr (std::is_same_v<Dynamics, qss2_compare>)
        return dynamics_type::qss2_compare;
    if constexpr (std::is_same_v<Dynamics, qss2_gain>)
        return dynamics_type::qss2_gain;
    if constexpr (std::is_same_v<Dynamics, qss2_sin>)
        return dynamics_type::qss2_sin;
    if constexpr (std::is_same_v<Dynamics, qss2_cos>)
        return dynamics_type::qss2_cos;
    if constexpr (std::is_same_v<Dynamics, qss2_log>)
        return dynamics_type::qss2_log;
    if constexpr (std::is_same_v<Dynamics, qss2_exp>)
        return dynamics_type::qss2_exp;

    if constexpr (std::is_same_v<Dynamics, qss3_integrator>)
        return dynamics_type::qss3_integrator;
    if constexpr (std::is_same_v<Dynamics, qss3_multiplier>)
        return dynamics_type::qss3_multiplier;
    if constexpr (std::is_same_v<Dynamics, qss3_cross>)
        return dynamics_type::qss3_cross;
    if constexpr (std::is_same_v<Dynamics, qss3_flipflop>)
        return dynamics_type::qss3_flipflop;
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
    if constexpr (std::is_same_v<Dynamics, qss3_inverse>)
        return dynamics_type::qss3_inverse;
    if constexpr (std::is_same_v<Dynamics, qss3_integer>)
        return dynamics_type::qss3_integer;
    if constexpr (std::is_same_v<Dynamics, qss3_compare>)
        return dynamics_type::qss3_compare;
    if constexpr (std::is_same_v<Dynamics, qss3_gain>)
        return dynamics_type::qss3_gain;
    if constexpr (std::is_same_v<Dynamics, qss3_sin>)
        return dynamics_type::qss3_sin;
    if constexpr (std::is_same_v<Dynamics, qss3_cos>)
        return dynamics_type::qss3_cos;
    if constexpr (std::is_same_v<Dynamics, qss3_log>)
        return dynamics_type::qss3_log;
    if constexpr (std::is_same_v<Dynamics, qss3_exp>)
        return dynamics_type::qss3_exp;

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

template<typename Function, typename... Args>
constexpr auto dispatch(const model& mdl, Function&& f, Args&&... args) noexcept
{
    switch (mdl.type) {
    case dynamics_type::qss1_integrator:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_integrator*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_multiplier:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_multiplier*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_cross:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_cross*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_flipflop:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_flipflop*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_filter:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_filter*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_power:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_power*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_square:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_square*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_sum_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_3:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_sum_3*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_4:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_sum_4*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_wsum_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_3:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_wsum_3*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_4:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_wsum_4*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_inverse:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_inverse*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_integer:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_integer*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_compare:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_compare*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_gain:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_gain*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_sin:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_sin*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_cos:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_cos*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_log:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_log*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss1_exp:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss1_exp*>(&mdl.dyn),
                           std::forward<Args>(args)...);

    case dynamics_type::qss2_integrator:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_integrator*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_multiplier:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_multiplier*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_cross:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_cross*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_flipflop:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_flipflop*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_filter:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_filter*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_power:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_power*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_square:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_square*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_sum_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_3:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_sum_3*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_4:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_sum_4*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_wsum_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_3:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_wsum_3*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_4:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_wsum_4*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_inverse:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_inverse*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_integer:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_integer*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_compare:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_compare*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_gain:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_gain*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_sin:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_sin*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_cos:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_cos*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_log:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_log*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss2_exp:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss2_exp*>(&mdl.dyn),
                           std::forward<Args>(args)...);

    case dynamics_type::qss3_integrator:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_integrator*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_multiplier:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_multiplier*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_cross:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_cross*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_flipflop:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_flipflop*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_filter:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_filter*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_power:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_power*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_square:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_square*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_sum_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_3:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_sum_3*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_4:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_sum_4*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_wsum_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_3:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_wsum_3*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_4:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_wsum_4*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_inverse:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_inverse*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_integer:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_integer*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_compare:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_compare*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_gain:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_gain*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_sin:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_sin*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_cos:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_cos*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_log:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_log*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::qss3_exp:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const qss3_exp*>(&mdl.dyn),
                           std::forward<Args>(args)...);

    case dynamics_type::counter:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const counter*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::queue:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const queue*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::dynamic_queue:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const dynamic_queue*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::priority_queue:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const priority_queue*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::generator:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const generator*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::constant:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const constant*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::accumulator_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const accumulator_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::time_func:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const time_func*>(&mdl.dyn),
                           std::forward<Args>(args)...);

    case dynamics_type::logical_and_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const logical_and_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::logical_and_3:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const logical_and_3*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::logical_or_2:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const logical_or_2*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::logical_or_3:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const logical_or_3*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    case dynamics_type::logical_invert:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const logical_invert*>(&mdl.dyn),
                           std::forward<Args>(args)...);

    case dynamics_type::hsm_wrapper:
        return std::invoke(std::forward<Function>(f),
                           *reinterpret_cast<const hsm_wrapper*>(&mdl.dyn),
                           std::forward<Args>(args)...);
    }

    unreachable();
}

template<typename Function, typename... Args>
constexpr auto dispatch(model& mdl, Function&& f, Args&&... args) noexcept
{
    switch (mdl.type) {
    case dynamics_type::qss1_integrator:
        return f(*reinterpret_cast<qss1_integrator*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_multiplier:
        return f(*reinterpret_cast<qss1_multiplier*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_cross:
        return f(*reinterpret_cast<qss1_cross*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_flipflop:
        return f(*reinterpret_cast<qss1_flipflop*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_filter:
        return f(*reinterpret_cast<qss1_filter*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_power:
        return f(*reinterpret_cast<qss1_power*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_square:
        return f(*reinterpret_cast<qss1_square*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_2:
        return f(*reinterpret_cast<qss1_sum_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_3:
        return f(*reinterpret_cast<qss1_sum_3*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_sum_4:
        return f(*reinterpret_cast<qss1_sum_4*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_2:
        return f(*reinterpret_cast<qss1_wsum_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_3:
        return f(*reinterpret_cast<qss1_wsum_3*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_wsum_4:
        return f(*reinterpret_cast<qss1_wsum_4*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_inverse:
        return f(*reinterpret_cast<qss1_inverse*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_integer:
        return f(*reinterpret_cast<qss1_integer*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_compare:
        return f(*reinterpret_cast<qss1_compare*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_gain:
        return f(*reinterpret_cast<qss1_gain*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_sin:
        return f(*reinterpret_cast<qss1_sin*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_cos:
        return f(*reinterpret_cast<qss1_cos*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_log:
        return f(*reinterpret_cast<qss1_log*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss1_exp:
        return f(*reinterpret_cast<qss1_exp*>(&mdl.dyn),
                 std::forward<Args>(args)...);

    case dynamics_type::qss2_integrator:
        return f(*reinterpret_cast<qss2_integrator*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_multiplier:
        return f(*reinterpret_cast<qss2_multiplier*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_cross:
        return f(*reinterpret_cast<qss2_cross*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_flipflop:
        return f(*reinterpret_cast<qss2_flipflop*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_filter:
        return f(*reinterpret_cast<qss2_filter*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_power:
        return f(*reinterpret_cast<qss2_power*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_square:
        return f(*reinterpret_cast<qss2_square*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_2:
        return f(*reinterpret_cast<qss2_sum_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_3:
        return f(*reinterpret_cast<qss2_sum_3*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_sum_4:
        return f(*reinterpret_cast<qss2_sum_4*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_2:
        return f(*reinterpret_cast<qss2_wsum_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_3:
        return f(*reinterpret_cast<qss2_wsum_3*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_wsum_4:
        return f(*reinterpret_cast<qss2_wsum_4*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_inverse:
        return f(*reinterpret_cast<qss2_inverse*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_integer:
        return f(*reinterpret_cast<qss2_integer*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_compare:
        return f(*reinterpret_cast<qss2_compare*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_gain:
        return f(*reinterpret_cast<qss2_gain*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_sin:
        return f(*reinterpret_cast<qss2_sin*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_cos:
        return f(*reinterpret_cast<qss2_cos*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_log:
        return f(*reinterpret_cast<qss2_log*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss2_exp:
        return f(*reinterpret_cast<qss2_exp*>(&mdl.dyn),
                 std::forward<Args>(args)...);

    case dynamics_type::qss3_integrator:
        return f(*reinterpret_cast<qss3_integrator*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_multiplier:
        return f(*reinterpret_cast<qss3_multiplier*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_cross:
        return f(*reinterpret_cast<qss3_cross*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_flipflop:
        return f(*reinterpret_cast<qss3_flipflop*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_filter:
        return f(*reinterpret_cast<qss3_filter*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_power:
        return f(*reinterpret_cast<qss3_power*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_square:
        return f(*reinterpret_cast<qss3_square*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_2:
        return f(*reinterpret_cast<qss3_sum_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_3:
        return f(*reinterpret_cast<qss3_sum_3*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_sum_4:
        return f(*reinterpret_cast<qss3_sum_4*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_2:
        return f(*reinterpret_cast<qss3_wsum_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_3:
        return f(*reinterpret_cast<qss3_wsum_3*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_wsum_4:
        return f(*reinterpret_cast<qss3_wsum_4*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_inverse:
        return f(*reinterpret_cast<qss3_inverse*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_integer:
        return f(*reinterpret_cast<qss3_integer*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_compare:
        return f(*reinterpret_cast<qss3_compare*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_gain:
        return f(*reinterpret_cast<qss3_gain*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_sin:
        return f(*reinterpret_cast<qss3_sin*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_cos:
        return f(*reinterpret_cast<qss3_cos*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_log:
        return f(*reinterpret_cast<qss3_log*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::qss3_exp:
        return f(*reinterpret_cast<qss3_exp*>(&mdl.dyn),
                 std::forward<Args>(args)...);

    case dynamics_type::counter:
        return f(*reinterpret_cast<counter*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::queue:
        return f(*reinterpret_cast<queue*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::dynamic_queue:
        return f(*reinterpret_cast<dynamic_queue*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::priority_queue:
        return f(*reinterpret_cast<priority_queue*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::generator:
        return f(*reinterpret_cast<generator*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::constant:
        return f(*reinterpret_cast<constant*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::accumulator_2:
        return f(*reinterpret_cast<accumulator_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::time_func:
        return f(*reinterpret_cast<time_func*>(&mdl.dyn),
                 std::forward<Args>(args)...);

    case dynamics_type::logical_and_2:
        return f(*reinterpret_cast<logical_and_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::logical_and_3:
        return f(*reinterpret_cast<logical_and_3*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::logical_or_2:
        return f(*reinterpret_cast<logical_or_2*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::logical_or_3:
        return f(*reinterpret_cast<logical_or_3*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    case dynamics_type::logical_invert:
        return f(*reinterpret_cast<logical_invert*>(&mdl.dyn),
                 std::forward<Args>(args)...);

    case dynamics_type::hsm_wrapper:
        return f(*reinterpret_cast<hsm_wrapper*>(&mdl.dyn),
                 std::forward<Args>(args)...);
    }

    unreachable();
}

template<dynamics Dynamics>
Dynamics& get_dyn(model& mdl) noexcept
{
    debug::ensure(dynamics_typeof<Dynamics>() == mdl.type);

    return *reinterpret_cast<Dynamics*>(&mdl.dyn);
}

template<dynamics Dynamics>
const Dynamics& get_dyn(const model& mdl) noexcept
{
    debug::ensure(dynamics_typeof<Dynamics>() == mdl.type);

    return *reinterpret_cast<const Dynamics*>(&mdl.dyn);
}

template<dynamics Dynamics>
constexpr const model& get_model(const Dynamics& d) noexcept
{
    const Dynamics* __mptr = &d;
    return *(const model*)((const char*)__mptr - offsetof(model, dyn));
}

template<dynamics Dynamics>
constexpr model& get_model(Dynamics& d) noexcept
{
    Dynamics* __mptr = &d;
    return *(model*)((char*)__mptr - offsetof(model, dyn));
}

// inline expected<message_id> get_input_port(model& src, int port_src)
// noexcept;
//
// inline status get_input_port(model& src, int port_src, message_id*& p)
// noexcept; inline expected<block_node_id> get_output_port(model& dst,
//                                                int    port_dst) noexcept;
// inline status                  get_output_port(model&          dst,
//                                                int             port_dst,
//                                                block_node_id*& p) noexcept;

inline bool is_ports_compatible(const dynamics_type mdl_src,
                                const int           o_port_index,
                                const dynamics_type mdl_dst,
                                const int /*i_port_index*/) noexcept
{
    // @TODO replace this function with a more readable large boolean table?

    switch (mdl_src) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_power:
    case dynamics_type::qss1_flipflop:
    case dynamics_type::qss1_square:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss1_inverse:
    case dynamics_type::qss2_integrator:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_power:
    case dynamics_type::qss2_flipflop:
    case dynamics_type::qss2_square:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss2_inverse:
    case dynamics_type::qss3_integrator:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_power:
    case dynamics_type::qss3_flipflop:
    case dynamics_type::qss3_square:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::qss3_wsum_4:
    case dynamics_type::qss3_inverse:
        if (any_equal(mdl_dst,
                      dynamics_type::logical_and_2,
                      dynamics_type::logical_and_3,
                      dynamics_type::logical_or_2,
                      dynamics_type::logical_or_3,
                      dynamics_type::logical_invert))
            return false;
        return true;

    case dynamics_type::counter:
    case dynamics_type::queue:
    case dynamics_type::dynamic_queue:
    case dynamics_type::priority_queue:
    case dynamics_type::generator:
    case dynamics_type::time_func:
    case dynamics_type::hsm_wrapper:
    case dynamics_type::accumulator_2:
    case dynamics_type::qss1_integer:
    case dynamics_type::qss2_integer:
    case dynamics_type::qss3_integer:
    case dynamics_type::qss1_compare:
    case dynamics_type::qss2_compare:
    case dynamics_type::qss3_compare:
    case dynamics_type::qss1_gain:
    case dynamics_type::qss2_gain:
    case dynamics_type::qss3_gain:
    case dynamics_type::qss1_sin:
    case dynamics_type::qss2_sin:
    case dynamics_type::qss3_sin:
    case dynamics_type::qss1_cos:
    case dynamics_type::qss2_cos:
    case dynamics_type::qss3_cos:
    case dynamics_type::qss1_log:
    case dynamics_type::qss2_log:
    case dynamics_type::qss3_log:
    case dynamics_type::qss1_exp:
    case dynamics_type::qss2_exp:
    case dynamics_type::qss3_exp:
        return true;

    case dynamics_type::constant:
        return true;

    case dynamics_type::qss2_cross:
    case dynamics_type::qss3_cross:
    case dynamics_type::qss1_cross:
        if (o_port_index == 2) {
            return any_equal(mdl_dst,
                             dynamics_type::counter,
                             dynamics_type::hsm_wrapper,
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
                dst_dyn.x[i].reset();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dst_dyn.y); i != e; ++i)
                dst_dyn.y[i] = undefined<output_port_id>();
    });
}

inline status initialize_source(simulation& sim, source& src) noexcept
{
    return sim.srcs.dispatch(src, source_operation_type::initialize);
}

inline status update_source(simulation& sim, source& src, double& val) noexcept
{
    if (src.is_empty())
        irt_check(sim.srcs.dispatch(src, source_operation_type::update));

    val = src.next();
    return success();
}

inline status finalize_source(simulation& sim, source& src) noexcept
{
    return sim.srcs.dispatch(src, source_operation_type::finalize);
}

//! observer

inline observer::observer() noexcept
  : buffer(default_buffer_size)
  , linearized_buffer(default_linearized_buffer_size)
{}

inline void observer::init(
  const buffer_size_t            buffer_size,
  const linearized_buffer_size_t linearized_buffer_size,
  const float                    ts) noexcept
{
    debug::ensure(time_step > 0.f);

    buffer.write(
      [](auto& buf, const auto len) {
          buf.clear();
          buf.reserve(len);
      },
      *buffer_size);

    linearized_buffer.write(
      [](auto& buf, const auto len) {
          buf.clear();
          buf.reserve(len);
      },
      *linearized_buffer_size);

    time_step = ts <= 0.f ? 1e-2f : time_step;
}

inline void observer::reset() noexcept
{
    buffer.write([](auto& buf) { buf.clear(); });
    linearized_buffer.write([](auto& buf) { buf.clear(); });

    states.reset();
}

inline void observer::clear() noexcept
{
    const auto have_data_lost = states[observer_flags::data_lost];

    reset();

    states.set(observer_flags::data_lost, have_data_lost);
}

inline void observer::update(const observation_message& msg) noexcept
{
    states.set(observer_flags::data_lost, states[observer_flags::buffer_full]);

    buffer.write(
      [](auto& buf, const auto& msg, auto& st) {
          if (not buf.empty() and buf.tail()->data()[0] == msg[0])
              *(buf.tail()) = msg; // overwrite value (at same date).
          else
              buf.force_enqueue(msg); // otherwise push_back new message.

          st.set(observer_flags::buffer_full, buf.available() <= 1);
      },
      msg,
      states);
}

inline bool observer::full() const noexcept
{
    return states[observer_flags::buffer_full];
}

inline status send_message(simulation&    sim,
                           output_port_id output_port,
                           real           r1,
                           real           r2,
                           real           r3) noexcept
{
    if (auto* y = sim.output_ports.try_to_get(output_port)) {
        y->msg[0] = r1;
        y->msg[1] = r2;
        y->msg[2] = r3;

        if (not sim.active_output_ports.can_alloc(1) and
            not sim.active_output_ports.grow<3, 2>())
            return new_error(simulation_errc::emitting_output_ports_full);

        sim.active_output_ports.push_back(output_port);
    }

    return success();
}

inline constexpr auto get_message(simulation&       sim,
                                  const input_port& port) noexcept
  -> std::span<message>
{
    debug::ensure(port.size == port.capacity);
    debug::ensure(port.position + port.size <= sim.message_buffer.size());

    return std::span(sim.message_buffer.data() + port.position, port.size);
}

template<size_t QssLevel>
inline auto get_qss_message(std::span<const message> msgs) noexcept
  -> const message&
{
    debug::ensure(not msgs.empty());

    if (msgs.empty()) {
        constexpr static const message msg_empty{};
        return msg_empty;
    }

    if (msgs.size() == 1) {
        return msgs.front();
    }

    if constexpr (QssLevel == 1)
        return *std::max_element(
          msgs.begin(),
          msgs.end(),
          [](const auto& a, const auto& b) noexcept -> bool {
              return a[0] < b[0];
          });

    if constexpr (QssLevel == 2)
        return *std::max_element(
          msgs.begin(),
          msgs.end(),
          [](const auto& a, const auto& b) noexcept -> bool {
              if (a[0] < b[0])
                  return true;

              if (a[0] == b[0])
                  return a[1] < b[1];

              return false;
          });

    if constexpr (QssLevel == 3)
        return *std::max_element(
          msgs.begin(),
          msgs.end(),
          [](const auto& a, const auto& b) noexcept -> bool {
              if (a[0] < b[0])
                  return true;

              if (a[0] == b[0]) {
                  if (a[1] < b[1])
                      return true;

                  if (a[1] == b[1])
                      return a[2] < b[2];
              }

              return false;
          });
}

/**
 * Get an @a hierarchical_state_machine pointer from the
 * @a hierarchical_state_machine::exec::id index in the
 * simulation @a hsms data_array.
 *
 * @return @a the pointer or a new error @c
 * simulation::hsms_error{} @a unknown_error{} and e_ulong_id{
 * idx }.
 */
inline auto get_hierarchical_state_machine(simulation&  sim,
                                           const hsm_id id) noexcept
  -> expected<hierarchical_state_machine*>
{
    if (auto* hsm = sim.hsms.try_to_get(id))
        return hsm;

    return new_error(simulation_errc::hsm_unknown);
}

//
// template<typename A>
// heap<A>
//

template<typename A>
constexpr heap<A>::heap(constrained_value<int, 512, INT_MAX> pcapacity) noexcept
{
    reserve(pcapacity.value());
}

template<typename A>
constexpr heap<A>::heap(const heap<A>& other) noexcept
{
    if (other.capacity > 0) {
        if (reserve(other.capacity)) {
            std::uninitialized_copy_n(other.nodes, other.max_size, nodes);

            m_size    = other.m_size;
            max_size  = other.max_size;
            capacity  = other.capacity;
            free_list = other.free_list;
            root      = other.root;
        }
    }
}

template<typename A>
constexpr heap<A>::heap(heap<A>&& other) noexcept
  : nodes(std::exchange(other.nodes, nullptr))
  , m_size(std::exchange(other.m_size, 0))
  , max_size(std::exchange(other.max_size, 0))
  , capacity(std::exchange(other.capacity, 0))
  , free_list(std::exchange(other.free_list, invalid_heap_handle))
  , root(std::exchange(other.root, invalid_heap_handle))
{}

template<typename A>
constexpr heap<A>& heap<A>::operator=(const heap<A>& other) noexcept
{
    if (&other != this) {
        auto copy = other;
        std::swap(copy, *this);
    }

    return *this;
}

template<typename A>
constexpr heap<A>& heap<A>::operator=(heap<A>&& other) noexcept
{
    if (&other != this) {
        clear();

        nodes     = std::exchange(other.nodes, nullptr);
        m_size    = std::exchange(other.m_size, 0);
        max_size  = std::exchange(other.max_size, 0);
        capacity  = std::exchange(other.capacity, 0);
        free_list = std::exchange(other.free_list, invalid_heap_handle);
        root      = std::exchange(other.root, invalid_heap_handle);
    }

    return *this;
}

template<typename A>
constexpr heap<A>::~heap() noexcept
{
    destroy();
}

template<typename A>
constexpr void heap<A>::destroy() noexcept
{
    if (nodes)
        A::deallocate(nodes, sizeof(node) * capacity);

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
    if (nodes) {
        constexpr node node_empty{};
        std::fill_n(nodes, capacity, node_empty);
    }

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

    if (std::cmp_less_equal(new_capacity, max_size))
        return false;

    auto* new_data =
      reinterpret_cast<node*>(A::allocate(sizeof(node) * new_capacity));
    if (not new_data)
        return false;

    if (nodes) {
        std::uninitialized_copy_n(nodes, max_size, new_data);
        A::deallocate(nodes, sizeof(node) * capacity);
    }

    nodes    = reinterpret_cast<node*>(new_data);
    capacity = static_cast<index_type>(new_capacity);

    return true;
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::alloc(time tn, model_id id) noexcept
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
    debug::ensure(not is_in_tree(elem));

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

    /* If the node `elem` is not already `pop()`, we detach the
     * node and merge children. */
    if (is_in_tree(elem)) {
        m_size--;
        const auto old_elem = elem;
        detach_subheap(elem);
        elem = merge_subheaps(elem);
        root = merge(root, elem);

        nodes[old_elem].child = invalid_heap_handle;
        nodes[old_elem].prev  = invalid_heap_handle;
        nodes[old_elem].next  = invalid_heap_handle;
    }
}

template<typename A>
constexpr typename heap<A>::handle heap<A>::pop() noexcept
{
    debug::ensure(m_size > 0);

    m_size--;

    const auto old_root = root;

    if (nodes[root].child == invalid_heap_handle) {
        root = invalid_heap_handle;
    } else {
        root                  = merge_subheaps(old_root);
        nodes[old_root].child = invalid_heap_handle;
        nodes[old_root].next  = invalid_heap_handle;
        nodes[old_root].prev  = invalid_heap_handle;
    }

    return old_root;
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
constexpr bool heap<A>::is_in_tree(handle h) const noexcept
{
    if (h == invalid_heap_handle)
        return false;

    if (h == root)
        return true;

    return nodes[h].child != invalid_heap_handle or
           nodes[h].prev != invalid_heap_handle or
           nodes[h].next != invalid_heap_handle;
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
inline scheduller<A>::scheduller(
  constrained_value<int, 512, INT_MAX> capacity) noexcept
  : m_heap(capacity)
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
inline void scheduller<A>::alloc(model& mdl, model_id id, time tn) noexcept
{
    debug::ensure(mdl.handle == invalid_heap_handle);

    mdl.handle = m_heap.alloc(tn, id);
}

template<typename A>
inline void scheduller<A>::reintegrate(model& mdl, time tn) noexcept
{
    debug::ensure(mdl.handle != invalid_heap_handle);

    m_heap.reintegrate(tn, mdl.handle);
}

template<typename A>
inline void scheduller<A>::remove(model& mdl) noexcept
{
    if (mdl.handle != invalid_heap_handle)
        m_heap.remove(mdl.handle);
}

template<typename A>
inline void scheduller<A>::free(model& mdl) noexcept
{
    if (mdl.handle != invalid_heap_handle) {
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
    debug::ensure(not time_domain<real>::is_infinity(tn));

    if (time_domain<real>::is_infinity(tn)) {
        remove(mdl);
    } else {
        if (tn < mdl.tn)
            m_heap.decrease(tn, mdl.handle);
        else if (tn > mdl.tn)
            m_heap.increase(tn, mdl.handle);
    }
}

template<typename A>
inline void scheduller<A>::decrease(model& mdl, time tn) noexcept
{
    debug::ensure(mdl.handle != invalid_heap_handle);
    debug::ensure(tn <= mdl.tn);

    m_heap.decrease(tn, mdl.handle);
}

template<typename A>
inline void scheduller<A>::increase(model& mdl, time tn) noexcept
{
    debug::ensure(mdl.handle != invalid_heap_handle);
    debug::ensure(tn <= mdl.tn);

    m_heap.increase(tn, mdl.handle);
}

template<typename A>
inline void scheduller<A>::pop(vector<model_id>& out) noexcept
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
inline bool scheduller<A>::is_in_tree(handle h) const noexcept
{
    return m_heap.is_in_tree(h);
}

template<typename A>
inline bool scheduller<A>::empty() const noexcept
{
    return m_heap.empty();
}

template<typename A>
unsigned scheduller<A>::size() const noexcept
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

inline constexpr void time_limit::set_bound(const double begin_,
                                            const double end_) noexcept
{
    if (begin_ < end_) {
        if (not std::isinf(begin_))
            m_begin = begin_;

        if (not std::isnan(end_))
            m_end = end_;
    }
}

inline constexpr void time_limit::set_duration(const double begin_,
                                               const double duration_) noexcept
{
    if (duration_ > 0) {
        if (not std::isinf(begin_)) {
            m_begin = begin_;
            m_end   = begin_ + duration_;
        }
    }
}

inline constexpr void time_limit::clear() noexcept
{
    m_begin = 0;
    m_end   = 100;
}

inline constexpr bool time_limit::expired(const double value) const noexcept
{
    return not(value < m_end);
}

inline constexpr time time_limit::duration() const noexcept
{
    return std::isinf(m_end) ? time_domain<time>::infinity : m_end - m_begin;
}

inline constexpr time time_limit::begin() const noexcept { return m_begin; }

inline constexpr time time_limit::end() const noexcept { return m_end; }

inline simulation::simulation(
  const simulation_reserve_definition&      res,
  const external_source_reserve_definition& p_srcs) noexcept
  : immediate_models(res.models.value())
  , immediate_observers(res.models.value())
  , active_output_ports(res.models.value())
  , message_buffer(res.connections.value())
  , parameters(res.models.value())
  , models(res.models.value())
  , hsms(res.hsms.value())
  , observers(res.models.value())
  , nodes(res.connections.value())
  , output_ports(res.connections.value())
  , dated_messages(res.dated_messages.value())
  , sched(res.models)
  , srcs(p_srcs)
{}

template<int Num, int Denum>
bool simulation::grow_models() noexcept
{
    static_assert(Num > 0 and Denum > 0 and Num > Denum);

    const auto nb = models.capacity() * Num / Denum;
    const auto req =
      std::cmp_equal(nb, models.capacity()) ? models.capacity() + 8 : nb;

    return models.reserve(req) and immediate_models.resize(req) and
           immediate_observers.resize(req) and parameters.resize(req) and
           observers.reserve(req) and sched.reserve(req);
}

inline bool simulation::grow_models_to(std::integral auto capacity) noexcept
{
    if (std::cmp_less(capacity, models.capacity()))
        return true;

    return models.reserve(capacity) and immediate_models.resize(capacity) and
           immediate_observers.resize(capacity) and
           parameters.resize(capacity) and observers.reserve(capacity) and
           sched.reserve(capacity);
}

template<int Num, int Denum>
bool simulation::grow_connections() noexcept
{
    static_assert(Num > 0 and Denum > 0 and Num > Denum);

    const auto nb = nodes.capacity() * Num / Denum;
    const auto req =
      std::cmp_equal(nb, nodes.capacity()) ? nodes.capacity() + 8 : nb;

    return nodes.reserve(req) and output_ports.reserve(req);
}

inline bool simulation::grow_connections_to(
  std::integral auto capacity) noexcept
{
    if (std::cmp_less(capacity, nodes.capacity()))
        return true;

    return nodes.reserve(capacity) and output_ports.reserve(capacity);
}

inline void simulation::destroy() noexcept
{
    immediate_models.destroy();
    immediate_observers.destroy();
    active_output_ports.destroy();
    message_buffer.destroy();
    parameters.destroy();

    models.destroy();
    hsms.destroy();
    observers.destroy();
    nodes.destroy();
    output_ports.destroy();

    dated_messages.destroy();
    sched.destroy();
    srcs.destroy();
}

inline simulation::~simulation() noexcept { destroy(); }

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

    immediate_models.clear();
    immediate_observers.clear();
    active_output_ports.clear();
    message_buffer.clear();

    dated_messages.clear();

    t = limits.begin();
}

//! @brief cleanup simulation and destroy all models and
//! connections
inline void simulation::clear() noexcept
{
    clean();

    models.clear();
    hsms.clear();
    observers.clear();
    nodes.clear();
    output_ports.clear();
    dated_messages.clear();
}

constexpr inline auto get_interpolate_type(const dynamics_type type) noexcept
  -> interpolate_type
{
    switch (type) {
    case dynamics_type::qss1_integrator:
    case dynamics_type::qss1_multiplier:
    case dynamics_type::qss1_cross:
    case dynamics_type::qss1_flipflop:
    case dynamics_type::qss1_power:
    case dynamics_type::qss1_square:
    case dynamics_type::qss1_sum_2:
    case dynamics_type::qss1_sum_3:
    case dynamics_type::qss1_sum_4:
    case dynamics_type::qss1_wsum_2:
    case dynamics_type::qss1_wsum_3:
    case dynamics_type::qss1_wsum_4:
    case dynamics_type::qss1_inverse:
    case dynamics_type::qss1_integer:
    case dynamics_type::qss1_compare:
    case dynamics_type::qss1_gain:
    case dynamics_type::qss1_sin:
    case dynamics_type::qss1_cos:
    case dynamics_type::qss1_log:
    case dynamics_type::qss1_exp:
        return interpolate_type::qss1;

    case dynamics_type::qss2_integrator:
    case dynamics_type::qss2_multiplier:
    case dynamics_type::qss2_cross:
    case dynamics_type::qss2_flipflop:
    case dynamics_type::qss2_power:
    case dynamics_type::qss2_square:
    case dynamics_type::qss2_sum_2:
    case dynamics_type::qss2_sum_3:
    case dynamics_type::qss2_sum_4:
    case dynamics_type::qss2_wsum_2:
    case dynamics_type::qss2_wsum_3:
    case dynamics_type::qss2_wsum_4:
    case dynamics_type::qss2_inverse:
    case dynamics_type::qss2_integer:
    case dynamics_type::qss2_compare:
    case dynamics_type::qss2_gain:
    case dynamics_type::qss2_sin:
    case dynamics_type::qss2_cos:
    case dynamics_type::qss2_log:
    case dynamics_type::qss2_exp:
        return interpolate_type::qss2;

    case dynamics_type::qss3_integrator:
    case dynamics_type::qss3_multiplier:
    case dynamics_type::qss3_flipflop:
    case dynamics_type::qss3_power:
    case dynamics_type::qss3_square:
    case dynamics_type::qss3_sum_2:
    case dynamics_type::qss3_sum_3:
    case dynamics_type::qss3_sum_4:
    case dynamics_type::qss3_wsum_2:
    case dynamics_type::qss3_wsum_3:
    case dynamics_type::qss3_wsum_4:
    case dynamics_type::qss3_inverse:
    case dynamics_type::qss3_integer:
    case dynamics_type::qss3_compare:
    case dynamics_type::qss3_gain:
    case dynamics_type::qss3_sin:
    case dynamics_type::qss3_cos:
    case dynamics_type::qss3_log:
    case dynamics_type::qss3_exp:
        return interpolate_type::qss3;

    default:
        return interpolate_type::none;
    }

    unreachable();
}

inline void simulation::observe(model& mdl, observer& obs) const noexcept
{
    mdl.obs_id = observers.get_id(obs);
    obs.model  = models.get_id(mdl);
    obs.type   = get_interpolate_type(mdl.type);
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

    sched.free(*mdl);
    models.free(*mdl);
}

template<typename Dynamics>
void simulation::do_deallocate(Dynamics& dyn) noexcept
{
    if constexpr (has_output_port<Dynamics>) {
        for (const auto y_id : dyn.y) {
            if (auto* y = output_ports.try_to_get(y_id)) {
                block_node* prev = nullptr;
                for (auto* block = nodes.try_to_get(y->next); block;
                     block       = nodes.try_to_get(block->next)) {
                    if (prev != nullptr)
                        nodes.free(*prev);
                    prev = block;
                }

                if (prev != nullptr)
                    nodes.free(*prev);

                output_ports.free(*y);
            }
        }
    }

    if constexpr (has_input_port<Dynamics>) {
        for (auto& elem : dyn.x) {
            elem.reset();
        }
    }

    std::destroy_at(&dyn);
}

inline bool simulation::can_connect(int number) const noexcept
{
    return nodes.can_alloc(number);
}

inline status simulation::connect(model&       src,
                                  int          port_src,
                                  const model& dst,
                                  int          port_dst) noexcept
{
    if (not is_ports_compatible(src, port_src, dst, port_dst))
        return new_error(simulation_errc::connection_incompatible);

    if (not can_connect(src, port_src, dst, port_dst))
        return new_error(simulation_errc::connection_already_exists);

    auto ret = dispatch(dst, [&]<typename Dynamics>(Dynamics& dyn) -> status {
        if constexpr (has_input_port<Dynamics>) {
            if (0 <= port_dst and port_dst < length(dyn.x))
                return success();

            return new_error(simulation_errc::input_port_error);
        }

        irt::unreachable();
    });

    if (ret.has_error())
        return ret.error();

    return dispatch(src, [&]<typename Dynamics>(Dynamics& dyn) -> status {
        if constexpr (has_output_port<Dynamics>) {
            return connect(dyn.y[port_src], get_id(dst), port_dst);
        }

        irt::unreachable();
    });
}

inline status simulation::connect(output_port& port,
                                  model_id     dst,
                                  int          port_dst) noexcept
{
    // First, we try to add the node into the static node vector.

    if (port.connections.can_alloc(1)) {
        port.connections.emplace_back(dst, port_dst);
        return success();
    }

    // If the linked list is empty, we create the first block_node and push the
    // node.

    auto* block = nodes.try_to_get(port.next);

    if (not block) {
        if (not nodes.can_alloc(1) and not nodes.grow<2, 1>())
            return new_error(simulation_errc::connection_container_full);

        auto& new_block = nodes.alloc();
        new_block.nodes.emplace_back(dst, port_dst);
        port.next = nodes.get_id(new_block);
        return success();
    }

    // Otherwise, we try to found a @c block_node in the linked list with a
    // place for a new node.

    block_node* prev = nullptr;
    for (auto* current = block; current;
         current       = nodes.try_to_get(current->next)) {

        if (current->nodes.can_alloc(1)) {
            current->nodes.emplace_back(dst, port_dst);
            return success();
        }

        prev = current;
    }

    // Finally, if the all the node vectors of the linked list are full, we add
    // a new @c block_node to the linked list.

    debug::ensure(prev != nullptr);

    if (not nodes.can_alloc(1) and not nodes.grow<2, 1>())
        return new_error(simulation_errc::connection_container_full);

    auto& new_block = nodes.alloc();
    new_block.nodes.emplace_back(dst, port_dst);
    prev->next = nodes.get_id(new_block);
    return success();
}

inline status simulation::connect(output_port_id& port,
                                  model_id        dst,
                                  int             port_dst) noexcept
{
    auto* y = output_ports.try_to_get(port);

    if (not y) {
        if (not output_ports.can_alloc(1) and not output_ports.grow<2, 1>())
            return new_error(simulation_errc::output_port_error);

        auto& new_y = output_ports.alloc();
        port        = output_ports.get_id(new_y);
        y           = &new_y;
    }

    return connect(*y, dst, port_dst);
}

inline bool simulation::can_connect(const model& src,
                                    int          port_src,
                                    const model& dst,
                                    int          port_dst) const noexcept
{
    return dispatch(src, [&]<typename Dynamics>(Dynamics& dyn) -> bool {
        if constexpr (has_output_port<Dynamics>) {
            if (const auto* y = output_ports.try_to_get(dyn.y[port_src])) {
                for (const auto& elem : y->connections)
                    if (elem.model == get_id(dst) and
                        elem.port_index == port_dst)
                        return false;

                for (const auto* block = nodes.try_to_get(y->next); block;
                     block             = nodes.try_to_get(block->next)) {

                    for (const auto& elem : block->nodes)
                        if (elem.model == get_id(dst) and
                            elem.port_index == port_dst)
                            return false;
                }
            }
        }

        return true;
    });
}

template<typename DynamicsSrc, typename DynamicsDst>
status simulation::connect_dynamics(DynamicsSrc& src,
                                    int          port_src,
                                    DynamicsDst& dst,
                                    int          port_dst) noexcept
{
    return connect(get_model(src), port_src, get_model(dst), port_dst);
}

inline void simulation::disconnect(model& src,
                                   int    port_src,
                                   model& dst,
                                   int    port_dst) noexcept
{
    dispatch(src, [&]<typename Dynamics>(Dynamics& dyn) noexcept {
        if constexpr (has_output_port<Dynamics>) {
            if (auto* y = output_ports.try_to_get(dyn.y[port_src])) {
                for (auto it = y->connections.begin();
                     it != y->connections.end();) {
                    if (it->model == models.get_id(dst) and
                        it->port_index == port_dst) {
                        y->connections.swap_pop_back(it);
                        return;
                    } else {
                        ++it;
                    }
                }

                block_node* prev = nullptr;

                for (auto* block = nodes.try_to_get(y->next); block;
                     block       = nodes.try_to_get(block->next)) {

                    auto connection_deleted = false;
                    for (auto it = block->nodes.begin();
                         it != block->nodes.end();) {
                        if (it->model == models.get_id(dst) &&
                            it->port_index == port_dst) {
                            block->nodes.swap_pop_back(it);
                            connection_deleted = true;
                            break;
                        } else {
                            ++it;
                        }
                    }

                    if (connection_deleted and block->nodes.empty()) {
                        if (prev != nullptr) {
                            prev->next = block->next;
                            nodes.free(*block);
                            block = prev;
                        } else {
                            if (auto* next_block =
                                  nodes.try_to_get(block->next)) {
                                block->nodes = next_block->nodes;
                                block->next  = next_block->next;
                                nodes.free(*next_block);
                            } else {
                                nodes.free(*block);
                                y->next = undefined<block_node_id>();
                                break;
                            }
                        }
                    } else {
                        prev = block;
                    }
                }
            }
        }
    });
}

inline status simulation::initialize() noexcept
{
    last_valid_t = limits.begin();
    t            = limits.begin();

    clean();

    for (auto& h : hsms)
        h.flags.set(hierarchical_state_machine::option::use_source,
                    h.compute_is_using_source());

    for (auto& mdl : models)
        irt_check(make_initialize(mdl, t));

    for (auto& obs : observers) {
        obs.reset();

        if (auto* mdl = models.try_to_get(obs.model)) {
            dispatch(*mdl, [&]<typename Dynamics>(Dynamics& dyn) {
                if constexpr (has_observation_function<Dynamics>) {
                    obs.update(dyn.observation(t, t - mdl->tl));
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
        for (int i = 0, e = length(dyn.x); i != e; ++i)
            dyn.x[i].reset();
    }

    // @attention Copy parameters to model before using the initialize
    // function. Be sure to not owerrite the parameters in the @c
    // dynamics::initialize function.
    parameters[models.get_id(mdl)].copy_to(mdl);

    if constexpr (has_initialize_function<Dynamics>)
        irt_check(dyn.initialize(*this));

    mdl.tl     = t;
    mdl.tn     = t + dyn.sigma;
    mdl.handle = invalid_heap_handle;

    sched.alloc(mdl, models.get_id(mdl), mdl.tn);

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
        for (auto& elem : dyn.x)
            elem.reset();
    }

    debug::ensure(mdl.tn >= t);

    mdl.tl = t;
    mdl.tn = t + dyn.sigma;
    if (dyn.sigma != 0 && mdl.tn == t)
        mdl.tn = std::nextafter(t, t + irt::one);

    debug::ensure(not sched.is_in_tree(mdl.handle));
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

inline status simulation::finalize() noexcept
{
    debug::ensure(std::isfinite(t));

    for (auto& mdl : models) {
        observer* obs = nullptr;
        if (is_defined(mdl.obs_id))
            obs = observers.try_to_get(mdl.obs_id);

        auto ret = dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
            return this->make_finalize(dyn, obs, t);
        });

        irt_check(ret);
    }

    return success();
}

inline status simulation::run() noexcept
{
    debug::ensure(std::isfinite(t));

    immediate_models.clear();
    immediate_observers.clear();

    if (sched.empty()) {
        t = time_domain<time>::infinity;
        return success();
    }

    last_valid_t = t;
    t            = sched.tn();

    if (limits.expired(t)) {
        t = limits.end();
        return success();
    }

    sched.pop(immediate_models);

    active_output_ports.clear();
    for (const auto id : immediate_models)
        if (auto* mdl = models.try_to_get(id); mdl)
            irt_check(make_transition(*mdl, t));

    // First, we compute the input_port::capacity.

    u32 global_messages_number = 0;

    for (const auto y_id : active_output_ports) {
        if (auto* y = output_ports.try_to_get(y_id)) {
            y->for_each(
              models,
              nodes,
              [](model&     mdl,
                 const auto port_index,
                 auto&      global_messages_number) noexcept {
                  dispatch(
                    mdl,
                    []<typename Dynamics>(
                      Dynamics&  dyn,
                      const auto port_index,
                      auto&      global_messages_number) noexcept {
                        if constexpr (has_input_port<Dynamics>) {
                            dyn.x[port_index].capacity += 1u;
                            dyn.x[port_index].position = 0u;
                            dyn.x[port_index].size     = 0u;

                            global_messages_number += 1;
                        }
                    },
                    port_index,
                    global_messages_number);
              },
              global_messages_number);
        }
    }

    // Finally, we copy the @c output_port::msg messages on the @c
    // message_buffer vector according to the @c input_port::capacity, index and
    // position.

    message_buffer.resize(global_messages_number);
    u32 global_position = 0;

    for (const auto y_id : active_output_ports) {
        if (auto* y = output_ports.try_to_get(y_id)) {
            y->for_each(
              models,
              nodes,
              [](auto&       mdl,
                 const auto  port_index,
                 auto&       sched,
                 const auto  t,
                 const auto& msg,
                 auto&       message_buffer,
                 auto&       global_position) {
                  dispatch(mdl, [&]<typename Dynamics>(Dynamics& dyn) {
                      if constexpr (has_input_port<Dynamics>) {
                          auto& x = dyn.x[port_index];

                          if (x.size == 0) {
                              x.position = global_position;
                              global_position += x.capacity;
                              sched.update(mdl, t);
                          }

                          const auto start_at = x.position + x.size;
                          ++x.size;

                          message_buffer[start_at] = msg;
                      }
                  });
              },
              sched,
              t,
              y->msg,
              message_buffer,
              global_position);
        }
    }

    return success();
}

template<typename Fn, typename... Args>
inline status simulation::run_with_cb(Fn&& fn, Args&&... args) noexcept
{
    debug::ensure(std::isfinite(t));

    immediate_models.clear();
    immediate_observers.clear();

    if (sched.empty()) {
        t = time_domain<time>::infinity;
        return success();
    }

    last_valid_t = t;
    t            = sched.tn();

    if (limits.expired(t)) {
        t = limits.end();
        return success();
    }

    sched.pop(immediate_models);

    // emitting_output_ports.clear();
    for (const auto id : immediate_models)
        if (auto* mdl = models.try_to_get(id); mdl)
            irt_check(make_transition(*mdl, t));

    fn(std::as_const(*this),
       std::span(immediate_models.data(), immediate_models.size()),
       std::forward<Args>(args)...);

    // for (int i = 0, e = length(emitting_output_ports); i != e; ++i) {
    //     auto* mdl = models.try_to_get(emitting_output_ports[i].model);
    //     if (!mdl)
    //         continue;

    //    debug::ensure(sched.is_in_tree(mdl->handle));
    //    sched.update(*mdl, t);

    //    if (not messages.can_alloc(1))
    //        return new_error(simulation_errc::messages_container_full);

    //    auto  port = emitting_output_ports[i].port;
    //    auto& msg  = emitting_output_ports[i].msg;

    //    dispatch(*mdl, [&]<typename Dynamics>(Dynamics& dyn) {
    //        if constexpr (has_input_port<Dynamics>) {
    //            auto* list = messages.try_to_get(dyn.x[port]);
    //            if (not list) {
    //                auto& new_list = messages.alloc();
    //                dyn.x[port]    = messages.get_id(new_list);
    //                list           = &new_list;
    //            }

    //            list->push_back({ msg[0], msg[1], msg[2] });
    //        }
    //    });
    //}

    return success();
}

//
// class HSM
//

inline hsm_wrapper::hsm_wrapper() noexcept = default;

inline hsm_wrapper::hsm_wrapper(const hsm_wrapper& other) noexcept
  : exec{ other.exec }
  , sigma{ other.sigma }
  , id{ other.id }
{}

inline status hsm_wrapper::initialize(simulation& sim) noexcept
{
    exec.clear();

    if (auto machine = get_hierarchical_state_machine(sim, id);
        machine.has_value()) {
        if ((*machine)->flags[hierarchical_state_machine::option::use_source]) {
            irt_check(initialize_source(sim, exec.source_value));
        }

        irt_check((*machine)->start(exec, sim.srcs));

        sigma = time_domain<time>::infinity;

        if (exec.current_state !=
              hierarchical_state_machine::invalid_state_id and
            not(*machine)->states[exec.current_state].is_terminal()) {
            switch ((*machine)->states[exec.current_state].condition.type) {
            case hierarchical_state_machine::condition_type::sigma:
                sigma = exec.timer;
                break;
            case irt::hierarchical_state_machine::condition_type::port:
                sigma = time_domain<time>::infinity;
                break;
            default:
                sigma = time_domain<time>::zero;
                break;
            }
        }

        if (exec.messages > 0)
            sigma = time_domain<time>::zero;

        return success();
    } else {
        return machine.error();
    }
}

inline status hsm_wrapper::transition(simulation& sim,
                                      time /*t*/,
                                      time /*e*/,
                                      time r) noexcept
{
    if (auto machine = get_hierarchical_state_machine(sim, id);
        machine.has_value()) {

        for (int i = 0, e = length(x); i != e; ++i) {
            const auto lst = get_message(sim, x[i]);
            if (not lst.empty()) {
                exec.values.set(e - 1 - i, true);

                for (auto& elem : lst)
                    exec.ports[e - 1 - i] = elem[0];
            }
        }

        exec.messages = 0;
        bool wait_timer, wait_msg, is_terminal;

        do {
            exec.previous_state = exec.current_state;

            switch ((*machine)->states[exec.current_state].condition.type) {
            case irt::hierarchical_state_machine::condition_type::sigma:
                exec.timer = r;
                if (r == 0.0) {
                    irt_check((*machine)->dispatch(
                      hierarchical_state_machine::event_type::wake_up,
                      exec,
                      sim.srcs));
                } else {
                    debug::ensure(exec.values.any());

                    irt_check((*machine)->dispatch(
                      hierarchical_state_machine::event_type::input_changed,
                      exec,
                      sim.srcs));
                }
                break;

            case irt::hierarchical_state_machine::condition_type::port:
                if (exec.values.any()) {
                    irt_check((*machine)->dispatch(
                      hierarchical_state_machine::event_type::input_changed,
                      exec,
                      sim.srcs));
                }
                break;

            default:
                irt_check((*machine)->dispatch(
                  hierarchical_state_machine::event_type::internal,
                  exec,
                  sim.srcs));
                break;
            }

            debug::ensure(exec.current_state !=
                          hierarchical_state_machine::invalid_state_id);

            wait_timer =
              (*machine)->states[exec.current_state].condition.type ==
              hierarchical_state_machine::condition_type::sigma;

            wait_msg = (*machine)->states[exec.current_state].condition.type ==
                       hierarchical_state_machine::condition_type::port;

            is_terminal = (*machine)->states[exec.current_state].is_terminal();

        } while (not(wait_timer or wait_msg or is_terminal));

        sigma = exec.messages ? 0.0
                : wait_timer  ? exec.timer
                : wait_msg    ? time_domain<time>::infinity
                : is_terminal ? time_domain<time>::infinity
                              : 0.0;

        return success();
    } else {
        return machine.error();
    }
}

inline status hsm_wrapper::lambda(simulation& sim) noexcept
{
    for (auto i = 0; i < exec.messages; ++i)
        irt_check(
          send_message(sim, y[exec.message_ports[i]], exec.message_values[i]));

    return success();
}

inline status hsm_wrapper::finalize(simulation& sim) noexcept
{
    if (auto machine = get_hierarchical_state_machine(sim, id);
        machine.has_value()) {
        if ((*machine)->flags[hierarchical_state_machine::option::use_source])
            irt_check(finalize_source(sim, exec.source_value));

        return success();
    } else {
        return machine.error();
    }
}

inline observation_message hsm_wrapper::observation(time t,
                                                    time /*e*/) const noexcept
{
    return {
        t, static_cast<real>(exec.current_state), exec.r1, exec.r2, exec.timer
    };
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
    fatal::ensure(models.can_alloc(1));

    auto& mdl  = models.alloc();
    mdl.type   = dynamics_typeof<Dynamics>();
    mdl.handle = invalid_heap_handle;

    parameters[models.get_id(mdl)].init_from(mdl.type);

    std::construct_at(reinterpret_cast<Dynamics*>(&mdl.dyn));
    auto& dyn = get_dyn<Dynamics>(mdl);

    if constexpr (has_input_port<Dynamics>)
        for (int i = 0, e = length(dyn.x); i != e; ++i)
            dyn.x[i].reset();

    if constexpr (has_output_port<Dynamics>)
        for (int i = 0, e = length(dyn.y); i != e; ++i)
            dyn.y[i] = undefined<output_port_id>();

    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        dyn.id = undefined<hsm_id>();

    return dyn;
}

//! @brief This function allocates dynamics and models.
inline model& simulation::clone(const model& mdl) noexcept
{
    fatal::ensure(models.can_alloc(1));

    auto& new_mdl  = models.alloc();
    new_mdl.type   = mdl.type;
    new_mdl.handle = invalid_heap_handle;

    dispatch(new_mdl, [&]<typename Dynamics>(Dynamics& dyn) -> void {
        const auto& src_dyn = get_dyn<Dynamics>(mdl);
        std::construct_at(&dyn, src_dyn);

        parameters[models.get_id(new_mdl)] = parameters[models.get_id(mdl)];

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i].reset();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = undefined<output_port_id>();
    });

    return new_mdl;
}

//! @brief This function allocates dynamics and models.
inline model& simulation::alloc(dynamics_type type) noexcept
{
    fatal::ensure(models.can_alloc(1));

    auto& mdl  = models.alloc();
    mdl.type   = type;
    mdl.handle = invalid_heap_handle;

    dispatch(mdl, []<typename Dynamics>(Dynamics& dyn) -> void {
        std::construct_at(&dyn);

        if constexpr (has_input_port<Dynamics>)
            for (int i = 0, e = length(dyn.x); i != e; ++i)
                dyn.x[i].reset();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = undefined<output_port_id>();

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            dyn.id = undefined<hsm_id>();
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

// inline expected<message_id> get_input_port(model& src, int port_src) noexcept
//{
//     return dispatch(
//       src, [&]<typename Dynamics>(Dynamics& dyn) -> expected<message_id> {
//           if constexpr (has_input_port<Dynamics>) {
//               if (port_src >= 0 && port_src < length(dyn.x)) {
//                   return dyn.x[port_src];
//               }
//           }
//
//           return new_error(simulation_errc::input_port_error);
//       });
// }
//
// inline status get_input_port(model& src, int port_src, message_id*& p)
// noexcept
//{
//     return dispatch(src,
//                     [port_src, &p]<typename Dynamics>(Dynamics& dyn) ->
//                     status {
//                         if constexpr (has_input_port<Dynamics>) {
//                             if (port_src >= 0 && port_src < length(dyn.x)) {
//                                 p = &dyn.x[port_src];
//                                 return success();
//                             }
//                         }
//
//                         return new_error(simulation_errc::input_port_error);
//                     });
// }
//
// inline expected<block_node_id> get_output_port(model& dst,
//                                                int    port_dst) noexcept
//{
//     return dispatch(
//       dst, [&]<typename Dynamics>(Dynamics& dyn) -> expected<block_node_id> {
//           if constexpr (has_output_port<Dynamics>) {
//               if (port_dst >= 0 && port_dst < length(dyn.y))
//                   return dyn.y[port_dst];
//           }
//
//           return new_error(simulation_errc::output_port_error);
//       });
// }
//
// inline status get_output_port(model&          dst,
//                               int             port_dst,
//                               block_node_id*& p) noexcept
//{
//     return dispatch(dst,
//                     [port_dst, &p]<typename Dynamics>(Dynamics& dyn) ->
//                     status {
//                         if constexpr (has_output_port<Dynamics>) {
//                             if (port_dst >= 0 && port_dst < length(dyn.y)) {
//                                 p = &dyn.y[port_dst];
//                                 return success();
//                             }
//                         }
//
//                         return new_error(simulation_errc::output_port_error);
//                     });
// }

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

        const auto lst = get_message(sim, x[0]);
        if (not lst.empty()) {
            for (const auto& msg : lst) {
                if (!sim.dated_messages.can_alloc(1))
                    return new_error(
                      simulation_errc::dated_messages_container_full);

                ar->push_head({ irt::real(t + ta), msg[0], msg[1], msg[2] });
            }
        }

        if (not lst.empty()) {
            sigma = lst.front()[0] - t;
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

        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            for (const auto& msg : lst) {
                if (!sim.dated_messages.can_alloc(1))
                    return new_error(
                      simulation_errc::dated_messages_container_full);

                real ta = zero;
                irt_check(update_source(sim, source_ta, ta));
                ar->push_head({ t + ta, msg[0], msg[1], msg[2] });
            }
        }

        if (not lst.empty()) {
            sigma = lst.front().data()[0] - t;
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
        return new_error(simulation_errc::dated_messages_container_full);

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

        const auto lst = get_message(sim, x[0]);

        if (not lst.empty()) {
            for (const auto& msg : lst) {
                real value = zero;

                irt_check(update_source(sim, source_ta, value));

                if (auto ret =
                      try_to_insert(sim, static_cast<real>(value) + t, msg);
                    !ret)
                    return new_error(
                      simulation_errc::dated_messages_container_full);
            }
        }

        if (not lst.empty()) {
            sigma = lst.front()[0] - t;
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

// source implementation

inline source::source(const constant_source_id id_) noexcept
  : id{ id_ }
  , type{ source_type::constant }
{}

inline source::source(const binary_file_source_id id_) noexcept
  : id{ id_ }
  , type{ source_type::binary_file }
{}

inline source::source(const text_file_source_id id_) noexcept
  : id{ id_ }
  , type{ source_type::text_file }
{}

inline source::source(const random_source_id id_) noexcept
  : id{ id_ }
  , type{ source_type::random }
{}

inline source::source(const source_type type_, const source_any_id id_) noexcept
  : id(id_)
  , type(type_)
{}

inline source::source(const source& src) noexcept
  : buffer(src.buffer)
  , chunk_id(src.chunk_id)
  , id(src.id)
  , type(src.type)
  , index(src.index)
{}

inline source::source(source&& src) noexcept
  : buffer(src.buffer)
  , chunk_id(src.chunk_id)
  , id(src.id)
  , type(src.type)
  , index(src.index)
{
    src.buffer = std::span<double>();
    src.index  = 0;
    src.type   = source_type::constant;
    src.id     = undefined<constant_source_id>();
    std::fill_n(src.chunk_id.data(), src.chunk_id.size(), 0);
}

inline source& source::operator=(const source& src) noexcept
{
    if (&src != this) {
        clear();
        type = src.type;
        id   = src.id;
    }

    return *this;
}

inline source& source::operator=(source&& src) noexcept
{
    if (&src != this) {
        buffer = std::exchange(src.buffer, std::span<double>());
        std::copy_n(src.buffer.data(), src.buffer.size(), buffer.data());
        type  = std::exchange(src.type, source_type::constant);
        index = std::exchange(src.index, 0);
        id    = std::exchange(src.id, undefined<constant_source_id>());
    }

    return *this;
}

inline void source::reset(const source_type   type_,
                          const source_any_id id_) noexcept
{
    reset();

    type = type_;
    id   = id_;
}

inline void source::reset(const i64 param) noexcept
{
    reset();

    const auto p_type = left(static_cast<u64>(param));
    const auto p_id   = right(static_cast<u64>(param));

    type = p_type <= 4 ? enum_cast<source_type>(p_type) : source_type::constant;

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
}

inline void source::reset() noexcept { index = 0; }

inline void source::clear() noexcept
{
    buffer = std::span<double>();
    id     = undefined<constant_source_id>();
    type   = source_type::constant;
    index  = 0;
    std::fill_n(chunk_id.data(), chunk_id.size(), 0);
}

inline bool source::is_empty() const noexcept
{
    return std::cmp_greater_equal(index, buffer.size());
}

inline double source::next() noexcept
{
    debug::ensure(not is_empty());

    if (std::cmp_greater_equal(index, buffer.size()))
        return 0.0;

    const auto old_index = index++;
    return buffer[static_cast<sz>(old_index)];
}

// node implementation

constexpr node::node(const model_id id, const i8 port) noexcept
  : model{ id }
  , port_index{ port }
{}

// input-port implementation

inline constexpr void input_port::reset() noexcept
{
    position = 0;
    size     = 0;
    capacity = 0;
}

// output-port implementation

template<typename Function, typename... Args>
void output_port::for_each(const data_array<model, model_id>&           models,
                           const data_array<block_node, block_node_id>& nodes,
                           Function&&                                   fn,
                           Args&&... args) const noexcept
{
    for (const auto& node : connections)
        if (const auto* mdl = models.try_to_get(node.model))
            std::invoke(fn, *mdl, node.port_index, args...);

    for (const auto* block = nodes.try_to_get(next); block;
         block             = nodes.try_to_get(block->next))
        for (const auto& elem : block->nodes)
            if (const auto* mdl = models.try_to_get(elem.model))
                std::invoke(fn, *mdl, elem.port_index, args...);
}

template<typename Function, typename... Args>
void output_port::for_each(data_array<model, model_id>&           models,
                           data_array<block_node, block_node_id>& nodes,
                           Function&&                             fn,
                           Args&&... args) noexcept
{
    for (auto it = connections.begin(); it != connections.end();) {
        if (model* mdl = models.try_to_get(it->model)) {
            std::invoke(fn, *mdl, it->port_index, args...);
            ++it;
        } else {
            connections.swap_pop_back(it);
        }
    }

    block_node* prev = nullptr;

    for (auto* block = nodes.try_to_get(next); block;
         block       = nodes.try_to_get(block->next)) {

        for (auto it = block->nodes.begin(); it != block->nodes.end();) {
            if (model* mdl = models.try_to_get(it->model)) {
                std::invoke(fn, *mdl, it->port_index, args...);
                ++it;
            } else {
                block->nodes.swap_pop_back(it);
            }
        }

        while (not block->nodes.empty() and connections.available() > 0) {
            connections.push_back(block->nodes.back());
            block->nodes.pop_back();
        }

        if (block->nodes.empty()) {
            if (prev == nullptr)
                next = block->next;
            else
                prev->next = block->next;

            nodes.free(*block);
        } else {
            prev = block;
        }
    }
}

} // namespace irt

#endif
