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
enum class message_id : u64;
enum class observer_id : u64;
enum class block_node_id : u64;
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
    u32              length = 8u;

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
    small_string<23>   name;
    vector<chunk_type> buffers; // buffer, size is defined by max_clients
    vector<u64>        offsets; // offset, size is defined by max_clients
    u32                max_clients = 1; // number of source max (must be >= 1).
    u64                max_reals   = 0; // number of real in the file.

    std::filesystem::path file_path;
    std::ifstream         ifs;
    file_path_id          file_id;
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
    small_string<23> name;
    chunk_type       buffer;
    u64              offset;

    std::filesystem::path file_path;
    std::ifstream         ifs;
    file_path_id          file_id;

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

    distribution_type distribution = distribution_type::uniform_real;
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
    source_type       type = source_type::constant;
    i16 index = 0; // The index of the next double to read in current chunk.
    std::array<u64, 4> chunk_id; // Current chunk. Use when restore is apply.

    //! Call to reset the position in the current chunk.
    void reset() noexcept { index = 0u; }

    //! Clear the source (buffer and external source access)
    void clear() noexcept
    {
        buffer = std::span<double>();
        id     = 0u;
        type   = source_type::constant;
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

    ~external_source() noexcept;

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

    status dispatch(source& src, const source::operation_type op) noexcept;

    //! Call the @c data_array<T, Id>::clear() function for all sources.
    void clear() noexcept;

    //! An example of error handlers to catch all error from the external
    //! source class and friend (@c binary_file_source, @c text_file_source,
    //! @c random_source and @c constant_source).
    // constexpr auto make_error_handlers() const noexcept;
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

struct qss_integrator_tag {};
struct qss_multiplier_tag {};
struct qss_cross_tag {};
struct qss_filter_tag {};
struct qss_power_tag {};
struct qss_square_tag {};
struct qss_sum_2_tag {};
struct qss_sum_3_tag {};
struct qss_sum_4_tag {};
struct qss_wsum_2_tag {};
struct qss_wsum_3_tag {};
struct qss_wsum_4_tag {};
struct counter_tag {};
struct queue_tag {};
struct dynamic_queue_tag {};
struct priority_queue_tag {};
struct generator_tag {};
struct constant_tag {};
struct time_func_tag {};
struct accumulator_2_tag {};
struct logical_and_2_tag {};
struct logical_and_3_tag {};
struct logical_or_2_tag {};
struct logical_or_3_tag {};
struct logical_invert_tag {};
struct hsm_wrapper_tag {};

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

   Two vertors of real and integer are available to initialize each type of @c
   irt::dynamics.
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
    void clear() noexcept;

    parameter& set_constant(real value, real offset) noexcept;
    parameter& set_cross(real threshold, bool detect_up) noexcept;
    parameter& set_integrator(real X, real dQ) noexcept;
    parameter& set_time_func(real offset, real timestep, int type) noexcept;
    parameter& set_wsum2(real v1, real coeff1, real v2, real coeff2) noexcept;
    parameter& set_wsum3(real v1,
                         real coeff1,
                         real v2,
                         real coeff2,
                         real v3,
                         real coeff3) noexcept;
    parameter& set_wsum4(real v1,
                         real coeff1,
                         real v2,
                         real coeff2,
                         real v3,
                         real coeff3,
                         real v4,
                         real coeff4) noexcept;
    parameter& set_hsm_wrapper(const u32 id) noexcept;
    parameter& set_hsm_wrapper(i64  i1,
                               i64  i2,
                               real r1,
                               real r2,
                               real timer) noexcept;
    parameter& set_hsm_wrapper(const u64                 id,
                               const source::source_type type) noexcept;

    std::array<real, 8> reals;
    std::array<i64, 8>  integers;
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

enum class observer_flags : u8 {
    none              = 0,
    data_available    = 1,
    buffer_full       = 2,
    data_lost         = 4,
    use_linear_buffer = 8,
    Count
};

struct observer {
    static inline constexpr auto default_buffer_size            = 4;
    static inline constexpr auto default_linearized_buffer_size = 256;

    using buffer_size_t            = constrained_value<int, 4, 64>;
    using linearized_buffer_size_t = constrained_value<int, 64, 32768>;

    using value_type = observation;

    /// Allocate raw and liearized buffers with default sizes.
    observer() noexcept;

    /// Allocate raw and linearized buffers with specified constrained sizes.
    observer(const buffer_size_t            buffer_size,
             const linearized_buffer_size_t linearized_buffer_size) noexcept;

    /// Change the raw and linearized buffers with specified constrained sizes
    /// and change the time-step.
    void init(const buffer_size_t            buffer_size,
              const linearized_buffer_size_t linearized_buffer_size,
              const float                    ts) noexcept;

    void reset() noexcept;
    void clear() noexcept;
    void update(const observation_message& msg) noexcept;
    bool full() const noexcept;
    void push_back(const observation& vec) noexcept;

    ring_buffer<observation_message> buffer;
    ring_buffer<observation>         linearized_buffer;

    model_id                 model     = undefined<model_id>();
    dynamics_type            type      = dynamics_type::qss1_integrator;
    float                    time_step = 1e-2f;
    std::pair<real, real>    limits;
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

/**
   Stores a list of @c node to make connection between models. Each output port
   of atomic model stores a @c block_node_id.

   Build a linked list of
 */
struct block_node {
    small_vector<node, 4> nodes;

    block_node_id next = undefined<block_node_id>();
};

struct output_message {
    output_message() noexcept  = default;
    ~output_message() noexcept = default;

    message  msg;
    model_id model = undefined<model_id>();
    i8       port  = 0;
};

static inline constexpr u32 invalid_heap_handle = 0xffffffff;

/**
   A pairing heap is a type of heap data structure with relatively simple
   implementation and excellent practical amortized performance, introduced by
   Michael Fredman, Robert Sedgewick, Daniel Sleator, and Robert Tarjan in 1986.
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

    constexpr ~heap() noexcept;

    /** Clear and free the allocated buffer. */
    constexpr void destroy() noexcept;

    /** Clear the buffer. Root equals null. */
    constexpr void clear() noexcept;

    /**
       Try to allocate a new buffer, copy the old buffer into the new one assign
       the new buffer.

       This function returns false and to nothing on the buffer if the @c
       new_capacity can hold the current capacity and if a new buffer can be
       allocate.
     */
    constexpr bool reserve(std::integral auto new_capacity) noexcept;

    /**
       Allocate a new node into the heap and insert the @c model_id and the @c
       time into the tree.
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
   The heap nodes are only allocated and freed using the @c heap::alloc() and @c
   heap::destroy() functions. These functions make a link between @c model and
   @c heap::node via the @c heap::node::id and the @c model::handle attributes.
   Make sure you check model and node at the same time. If a model is allocate,
   you must allocate a node before using the scheduler. If a model is delete,
   you must free the node to free memory.

   A node is detached from the heap if the handle `node::child`, `node::next`
   and `node::prev` are null (`irt::invalid_heap_handle`). To detach a node, you
   can use the `heap::pop()` or `heap::remove()` functions.
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
       Allocate a new @c heap::node and makes a link between @c heap::node and
       @c model (@c heap::node::id equal @c id and @c mdl.handle equal to the
       newly @c handle.

       This function @c abort if the scheduller fail to allocate more nodes. Use
       the @c can_alloc() before use.
     */
    void alloc(model& mdl, model_id id, time tn) noexcept;

    /**
       Unlink the @c model and the @c heap::node if it exists and free memory
       used by the @c heap::node.
     */
    void free(model& mdl) noexcept;

    /**
       Insert or reinsert the node into the @c heap. Use this function only and
       only if the node is detached via the @c remove() or the @c pop()
       functions.
     */
    void reintegrate(model& mdl, time tn) noexcept;

    /**
       Remove the @c node from the tree of the heap. The @c node can be reuse
       with the @c reintegrate() function.
     */
    void remove(model& mdl) noexcept;

    /**
       According to the @c tn parameter, @c update function call the @c increase
       or @c decrease function. You can you this function only if the node is in
       the tree.
     */
    void update(model& mdl, time tn) noexcept;
    void increase(model& mdl, time tn) noexcept;
    void decrease(model& mdl, time tn) noexcept;

    /**
       Remove all @c node with the same @c tn from the tree of the heap. The @c
       node can be reuse with the @c reintegrate() function.
     */
    void pop(vector<model_id>& out) noexcept;

    /** Returns the top event @c time-next in the heap. */
    time tn() const noexcept;

    /** Returns the @c time-next event for handle. */
    time tn(handle h) const noexcept;

    /**
       Return true if the node @c h is a valid heap handle and if `h` is equal
       to `root` or if the node prev, next or child handle are not null.
     */
    bool is_in_tree(handle h) const noexcept;

    bool empty() const noexcept;

    unsigned size() const noexcept;
    int      ssize() const noexcept;
};

class simulation
{
public:
    vector<output_message> emitting_output_ports;
    vector<model_id>       immediate_models;
    vector<observer_id>    immediate_observers;

    /** A vector of the size of @c models data_array. */
    vector<parameter> parameters;

    data_array<model, model_id>                      models;
    data_array<hierarchical_state_machine, hsm_id>   hsms;
    data_array<observer, observer_id>                observers;
    data_array<block_node, block_node_id>            nodes;
    data_array<small_vector<message, 8>, message_id> messages;

    data_array<ring_buffer<dated_message>, dated_message_id> dated_messages;

    scheduller<allocator<new_delete_memory_resource>> sched;

    external_source srcs;

    time t = time_domain<time>::infinity;

    /**
       The latest not infinity simulation time. At begin of the simulation, @c
       last_valid_t equals the begin date. During simulation @c last_valid_t
       stores the latest valid simulation time (neither infinity.
     */
    time last_valid_t = 0.0;

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

    /** Grow models dependant data-array and vectors according to the factor Num
     *  Denum.
     * @return true if success.
     */
    template<int Num, int Denum>
    bool grow_models() noexcept;

    /** Grow models dependant data-array and vectors according to @a capacity.
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

    status connect(block_node_id& port, model_id dst, int port_dst) noexcept;

    template<typename Function, typename... Args>
    void for_each(block_node_id& port, Function&& fn, Args&&... args) noexcept
    {
        block_node* prev = nullptr;

        for (auto* block = nodes.try_to_get(port); block;
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
                            port = undefined<block_node_id>();
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
    void for_each(const block_node_id& port,
                  Function&&           fn,
                  Args&&... args) const noexcept
    {
        for (const auto* block = nodes.try_to_get(port); block;
             block             = nodes.try_to_get(block->next)) {
            for (auto it = block->nodes.begin(); it != block->nodes.end();
                 ++it) {
                if (auto* mdl = models.try_to_get(it->model)) {
                    std::invoke(std::forward<Function>(fn),
                                *mdl,
                                it->port_index,
                                std::forward<Args>(args)...);
                }
            }
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

inline status send_message(simulation&    sim,
                           block_node_id& output_port,
                           real           r1,
                           real           r2 = 0.0,
                           real           r3 = 0.0) noexcept;

/*****************************************************************************
 *
 * Qss1 part
 *
 ****************************************************************************/

template<int QssLevel>
struct abstract_integrator;

template<>
struct abstract_integrator<1> {
    message_id    x[2] = {};
    block_node_id y[1] = {};
    real          dQ;
    real          X;
    real          q;
    real          u;
    time          sigma = time_domain<time>::zero;

    enum port_name { port_x_dot, port_reset };

    struct X_error {};
    struct dQ_error {};

    abstract_integrator() = default;

    abstract_integrator(const abstract_integrator& other) noexcept
      : dQ(other.dQ)
      , X(other.X)
      , q(other.q)
      , u(other.u)
      , sigma(other.sigma)
    {}

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

        if (sigma != zero) {
            if (u == zero)
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
        q     = X;
        sigma = time_domain<time>::zero;
        return success();
    }

    status internal() noexcept
    {
        X += sigma * u;
        q = X;

        sigma = u == zero ? time_domain<time>::infinity : dQ / std::abs(u);

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
    message_id    x[2] = {};
    block_node_id y[1] = {};
    real          dQ;
    real          X;
    real          u;
    real          mu;
    real          q;
    real          mq;
    time          sigma = time_domain<time>::zero;

    enum port_name { port_x_dot, port_reset };

    struct X_error {};
    struct dQ_error {};

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
        const real value_x     = msg[0];
        const real value_slope = msg[1];

        X += (u * e) + (mu / two) * (e * e);
        u  = value_x;
        mu = value_slope;

        if (sigma != zero) {
            q += mq * e;
            const real a = mu / two;
            const real b = u - mq;
            real       c = X - q + dQ;
            real       s;
            sigma = time_domain<time>::infinity;

            if (a == zero) {
                if (b != zero) {
                    s = -c / b;
                    if (s > zero)
                        sigma = s;

                    c = X - q - dQ;
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

                c = X - q - dQ;
                s = (-b + std::sqrt(b * b - four * a * c)) / two / a;
                if ((s > zero) && (s < sigma))
                    sigma = s;

                s = (-b - std::sqrt(b * b - four * a * c)) / two / a;
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

        sigma = mu == zero ? time_domain<time>::infinity
                           : std::sqrt(two * dQ / std::abs(mu));

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
    message_id    x[2] = {};
    block_node_id y[1] = {};
    real          dQ;
    real          X;
    real          u;
    real          mu;
    real          pu;
    real          q;
    real          mq;
    real          pq;
    time          sigma = time_domain<time>::zero;

    enum port_name { port_x_dot, port_reset };

    struct X_error {};
    struct dQ_error {};

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
        constexpr real pi_div_3         = 1.0471975511965976;
        const real     value_x          = msg[0];
        const real     value_slope      = msg[1];
        const real     value_derivative = msg[2];

        X  = X + u * e + (mu * e * e) / two + (pu * e * e * e) / three;
        u  = value_x;
        mu = value_slope;
        pu = value_derivative;

        if (sigma != zero) {
            q      = q + mq * e + pq * e * e;
            mq     = mq + two * pq * e;
            auto a = mu / two - pq;
            auto b = u - mq;
            auto c = X - q - dQ;
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
                c  = c + real(6) * dQ / pu;
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
                    c  = c + two * dQ;
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
                        auto x2 = x1 - two * dQ / b;
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

            if ((std::abs(X - q)) > dQ)
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

        sigma = pu == zero ? time_domain<time>::infinity
                           : std::pow(std::abs(three * dQ / pu), one / three);

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

    message_id    x[1] = {};
    block_node_id y[1] = {};
    time          sigma;

    real value[QssLevel];
    real n;

    abstract_power() noexcept = default;

    abstract_power(const abstract_power& other) noexcept
      : n(other.n)
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
                                    (value[1] * value[1] / two) +
                                  n * std::pow(value[0], n - 1) * value[2]);

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
            auto X = std::pow(value[0], n);
            return { t, X };
        }

        if constexpr (QssLevel == 2) {
            auto X = std::pow(value[0], n);
            auto u = n * std::pow(value[0], n - 1) * value[1];

            return qss_observation(X, u, t, e);
        }

        if constexpr (QssLevel == 3) {
            auto X  = std::pow(value[0], n);
            auto u  = n * std::pow(value[0], n - 1) * value[1];
            auto mu = n * (n - 1) * std::pow(value[0], n - 2) *
                        (value[1] * value[1] / two) +
                      n * std::pow(value[0], n - 1) * value[2];

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

    message_id    x[1] = {};
    block_node_id y[1] = {};
    time          sigma;

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

    message_id    x[PortNumber] = {};
    block_node_id y[1]          = {};
    time          sigma;

    real values[QssLevel * PortNumber] = { 0 };

    abstract_sum() noexcept = default;

    abstract_sum(const abstract_sum& other) noexcept
      : sigma(other.sigma)
    {
        std::copy_n(other.values, std::size(other.values), values);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        if constexpr (QssLevel >= 2) {
            std::fill_n(
              values + PortNumber, std::size(values) - PortNumber, zero);
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

    message_id    x[PortNumber] = {};
    block_node_id y[1]          = {};
    time          sigma;

    real input_coeffs[PortNumber]      = { 0 };
    real values[QssLevel * PortNumber] = { 0 };

    abstract_wsum() noexcept = default;

    abstract_wsum(const abstract_wsum& other) noexcept
      : sigma(other.sigma)
    {
        std::copy_n(
          other.input_coeffs, std::size(other.input_coeffs), input_coeffs);

        std::copy_n(other.values, std::size(other.values), values);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        if constexpr (QssLevel >= 2) {
            std::fill_n(std::begin(values) + PortNumber,
                        std::size(values) - PortNumber,
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
                value += input_coeffs[i] * values[i];

            return send_message(sim, y[0], value);
        }

        if constexpr (QssLevel == 2) {
            real value = zero;
            real slope = zero;

            for (int i = 0; i != PortNumber; ++i) {
                value += input_coeffs[i] * values[i];
                slope += input_coeffs[i] * values[i + PortNumber];
            }

            return send_message(sim, y[0], value, slope);
        }

        if constexpr (QssLevel == 3) {
            real value      = zero;
            real slope      = zero;
            real derivative = zero;

            for (int i = 0; i != PortNumber; ++i) {
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
                value += input_coeffs[i] * values[i];

            return { t, value };
        }

        if constexpr (QssLevel == 2) {
            real value = zero;
            real slope = zero;

            for (int i = 0; i != PortNumber; ++i) {
                value += input_coeffs[i] * values[i];
                slope += input_coeffs[i] * values[i + PortNumber];
            }

            return qss_observation(value, slope, t, e);
        }

        if constexpr (QssLevel == 3) {
            real value      = zero;
            real slope      = zero;
            real derivative = zero;

            for (int i = 0; i != PortNumber; ++i) {
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

template<int QssLevel>
struct abstract_multiplier {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    message_id    x[2] = {};
    block_node_id y[1] = {};
    time          sigma;

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
    message_id x[1] = {};
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
    enum { x_value, x_t, x_add_tr, x_mult_tr };
    enum class option : u8 {
        none             = 0,
        stop_on_error    = 1 << 0,
        ta_use_source    = 1 << 1,
        value_use_source = 1 << 2,
        Count
    };

    message_id    x[4] = {};
    block_node_id y[1] = {};
    time          sigma;
    real          value;

    real             offset;
    source           source_ta;
    source           source_value;
    bitflags<option> flags;

    generator() noexcept = default;

    generator(const generator& other) noexcept
      : sigma(other.sigma)
      , value(other.value)
      , offset(other.offset)
      , source_ta(other.source_ta)
      , source_value(other.source_value)
      , flags(option::stop_on_error,
              option::ta_use_source,
              option::value_use_source)
    {}

    status initialize(simulation& sim) noexcept
    {
        sigma = offset;

        if (flags[option::ta_use_source]) {
            if (flags[option::stop_on_error]) {
                irt_check(initialize_source(sim, source_ta));
            } else {
                (void)initialize_source(sim, source_ta);
            }
        } else {
            sigma = time_domain<time>::infinity;
        }

        if (flags[option::value_use_source]) {
            if (flags[option::stop_on_error]) {
                irt_check(initialize_source(sim, source_value));
            } else {
                (void)initialize_source(sim, source_value);
            }
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
        sigma = time_domain<time>::infinity;

        if (not flags[option::value_use_source]) {
            auto* lst_value = sim.messages.try_to_get(x[x_value]);

            if (lst_value and not lst_value->empty()) {
                for (const auto& msg : *lst_value) {
                    value = msg[0];
                    sigma = r;
                }
            }
        }

        if (not flags[option::ta_use_source]) {
            auto* lst_t       = sim.messages.try_to_get(x[x_t]);
            auto* lst_add_tr  = sim.messages.try_to_get(x[x_add_tr]);
            auto* lst_mult_tr = sim.messages.try_to_get(x[x_mult_tr]);

            real t = time_domain<time>::infinity;
            if (lst_t and not lst_t->empty())
                for (const auto& msg : *lst_t)
                    t = std::min(msg[0], t);

            real add_tr = time_domain<time>::infinity;
            if (lst_add_tr and not lst_add_tr->empty())
                for (const auto& msg : *lst_add_tr)
                    add_tr = std::min(msg[0], add_tr);

            real mult_tr = zero;
            if (lst_mult_tr and not lst_mult_tr->empty())
                for (const auto& msg : *lst_mult_tr)
                    mult_tr = std::max(msg[0], mult_tr);

            if (t >= zero) {
                sigma = t;
            } else {
                if (std::isfinite(add_tr))
                    sigma = r + add_tr;

                if (std::isnormal(mult_tr))
                    sigma = r * mult_tr;
            }

            if (sigma < 0)
                sigma = zero;
        } else {
            real local_sigma = 0;
            real local_value = 0;

            if (flags[option::stop_on_error]) {
                irt_check(update_source(sim, source_ta, local_sigma));
                irt_check(update_source(sim, source_value, local_value));
                sigma = static_cast<real>(local_sigma);
                value = static_cast<real>(local_value);
            } else {
                if (auto ret = update_source(sim, source_ta, local_sigma); !ret)
                    sigma = time_domain<time>::infinity;
                else
                    sigma = static_cast<real>(local_sigma);

                if (auto ret = update_source(sim, source_value, local_value);
                    !ret)
                    value = 0;
                else
                    value = static_cast<real>(local_value);
            }
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
    block_node_id y[1] = {};
    time          sigma;

    enum class init_type : i8 {
        // A constant value initialized at startup of the
        // simulation.
        // Use
        // the @c default_value.
        constant,

        // The numbers of incoming connections on all input ports
        // of the
        // component. The @c default_value is filled via the
        // component
        // to
        // simulation algorithm. Otherwise, the default value is
        // unmodified.
        incoming_component_all,

        // The number of outcoming connections on all output
        // ports of
        // the
        // component. The @c default_value is filled via the
        // component
        // to
        // simulation algorithm. Otherwise, the default value is
        // unmodified.
        outcoming_component_all,

        // The number of incoming connections on the nth input
        // port of
        // the
        // component. Use the @c port attribute to specify the
        // identifier of
        // the port. The @c default_value is filled via the
        // component to
        // simulation algorithm. Otherwise, the default value is
        // unmodified.
        incoming_component_n,

        // The number of incoming connections on the nth output
        // ports of
        // the
        // component. Use the @c port attribute to specify the
        // identifier of
        // the port. The @c default_value is filled via the
        // component to
        // simulation algorithm. Otherwise, the default value is
        // unmodified.
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
    message_id    x[1] = {};
    block_node_id y[3] = {};

    time sigma;
    real lower_threshold;
    real upper_threshold;
    real value[QssLevel];
    bool reach_lower_threshold = false;
    bool reach_upper_threshold = false;

    struct threshold_condition_error {};

    abstract_filter() noexcept = default;

    abstract_filter(const abstract_filter& other) noexcept
      : sigma(other.sigma)
      , lower_threshold(other.lower_threshold)
      , upper_threshold(other.upper_threshold)
      , reach_lower_threshold(other.reach_lower_threshold)
      , reach_upper_threshold(other.reach_upper_threshold)
    {
        std::copy_n(other.value, QssLevel, value);
    }

    status initialize(simulation& /*sim*/) noexcept
    {
        if (lower_threshold >= upper_threshold)
            return new_error(
              simulation_errc::abstract_filter_threshold_condition_error);

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
    message_id    x[PortNumber];
    block_node_id y[1];
    time          sigma = time_domain<time>::infinity;

    bool values[PortNumber];

    bool is_valid      = true;
    bool value_changed = false;

    abstract_logical() noexcept = default;

    status initialize(simulation& /*sim*/) noexcept
    {
        AbstractLogicalTester tester{};
        is_valid = tester(std::begin(values), std::end(values));
        sigma =
          is_valid ? time_domain<time>::zero : time_domain<time>::infinity;
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
    message_id    x[1] = {};
    block_node_id y[1] = {};

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
        value_changed = false;
        auto* lst     = sim.messages.try_to_get(x[0]);

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
        none       = 0,
        use_source = 1 << 0 /**< HSM can use external data in the
                               action part. */
        ,
        Count
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
        i32                 i1    = 0;
        i32                 i2    = 0;
        real                r1    = 0.0;
        real                r2    = 0.0;
        time                timer = time_domain<time>::infinity;
        std::array<real, 4> ports; //<! Stores first part of input message.

        std::array<real, 4> message_values;
        std::array<u8, 4>   message_ports;
        int                 messages = 0;
        source              source_value;

        std::bitset<4> values; //<! Bit storage message available on X port.

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

    state_id         top_state = invalid_state_id;
    bitflags<option> flags;
};

auto get_hierarchical_state_machine(simulation& sim, hsm_id id) noexcept
  -> expected<hierarchical_state_machine*>;

struct hsm_wrapper {
    using hsm      = hierarchical_state_machine;
    using state_id = hsm::state_id;

    message_id    x[4] = {};
    block_node_id y[4] = {};

    hsm::execution exec;

    real sigma;

    u32 id = 0; //!< stores the @a ordinal value of an @a hsm_id
                //!< from the simulation hsms data_array.

    hsm_wrapper() noexcept;
    hsm_wrapper(const hsm_wrapper& other) noexcept;

    status initialize(simulation& sim) noexcept;
    status transition(simulation& sim, time t, time e, time r) noexcept;
    status lambda(simulation& sim) noexcept;
    status finalize(simulation& sim) noexcept;
    observation_message observation(time t, time e) const noexcept;
};

template<int PortNumber>
struct accumulator {
    message_id x[2 * PortNumber] = {};
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

    observation_message observation(time t, time /*e*/) const noexcept
    {
        return { t, number };
    }
};

template<int QssLevel>
struct abstract_cross {
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    message_id    x[4] = {};
    block_node_id y[3] = {};
    time          sigma;

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

        value[0] = threshold - one;

        sigma           = time_domain<time>::infinity;
        last_reset      = time_domain<time>::infinity;
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
    constexpr real pi = 3.141592653589793238462643383279502884;

    constexpr const real mult = two * pi * f0;

    return std::sin(mult * t);
}

inline real square_time_function(real t) noexcept { return t * t; }

inline real time_function(real t) noexcept { return t; }

struct time_func {
    block_node_id y[1] = {};
    time          sigma;

    real offset   = 0;
    real timestep = 0.01;
    real value;
    real (*f)(real) = nullptr;

    time_func() noexcept = default;

    time_func(const time_func& other) noexcept
      : sigma(other.sigma)
      , value(other.value)
      , f(other.f)
    {}

    status initialize(simulation& /*sim*/) noexcept
    {
        sigma = offset;
        value = 0.0;
        return success();
    }

    status transition(simulation& /*sim*/,
                      time t,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        value = (*f)(t);
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
    message_id    x[1] = {};
    block_node_id y[1] = {};
    time          sigma;

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
    message_id       x[1] = {};
    block_node_id    y[1] = {};
    time             sigma;
    dated_message_id fifo = undefined<dated_message_id>();

    source source_ta;
    bool   stop_on_error = false;

    dynamic_queue() noexcept = default;

    dynamic_queue(const dynamic_queue& other) noexcept
      : sigma(other.sigma)
      , fifo(undefined<dated_message_id>())
      , source_ta(other.source_ta)
      , stop_on_error(other.stop_on_error)
    {}

    status initialize(simulation& sim) noexcept
    {
        sigma = time_domain<time>::infinity;
        fifo  = undefined<dated_message_id>();

        if (stop_on_error) {
            irt_check(initialize_source(sim, source_ta));
        } else {
            (void)initialize_source(sim, source_ta);
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
    message_id       x[1] = {};
    block_node_id    y[1] = {};
    time             sigma;
    dated_message_id fifo = undefined<dated_message_id>();
    real             ta   = 1.0;

    source source_ta;
    bool   stop_on_error = false;

    priority_queue() noexcept = default;

    priority_queue(const priority_queue& other) noexcept
      : sigma(other.sigma)
      , fifo(undefined<dated_message_id>())
      , ta(other.ta)
      , source_ta(other.source_ta)
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
            irt_check(initialize_source(sim, source_ta));
        } else {
            (void)initialize_source(sim, source_ta);
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

template<typename Dynamics>
concept dynamics =
  std::is_same_v<Dynamics, qss1_integrator> or
  std::is_same_v<Dynamics, qss1_multiplier> or
  std::is_same_v<Dynamics, qss1_cross> or
  std::is_same_v<Dynamics, qss1_filter> or
  std::is_same_v<Dynamics, qss1_power> or
  std::is_same_v<Dynamics, qss1_square> or
  std::is_same_v<Dynamics, qss1_sum_2> or
  std::is_same_v<Dynamics, qss1_sum_3> or
  std::is_same_v<Dynamics, qss1_sum_4> or
  std::is_same_v<Dynamics, qss1_wsum_2> or
  std::is_same_v<Dynamics, qss1_wsum_3> or
  std::is_same_v<Dynamics, qss1_wsum_4> or
  std::is_same_v<Dynamics, qss2_integrator> or
  std::is_same_v<Dynamics, qss2_multiplier> or
  std::is_same_v<Dynamics, qss2_cross> or
  std::is_same_v<Dynamics, qss2_filter> or
  std::is_same_v<Dynamics, qss2_power> or
  std::is_same_v<Dynamics, qss2_square> or
  std::is_same_v<Dynamics, qss2_sum_2> or
  std::is_same_v<Dynamics, qss2_sum_3> or
  std::is_same_v<Dynamics, qss2_sum_4> or
  std::is_same_v<Dynamics, qss2_wsum_2> or
  std::is_same_v<Dynamics, qss2_wsum_3> or
  std::is_same_v<Dynamics, qss2_wsum_4> or
  std::is_same_v<Dynamics, qss3_integrator> or
  std::is_same_v<Dynamics, qss3_multiplier> or
  std::is_same_v<Dynamics, qss3_cross> or
  std::is_same_v<Dynamics, qss3_filter> or
  std::is_same_v<Dynamics, qss3_power> or
  std::is_same_v<Dynamics, qss3_square> or
  std::is_same_v<Dynamics, qss3_sum_2> or
  std::is_same_v<Dynamics, qss3_sum_3> or
  std::is_same_v<Dynamics, qss3_sum_4> or
  std::is_same_v<Dynamics, qss3_wsum_2> or
  std::is_same_v<Dynamics, qss3_wsum_3> or
  std::is_same_v<Dynamics, qss3_wsum_4> or std::is_same_v<Dynamics, counter> or
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
    real tl     = 0.0;
    real tn     = time_domain<time>::infinity;
    u32  handle = invalid_heap_handle;

    observer_id   obs_id = observer_id{ 0 };
    dynamics_type type;

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

/**
   @brief dispatch the callable @c f with @c args argument
   according to @c type.

   @param type
   @param f
   @param args

   @verbatim
   dispatch(mdl.type, []<typename Tag>(
       const Tag t, const float x, const float y) -> bool {
           if constexpr(std::is_same_v(t, hsm_wrapper_tag)) {
               // todo
           }
       }));
   @endverbatim
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

inline expected<message_id> get_input_port(model& src, int port_src) noexcept;

inline status get_input_port(model& src, int port_src, message_id*& p) noexcept;
inline expected<block_node_id> get_output_port(model& dst,
                                               int    port_dst) noexcept;
inline status                  get_output_port(model&          dst,
                                               int             port_dst,
                                               block_node_id*& p) noexcept;

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
                dst_dyn.x[i] = undefined<message_id>();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dst_dyn.y); i != e; ++i)
                dst_dyn.y[i] = undefined<block_node_id>();
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

inline observer::observer() noexcept
  : buffer(default_buffer_size)
  , linearized_buffer(default_linearized_buffer_size)
{}

inline observer::observer(
  const buffer_size_t            buffer_size,
  const linearized_buffer_size_t linearized_buffer_size) noexcept
  : buffer(*buffer_size)
  , linearized_buffer(*linearized_buffer_size)
{}

inline void observer::init(
  const buffer_size_t            buffer_size,
  const linearized_buffer_size_t linearized_buffer_size,
  const float                    ts) noexcept
{
    debug::ensure(time_step > 0.f);

    buffer.clear();
    buffer.reserve(*buffer_size);
    linearized_buffer.clear();
    linearized_buffer.reserve(*linearized_buffer_size);
    time_step = ts <= zero ? 1e-2f : time_step;
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

inline void observer::update(const observation_message& msg) noexcept
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

    if (buffer.available() <= 1)
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

inline status send_message(simulation&    sim,
                           block_node_id& output_port,
                           real           r1,
                           real           r2,
                           real           r3) noexcept
{
    block_node* prev = nullptr;

    for (auto* block = sim.nodes.try_to_get(output_port); block;
         block       = sim.nodes.try_to_get(block->next)) {

        for (auto it = block->nodes.begin(); it != block->nodes.end();) {
            if (auto* mdl = sim.models.try_to_get(it->model); not mdl) {
                block->nodes.swap_pop_back(it);
            } else {
                if (not sim.emitting_output_ports.can_alloc(1))
                    return new_error(
                      simulation_errc::emitting_output_ports_full);

                auto& output_message = sim.emitting_output_ports.emplace_back();
                output_message.msg[0] = r1;
                output_message.msg[1] = r2;
                output_message.msg[2] = r3;
                output_message.model  = it->model;
                output_message.port   = it->port_index;

                ++it;
            }
        }

        if (block->nodes.empty()) { /* If the block is empty, free the
                                       block from the linked list. */

            if (prev != nullptr) {
                prev->next = block->next;
                sim.nodes.free(*block);
                block = prev;
            } else {
                if (auto* next_block = sim.nodes.try_to_get(block->next)) {
                    block->nodes = next_block->nodes;
                    block->next  = next_block->next;
                    sim.nodes.free(*next_block);
                } else {
                    sim.nodes.free(*block);
                    output_port = undefined<block_node_id>();
                    break;
                }
            }
        } else {
            prev = block;
        }
    }

    return success();
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
inline auto get_hierarchical_state_machine(simulation& sim,
                                           const u32   idx) noexcept
  -> expected<hierarchical_state_machine*>
{
    if (auto* hsm = sim.hsms.try_to_get_from_pos(idx); hsm)
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
  const simulation_reserve_definition&      res,
  const external_source_reserve_definition& p_srcs) noexcept
  : emitting_output_ports(res.connections.value())
  , immediate_models(res.models.value())
  , immediate_observers(res.models.value())
  , parameters(res.models.value())
  , models(res.models.value())
  , hsms(res.hsms.value())
  , observers(res.models.value())
  , nodes(res.connections.value())
  , messages(res.connections.value())
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

    return emitting_output_ports.resize(req) and nodes.reserve(req) and
           messages.reserve(req);
}

inline bool simulation::grow_connections_to(
  std::integral auto capacity) noexcept
{
    if (std::cmp_less(capacity, nodes.capacity()))
        return true;

    return emitting_output_ports.resize(capacity) and
           nodes.reserve(capacity) and messages.reserve(capacity);
}

inline void simulation::destroy() noexcept
{
    models.destroy();
    parameters.destroy();
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

    messages.clear();
    dated_messages.clear();

    emitting_output_ports.clear();
    immediate_models.clear();
    immediate_observers.clear();
}

//! @brief cleanup simulation and destroy all models and
//! connections
inline void simulation::clear() noexcept
{
    clean();

    nodes.clear();
    models.clear();
    observers.clear();
}

inline void simulation::observe(model& mdl, observer& obs) const noexcept
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

    sched.free(*mdl);
    models.free(*mdl);
}

template<typename Dynamics>
void simulation::do_deallocate(Dynamics& dyn) noexcept
{
    if constexpr (has_output_port<Dynamics>) {
        for (auto& elem : dyn.y) {
            nodes.free(elem);
            elem = undefined<block_node_id>();
        }
    }

    if constexpr (has_input_port<Dynamics>) {
        for (auto& elem : dyn.x) {
            messages.free(elem);
            elem = undefined<message_id>();
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

inline status simulation::connect(block_node_id& port,
                                  model_id       dst,
                                  int            port_dst) noexcept
{
    if (auto* block = nodes.try_to_get(port); not block) {
        if (not nodes.can_alloc(1) and not nodes.grow<2, 1>())
            return new_error(simulation_errc::connection_container_full);

        auto& new_block = nodes.alloc();
        new_block.nodes.emplace_back(dst, port_dst);
        port = nodes.get_id(new_block);
        return success();
    } else {
        block_node* prev = nullptr;
        for (auto* block = nodes.try_to_get(port); block;
             block       = nodes.try_to_get(block->next)) {

            if (block->nodes.can_alloc(1)) {
                block->nodes.emplace_back(dst, port_dst);
                return success();
            }

            prev = block;
        }

        debug::ensure(prev != nullptr);

        if (not nodes.can_alloc(1))
            return new_error(simulation_errc::connection_container_full);

        auto& new_block = nodes.alloc();
        new_block.nodes.emplace_back(dst, port_dst);
        prev->next = nodes.get_id(new_block);
        return success();
    }
}

inline bool simulation::can_connect(const model& src,
                                    int          port_src,
                                    const model& dst,
                                    int          port_dst) const noexcept
{
    return dispatch(src, [&]<typename Dynamics>(Dynamics& dyn) -> bool {
        if constexpr (has_output_port<Dynamics>) {
            for (const auto* block = nodes.try_to_get(dyn.y[port_src]); block;
                 block             = nodes.try_to_get(block->next)) {

                for (const auto& elem : block->nodes)
                    if (elem.model == get_id(dst) and
                        elem.port_index == port_dst)
                        return false;
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
            block_node* prev = nullptr;

            for (auto* block = nodes.try_to_get(dyn.y[port_src]); block;
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
                        if (auto* next_block = nodes.try_to_get(block->next)) {
                            block->nodes = next_block->nodes;
                            block->next  = next_block->next;
                            nodes.free(*next_block);
                        } else {
                            nodes.free(*block);
                            dyn.y[port_src] = undefined<block_node_id>();
                            break;
                        }
                    }
                } else {
                    prev = block;
                }
            }
        }
    });
}

inline status simulation::initialize() noexcept
{
    debug::ensure(std::isfinite(t));

    last_valid_t = t;

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
        for (int i = 0, e = length(dyn.x); i != e; ++i) {
            if (not messages.try_to_get(dyn.x[i]))
                dyn.x[i] = undefined<message_id>();
        }
    }

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

        debug::ensure(sched.is_in_tree(mdl->handle));
        sched.update(*mdl, t);

        if (not messages.can_alloc(1))
            return new_error(simulation_errc::messages_container_full);

        auto  port = emitting_output_ports[i].port;
        auto& msg  = emitting_output_ports[i].msg;

        dispatch(*mdl, [&]<typename Dynamics>(Dynamics& dyn) {
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
    if (t = sched.tn(); time_domain<time>::is_infinity(t))
        return success();

    sched.pop(immediate_models);

    emitting_output_ports.clear();
    for (const auto id : immediate_models)
        if (auto* mdl = models.try_to_get(id); mdl)
            irt_check(make_transition(*mdl, t));

    fn(std::as_const(*this),
       std::span(immediate_models.data(), immediate_models.size()),
       std::forward<Args>(args)...);

    for (int i = 0, e = length(emitting_output_ports); i != e; ++i) {
        auto* mdl = models.try_to_get(emitting_output_ports[i].model);
        if (!mdl)
            continue;

        debug::ensure(sched.is_in_tree(mdl->handle));
        sched.update(*mdl, t);

        if (not messages.can_alloc(1))
            return new_error(simulation_errc::messages_container_full);

        auto  port = emitting_output_ports[i].port;
        auto& msg  = emitting_output_ports[i].msg;

        dispatch(*mdl, [&]<typename Dynamics>(Dynamics& dyn) {
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
            auto* lst = sim.messages.try_to_get(x[i]);
            if (lst and not lst->empty()) {
                exec.values.set(i, true);

                for (auto& elem : *lst)
                    exec.ports[i] = elem[0];
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
            dyn.x[i] = undefined<message_id>();

    if constexpr (has_output_port<Dynamics>)
        for (int i = 0, e = length(dyn.y); i != e; ++i)
            dyn.y[i] = undefined<block_node_id>();

    if constexpr (std::is_same_v<Dynamics, hsm_wrapper>)
        dyn.id = 0;

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
                dyn.x[i] = undefined<message_id>();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = undefined<block_node_id>();
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
                dyn.x[i] = undefined<message_id>();

        if constexpr (has_output_port<Dynamics>)
            for (int i = 0, e = length(dyn.y); i != e; ++i)
                dyn.y[i] = undefined<block_node_id>();

        if constexpr (std::is_same_v<Dynamics, hsm_wrapper>) {
            dyn.id = 0u;
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

inline expected<message_id> get_input_port(model& src, int port_src) noexcept
{
    return dispatch(
      src, [&]<typename Dynamics>(Dynamics& dyn) -> expected<message_id> {
          if constexpr (has_input_port<Dynamics>) {
              if (port_src >= 0 && port_src < length(dyn.x)) {
                  return dyn.x[port_src];
              }
          }

          return new_error(simulation_errc::input_port_error);
      });
}

inline status get_input_port(model& src, int port_src, message_id*& p) noexcept
{
    return dispatch(src,
                    [port_src, &p]<typename Dynamics>(Dynamics& dyn) -> status {
                        if constexpr (has_input_port<Dynamics>) {
                            if (port_src >= 0 && port_src < length(dyn.x)) {
                                p = &dyn.x[port_src];
                                return success();
                            }
                        }

                        return new_error(simulation_errc::input_port_error);
                    });
}

inline expected<block_node_id> get_output_port(model& dst,
                                               int    port_dst) noexcept
{
    return dispatch(
      dst, [&]<typename Dynamics>(Dynamics& dyn) -> expected<block_node_id> {
          if constexpr (has_output_port<Dynamics>) {
              if (port_dst >= 0 && port_dst < length(dyn.y))
                  return dyn.y[port_dst];
          }

          return new_error(simulation_errc::output_port_error);
      });
}

inline status get_output_port(model&          dst,
                              int             port_dst,
                              block_node_id*& p) noexcept
{
    return dispatch(dst,
                    [port_dst, &p]<typename Dynamics>(Dynamics& dyn) -> status {
                        if constexpr (has_output_port<Dynamics>) {
                            if (port_dst >= 0 && port_dst < length(dyn.y)) {
                                p = &dyn.y[port_dst];
                                return success();
                            }
                        }

                        return new_error(simulation_errc::output_port_error);
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
                    return new_error(
                      simulation_errc::dated_messages_container_full);

                ar->push_head({ irt::real(t + ta), msg[0], msg[1], msg[2] });
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
                    return new_error(
                      simulation_errc::dated_messages_container_full);

                real ta = zero;
                if (stop_on_error) {
                    irt_check(update_source(sim, source_ta, ta));
                    ar->push_head(
                      { t + static_cast<real>(ta), msg[0], msg[1], msg[2] });
                } else {
                    if (auto ret = update_source(sim, source_ta, ta); !ret)
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

        auto* lst = sim.messages.try_to_get(x[0]);

        if (lst) {
            for (const auto& msg : *lst) {
                real value = zero;

                if (stop_on_error) {
                    irt_check(update_source(sim, source_ta, value));

                    if (auto ret =
                          try_to_insert(sim, static_cast<real>(value) + t, msg);
                        !ret)
                        return new_error(
                          simulation_errc::dated_messages_container_full);
                } else {
                    if (auto ret = update_source(sim, source_ta, value); !ret) {
                        if (auto ret2 = try_to_insert(
                              sim, static_cast<real>(value) + t, msg);
                            !ret2)
                            return new_error(
                              simulation_errc::dated_messages_container_full);
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

} // namespace irt

#endif
