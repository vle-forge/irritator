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

#include <string_view>
#include <type_traits>

#include <cmath>
#include <cstdint>
#include <cstring>

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

static inline bool is_fatal_breakpoint = true;

}

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

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using sz = size_t;

inline u16
make_halfword(u8 a, u8 b) noexcept
{
    return static_cast<u16>((a << 8) | b);
}

inline void
unpack_halfword(u16 halfword, u8* a, u8* b) noexcept
{
    *a = static_cast<u8>((halfword >> 8) & 0xff);
    *b = static_cast<u8>(halfword & 0xff);
}

inline u32
make_word(u16 a, u16 b) noexcept
{
    return (static_cast<u32>(a) << 16) | static_cast<u32>(b);
}

inline void
unpack_word(u32 word, u16* a, u16* b) noexcept
{
    *a = static_cast<u16>((word >> 16) & 0xffff);
    *b = static_cast<u16>(word & 0xffff);
}

inline u64
make_doubleword(u32 a, u32 b) noexcept
{
    return (static_cast<u64>(a) << 32) | static_cast<u64>(b);
}

inline void
unpack_doubleword(u64 doubleword, u32* a, u32* b) noexcept
{
    *a = static_cast<u32>((doubleword >> 32) & 0xffffffff);
    *b = static_cast<u32>(doubleword & 0xffffffff);
}

template<typename Integer>
constexpr typename std::make_unsigned<Integer>::type
to_unsigned(Integer value)
{
    irt_assert(value >= 0);
    return static_cast<typename std::make_unsigned<Integer>::type>(value);
}

template<class C>
constexpr int
length(const C& c) noexcept
{
    return static_cast<int>(c.size());
}

template<class T, size_t N>
constexpr int
length(const T (&array)[N]) noexcept
{
    (void)array;

    return static_cast<int>(N);
}

template<typename Identifier>
constexpr Identifier
undefined() noexcept
{
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    return static_cast<Identifier>(0);
}

template<typename Identifier>
constexpr bool
is_undefined(Identifier id) noexcept
{
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    return id == undefined<Identifier>();
}

template<typename Identifier>
constexpr bool
is_defined(Identifier id) noexcept
{
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    return id != undefined<Identifier>();
}

/**
 * @brief Enummeration to integral
 */
template<class Enum, class Integer = typename std::underlying_type<Enum>::type>
constexpr Integer
ordinal(Enum e) noexcept
{
    static_assert(std::is_enum<Enum>::value,
                  "Identifier must be a enumeration");
    return static_cast<Integer>(e);
}

/**
 * @brief Integral to enumeration
 */
template<class Enum, class Integer = typename std::underlying_type<Enum>::type>
constexpr Enum
enum_cast(Integer i) noexcept
{
    static_assert(std::is_enum<Enum>::value,
                  "Identifier must be a enumeration");
    return static_cast<Enum>(i);
}

/**
 * @brief returns an iterator to the result or end if not found
 *
 * Binary search function which returns an iterator to the result or end if
 * not found using the lower_bound standard function.
 */
template<typename Iterator, typename T>
Iterator
binary_find(Iterator begin, Iterator end, const T& value)
{
    Iterator i = std::lower_bound(begin, end, value);
    if (i != end && !(value < *i))
        return i;
    else
        return end;
}

/**
 * @brief returns an iterator to the result or end if not found
 *
 * Binary search function which returns an iterator to the result or end if
 * not found using the lower_bound standard function.
 */
template<typename Iterator, typename T, typename Compare>
Iterator
binary_find(Iterator begin, Iterator end, const T& value, Compare comp)
{
    Iterator i = std::lower_bound(begin, end, value, comp);
    if (i != end && !comp(*i, value))
        return i;
    else
        return end;
}

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
    simulation_not_enough_memory_message_list_allocator,
    simulation_not_enough_memory_input_port_list_allocator,
    simulation_not_enough_memory_output_port_list_allocator,
    data_array_init_capacity_error,
    data_array_not_enough_memory,
    data_array_archive_init_capacity_error,
    data_array_archive_not_enough_memory,
    array_init_capacity_zero,
    array_init_capacity_too_big,
    array_init_not_enough_memory,
    vector_init_capacity_zero,
    vector_init_capacity_too_big,
    vector_init_not_enough_memory,
    source_unknown,
    source_empty,
    dynamics_unknown_id,
    dynamics_unknown_port_id,
    dynamics_not_enough_memory,
    model_connect_output_port_unknown,
    model_connect_input_port_unknown,
    model_connect_already_exist,
    model_connect_bad_dynamics,
    model_queue_bad_ta,
    model_queue_empty_allocator,
    model_queue_full,
    model_dynamic_queue_source_is_null,
    model_dynamic_queue_empty_allocator,
    model_dynamic_queue_full,
    model_priority_queue_source_is_null,
    model_priority_queue_empty_allocator,
    model_priority_queue_full,
    model_integrator_dq_error,
    model_integrator_X_error,
    model_integrator_internal_error,
    model_integrator_output_error,
    model_integrator_running_without_x_dot,
    model_integrator_ta_with_bad_x_dot,
    model_generator_null_ta_source,
    model_generator_empty_ta_source,
    model_generator_null_value_source,
    model_generator_empty_value_source,
    model_quantifier_bad_quantum_parameter,
    model_quantifier_bad_archive_length_parameter,
    model_quantifier_shifting_value_neg,
    model_quantifier_shifting_value_less_1,
    model_time_func_bad_init_message,
    model_flow_bad_samplerate,
    model_flow_bad_data,
    gui_not_enough_memory,
    io_not_enough_memory,
    io_file_format_error,
    io_file_format_source_number_error,
    io_file_source_full,
    io_file_format_model_error,
    io_file_format_model_number_error,
    io_file_format_model_unknown,
    io_file_format_dynamics_unknown,
    io_file_format_dynamics_limit_reach,
    io_file_format_dynamics_init_error
};

constexpr i8
status_last() noexcept
{
    return static_cast<i8>(status::io_file_format_dynamics_init_error);
}

constexpr sz
status_size() noexcept
{
    return static_cast<sz>(status_last() + static_cast<i8>(1));
}

constexpr bool
is_success(status s) noexcept
{
    return s == status::success;
}

constexpr bool
is_bad(status s) noexcept
{
    return s != status::success;
}

template<typename... Args>
constexpr bool
is_status_equal(status s, Args... args) noexcept
{
    return ((s == args) || ... || false);
}

inline status
check_return(status s) noexcept
{
    if (s != status::success)
        irt_breakpoint();

    return s;
}

template<typename T, typename... Args>
constexpr bool
match(const T& s, Args... args) noexcept
{
    return ((s == args) || ... || false);
}

template<class T, class... Rest>
constexpr bool
are_all_same() noexcept
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
 * Definition of a lightweight std::function
 *
 ****************************************************************************/

template<class F>
class function_ref;

template<class R, class... Args>
class function_ref<R(Args...)>
{
public:
    constexpr function_ref() noexcept = default;

    /// Creates a `function_ref` which refers to the same callable as `rhs`.
    constexpr function_ref(const function_ref<R(Args...)>& rhs) noexcept =
      default;

    /// Constructs a `function_ref` referring to `f`.
    ///
    /// \synopsis template <typename F> constexpr function_ref(F &&f) noexcept
    template<
      typename F,
      std::enable_if_t<!std::is_same<std::decay_t<F>, function_ref>::value &&
                       std::is_invocable_r<R, F&&, Args...>::value>* = nullptr>
    constexpr function_ref(F&& f) noexcept
      : obj(const_cast<void*>(reinterpret_cast<const void*>(std::addressof(f))))
    {
        cb = [](void* obj, Args... args) -> R {
            return std::invoke(
              *reinterpret_cast<typename std::add_pointer<F>::type>(obj),
              std::forward<Args>(args)...);
        };
    }

    /// Makes `*this` refer to the same callable as `rhs`.
    constexpr function_ref<R(Args...)>& operator=(
      const function_ref<R(Args...)>& rhs) noexcept = default;

    /// Makes `*this` refer to `f`.
    ///
    /// \synopsis template <typename F> constexpr function_ref &operator=(F &&f)
    /// noexcept;
    template<
      typename F,
      std::enable_if_t<std::is_invocable_r<R, F&&, Args...>::value>* = nullptr>
    constexpr function_ref<R(Args...)>& operator=(F&& f) noexcept
    {
        obj = reinterpret_cast<void*>(std::addressof(f));
        cb = [](void* obj, Args... args) {
            return std::invoke(
              *reinterpret_cast<typename std::add_pointer<F>::type>(obj),
              std::forward<Args>(args)...);
        };

        return *this;
    }

    constexpr void swap(function_ref<R(Args...)>& rhs) noexcept
    {
        std::swap(obj, rhs.obj);
        std::swap(cb, rhs.cb);
    }

    R operator()(Args... args) const
    {
        return cb(obj, std::forward<Args>(args)...);
    }

    constexpr bool empty() const noexcept
    {
        return obj == nullptr;
    }

    constexpr void reset() noexcept
    {
        obj = nullptr;
        cb = nullptr;
    }

private:
    void* obj = nullptr;
    R (*cb)(void*, Args...) = nullptr;
};

/// Swaps the referred callables of `lhs` and `rhs`.
template<typename R, typename... Args>
constexpr void
swap(function_ref<R(Args...)>& lhs, function_ref<R(Args...)>& rhs) noexcept
{
    lhs.swap(rhs);
}

template<typename R, typename... Args>
function_ref(R (*)(Args...)) -> function_ref<R(Args...)>;

template<typename R, typename... Args>
function_ref(R (*)(Args...) noexcept) -> function_ref<R(Args...) noexcept>;

/*****************************************************************************
 *
 * Allocator
 *
 ****************************************************************************/

using global_alloc_function_type = function_ref<void*(sz size)>;
using global_free_function_type = function_ref<void(void* ptr)>;

static inline void*
malloc_wrapper(sz size)
{
    return std::malloc(size);
}

static inline void
free_wrapper(void* ptr)
{
    if (ptr)
        std::free(ptr);
}

static inline global_alloc_function_type g_alloc_fn{ malloc_wrapper };
static inline global_free_function_type g_free_fn{ free_wrapper };

/*****************************************************************************
 *
 * Definition of Time
 *
 * TODO:
 * - enable template definition of float or [float|double,bool absolute]
 *   representation of time?
 *
 ****************************************************************************/

using time = double;

template<typename T>
struct time_domain
{};

template<>
struct time_domain<time>
{
    using time_type = time;

    static constexpr const double infinity =
      std::numeric_limits<double>::infinity();
    static constexpr const double negative_infinity =
      -std::numeric_limits<double>::infinity();
    static constexpr const double zero = 0;

    static constexpr bool is_infinity(time t) noexcept
    {
        return t == infinity || t == negative_infinity;
    }

    static constexpr bool is_zero(time t) noexcept
    {
        return t == zero;
    }
};

/*****************************************************************************
 *
 * Small string
 *
 ****************************************************************************/

template<size_t length = 8>
class small_string
{
    char buffer_[length];
    u8 size_;

public:
    using iterator = char*;
    using const_iterator = const char*;
    using size_type = u8;

    static_assert(length > size_t{ 1 } && length < size_t{ 254 });

    constexpr small_string() noexcept
    {
        clear();
    }

    constexpr small_string(const small_string& str) noexcept
    {
        std::copy_n(str.buffer_, str.size_, buffer_);
        buffer_[str.size_] = '\0';
        size_ = str.size_;
    }

    constexpr small_string(small_string&& str) noexcept
    {
        std::copy_n(str.buffer_, str.size_, buffer_);
        buffer_[str.size_] = '\0';
        size_ = str.size_;
        str.clear();
    }

    constexpr small_string& operator=(const small_string& str) noexcept
    {
        if (&str != this) {
            std::copy_n(str.buffer_, str.size_, buffer_);
            buffer_[str.size_] = '\0';
            size_ = str.size_;
        }

        return *this;
    }

    constexpr small_string& operator=(small_string&& str) noexcept
    {
        if (&str != this) {
            std::copy_n(str.buffer_, str.size_, buffer_);
            buffer_[str.size_] = '\0';
            size_ = str.size_;
        }

        return *this;
    }

    constexpr small_string(const char* str) noexcept
    {
        std::strncpy(buffer_, str, length - 1);
        buffer_[length - 1] = '\0';
        size_ = static_cast<u8>(std::strlen(buffer_));
    }

    constexpr small_string(const std::string_view str) noexcept
    {
        assign(str);
    }

    constexpr bool empty() const noexcept
    {
        constexpr unsigned char zero{ 0 };

        return zero == size_;
    }

    constexpr void size(std::size_t sz) noexcept
    {
        size_ = static_cast<u8>(std::min(sz, length - 1));
        buffer_[size_] = '\0';
    }

    constexpr std::size_t size() const noexcept
    {
        return size_;
    }

    constexpr std::size_t capacity() const noexcept
    {
        return length;
    }

    constexpr void assign(const std::string_view str) noexcept
    {
        const size_t copy_length = std::min(str.size(), length - 1);

        std::memcpy(buffer_, str.data(), copy_length);
        buffer_[copy_length] = '\0';

        size_ = static_cast<u8>(copy_length);
    }

    constexpr std::string_view sv() const noexcept
    {
        return { buffer_, size_ };
    }

    constexpr void clear() noexcept
    {
        std::fill_n(buffer_, length, '\0');
        size_ = 0;
    }

    constexpr const char* c_str() const noexcept
    {
        return buffer_;
    }

    constexpr iterator begin() noexcept
    {
        return buffer_;
    }

    constexpr iterator end() noexcept
    {
        return buffer_ + size_;
    }

    constexpr const_iterator begin() const noexcept
    {
        return buffer_;
    }

    constexpr const_iterator end() const noexcept
    {
        return buffer_ + size_;
    }

    constexpr bool operator==(const small_string& rhs) const noexcept
    {
        return std::strncmp(buffer_, rhs.buffer_, length) == 0;
    }

    constexpr bool operator!=(const small_string& rhs) const noexcept
    {
        return std::strncmp(buffer_, rhs.buffer_, length) != 0;
    }

    constexpr bool operator>(const small_string& rhs) const noexcept
    {
        return std::strncmp(buffer_, rhs.buffer_, length) > 0;
    }

    constexpr bool operator<(const small_string& rhs) const noexcept
    {
        return std::strncmp(buffer_, rhs.buffer_, length) < 0;
    }

    constexpr bool operator==(const char* rhs) const noexcept
    {
        return std::strncmp(buffer_, rhs, length) == 0;
    }

    constexpr bool operator!=(const char* rhs) const noexcept
    {
        return std::strncmp(buffer_, rhs, length) != 0;
    }

    constexpr bool operator>(const char* rhs) const noexcept
    {
        return std::strncmp(buffer_, rhs, length) > 0;
    }

    constexpr bool operator<(const char* rhs) const noexcept
    {
        return std::strncmp(buffer_, rhs, length) < 0;
    }
};

/*****************************************************************************
 *
 * value
 *
 ****************************************************************************/

struct message
{
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    double real[4];

    constexpr size_type size() const noexcept
    {
        return real[3] ? 4u : real[2] ? 3u : real[1] ? 2u : real[0] ? 1u : 0u;
    }

    constexpr difference_type ssize() const noexcept
    {
        return real[3] ? 4 : real[2] ? 3 : real[1] ? 2 : real[0] ? 1 : 0;
    }

    constexpr message() noexcept
      : real{ 0., 0., 0., 0. }
    {}

    template<typename... Args>
    constexpr message(Args&&... args)
      : real{ std::forward<Args>(args)... }
    {
        auto size = sizeof...(args);
        for (; size != std::size(real); ++size)
            real[size] = 0.;
    }

    constexpr double operator[](const difference_type i) const noexcept
    {
        return real[i];
    }

    constexpr double& operator[](const difference_type i) noexcept
    {
        return real[i];
    }

    constexpr void reset() noexcept
    {
        std::fill_n(std::data(real), std::size(real), 0.);
    }
};

struct dated_message
{
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    double real[5];

    constexpr dated_message() noexcept
      : real{ 0., 0., 0., 0., 0. }
    {}

    template<typename... Args>
    constexpr dated_message(Args&&... args)
      : real{ std::forward<Args>(args)... }
    {
        auto size = sizeof...(args);
        for (; size != std::size(real); ++size)
            real[size] = 0.;
    }

    constexpr double operator[](const difference_type i) const noexcept
    {
        return real[i];
    }

    constexpr double& operator[](const difference_type i) noexcept
    {
        return real[i];
    }

    constexpr void reset() noexcept
    {
        std::fill_n(std::data(real), std::size(real), 0.);
    }

    inline bool operator<(const dated_message& rhs) const noexcept
    {
        return real[0] < rhs.real[0];
    }

    inline bool operator==(const dated_message& rhs) const noexcept
    {
        return real[0] == rhs.real[0];
    }
};

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

    union block
    {
        block* next;
        typename std::aligned_storage<sizeof(T), alignof(T)>::type storage;
    };

private:
    block* blocks{ nullptr };    // contains all preallocated blocks
    block* free_head{ nullptr }; // a free list
    sz size{ 0 };                // number of active elements allocated
    sz max_size{ 0 }; // number of elements allocated (with free_head)
    sz capacity{ 0 }; // capacity of the allocator

public:
    block_allocator() = default;

    block_allocator(const block_allocator&) = delete;
    block_allocator& operator=(const block_allocator&) = delete;

    ~block_allocator() noexcept
    {
        if (blocks)
            g_free_fn(blocks);
    }

    status init(std::size_t new_capacity) noexcept
    {
        if (new_capacity == 0)
            return status::block_allocator_bad_capacity;

        if (new_capacity != capacity) {
            if (blocks)
                g_free_fn(blocks);

            blocks =
              static_cast<block*>(g_alloc_fn(new_capacity * sizeof(block)));
            if (blocks == nullptr)
                return status::block_allocator_not_enough_memory;
        }

        size = 0;
        max_size = 0;
        capacity = new_capacity;
        free_head = nullptr;

        return status::success;
    }

    void reset() noexcept
    {
        if (capacity > 0) {
            size = 0;
            max_size = 0;
            free_head = nullptr;
        }
    }

    T* alloc() noexcept
    {
        block* new_block = nullptr;

        if (free_head != nullptr) {
            new_block = free_head;
            free_head = free_head->next;
        } else {
            irt_assert(max_size < capacity);
            new_block = reinterpret_cast<block*>(&blocks[max_size++]);
        }
        ++size;

        return reinterpret_cast<T*>(new_block);
    }

    bool can_alloc() noexcept
    {
        return free_head != nullptr || max_size < capacity;
    }

    void free(T* n) noexcept
    {
        irt_assert(n);

        block* ptr = reinterpret_cast<block*>(n);

        ptr->next = free_head;
        free_head = ptr;

        --size;

        if (size == 0) {         // A special part: if it no longer exists
            max_size = 0;        // we reset the free list and the number
            free_head = nullptr; // of elements allocated.
        }
    }

    bool can_alloc(size_t number) const noexcept
    {
        return number + size < capacity;
    }
};

template<typename T>
class shared_flat_list
{
public:
    struct node_type
    {
        T value;
        node_type* next = nullptr;
    };

public:
    using allocator_type = block_allocator<node_type>;
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;

    class iterator
    {
    private:
        node_type* node{ nullptr };

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        using difference_type = std::ptrdiff_t;

        iterator() noexcept = default;

        iterator(node_type* n) noexcept
          : node(n)
        {}

        iterator(const iterator& other) noexcept
          : node(other.node)
        {}

        iterator& operator=(const iterator& other) noexcept
        {
            node = other.node;
            return *this;
        }

        T& operator*() noexcept
        {
            return node->value;
        }

        T* operator->() noexcept
        {
            return &node->value;
        }

        iterator operator++() noexcept
        {
            if (node != nullptr)
                node = node->next;

            return *this;
        }

        iterator operator++(int) noexcept
        {
            iterator tmp(*this);

            if (node != nullptr)
                node = node->next;

            return tmp;
        }

        bool operator==(const iterator& other) const noexcept
        {
            return node == other.node;
        }

        bool operator!=(const iterator& other) const noexcept
        {
            return node != other.node;
        }

        void swap(iterator& other) noexcept
        {
            std::swap(node, other.node);
        }

        friend class shared_flat_list<T>;
    };

    class const_iterator
    {
    private:
        const node_type* node{ nullptr };

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        using difference_type = std::ptrdiff_t;

        const_iterator() noexcept = default;

        const_iterator(node_type* n) noexcept
          : node(n)
        {}

        const_iterator(const const_iterator& other) noexcept
          : node(other.node)
        {}

        const_iterator& operator=(const const_iterator& other) noexcept
        {
            node = other.node;
            return *this;
        }

        const T& operator*() noexcept
        {
            return node->value;
        }

        const T* operator->() noexcept
        {
            return &node->value;
        }

        const_iterator operator++() noexcept
        {
            if (node != nullptr)
                node = node->next;

            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            const_iterator tmp(*this);

            if (node != nullptr)
                node = node->next;

            return tmp;
        }

        bool operator==(const const_iterator& other) const noexcept
        {
            return node == other.node;
        }

        bool operator!=(const const_iterator& other) const noexcept
        {
            return node != other.node;
        }

        void swap(const_iterator& other) noexcept
        {
            std::swap(node, other.node);
        }

        friend class shared_flat_list<T>;
    };

private:
    node_type* node{ nullptr };

public:
    shared_flat_list() = default;

    shared_flat_list(const shared_flat_list& other) = delete;
    shared_flat_list& operator=(const shared_flat_list& other) = delete;
    shared_flat_list(shared_flat_list&& other) = delete;
    shared_flat_list& operator=(shared_flat_list&& other) = delete;

    ~shared_flat_list() noexcept = default;

    void clear(allocator_type& allocator) noexcept
    {
        node_type* prev = node;

        while (node != nullptr) {
            node = node->next;
            allocator.free(prev);
            prev = node;
        }
    }

    bool empty() const noexcept
    {
        return node == nullptr;
    }

    iterator begin() noexcept
    {
        return iterator(node);
    }

    iterator end() noexcept
    {
        return iterator(nullptr);
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(node);
    }

    const_iterator end() const noexcept
    {
        return const_iterator(nullptr);
    }

    reference front() noexcept
    {
        irt_assert(!empty());

        return node->value;
    }

    const_reference front() const noexcept
    {
        irt_assert(!empty());

        return node->value;
    }

    template<typename... Args>
    iterator emplace_front(allocator_type& allocator, Args&&... args) noexcept
    {
        node_type* new_node = allocator.alloc();

        new (&new_node->value) T(std::forward<Args>(args)...);

        new_node->next = node;
        node = new_node;

        return begin();
    }

    template<typename... Args>
    iterator emplace_after(allocator_type& allocator,
                           iterator it,
                           Args&&... args) noexcept
    {
        node_type* new_node = allocator.alloc();
        new (&new_node->value) T(std::forward<Args>(args)...);

        if (it->node == nullptr)
            return emplace_front(std::forward<Args>(args)...);

        new_node->next = it->node->next;
        it->node->next = new_node;

        return iterator(new_node);
    }

    template<typename... Args>
    iterator try_emplace_front(allocator_type& allocator,
                               Args&&... args) noexcept
    {
        auto [success, new_node] = allocator.try_alloc();

        if (!success)
            return end();

        new (&new_node->value) T(std::forward<Args>(args)...);

        new_node->next = node;
        node = new_node;

        return begin();
    }

    template<typename... Args>
    iterator try_emplace_after(allocator_type& allocator,
                               iterator it,
                               Args&&... args) noexcept
    {
        auto [success, new_node] = allocator.try_alloc();

        if (!success)
            return end();

        new (&new_node->value) T(std::forward<Args>(args)...);

        if (it->node == nullptr)
            return emplace_front(std::forward<Args>(args)...);

        new_node->next = it->node->next;
        it->node->next = new_node;

        return iterator(new_node);
    }

    void pop_front(allocator_type& allocator) noexcept
    {
        if (node == nullptr)
            return;

        node_type* to_delete = node;
        node = node->next;

        if constexpr (!std::is_trivial_v<T>)
            to_delete->value.~T();

        allocator.free(to_delete);
    }

    iterator erase_after(allocator_type& allocator, iterator it) noexcept
    {
        if (it.node == nullptr)
            return end();

        node_type* to_delete = it.node->next;
        if (to_delete == nullptr)
            return end();

        node_type* next = to_delete->next;
        it.node->next = next;

        if constexpr (!std::is_trivial_v<T>)
            to_delete->value.~T();

        allocator.free(to_delete);

        return iterator(next);
    }
};

template<typename T>
class flat_list
{
public:
    struct node_type
    {
        T value;
        node_type* next = nullptr;
    };

public:
    using allocator_type = block_allocator<node_type>;
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;

    class iterator
    {
    private:
        node_type* node{ nullptr };

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        using difference_type = std::ptrdiff_t;

        iterator() noexcept = default;

        iterator(node_type* n) noexcept
          : node(n)
        {}

        iterator(const iterator& other) noexcept
          : node(other.node)
        {}

        iterator& operator=(const iterator& other) noexcept
        {
            node = other.node;
            return *this;
        }

        T& operator*() noexcept
        {
            return node->value;
        }

        T* operator->() noexcept
        {
            return &node->value;
        }

        iterator operator++() noexcept
        {
            if (node != nullptr)
                node = node->next;

            return *this;
        }

        iterator operator++(int) noexcept
        {
            iterator tmp(*this);

            if (node != nullptr)
                node = node->next;

            return tmp;
        }

        bool operator==(const iterator& other) const noexcept
        {
            return node == other.node;
        }

        bool operator!=(const iterator& other) const noexcept
        {
            return node != other.node;
        }

        void swap(iterator& other) noexcept
        {
            std::swap(node, other.node);
        }

        friend class flat_list<T>;
    };

    class const_iterator
    {
    private:
        const node_type* node{ nullptr };

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using pointer = T*;
        using reference = T&;
        using difference_type = std::ptrdiff_t;

        const_iterator() noexcept = default;

        const_iterator(node_type* n) noexcept
          : node(n)
        {}

        const_iterator(const const_iterator& other) noexcept
          : node(other.node)
        {}

        const_iterator& operator=(const const_iterator& other) noexcept
        {
            node = other.node;
            return *this;
        }

        const T& operator*() noexcept
        {
            return node->value;
        }

        const T* operator->() noexcept
        {
            return &node->value;
        }

        const_iterator operator++() noexcept
        {
            if (node != nullptr)
                node = node->next;

            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            const_iterator tmp(*this);

            if (node != nullptr)
                node = node->next;

            return tmp;
        }

        bool operator==(const const_iterator& other) const noexcept
        {
            return node == other.node;
        }

        bool operator!=(const const_iterator& other) const noexcept
        {
            return node != other.node;
        }

        void swap(const_iterator& other) noexcept
        {
            std::swap(node, other.node);
        }

        friend class flat_list<T>;
    };

private:
    allocator_type* allocator{ nullptr };
    node_type* node{ nullptr };

public:
    flat_list() = default;

    flat_list(allocator_type* allocator_new) noexcept
      : allocator(allocator_new)
    {}

    flat_list(const flat_list& other) = delete;
    flat_list& operator=(const flat_list& other) = delete;

    flat_list(flat_list&& other) noexcept
      : allocator(other.allocator)
      , node(other.node)
    {
        other.allocator = nullptr;
        other.node = nullptr;
    }

    void set_allocator(allocator_type* allocator_new) noexcept
    {
        clear();
        allocator = allocator_new;
        node = nullptr;
    }

    flat_list& operator=(flat_list&& other) noexcept
    {
        if (this != &other) {
            clear();
            allocator = other.allocator;
            other.allocator = nullptr;
            node = other.node;
            other.node = nullptr;
        }

        return *this;
    }

    ~flat_list() noexcept
    {
        clear();
    }

    void clear() noexcept
    {
        node_type* prev = node;

        while (node != nullptr) {
            node = node->next;
            allocator->free(prev);
            prev = node;
        }
    }

    bool empty() const noexcept
    {
        return node == nullptr;
    }

    iterator begin() noexcept
    {
        return iterator(node);
    }

    iterator end() noexcept
    {
        return iterator(nullptr);
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(node);
    }

    const_iterator end() const noexcept
    {
        return const_iterator(nullptr);
    }

    reference front() noexcept
    {
        irt_assert(!empty());

        return node->value;
    }

    const_reference front() const noexcept
    {
        irt_assert(!empty());

        return node->value;
    }

    template<typename... Args>
    iterator emplace_front(Args&&... args) noexcept
    {
        node_type* new_node = allocator->alloc();

        new (&new_node->value) T(std::forward<Args>(args)...);

        new_node->next = node;
        node = new_node;

        return begin();
    }

    template<typename... Args>
    iterator emplace_after(iterator it, Args&&... args) noexcept
    {
        node_type* new_node = allocator->alloc();
        new (&new_node->value) T(std::forward<Args>(args)...);

        if (it->node == nullptr)
            return emplace_front(std::forward<Args>(args)...);

        new_node->next = it->node->next;
        it->node->next = new_node;

        return iterator(new_node);
    }

    template<typename... Args>
    iterator try_emplace_front(Args&&... args) noexcept
    {
        auto [success, new_node] = allocator->try_alloc();

        if (!success)
            return end();

        new (&new_node->value) T(std::forward<Args>(args)...);

        new_node->next = node;
        node = new_node;

        return begin();
    }

    template<typename... Args>
    iterator try_emplace_after(iterator it, Args&&... args) noexcept
    {
        auto [success, new_node] = allocator->try_alloc();

        if (!success)
            return end();

        new (&new_node->value) T(std::forward<Args>(args)...);

        if (it->node == nullptr)
            return emplace_front(std::forward<Args>(args)...);

        new_node->next = it->node->next;
        it->node->next = new_node;

        return iterator(new_node);
    }

    void pop_front() noexcept
    {
        if (node == nullptr)
            return;

        node_type* to_delete = node;
        node = node->next;

        if constexpr (!std::is_trivial_v<T>)
            to_delete->value.~T();

        allocator->free(to_delete);
    }

    iterator erase_after(iterator it) noexcept
    {
        irt_assert(allocator);

        if (it.node == nullptr)
            return end();

        node_type* to_delete = it.node->next;
        if (to_delete == nullptr)
            return end();

        node_type* next = to_delete->next;
        it.node->next = next;

        if constexpr (!std::is_trivial_v<T>)
            to_delete->value.~T();

        allocator->free(to_delete);

        return iterator(next);
    }
};

template<typename T>
class flat_double_list
{
private:
    struct node_type
    {
        T value;
        node_type* next = nullptr;
        node_type* prev = nullptr;
    };

public:
    using allocator_type = block_allocator<node_type>;
    using value_type = T;
    using reference = T&;
    using pointer = T*;

    class iterator
    {
    private:
        flat_double_list* list{ nullptr };
        node_type* node{ nullptr };

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        iterator() noexcept = default;

        iterator(flat_double_list* lst, node_type* n) noexcept
          : list(lst)
          , node(n)
        {}

        iterator(const iterator& other) noexcept
          : list(other.list)
          , node(other.node)
        {}

        iterator& operator=(const iterator& other) noexcept
        {
            list = other.list;
            node = other.node;
            return *this;
        }

        T& operator*() noexcept
        {
            return node->value;
        }

        T* operator*() const noexcept
        {
            return node->value;
        }

        pointer operator->() noexcept
        {
            return &(node->value);
        }

        pointer operator->() const noexcept
        {
            return &node->value;
        }

        iterator operator++() noexcept
        {
            node = (node == nullptr) ? list->m_front : node->next;

            return *this;
        }

        iterator operator++(int) noexcept
        {
            iterator tmp(*this);

            node = (node == nullptr) ? list->m_front : node->next;

            return tmp;
        }

        iterator operator--() noexcept
        {
            node = (node == nullptr) ? list->m_back : node->prev;

            return *this;
        }

        iterator operator--(int) noexcept
        {
            iterator tmp(*this);

            node = (node == nullptr) ? list->m_back : node->prev;

            return tmp;
        }

        bool operator==(const iterator& other) const noexcept
        {
            return node == other.node;
        }

        bool operator!=(const iterator& other) const noexcept
        {
            return node != other.node;
        }

        void swap(iterator& other) noexcept
        {
            std::swap(list, other.list);
            std::swap(node, other.node);
        }

        friend class flat_double_list<T>;
    };

    class const_iterator
    {
    private:
        const flat_double_list* list{ nullptr };
        node_type* node{ nullptr };

    public:
        using const_iterator_category = std::bidirectional_iterator_tag;
        using value_type = T;
        using pointer = T*;
        using reference = T&;

        const_iterator() noexcept = default;

        const_iterator(const flat_double_list* lst, node_type* n) noexcept
          : list(lst)
          , node(n)
        {}

        const_iterator(const const_iterator& other) noexcept
          : list(other.list)
          , node(other.node)
        {}

        const_iterator& operator=(const const_iterator& other) noexcept
        {
            list = other.list;
            node = other.node;
            return *this;
        }

        const T& operator*() noexcept
        {
            return node->value;
        }

        const T* operator->() noexcept
        {
            return &(node->value);
        }

        const_iterator operator++() noexcept
        {
            node = (node == nullptr) ? list->m_front : node->next;

            return *this;
        }

        const_iterator operator++(int) noexcept
        {
            const_iterator tmp(*this);

            node = (node == nullptr) ? list->m_front : node->next;

            return tmp;
        }

        const_iterator operator--() noexcept
        {
            node = (node == nullptr) ? list->m_back : node->prev;

            return *this;
        }

        const_iterator operator--(int) noexcept
        {
            const_iterator tmp(*this);

            node = (node == nullptr) ? list->m_back : node->prev;

            return tmp;
        }

        bool operator==(const const_iterator& other) const noexcept
        {
            return node == other.node;
        }

        bool operator!=(const const_iterator& other) const noexcept
        {
            return node != other.node;
        }

        void swap(const_iterator& other) noexcept
        {
            std::swap(list, other.list);
            std::swap(node, other.node);
        }

        friend class flat_double_list<T>;
    };

private:
    allocator_type* m_allocator{ nullptr };
    node_type* m_front{ nullptr };
    node_type* m_back{ nullptr };
    i64 m_size{ 0 };

public:
    flat_double_list() = default;

    flat_double_list(allocator_type* allocator)
      : m_allocator(allocator)
    {}

    flat_double_list(const flat_double_list& other) = delete;
    flat_double_list& operator=(const flat_double_list& other) = delete;

    flat_double_list(flat_double_list&& other) noexcept
      : m_allocator(other.m_allocator)
      , m_front(other.m_front)
      , m_back(other.m_back)
      , m_size(other.m_size)
    {
        other.m_allocator = nullptr;
        other.m_front = nullptr;
        other.m_back = nullptr;
        other.m_size = 0;
    }

    void set_allocator(allocator_type* allocator) noexcept
    {
        clear();
        m_allocator = allocator;
        m_front = nullptr;
        m_back = nullptr;
        m_size = 0;
    }

    allocator_type* get_allocator() const noexcept
    {
        return m_allocator;
    }

    flat_double_list& operator=(flat_double_list&& other) noexcept
    {
        if (this != &other) {
            clear();
            m_allocator = other.m_allocator;
            other.m_allocator = nullptr;
            m_front = other.m_front;
            other.m_front = nullptr;
            m_back = other.m_back;
            other.m_back = nullptr;
            m_size = other.m_size;
            other.m_size = 0;
        }

        return *this;
    }

    ~flat_double_list() noexcept
    {
        clear();
    }

    void clear() noexcept
    {
        node_type* prev = m_front;

        while (m_front != nullptr) {
            m_front = m_front->next;
            m_allocator->free(prev);
            prev = m_front;
        }

        m_size = 0;
        m_back = nullptr;
    }

    iterator begin() noexcept
    {
        return iterator(this, m_front);
    }

    iterator end() noexcept
    {
        return iterator(this, nullptr);
    }

    const_iterator begin() const noexcept
    {
        return const_iterator(this, m_front);
    }

    const_iterator end() const noexcept
    {
        return const_iterator(this, nullptr);
    }

    reference front() noexcept
    {
        return m_front->value;
    }

    reference back() noexcept
    {
        return m_back->value;
    }

    reference front() const noexcept
    {
        return m_front->value;
    }

    reference back() const noexcept
    {
        return m_back->value;
    }

    /**
     * @brief Inserts a new element into the container directly before pos.
     * @tparam ...Args
     * @param pos
     * @param ...args
     * @return
     */
    template<typename... Args>
    iterator emplace(iterator pos, Args&&... args) noexcept
    {
        if (!pos.node)
            return emplace_back(std::forward<Args>(args)...);

        if (!pos.node->prev)
            return emplace_front(std::forward<Args>(args)...);

        node_type* new_node = m_allocator->alloc();
        new (&new_node->value) T(std::forward<Args>(args)...);
        ++m_size;

        new_node->prev = pos.node->prev;
        new_node->next = pos.node;
        pos.node->prev->next = new_node;
        pos.node->prev = new_node;
        return iterator(this, new_node);
    }

    template<typename... Args>
    iterator emplace_front(Args&&... args) noexcept
    {
        node_type* new_node = m_allocator->alloc();
        new (&new_node->value) T(std::forward<Args>(args)...);
        ++m_size;

        if (m_front) {
            new_node->prev = nullptr;
            new_node->next = m_front;
            m_front->prev = new_node;
            m_front = new_node;
        } else {
            new_node->prev = nullptr;
            new_node->next = nullptr;
            m_front = new_node;
            m_back = new_node;
        }

        return begin();
    }

    template<typename... Args>
    iterator emplace_back(Args&&... args) noexcept
    {
        node_type* new_node = m_allocator->alloc();
        new (&new_node->value) T(std::forward<Args>(args)...);
        ++m_size;

        if (m_back) {
            new_node->prev = m_back;
            new_node->next = nullptr;
            m_back->next = new_node;
            m_back = new_node;
        } else {
            new_node->prev = nullptr;
            new_node->next = nullptr;
            m_front = new_node;
            m_back = new_node;
        }

        return iterator(this, new_node);
    }

    template<typename... Args>
    iterator try_emplace_front(Args&&... args) noexcept
    {
        auto [success, new_node] = m_allocator->try_alloc();

        if (!success)
            return end();

        new (&new_node->value) T(std::forward<Args>(args)...);
        ++m_size;

        if (m_front) {
            new_node->prev = nullptr;
            new_node->next = m_front;
            m_front->prev = new_node;
            m_front = new_node;
        } else {
            new_node->prev = nullptr;
            new_node->next = nullptr;
            m_front = new_node;
            m_back = new_node;
        }

        return begin();
    }

    template<typename... Args>
    iterator try_emplace_back(Args&&... args) noexcept
    {
        auto [success, new_node] = m_allocator->try_alloc();

        if (!success)
            return end();

        new (&new_node->value) T(std::forward<Args>(args)...);
        ++m_size;

        if (m_back) {
            new_node->prev = m_back;
            new_node->next = nullptr;
            m_back->next = new_node;
            m_back = new_node;
        } else {
            new_node->prev = nullptr;
            new_node->next = nullptr;
            m_front = new_node;
            m_back = new_node;
        }

        return iterator(this, new_node);
    }

    void pop_front() noexcept
    {
        if (m_front == nullptr)
            return;

        node_type* to_delete = m_front;
        m_front = m_front->next;

        if (m_front)
            m_front->prev = nullptr;
        else
            m_back = nullptr;

        if constexpr (!std::is_trivial_v<T>)
            to_delete->value.~T();

        m_allocator->free(to_delete);
        --m_size;
    }

    void pop_back() noexcept
    {
        if (m_back == nullptr)
            return;

        node_type* to_delete = m_back;
        m_back = m_back->prev;

        if (m_back)
            m_back->next = nullptr;
        else
            m_front = nullptr;

        if constexpr (!std::is_trivial_v<T>)
            to_delete->value.~T();

        m_allocator->free(to_delete);
        --m_size;
    }

    bool empty() const noexcept
    {
        return m_size == 0;
    }

    i64 size() const noexcept
    {
        return m_size;
    }
};

/*****************************************************************************
 *
 * data-array
 *
 ****************************************************************************/

enum class model_id : std::uint64_t;
enum class dynamics_id : std::uint64_t;
enum class message_id : std::uint64_t;
enum class observer_id : std::uint64_t;

template<typename T>
constexpr u32
get_index(T identifier) noexcept
{
    return static_cast<u32>(
      static_cast<typename std::underlying_type<T>::type>(identifier) &
      0x00000000ffffffff);
}

template<typename T>
constexpr u32
get_key(T identifier) noexcept
{
    return static_cast<u32>(
      (static_cast<typename std::underlying_type<T>::type>(identifier) &
       0xffffffff00000000) >>
      32);
}

template<typename T>
constexpr u32
get_max_key() noexcept
{
    return static_cast<u32>(0xffffffff00000000 >> 32);
}

template<typename T>
constexpr u32
get_max_size() noexcept
{
    return std::numeric_limits<u32>::max();
}

template<typename T>
constexpr bool
is_valid(T identifier) noexcept
{
    return get_key(identifier) > 0;
}

template<typename T>
constexpr T
make_id(u32 key, u32 index) noexcept
{
    u64 identifier = key;
    identifier <<= 32;
    identifier |= index;

    T ret{ identifier };
    return ret;

    // return static_cast<T>(identifier);
}

template<typename T>
constexpr u32
make_next_key(u32 key) noexcept
{
    return key == get_max_key<T>() ? 1u : key + 1;
}

/**
 * @brief An optimized fixed size array for dynamics objects.
 *
 * A container to handle everything from trivial, pod or object.
 * - linear memory/iteration
 * - O(1) alloc/free
 * - stable indices
 * - weak references
 * - zero overhead dereferences
 *
 * @tparam T The type of object the data_array holds.
 * @tparam Identifier A enum class identifier to store identifier unsigned
 *     number.
 * @todo Make Identifier split key|id automatically for all unsigned.
 */
template<typename T, typename Identifier>
class data_array
{
private:
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    struct item
    {
        T item;
        Identifier id;
    };

    item* m_items = nullptr; // items vector.
    u32 m_max_size = 0;      // total size
    u32 m_max_used = 0;      // highest index ever allocated
    u32 m_capacity = 0;      // num allocated items
    u32 m_next_key = 1;      // [1..2^32] (don't let == 0)
    u32 m_free_head = none;  // index of first free entry

public:
    using identifier_type = Identifier;
    using value_type = T;

    static constexpr u32 none = std::numeric_limits<u32>::max();

    data_array() = default;

    ~data_array() noexcept
    {
        clear();

        if (m_items)
            g_free_fn(m_items);
    }

    /**
     * @brief Initialize the underlying byffer of T.
     *
     * @return @c is_success(status) or is_bad(status).
     */
    status init(std::size_t capacity) noexcept
    {
        clear();

        if (capacity > get_max_size<Identifier>())
            return status::data_array_init_capacity_error;

        m_items = static_cast<item*>(g_alloc_fn(capacity * sizeof(item)));
        if (!m_items)
            return status::data_array_not_enough_memory;

        m_max_size = 0;
        m_max_used = 0;
        m_capacity = static_cast<u32>(capacity);
        m_next_key = 1;
        m_free_head = none;

        return status::success;
    }

    /**
     * @brief Resets data members
     *
     * Run destructor on outstanding items and re-initialize of size.
     */
    void clear() noexcept
    {
        if constexpr (!std::is_trivial_v<T>) {
            for (u32 i = 0; i != m_max_used; ++i) {
                if (is_valid(m_items[i].id)) {
                    m_items[i].item.~T();
                    m_items[i].id = static_cast<identifier_type>(0);
                }
            }
        }

        m_max_size = 0;
        m_max_used = 0;
        m_next_key = 1;
        m_free_head = none;
    }

    /**
     * @brief Alloc a new element.
     *
     * If allocator can not allocate new item, this function abort() if NDEBUG
     * macro is not defined. Before using this function, tries @c can_alloc()
     * for example.
     *
     * Use @c m_free_head if not empty or use a new items from buffer
     * (@m_item[max_used++]). The id is set to from @c next_key++ << 32) |
     * index to build unique identifier.
     *
     * @return A reference to the newly allocated element.
     */
    template<typename... Args>
    T& alloc(Args&&... args) noexcept
    {
        assert(can_alloc(1) && "check alloc() with full() before using use.");

        u32 new_index;

        if (m_free_head != none) {
            new_index = m_free_head;
            if (is_valid(m_items[m_free_head].id))
                m_free_head = none;
            else
                m_free_head = get_index(m_items[m_free_head].id);
        } else {
            new_index = m_max_used++;
        }

        new (&m_items[new_index].item) T(std::forward<Args>(args)...);

        m_items[new_index].id = make_id<Identifier>(m_next_key, new_index);
        m_next_key = make_next_key<Identifier>(m_next_key);

        ++m_max_size;

        return m_items[new_index].item;
    }

    /**
     * @brief Alloc a new element.
     *
     * Use @c m_free_head if not empty or use a new items from buffer
     * (@m_item[max_used++]). The id is set to from @c next_key++ << 32) |
     * index to build unique identifier.
     *
     * @return A pair with a boolean if the allocation success and a pointer to
     *     the newly element.
     */
    template<typename... Args>
    std::pair<bool, T*> try_alloc(Args&&... args) noexcept
    {
        if (!can_alloc(1))
            return { false, nullptr };

        u32 new_index;

        if (m_free_head != none) {
            new_index = m_free_head;
            if (is_valid(m_items[m_free_head].id))
                m_free_head = none;
            else
                m_free_head = get_index(m_items[m_free_head].id);
        } else {
            new_index = m_max_used++;
        }

        new (&m_items[new_index].item) T(std::forward<Args>(args)...);

        m_items[new_index].id = make_id<Identifier>(m_next_key, new_index);
        m_next_key = make_next_key<Identifier>(m_next_key);

        ++m_max_size;

        return { true, &m_items[new_index].item };
    }

    /**
     * @brief Free the element @c t.
     *
     * Internally, puts the elelent @c t entry on free list and use id to store
     * next.
     */
    void free(T& t) noexcept
    {
        auto id = get_id(t);
        auto index = get_index(id);

        irt_assert(&m_items[index] == static_cast<void*>(&t));
        irt_assert(m_items[index].id == id);
        irt_assert(is_valid(id));

        if constexpr (!std::is_trivial_v<T>)
            m_items[index].item.~T();

        m_items[index].id = static_cast<Identifier>(m_free_head);
        m_free_head = index;

        --m_max_size;
    }

    /**
     * @brief Free the element pointer by @c id.
     *
     * Internally, puts the elelent @c t entry on free list and use id to store
     * next.
     */
    void free(Identifier id) noexcept
    {
        auto index = get_index(id);

        irt_assert(m_items[index].id == id);
        irt_assert(is_valid(id));

        if constexpr (std::is_trivial_v<T>)
            m_items[index].item.~T();

        m_items[index].id = static_cast<Identifier>(m_free_head);
        m_free_head = index;

        --m_max_size;
    }

    /**
     * @brief Accessor to the id part of the item
     *
     * @return @c Identifier.
     */
    Identifier get_id(const T* t) const noexcept
    {
        irt_assert(t != nullptr);

        auto* ptr = reinterpret_cast<const item*>(t);
        return ptr->id;
    }

    /**
     * @brief Accessor to the id part of the item
     *
     * @return @c Identifier.
     */
    Identifier get_id(const T& t) const noexcept
    {
        auto* ptr = reinterpret_cast<const item*>(&t);
        return ptr->id;
    }

    /**
     * @brief Accessor to the item part of the id.
     *
     * @return @c T
     */
    T& get(Identifier id) noexcept
    {
        return m_items[get_index(id)].item;
    }

    /**
     * @brief Accessor to the item part of the id.
     *
     * @return @c T
     */
    const T& get(Identifier id) const noexcept
    {
        return m_items[get_index(id)].item;
    }

    /**
     * @brief Get a T from an ID.
     *
     * @details Validates ID, then returns item, returns null if invalid.
     * For cases like AI references and others where 'the thing might have
     * been deleted out from under me.
     */
    T* try_to_get(Identifier id) const noexcept
    {
        if (get_key(id)) {
            auto index = get_index(id);
            if (m_items[index].id == id)
                return &m_items[index].item;
        }

        return nullptr;
    }

    T* try_to_get(u32 index) const noexcept
    {
        if (is_valid(m_items[index].id))
            return &m_items[index].item;

        return nullptr;
    }

    /**
     * @brief Return next valid item.
     * @code
     * data_array<int> d;
     * ...
     * int* value = nullptr;
     * while (d.next(value)) {
     *     std::cout << *value;
     * }
     * @endcode
     *
     * Loop item where @c id & 0xffffffff00000000 != 0 (i.e. items not on free
     * list).
     *
     * @return true if the paramter @c t is valid false otherwise.
     */
    bool next(T*& t) const noexcept
    {
        u32 index;

        if (t) {
            auto id = get_id(*t);
            index = get_index(id);
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

    constexpr bool full() const noexcept
    {
        return m_free_head == none && m_max_used == m_capacity;
    }

    constexpr std::size_t size() const noexcept
    {
        return m_max_size;
    }

    constexpr bool can_alloc(std::size_t place) const noexcept
    {
        return m_capacity - m_max_size >= place;
    }

    constexpr bool can_alloc() const noexcept
    {
        return m_capacity - m_max_size >= 1u;
    }

    constexpr u32 max_size() const noexcept
    {
        return m_max_size;
    }

    constexpr u32 max_used() const noexcept
    {
        return m_max_used;
    }

    constexpr u32 capacity() const noexcept
    {
        return m_capacity;
    }

    constexpr u32 next_key() const noexcept
    {
        return m_next_key;
    }

    constexpr bool is_free_list_empty() const noexcept
    {
        return m_free_head == none;
    }
};

struct record
{
    record() noexcept = default;

    record(double x_dot_, time date_) noexcept
      : x_dot{ x_dot_ }
      , date{ date_ }
    {}

    double x_dot{ 0 };
    time date{ time_domain<time>::infinity };
};

/**
 * @brief Pairing heap implementation.
 *
 * A pairing heap is a type of heap data structure with relatively simple
 * implementation and excellent practical amortized performance, introduced by
 * Michael Fredman, Robert Sedgewick, Daniel Sleator, and Robert Tarjan in
 * 1986.
 *
 * https://en.wikipedia.org/wiki/Pairing_heap
 */
class heap
{
private:
    struct node
    {
        time tn;
        model_id id;

        node* prev;
        node* next;
        node* child;
    };

    size_t m_size{ 0 };
    size_t max_size{ 0 };
    size_t capacity{ 0 };
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

    status init(size_t new_capacity) noexcept
    {
        if (new_capacity == 0)
            return status::head_allocator_bad_capacity;

        if (new_capacity != capacity) {
            if (nodes)
                g_free_fn(nodes);

            nodes = static_cast<node*>(g_alloc_fn(new_capacity * sizeof(node)));
            if (nodes == nullptr)
                return status::head_allocator_not_enough_memory;
        }

        m_size = 0;
        max_size = 0;
        capacity = new_capacity;
        root = nullptr;
        free_list = nullptr;

        return status::success;
    }

    void clear()
    {
        m_size = 0;
        max_size = 0;
        root = nullptr;
        free_list = nullptr;
    }

    handle insert(time tn, model_id id) noexcept
    {
        node* new_node;

        if (free_list) {
            new_node = free_list;
            free_list = free_list->next;
        } else {
            new_node = &nodes[max_size++];
        }

        new_node->tn = tn;
        new_node->id = id;
        new_node->prev = nullptr;
        new_node->next = nullptr;
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
            elem->prev = nullptr;
            elem->child = nullptr;
            elem->id = static_cast<model_id>(0);

            elem->next = free_list;
            free_list = elem;
        }
    }

    void insert(handle elem) noexcept
    {
        elem->prev = nullptr;
        elem->next = nullptr;
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

    size_t size() const noexcept
    {
        return m_size;
    }

    size_t full() const noexcept
    {
        return m_size == capacity;
    }

    bool empty() const noexcept
    {
        return root == nullptr;
    }

    handle top() const noexcept
    {
        return root;
    }

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

            a->next = b->next;
            b->next = a->child;
            a->child = b;
            b->prev = a;

            return a;
        }

        if (b->child != nullptr)
            b->child->prev = a;

        if (a->prev != nullptr && a->prev->child != a)
            a->prev->next = b;

        b->prev = a->prev;
        a->prev = b;
        a->next = b->child;
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

struct source
{
    enum class operation_type
    {
        initialize, // Use to initialize the buffer at simulation init step.
        update,     // Use to update the buffer when all values are read.
        finalize    // Use to clear the buffer at simulation finalize step.
    };

    double* buffer = nullptr;
    u64 id = 0;    // The identifier of the external source (see operation())
    int type = -1; // The type of the external source (see operation())
    int size = 0;
    int index = 0;

    void reset() noexcept
    {
        buffer = nullptr;
        size = 0;
        index = 0;
        type = -1;
        id = 0;
    }

    void clear() noexcept
    {
        buffer = nullptr;
        size = 0;
        index = 0;
    }

    bool next(double& value) noexcept
    {
        if (index >= size)
            return false;

        value = buffer[index++];

        return true;
    }
};

/**
 * @brief Call in the initialize function of the models.
 * @param sim The simulation.
 * @param src The sources.
 * @return
 */
inline status
initialize_source(simulation& sim, source& src) noexcept;

inline status
update_source(simulation& sim, source& src, double& val) noexcept;

inline status
finalize_source(simulation& sim, source& src) noexcept;

/*****************************************************************************
 *
 * DEVS Model / Simulation entities
 *
 ****************************************************************************/

enum class dynamics_type : i32
{
    none,

    qss1_integrator,
    qss1_multiplier,
    qss1_cross,
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
    qss3_power,
    qss3_square,
    qss3_cross,
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
    flow
};

constexpr i8
dynamics_type_last() noexcept
{
    return static_cast<i8>(dynamics_type::flow);
}

constexpr sz
dynamics_type_size() noexcept
{
    return static_cast<sz>(dynamics_type_last() + 1);
}

struct observer;

struct observer
{
    enum class status
    {
        initialize,
        run,
        finalize
    };

    using update_fn = function_ref<void(const observer&,
                                        const dynamics_type,
                                        const time,
                                        const time,
                                        const observer::status)>;

    observer(const char* name_, update_fn cb_) noexcept
      : cb(cb_)
      , name(name_)
    {}

    update_fn cb;
    small_string<8> name;
    model_id model = static_cast<model_id>(0);
    message msg;
};

namespace detail {

template<typename, template<typename...> class, typename...>
struct is_detected : std::false_type
{};

template<template<class...> class Operation, typename... Arguments>
struct is_detected<std::void_t<Operation<Arguments...>>,
                   Operation,
                   Arguments...> : std::true_type
{};

template<typename T, T>
struct helper
{};

} // namespace detail

template<template<class...> class Operation, typename... Arguments>
using is_detected = detail::is_detected<std::void_t<>, Operation, Arguments...>;

template<template<class...> class Operation, typename... Arguments>
constexpr bool is_detected_v =
  detail::is_detected<std::void_t<>, Operation, Arguments...>::value;

template<class T>
using lambda_function_t =
  decltype(detail::helper<status (T::*)(), &T::lambda>{});

template<class T>
using transition_function_t =
  decltype(detail::helper<status (T::*)(time, time, time), &T::transition>{});

template<class T>
using observation_function_t =
  decltype(detail::helper<message (T::*)(const time) const, &T::observation>{});

template<class T>
using initialize_function_t =
  decltype(detail::helper<status (T::*)(), &T::initialize>{});

template<typename T>
using has_input_port_t = decltype(&T::x);

template<typename T>
using has_output_port_t = decltype(&T::y);

template<typename T>
using has_init_port_t = decltype(&T::init);

template<typename T>
using has_sim_attribute_t = decltype(&T::sim);

struct node
{
    node() = default;

    node(const model_id model_, const int port_index_) noexcept
      : model(model_)
      , port_index(port_index_)
    {}

    model_id model = model_id{ 0 };
    int port_index = 0;
};

struct port
{
    shared_flat_list<node> connections;
    flat_list<message> messages;
    // message* output = nullptr;

    // port& operator=(const double v) noexcept
    // {
    //     output[0] = v;
    //     return *this;
    // }

    // port& operator=(const std::initializer_list<double>& v) noexcept
    // {
    //     std::copy_n(std::data(v), std::size(v), &output->real[0]);
    //     return *this;
    // }
};

struct none
{
    time sigma = time_domain<time>::infinity;
};

struct integrator
{
    port x[3];
    port y[1];
    time sigma = time_domain<time>::zero;

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

    double default_current_value = 0.0;
    double default_reset_value = 0.0;
    flat_double_list<record> archive;

    double current_value = 0.0;
    double reset_value = 0.0;
    double up_threshold = 0.0;
    double down_threshold = 0.0;
    double last_output_value = 0.0;
    double expected_value = 0.0;
    bool reset = false;
    state st = state::init;

    integrator() = default;

    integrator(const integrator& other) noexcept
      : default_current_value(other.default_current_value)
      , default_reset_value(other.default_reset_value)
      , archive(other.archive.get_allocator())
      , current_value(other.current_value)
      , reset_value(other.reset_value)
      , up_threshold(other.up_threshold)
      , down_threshold(other.down_threshold)
      , last_output_value(other.last_output_value)
      , expected_value(other.expected_value)
      , reset(other.reset)
      , st(other.st)
    {}

    status initialize() noexcept
    {
        current_value = default_current_value;
        reset_value = default_reset_value;
        up_threshold = 0.0;
        down_threshold = 0.0;
        last_output_value = 0.0;
        expected_value = 0.0;
        reset = false;
        archive.clear();
        st = state::init;

        sigma = time_domain<time>::zero;

        return status::success;
    }

    status external(port& port_quanta,
                    port& port_x_dot,
                    port& port_reset,
                    time t) noexcept
    {
        for (const auto& msg : port_quanta.messages) {
            up_threshold = msg.real[0];
            down_threshold = msg.real[1];

            if (st == state::wait_for_quanta)
                st = state::running;

            if (st == state::wait_for_both)
                st = state::wait_for_x_dot;
        }

        for (const auto& msg : port_x_dot.messages) {
            archive.emplace_back(msg.real[0], t);

            if (st == state::wait_for_x_dot)
                st = state::running;

            if (st == state::wait_for_both)
                st = state::wait_for_quanta;
        }

        for (const auto& msg : port_reset.messages) {
            reset_value = msg.real[0];
            reset = true;
        }

        if (st == state::running) {
            current_value = compute_current_value(t);
            expected_value = compute_expected_value();
        }

        return status::success;
    }

    status internal(time t) noexcept
    {
        switch (st) {
        case state::running: {
            last_output_value = expected_value;

            const double last_derivative_value = archive.back().x_dot;
            archive.clear();
            archive.emplace_back(last_derivative_value, t);
            current_value = expected_value;
            st = state::wait_for_quanta;
            return status::success;
        }
        case state::init:
            st = state::wait_for_both;
            last_output_value = current_value;
            return status::success;
        default:
            return status::model_integrator_internal_error;
        }
    }

    status transition(time t, time /*e*/, time r) noexcept
    {
        auto& port_1 = x[port_quanta];
        auto& port_2 = x[port_x_dot];
        auto& port_3 = x[port_reset];

        if (port_1.messages.empty() && port_2.messages.empty() &&
            port_3.messages.empty()) {
            irt_return_if_bad(internal(t));
        } else {
            if (time_domain<time>::is_zero(r))
                irt_return_if_bad(internal(t));

            irt_return_if_bad(external(port_1, port_2, port_3, t));
        }

        return ta();
    }

    status lambda() noexcept
    {
        switch (st) {
        case state::running:
            y[0].messages.emplace_front(expected_value);
            return status::success;
        case state::init:
            y[0].messages.emplace_front(current_value);
            return status::success;
        default:
            return status::model_integrator_output_error;
        }

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { last_output_value };
    }

    status ta() noexcept
    {
        if (st == state::running) {
            irt_return_if_fail(!archive.empty(),
                               status::model_integrator_running_without_x_dot);

            const auto current_derivative = archive.back().x_dot;

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

    double compute_current_value(time t) const noexcept
    {
        if (archive.empty())
            return reset ? reset_value : last_output_value;

        auto val = reset ? reset_value : last_output_value;
        auto end = archive.end();
        auto it = archive.begin();
        auto next = archive.begin();

        if (next != end)
            ++next;

        for (; next != end; it = next++)
            val += (next->date - it->date) * it->x_dot;

        val += (t - archive.back().date) * archive.back().x_dot;

        if (up_threshold < val) {
            return up_threshold;
        } else if (down_threshold > val) {
            return down_threshold;
        } else {
            return val;
        }
    }

    double compute_expected_value() const noexcept
    {
        const auto current_derivative = archive.back().x_dot;

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
    port x[2];
    port y[1];
    double default_X = 0.;
    double default_dQ = 0.01;
    double X;
    double q;
    double u;
    time sigma = time_domain<time>::zero;

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
    {}

    status initialize() noexcept
    {
        irt_return_if_fail(std::isfinite(default_X),
                           status::model_integrator_X_error);

        irt_return_if_fail(std::isfinite(default_dQ) && default_dQ > 0.,
                           status::model_integrator_X_error);

        X = default_X;
        q = std::floor(X / default_dQ) * default_dQ;
        u = 0.;

        sigma = time_domain<time>::zero;

        return status::success;
    }

    status external(const double value_x, const time e) noexcept
    {
        X += e * u;
        u = value_x;

        if (sigma != 0.) {
            if (u == 0.)
                sigma = time_domain<time>::infinity;
            else if (u > 0.)
                sigma = (q + default_dQ - X) / u;
            else
                sigma = (q - default_dQ - X) / u;
        }

        return status::success;
    }

    status reset(const double value_reset) noexcept
    {
        X = value_reset;
        q = X;
        sigma = time_domain<time>::zero;
        return status::success;
    }

    status internal() noexcept
    {
        X += sigma * u;
        q = X;

        sigma =
          u == 0. ? time_domain<time>::infinity : default_dQ / std::abs(u);

        return status::success;
    }

    status transition(time /*t*/, time e, time /*r*/) noexcept
    {
        auto& port_x = x[port_x_dot];
        auto& port_r = x[port_reset];

        if (port_x.messages.empty() && port_r.messages.empty()) {
            irt_return_if_bad(internal());
        } else {
            if (!port_r.messages.empty()) {
                irt_return_if_bad(reset(port_r.messages.front()[0]));
            } else {
                irt_return_if_bad(external(port_x.messages.front()[0], e));
            }
        }

        return status::success;
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(X + u * sigma);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { X, u };
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
    port x[2];
    port y[1];
    double default_X = 0.;
    double default_dQ = 0.01;
    double X;
    double u;
    double mu;
    double q;
    double mq;
    time sigma = time_domain<time>::zero;

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
    {}

    status initialize() noexcept
    {
        irt_return_if_fail(std::isfinite(default_X),
                           status::model_integrator_X_error);

        irt_return_if_fail(std::isfinite(default_dQ) && default_dQ > 0.,
                           status::model_integrator_X_error);

        X = default_X;

        u = 0.;
        mu = 0.;
        q = X;
        mq = 0.;

        sigma = time_domain<time>::zero;

        return status::success;
    }

    status external(const double value_x,
                    const double value_slope,
                    const time e) noexcept
    {
        X += (u * e) + (mu / 2.0) * (e * e);
        u = value_x;
        mu = value_slope;

        if (sigma != 0) {
            q += mq * e;
            const double a = mu / 2;
            const double b = u - mq;
            double c = X - q + default_dQ;
            double s;
            sigma = time_domain<time>::infinity;

            if (a == 0) {
                if (b != 0) {
                    s = -c / b;
                    if (s > 0)
                        sigma = s;

                    c = X - q - default_dQ;
                    s = -c / b;
                    if ((s > 0) && (s < sigma))
                        sigma = s;
                }
            } else {
                s = (-b + std::sqrt(b * b - 4. * a * c)) / 2. / a;
                if (s > 0.)
                    sigma = s;

                s = (-b - std::sqrt(b * b - 4. * a * c)) / 2. / a;
                if ((s > 0.) && (s < sigma))
                    sigma = s;

                c = X - q - default_dQ;
                s = (-b + std::sqrt(b * b - 4. * a * c)) / 2. / a;
                if ((s > 0.) && (s < sigma))
                    sigma = s;

                s = (-b - std::sqrt(b * b - 4. * a * c)) / 2. / a;
                if ((s > 0.) && (s < sigma))
                    sigma = s;
            }

            if (((X - q) > default_dQ) || ((q - X) > default_dQ))
                sigma = time_domain<time>::zero;
        }

        return status::success;
    }

    status internal() noexcept
    {
        X += u * sigma + mu / 2. * sigma * sigma;
        q = X;
        u += mu * sigma;
        mq = u;

        sigma = mu == 0. ? time_domain<time>::infinity
                         : std::sqrt(2. * default_dQ / std::abs(mu));

        return status::success;
    }

    status reset(const double value_reset) noexcept
    {
        X = value_reset;
        q = X;
        sigma = time_domain<time>::zero;
        return status::success;
    }

    status transition(time /*t*/, time e, time /*r*/) noexcept
    {
        auto& port_x = x[port_x_dot];
        auto& port_r = x[port_reset];

        if (port_x.messages.empty() && port_r.messages.empty())
            irt_return_if_bad(internal());
        else {
            if (!port_r.messages.empty()) {
                irt_return_if_bad(reset(port_r.messages.front()[0]));
            } else {
                irt_return_if_bad(external(
                  port_x.messages.front()[0], port_x.messages.front()[1], e));
            }
        }

        return status::success;
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(X + u * sigma + mu * sigma * sigma / 2.,
                                    u + mu * sigma);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { X, u, mu };
    }
};

template<>
struct abstract_integrator<3>
{
    port x[2];
    port y[1];
    double default_X = 0.;
    double default_dQ = 0.01;
    double X;
    double u;
    double mu;
    double pu;
    double q;
    double mq;
    double pq;
    time sigma = time_domain<time>::zero;

    enum port_name
    {
        port_x_dot,
        port_reset
    };

    abstract_integrator() = default;

    status initialize() noexcept
    {
        irt_return_if_fail(std::isfinite(default_X),
                           status::model_integrator_X_error);

        irt_return_if_fail(std::isfinite(default_dQ) && default_dQ > 0.,
                           status::model_integrator_X_error);

        X = default_X;
        u = 0;
        mu = 0;
        pu = 0;
        q = default_X;
        mq = 0;
        pq = 0;
        sigma = time_domain<time>::zero;

        return status::success;
    }

    status external(const double value_x,
                    const double value_slope,
                    const double value_derivative,
                    const time e) noexcept
    {
#if irt_have_numbers == 1
        constexpr double pi_div_3 = std::numbers::pi_v<double> / 3.;
#else
        constexpr double pi_div_3 = 1.0471975511965976;
#endif

        X = X + u * e + (mu * e * e) / 2. + (pu * e * e * e) / 3.;
        u = value_x;
        mu = value_slope;
        pu = value_derivative;

        if (sigma != 0.) {
            q = q + mq * e + pq * e * e;
            mq = mq + 2. * pq * e;
            auto a = mu / 2. - pq;
            auto b = u - mq;
            auto c = X - q - default_dQ;
            auto s = 0.;

            if (pu != 0) {
                a = 3. * a / pu;
                b = 3. * b / pu;
                c = 3. * c / pu;
                auto v = b - a * a / 3.;
                auto w = c - b * a / 3. + 2. * a * a * a / 27.;
                auto i1 = -w / 2.;
                auto i2 = i1 * i1 + v * v * v / 27.;

                if (i2 > 0) {
                    i2 = std::sqrt(i2);
                    auto A = i1 + i2;
                    auto B = i1 - i2;

                    if (A > 0.)
                        A = std::pow(A, 1. / 3.);
                    else
                        A = -std::pow(std::abs(A), 1. / 3.);
                    if (B > 0.)
                        B = std::pow(B, 1. / 3.);
                    else
                        B = -std::pow(std::abs(B), 1. / 3.);

                    s = A + B - a / 3.;
                    if (s < 0.)
                        s = time_domain<time>::infinity;
                } else if (i2 == 0.) {
                    auto A = i1;
                    if (A > 0.)
                        A = std::pow(A, 1. / 3.);
                    else
                        A = -std::pow(std::abs(A), 1. / 3.);
                    auto x1 = 2. * A - a / 3.;
                    auto x2 = -(A + a / 3.);
                    if (x1 < 0.) {
                        if (x2 < 0.) {
                            s = time_domain<time>::infinity;
                        } else {
                            s = x2;
                        }
                    } else if (x2 < 0.) {
                        s = x1;
                    } else if (x1 < x2) {
                        s = x1;
                    } else {
                        s = x2;
                    }
                } else {
                    auto arg = w * std::sqrt(27. / (-v)) / (2. * v);
                    arg = std::acos(arg) / 3.;
                    auto y1 = 2 * std::sqrt(-v / 3.);
                    auto y2 = -y1 * std::cos(pi_div_3 - arg) - a / 3.;
                    auto y3 = -y1 * std::cos(pi_div_3 + arg) - a / 3.;
                    y1 = y1 * std::cos(arg) - a / 3.;
                    if (y1 < 0.) {
                        s = time_domain<time>::infinity;
                    } else if (y3 < 0.) {
                        s = y1;
                    } else if (y2 < 0.) {
                        s = y3;
                    } else {
                        s = y2;
                    }
                }
                c = c + 6. * default_dQ / pu;
                w = c - b * a / 3. + 2. * a * a * a / 27.;
                i1 = -w / 2;
                i2 = i1 * i1 + v * v * v / 27.;
                if (i2 > 0.) {
                    i2 = std::sqrt(i2);
                    auto A = i1 + i2;
                    auto B = i1 - i2;
                    if (A > 0)
                        A = std::pow(A, 1. / 3.);
                    else
                        A = -std::pow(std::abs(A), 1. / 3.);
                    if (B > 0.)
                        B = std::pow(B, 1. / 3.);
                    else
                        B = -std::pow(std::abs(B), 1. / 3.);
                    sigma = A + B - a / 3.;

                    if (s < sigma || sigma < 0.) {
                        sigma = s;
                    }
                } else if (i2 == 0.) {
                    auto A = i1;
                    if (A > 0.)
                        A = std::pow(A, 1. / 3.);
                    else
                        A = -std::pow(std::abs(A), 1. / 3.);
                    auto x1 = 2. * A - a / 3.;
                    auto x2 = -(A + a / 3.);
                    if (x1 < 0.) {
                        if (x2 < 0.) {
                            sigma = time_domain<time>::infinity;
                        } else {
                            sigma = x2;
                        }
                    } else if (x2 < 0.) {
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
                    auto arg = w * std::sqrt(27. / (-v)) / (2. * v);
                    arg = std::acos(arg) / 3.;
                    auto y1 = 2. * std::sqrt(-v / 3.);
                    auto y2 = -y1 * std::cos(pi_div_3 - arg) - a / 3.;
                    auto y3 = -y1 * std::cos(pi_div_3 + arg) - a / 3.;
                    y1 = y1 * std::cos(arg) - a / 3.;
                    if (y1 < 0.) {
                        sigma = time_domain<time>::infinity;
                    } else if (y3 < 0.) {
                        sigma = y1;
                    } else if (y2 < 0.) {
                        sigma = y3;
                    } else {
                        sigma = y2;
                    }
                    if (s < sigma) {
                        sigma = s;
                    }
                }
            } else {
                if (a != 0.) {
                    auto x1 = b * b - 4 * a * c;
                    if (x1 < 0.) {
                        s = time_domain<time>::infinity;
                    } else {
                        x1 = std::sqrt(x1);
                        auto x2 = (-b - x1) / 2. / a;
                        x1 = (-b + x1) / 2. / a;
                        if (x1 < 0.) {
                            if (x2 < 0.) {
                                s = time_domain<time>::infinity;
                            } else {
                                s = x2;
                            }
                        } else if (x2 < 0.) {
                            s = x1;
                        } else if (x1 < x2) {
                            s = x1;
                        } else {
                            s = x2;
                        }
                    }
                    c = c + 2. * default_dQ;
                    x1 = b * b - 4. * a * c;
                    if (x1 < 0.) {
                        sigma = time_domain<time>::infinity;
                    } else {
                        x1 = std::sqrt(x1);
                        auto x2 = (-b - x1) / 2. / a;
                        x1 = (-b + x1) / 2. / a;
                        if (x1 < 0.) {
                            if (x2 < 0.) {
                                sigma = time_domain<time>::infinity;
                            } else {
                                sigma = x2;
                            }
                        } else if (x2 < 0.) {
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
                    if (b != 0.) {
                        auto x1 = -c / b;
                        auto x2 = x1 - 2 * default_dQ / b;
                        if (x1 < 0.)
                            x1 = time_domain<time>::infinity;
                        if (x2 < 0.)
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
        X = X + u * sigma + (mu * sigma * sigma) / 2. +
            (pu * sigma * sigma * sigma) / 3.;
        q = X;
        u = u + mu * sigma + pu * pow(sigma, 2);
        mq = u;
        mu = mu + 2 * pu * sigma;
        pq = mu / 2;

        sigma = pu == 0. ? time_domain<time>::infinity
                         : std::pow(std::abs(3. * default_dQ / pu), 1. / 3.);

        return status::success;
    }

    status reset(const double value_reset) noexcept
    {
        X = value_reset;
        q = X;
        sigma = time_domain<time>::zero;

        return status::success;
    }

    status transition(time /*t*/, time e, time /*r*/) noexcept
    {
        auto& port_x = x[port_x_dot];
        auto& port_r = x[port_reset];

        if (port_x.messages.empty() && port_r.messages.empty())
            irt_return_if_bad(internal());
        else {
            if (!port_r.messages.empty())
                irt_return_if_bad(reset(port_r.messages.front()[0]));
            else
                irt_return_if_bad(external(port_x.messages.front()[0],
                                           port_x.messages.front()[1],
                                           port_x.messages.front()[2],
                                           e));
        }

        return status::success;
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(X + u * sigma + (mu * sigma * sigma) / 2. +
                                      (pu * sigma * sigma * sigma) / 3.,
                                    u + mu * sigma + pu * sigma * sigma,
                                    mu / 2. + pu * sigma);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { X, u, mu, pu };
    }
};

using qss1_integrator = abstract_integrator<1>;
using qss2_integrator = abstract_integrator<2>;
using qss3_integrator = abstract_integrator<3>;

template<int QssLevel>
struct abstract_power
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    port x[1];
    port y[1];
    time sigma;

    double value[QssLevel];
    double default_n;

    abstract_power() noexcept = default;

    status initialize() noexcept
    {
        std::fill_n(value, QssLevel, 0.0);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        if constexpr (QssLevel == 1) {
            y[0].messages.emplace_front(std::pow(value[0], default_n));
        }

        if constexpr (QssLevel == 2) {
            y[0].messages.emplace_front(
              std::pow(value[0], default_n),
              default_n * std::pow(value[0], default_n - 1) * value[1]);
        }

        if constexpr (QssLevel == 3) {
            y[0].messages.emplace_front(
              std::pow(value[0], default_n),
              default_n * std::pow(value[0], default_n - 1) * value[1],
              default_n * (default_n - 1) * std::pow(value[0], default_n - 2) *
                  (value[1] * value[1] / 2.0) +
                default_n * std::pow(value[0], default_n - 1) * value[2]);
        }

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {
        sigma = time_domain<time>::infinity;

        if (!x[0].messages.empty()) {
            auto& msg = x[0].messages.front();

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

    message observation(const time /*e*/) const noexcept
    {
        return { value[0] };
    }
};

using qss1_power = abstract_power<1>;
using qss2_power = abstract_power<2>;
using qss3_power = abstract_power<3>;

template<int QssLevel>
struct abstract_square
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    port x[1];
    port y[1];
    time sigma;

    double value[QssLevel];

    abstract_square() noexcept = default;

    status initialize() noexcept
    {
        std::fill_n(value, QssLevel, 0.0);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        if constexpr (QssLevel == 1) {
            y[0].messages.emplace_front(value[0] * value[0]);
        }

        if constexpr (QssLevel == 2) {
            y[0].messages.emplace_front(value[0] * value[0],
                                        2. * value[0] * value[1]);
        }

        if constexpr (QssLevel == 3) {
            y[0].messages.emplace_front(value[0] * value[0],
                                        2. * value[0] * value[1],
                                        2. * value[0] * value[2] +
                                          value[1] * value[1]);
        }

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {
        sigma = time_domain<time>::infinity;

        if (!x[0].messages.empty()) {
            auto& msg = x[0].messages.front();

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

    message observation(const time /*e*/) const noexcept
    {
        return { value[0] };
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

    port x[PortNumber];
    port y[1];
    time sigma;

    double values[QssLevel * PortNumber];

    abstract_sum() noexcept = default;

    status initialize() noexcept
    {
        std::fill_n(values, QssLevel * PortNumber, 0.0);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        if constexpr (QssLevel == 1) {
            double value = 0.;
            for (int i = 0; i != PortNumber; ++i)
                value += values[i];

            y[0].messages.emplace_front(value);
        }

        if constexpr (QssLevel == 2) {
            double value = 0.;
            double slope = 0.;

            for (int i = 0; i != PortNumber; ++i) {
                value += values[i];
                slope += values[i + PortNumber];
            }

            y[0].messages.emplace_front(value, slope);
        }

        if constexpr (QssLevel == 3) {
            double value = 0.;
            double slope = 0.;
            double derivative = 0.;

            for (size_t i = 0; i != PortNumber; ++i) {
                value += values[i];
                slope += values[i + PortNumber];
                derivative += values[i + PortNumber + PortNumber];
            }

            y[0].messages.emplace_front(value, slope, derivative);
        }

        return status::success;
    }

    status transition(time /*t*/, [[maybe_unused]] time e, time /*r*/) noexcept
    {
        bool message = false;

        if constexpr (QssLevel == 1) {
            for (size_t i = 0; i != PortNumber; ++i) {
                for (const auto& msg : x[i].messages) {
                    values[i] = msg[0];
                    message = true;
                }
            }
        }

        if constexpr (QssLevel == 2) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (x[i].messages.empty()) {
                    values[i] += values[i + PortNumber] * e;
                } else {
                    for (const auto& msg : x[i].messages) {
                        values[i] = msg[0];
                        values[i + PortNumber] = msg[1];
                        message = true;
                    }
                }
            }
        }

        if constexpr (QssLevel == 3) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (x[i].messages.empty()) {
                    values[i] += values[i + PortNumber] * e +
                                 values[i + PortNumber + PortNumber] * e * e;
                    values[i + PortNumber] +=
                      2 * values[i + PortNumber + PortNumber] * e;
                } else {
                    for (const auto& msg : x[i].messages) {
                        values[i] = msg[0];
                        values[i + PortNumber] = msg[1];
                        values[i + PortNumber + PortNumber] = msg[2];
                        message = true;
                    }
                }
            }
        }

        sigma = message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    message observation([[maybe_unused]] const time e) const noexcept
    {
        double value = 0.;

        if constexpr (QssLevel == 1) {
            for (size_t i = 0; i != PortNumber; ++i)
                value += values[i];
        }

        if constexpr (QssLevel >= 2) {
            for (size_t i = 0; i != PortNumber; ++i)
                value += values[i + PortNumber] * e;
        }

        if constexpr (QssLevel >= 3) {
            for (size_t i = 0; i != PortNumber; ++i)
                value += values[i + PortNumber + PortNumber] * e * e;
        }

        return { value };
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

    port x[PortNumber];
    port y[1];
    time sigma;

    double default_input_coeffs[PortNumber] = { 0 };
    double values[QssLevel * PortNumber];

    abstract_wsum() noexcept = default;

    status initialize() noexcept
    {
        std::fill_n(values, QssLevel * PortNumber, 0.);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        if constexpr (QssLevel == 1) {
            double value = 0.0;

            for (int i = 0; i != PortNumber; ++i)
                value += default_input_coeffs[i] * values[i];

            y[0].messages.emplace_front(value);
        }

        if constexpr (QssLevel == 2) {
            double value = 0.;
            double slope = 0.;

            for (int i = 0; i != PortNumber; ++i) {
                value += default_input_coeffs[i] * values[i];
                slope += default_input_coeffs[i] * values[i + PortNumber];
            }

            y[0].messages.emplace_front(value, slope);
        }

        if constexpr (QssLevel == 3) {
            double value = 0.;
            double slope = 0.;
            double derivative = 0.;

            for (int i = 0; i != PortNumber; ++i) {
                value += default_input_coeffs[i] * values[i];
                slope += default_input_coeffs[i] * values[i + PortNumber];
                derivative +=
                  default_input_coeffs[i] * values[i + PortNumber + PortNumber];
            }

            y[0].messages.emplace_front(value, slope, derivative);
        }

        return status::success;
    }

    status transition(time /*t*/, [[maybe_unused]] time e, time /*r*/) noexcept
    {
        bool message = false;

        if constexpr (QssLevel == 1) {
            for (size_t i = 0; i != PortNumber; ++i) {
                for (const auto& msg : x[i].messages) {
                    values[i] = msg[0];
                    message = true;
                }
            }
        }

        if constexpr (QssLevel == 2) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (x[i].messages.empty()) {
                    values[i] += values[i + PortNumber] * e;
                } else {
                    for (const auto& msg : x[i].messages) {
                        values[i] = msg[0];
                        values[i + PortNumber] = msg[1];
                        message = true;
                    }
                }
            }
        }

        if constexpr (QssLevel == 3) {
            for (size_t i = 0; i != PortNumber; ++i) {
                if (x[i].messages.empty()) {
                    values[i] += values[i + PortNumber] * e +
                                 values[i + PortNumber + PortNumber] * e * e;
                    values[i + PortNumber] +=
                      2 * values[i + PortNumber + PortNumber] * e;
                } else {
                    for (const auto& msg : x[i].messages) {
                        values[i] = msg[0];
                        values[i + PortNumber] = msg[1];
                        values[i + PortNumber + PortNumber] = msg[2];
                        message = true;
                    }
                }
            }
        }

        sigma = message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    message observation([[maybe_unused]] const time e) const noexcept
    {
        double value = 0.;

        if constexpr (QssLevel >= 1) {
            for (int i = 0; i != PortNumber; ++i)
                value += default_input_coeffs[i] * values[i];
        }

        if constexpr (QssLevel >= 2) {
            for (int i = 0; i != PortNumber; ++i)
                value += default_input_coeffs[i] * values[i + PortNumber] * e;
        }

        if constexpr (QssLevel >= 3) {
            for (size_t i = 0; i != PortNumber; ++i)
                value += default_input_coeffs[i] *
                         values[i + PortNumber + PortNumber] * e * e;
        }

        return { value };
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

    port x[2];
    port y[1];
    time sigma;

    double values[QssLevel * 2];

    abstract_multiplier() noexcept = default;

    status initialize() noexcept
    {
        std::fill_n(values, QssLevel * 2, 0.);
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        if constexpr (QssLevel == 1) {
            y[0].messages.emplace_front(values[0] * values[1]);
        }

        if constexpr (QssLevel == 2) {
            y[0].messages.emplace_front(values[0] * values[1],
                                        values[2 + 0] * values[1] +
                                          values[2 + 1] * values[0]);
        }

        if constexpr (QssLevel == 3) {
            y[0].messages.emplace_front(
              values[0] * values[1],
              values[2 + 0] * values[1] + values[2 + 1] * values[0],
              values[0] * values[2 + 2 + 1] + values[2 + 0] * values[2 + 1] +
                values[2 + 2 + 0] * values[1]);
        }

        return status::success;
    }

    status transition(time /*t*/, [[maybe_unused]] time e, time /*r*/) noexcept
    {
        bool message_port_0 = false;
        bool message_port_1 = false;
        sigma = time_domain<time>::infinity;

        for (const auto& msg : x[0].messages) {
            sigma = time_domain<time>::zero;
            message_port_0 = true;
            values[0] = msg[0];

            if constexpr (QssLevel >= 2)
                values[2 + 0] = msg[1];

            if constexpr (QssLevel == 3)
                values[2 + 2 + 0] = msg[2];
        }

        for (const auto& msg : x[1].messages) {
            message_port_1 = true;
            sigma = time_domain<time>::zero;
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

    message observation([[maybe_unused]] const time e) const noexcept
    {
        if constexpr (QssLevel == 1)
            return values[0] * values[1];

        if constexpr (QssLevel == 2)
            return (values[0] + e * values[2 + 0]) *
                   (values[1] + e * values[2 + 1]);

        if constexpr (QssLevel == 3)
            return (values[0] + e * values[2 + 0] + e * e * values[2 + 2 + 0]) *
                   (values[1] + e * values[2 + 1] + e * e * values[2 + 2 + 1]);
    }
};

using qss1_multiplier = abstract_multiplier<1>;
using qss2_multiplier = abstract_multiplier<2>;
using qss3_multiplier = abstract_multiplier<3>;

struct quantifier
{
    port x[1];
    port y[1];
    time sigma = time_domain<time>::infinity;

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

    double default_step_size = 0.001;
    int default_past_length = 3;
    adapt_state default_adapt_state = adapt_state::possible;
    bool default_zero_init_offset = false;
    flat_double_list<record> archive;

    double m_upthreshold = 0.0;
    double m_downthreshold = 0.0;
    double m_offset = 0.0;
    double m_step_size = 0.0;
    int m_step_number = 0;
    int m_past_length = 0;
    bool m_zero_init_offset = false;
    state m_state = state::init;
    adapt_state m_adapt_state = adapt_state::possible;

    quantifier() noexcept = default;

    quantifier(const quantifier& other) noexcept
      : default_step_size(other.default_step_size)
      , default_past_length(other.default_past_length)
      , default_adapt_state(other.default_adapt_state)
      , default_zero_init_offset(other.default_zero_init_offset)
      , archive(other.archive.get_allocator())
      , m_upthreshold(other.m_upthreshold)
      , m_downthreshold(other.m_downthreshold)
      , m_offset(other.m_offset)
      , m_step_size(other.m_step_size)
      , m_step_number(other.m_step_number)
      , m_past_length(other.m_past_length)
      , m_zero_init_offset(other.m_zero_init_offset)
      , m_state(other.m_state)
      , m_adapt_state(other.m_adapt_state)
    {}

    status initialize() noexcept
    {
        m_step_size = default_step_size;
        m_past_length = default_past_length;
        m_zero_init_offset = default_zero_init_offset;
        m_adapt_state = default_adapt_state;
        m_upthreshold = 0.0;
        m_downthreshold = 0.0;
        m_offset = 0.0;
        m_step_number = 0;
        archive.clear();
        m_state = state::init;

        irt_return_if_fail(m_step_size > 0,
                           status::model_quantifier_bad_quantum_parameter);

        irt_return_if_fail(
          m_past_length > 2,
          status::model_quantifier_bad_archive_length_parameter);

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status external(port& p, time t) noexcept
    {
        double val = 0.0, shifting_factor = 0.0;

        {
            double sum = 0.0;
            double nb = 0.0;
            for (const auto& msg : p.messages) {
                sum += msg.real[0];
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
                store_change(val >= m_upthreshold ? m_step_size : -m_step_size,
                             t);

                shifting_factor = shift_quanta();

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

    status transition(time t, time /*e*/, time r) noexcept
    {
        if (x[0].messages.empty()) {
            irt_return_if_bad(internal());
        } else {
            if (time_domain<time>::is_zero(r))
                irt_return_if_bad(internal());

            irt_return_if_bad(external(x[0], t));
        }

        return ta();
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(m_upthreshold, m_downthreshold);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { m_upthreshold, m_downthreshold };
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
        const auto step_number = static_cast<double>(m_step_number);

        m_upthreshold = m_offset + m_step_size * (step_number + 1.0);
        m_downthreshold = m_offset + m_step_size * (step_number - 1.0);
    }

    void update_thresholds(double factor) noexcept
    {
        const auto step_number = static_cast<double>(m_step_number);

        m_upthreshold = m_offset + m_step_size * (step_number + (1.0 - factor));
        m_downthreshold =
          m_offset + m_step_size * (step_number - (1.0 - factor));
    }

    void update_thresholds(double factor, direction d) noexcept
    {
        const auto step_number = static_cast<double>(m_step_number);

        if (d == direction::up) {
            m_upthreshold =
              m_offset + m_step_size * (step_number + (1.0 - factor));
            m_downthreshold = m_offset + m_step_size * (step_number - 1.0);
        } else {
            m_upthreshold = m_offset + m_step_size * (step_number + 1.0);
            m_downthreshold =
              m_offset + m_step_size * (step_number - (1.0 - factor));
        }
    }

    void init_step_number_and_offset(double value) noexcept
    {
        m_step_number = static_cast<int>(std::floor(value / m_step_size));

        if (m_zero_init_offset) {
            m_offset = 0.0;
        } else {
            m_offset = value - static_cast<double>(m_step_number) * m_step_size;
        }
    }

    double shift_quanta()
    {
        double factor = 0.0;

        if (oscillating(m_past_length - 1) &&
            ((archive.back().date - archive.front().date) != 0)) {
            double acc = 0.0;
            double local_estim;
            double cnt = 0;

            auto it_2 = archive.begin();
            auto it_0 = it_2++;
            auto it_1 = it_2++;

            for (i64 i = 0; i < archive.size() - 2; ++i) {
                if ((it_2->date - it_0->date) != 0) {
                    if ((archive.back().x_dot * it_1->x_dot) > 0.0) {
                        local_estim = 1 - (it_1->date - it_0->date) /
                                            (it_2->date - it_0->date);
                    } else {
                        local_estim =
                          (it_1->date - it_0->date) / (it_2->date - it_0->date);
                    }

                    acc += local_estim;
                    cnt += 1.0;
                }
            }

            acc = acc / cnt;
            factor = acc;
            archive.clear();
        }

        return factor;
    }

    void store_change(double val, time t) noexcept
    {
        archive.emplace_back(val, t);

        while (archive.size() > m_past_length)
            archive.pop_front();
    }

    bool oscillating(const int range) noexcept
    {
        if ((range + 1) > archive.size())
            return false;

        const i64 limit = archive.size() - range;
        auto it = --archive.end();
        auto next = it--;

        for (int i = 0; i < limit; ++i, next = it--)
            if (it->x_dot * next->x_dot > 0)
                return false;

        return true;
    }

    bool monotonous(const int range) noexcept
    {
        if ((range + 1) > archive.size())
            return false;

        auto it = archive.begin();
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

    port x[PortNumber];
    port y[1];
    time sigma;

    double default_values[PortNumber];
    double default_input_coeffs[PortNumber];

    double values[PortNumber];
    double input_coeffs[PortNumber];

    adder() noexcept
    {
        std::fill_n(std::begin(default_values),
                    PortNumber,
                    1.0 / static_cast<double>(PortNumber));

        std::fill_n(std::begin(default_input_coeffs), PortNumber, 0.0);
    }

    status initialize() noexcept
    {
        std::copy_n(std::begin(default_values), PortNumber, std::begin(values));

        std::copy_n(std::begin(default_input_coeffs),
                    PortNumber,
                    std::begin(input_coeffs));

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        double to_send = 0.0;

        for (size_t i = 0; i != PortNumber; ++i)
            to_send += input_coeffs[i] * values[i];

        y[0].messages.emplace_front(to_send);

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {
        bool have_message = false;

        for (size_t i = 0; i != PortNumber; ++i) {
            for (const auto& msg : x[i].messages) {
                values[i] = msg.real[0];

                have_message = true;
            }
        }

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        double ret = 0.0;

        for (size_t i = 0; i != PortNumber; ++i)
            ret += input_coeffs[i] * values[i];

        return { ret };
    }
};

template<size_t PortNumber>
struct mult
{
    static_assert(PortNumber > 1, "mult model need at least two input port");

    port x[PortNumber];
    port y[1];
    time sigma;

    double default_values[PortNumber];
    double default_input_coeffs[PortNumber];

    double values[PortNumber];
    double input_coeffs[PortNumber];

    mult() noexcept
    {
        std::fill_n(std::begin(default_values), PortNumber, 1.0);
        std::fill_n(std::begin(default_input_coeffs), PortNumber, 0.0);
    }

    status initialize() noexcept
    {
        std::copy_n(std::begin(default_values), PortNumber, std::begin(values));

        std::copy_n(std::begin(default_input_coeffs),
                    PortNumber,
                    std::begin(input_coeffs));

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        double to_send = 1.0;

        for (size_t i = 0; i != PortNumber; ++i)
            to_send *= std::pow(values[i], input_coeffs[i]);

        y[0].messages.emplace_front(to_send);

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {
        bool have_message = false;
        for (size_t i = 0; i != PortNumber; ++i) {
            for (const auto& msg : x[i].messages) {
                values[i] = msg[0];
                have_message = true;
            }
        }

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        double ret = 1.0;

        for (size_t i = 0; i != PortNumber; ++i)
            ret *= std::pow(values[i], input_coeffs[i]);

        return { ret };
    }
};

struct counter
{
    port x[1];
    time sigma;
    i64 number;

    status initialize() noexcept
    {
        number = { 0 };
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {
        const auto diff =
          std::distance(std::begin(x[0].messages), std::end(x[0].messages));

        number += static_cast<i64>(diff);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { static_cast<double>(number) };
    }
};

struct generator
{
    port y[1];
    time sigma;
    double value;

    simulation* sim = nullptr;
    double default_offset = 0.0;
    source default_source_ta;
    source default_source_value;
    bool stop_on_error = false;

    status initialize() noexcept
    {
        sigma = default_offset;

        if (stop_on_error) {
            irt_return_if_bad(initialize_source(*sim, default_source_ta));
            irt_return_if_bad(initialize_source(*sim, default_source_value));
        } else {
            (void)initialize_source(*sim, default_source_ta);
            (void)initialize_source(*sim, default_source_value);
        }

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {
        if (stop_on_error) {
            irt_return_if_bad(update_source(*sim, default_source_ta, sigma));
            irt_return_if_bad(update_source(*sim, default_source_value, value));
        } else {
            if (is_bad(update_source(*sim, default_source_ta, sigma)))
                sigma = time_domain<time>::infinity;

            if (is_bad(update_source(*sim, default_source_value, value)))
                value = 0.0;
        }

        return status::success;
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(value);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { value };
    }
};

struct constant
{
    port y[1];
    time sigma;

    double default_value = 0.0;

    double value = 0.0;

    status initialize() noexcept
    {
        sigma = time_domain<time>::zero;

        value = default_value;

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(value);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { value };
    }
};

struct flow
{
    port y[1];
    time sigma;

    double default_samplerate = 44100.0;
    double* default_data = nullptr;
    double* default_sigmas = nullptr;
    sz default_size = 0u;

    double accu_sigma;
    sz i;

    status initialize() noexcept
    {
        irt_return_if_fail(default_samplerate > 0.,
                           status::model_flow_bad_samplerate);

        irt_return_if_fail(default_data != nullptr &&
                             default_sigmas != nullptr && default_size > 1,
                           status::model_flow_bad_data);

        sigma = 1.0 / default_samplerate;
        accu_sigma = 0.;
        i = 0;

        return status::success;
    }

    status transition(time t, time /*e*/, time /*r*/) noexcept
    {
        for (; i < default_size; ++i) {
            accu_sigma += default_sigmas[i];

            if (accu_sigma > t) {
                sigma = default_sigmas[i];
                return status::success;
            }
        }

        sigma = time_domain<time>::infinity;
        i = default_size - 1;

        return status::success;
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(default_data[i]);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { default_data[i] };
    }
};

template<size_t PortNumber>
struct accumulator
{
    port x[2 * PortNumber];
    time sigma;
    double number;
    double numbers[PortNumber];

    status initialize() noexcept
    {
        number = 0.0;
        std::fill_n(numbers, PortNumber, 0.0);

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {

        for (size_t i = 0; i != PortNumber; ++i)
            for (const auto& msg : x[i + PortNumber].messages)
                numbers[i] = msg[0];

        for (size_t i = 0; i != PortNumber; ++i)
            for (const auto& msg : x[i].messages)
                if (msg[0] != 0.0)
                    number += numbers[i];

        return status::success;
    }
};

struct cross
{
    port x[4];
    port y[2];
    time sigma;

    double default_threshold = 0.0;

    double threshold;
    double value;
    double if_value;
    double else_value;
    double result;
    double event;

    enum port_name
    {
        port_value,
        port_if_value,
        port_else_value,
        port_threshold
    };

    status initialize() noexcept
    {
        threshold = default_threshold;
        value = threshold - 1.0;
        if_value = 0.0;
        else_value = 0.0;
        result = 0.0;
        event = 0.0;

        sigma = time_domain<time>::zero;

        return status::success;
    }

    status transition(time /*t*/, time /*e*/, time /*r*/) noexcept
    {
        bool have_message = false;
        bool have_message_value = false;
        event = 0.0;

        for (const auto& msg : x[port_threshold].messages) {
            threshold = msg.real[0];
            have_message = true;
        }

        for (const auto& msg : x[port_value].messages) {
            value = msg.real[0];
            have_message_value = true;
            have_message = true;
        }

        for (const auto& msg : x[port_if_value].messages) {
            if_value = msg.real[0];
            have_message = true;
        }

        for (const auto& msg : x[port_else_value].messages) {
            else_value = msg.real[0];
            have_message = true;
        }

        if (have_message_value) {
            event = 0.0;
            if (value >= threshold) {
                else_value = if_value;
                event = 1.0;
            }
        }

        result = else_value;

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(result);
        y[1].messages.emplace_front(event);

        return status::success;
    }

    message observation(const time /*e*/) const noexcept
    {
        return { value, if_value, else_value };
    }
};

template<int QssLevel>
struct abstract_cross
{
    static_assert(1 <= QssLevel && QssLevel <= 3, "Only for Qss1, 2 and 3");

    port x[4];
    port y[3];
    time sigma;

    double default_threshold = 0.0;
    bool default_detect_up = true;

    double threshold;
    double if_value[QssLevel];
    double else_value[QssLevel];
    double value[QssLevel];
    double last_reset;
    bool reach_threshold;
    bool detect_up;

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
        o_event
    };

    status initialize() noexcept
    {
        std::fill_n(if_value, QssLevel, 0.);
        std::fill_n(else_value, QssLevel, 0.);
        std::fill_n(value, QssLevel, 0.);

        threshold = default_threshold;
        value[0] = threshold - 1.0;

        sigma = time_domain<time>::infinity;
        last_reset = time_domain<time>::infinity;
        detect_up = default_detect_up;
        reach_threshold = false;

        return status::success;
    }

    void compute_wake_up() noexcept
    {
        if constexpr (QssLevel == 1) {
            sigma = time_domain<time>::infinity;
        }

        if constexpr (QssLevel == 2) {
            sigma = time_domain<time>::infinity;
            if (value[1]) {
                const auto a = value[1];
                const auto b = value[0] - threshold;
                const auto d = -b * a;

                if (d > 0.)
                    sigma = d;
            }
        }

        if constexpr (QssLevel == 3) {
            sigma = time_domain<time>::infinity;
            if (value[1]) {
                if (value[2]) {
                    const auto a = value[2];
                    const auto b = value[1];
                    const auto c = value[0] - threshold;
                    const auto d = b * b - 4. * a * c;

                    if (d > 0.) {
                        const auto x1 = (-b + std::sqrt(d)) / (2. * a);
                        const auto x2 = (-b - std::sqrt(d)) / (2. * a);

                        if (x1 > 0.) {
                            if (x2 > 0.) {
                                sigma = std::min(x1, x2);
                            } else {
                                sigma = x1;
                            }
                        } else {
                            if (x2 > 0)
                                sigma = x2;
                        }
                    }
                    if (d == 0.) {
                        const auto x = -b / (2. * a);
                        if (x > 0.)
                            sigma = x;
                    }
                } else {
                    const auto a = value[1];
                    const auto b = value[0] - threshold;
                    const auto d = -b * a;

                    if (d > 0.)
                        sigma = d;
                }
            }
        }
    }

    status transition(time t, [[maybe_unused]] time e, time /*r*/) noexcept
    {
        auto& p_threshold = x[port_threshold];
        auto& p_if_value = x[port_if_value];
        auto& p_else_value = x[port_else_value];
        auto& p_value = x[port_value];

        const auto old_else_value = else_value[0];

        for (const auto& msg : p_threshold.messages)
            threshold = msg[0];

        if (p_if_value.messages.empty()) {
            if constexpr (QssLevel == 2)
                if_value[0] += if_value[1] * e;
            if constexpr (QssLevel == 3) {
                if_value[0] += if_value[1] * e + if_value[2] * e * e;
                if_value[1] += 2. * if_value[2] * e;
            }
        } else {
            for (const auto& msg : p_if_value.messages) {
                if_value[0] = msg[0];
                if constexpr (QssLevel >= 2)
                    if_value[1] = msg[1];
                if constexpr (QssLevel == 3)
                    if_value[2] = msg[2];
            }
        }

        if (p_else_value.messages.empty()) {
            if constexpr (QssLevel == 2)
                else_value[0] += else_value[1] * e;
            if constexpr (QssLevel == 3) {
                else_value[0] += else_value[1] * e + else_value[2] * e * e;
                else_value[1] += 2. * else_value[2] * e;
            }
        } else {
            for (const auto& msg : p_else_value.messages) {
                else_value[0] = msg[0];
                if constexpr (QssLevel >= 2)
                    else_value[1] = msg[1];
                if constexpr (QssLevel == 3)
                    else_value[2] = msg[2];
            }
        }

        if (p_value.messages.empty()) {
            if constexpr (QssLevel == 2)
                value[0] += value[1] * e;
            if constexpr (QssLevel == 3) {
                value[0] += value[1] * e + value[2] * e * e;
                value[1] += 2. * value[2] * e;
            }
        } else {
            for (const auto& msg : p_value.messages) {
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
                last_reset = t;
                reach_threshold = true;
                sigma = time_domain<time>::zero;
            } else
                sigma = time_domain<time>::infinity;
        } else if (old_else_value != else_value[0]) {
            sigma = time_domain<time>::zero;
        } else {
            compute_wake_up();
        }

        return status::success;
    }

    status lambda() noexcept
    {
        if constexpr (QssLevel == 1) {
            y[o_else_value].messages.emplace_front(else_value[0]);
            if (reach_threshold) {
                y[o_if_value].messages.emplace_front(if_value[0]);
                y[o_event].messages.emplace_front(1.0);
            }
        }

        if constexpr (QssLevel == 2) {
            y[o_else_value].messages.emplace_front(else_value[0],
                                                   else_value[1]);
            if (reach_threshold) {
                y[o_if_value].messages.emplace_front(if_value[0], if_value[1]);
                y[o_event].messages.emplace_front(1.0);
            }
        }

        if constexpr (QssLevel == 3) {
            y[o_else_value].messages.emplace_front(
              else_value[0], else_value[1], else_value[2]);
            if (reach_threshold) {
                y[o_if_value].messages.emplace_front(
                  if_value[0], if_value[1], if_value[2]);
                y[o_event].messages.emplace_front(1.0);
            }
        }

        return status::success;
    }

    message observation(const time /*t*/) const noexcept
    {
        return { value[0], if_value[0], else_value[0] };
    }
};

using qss1_cross = abstract_cross<1>;
using qss2_cross = abstract_cross<2>;
using qss3_cross = abstract_cross<3>;

inline double
sin_time_function(double t) noexcept
{
    const double f0 = 0.1;
    const double pi = std::acos(-1);
    return std::sin(2 * pi * f0 * t);
}

inline double
square_time_function(double t) noexcept
{
    return t * t;
}

inline double
time_function(double t) noexcept
{
    return t;
}

struct time_func
{
    port y[1];
    time sigma;

    double default_sigma = 0.01;
    double (*default_f)(double) = &time_function;

    double value;
    double (*f)(double) = nullptr;

    status initialize() noexcept
    {
        f = default_f;
        sigma = default_sigma;
        value = 0.0;
        return status::success;
    }

    status transition(time t, time /*e*/, time /*r*/) noexcept
    {
        value = (*f)(t);
        return status::success;
    }

    status lambda() noexcept
    {
        y[0].messages.emplace_front(value);

        return status::success;
    }

    message observation(const time /*t*/) const noexcept
    {
        return { value };
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
    port x[1];
    port y[1];
    time sigma;
    flat_double_list<dated_message> queue;

    double default_ta = 1.0;

    status initialize() noexcept
    {
        if (default_ta <= 0)
            irt_bad_return(status::model_queue_bad_ta);

        if (!queue.get_allocator())
            irt_bad_return(status::model_queue_empty_allocator);

        sigma = time_domain<time>::infinity;
        queue.clear();

        return status::success;
    }

    status transition(time t, time /*e*/, time /*r*/) noexcept
    {
        while (!queue.empty() && queue.front().real[0] <= t)
            queue.pop_front();

        for (const auto& msg : x[0].messages) {
            if (!queue.get_allocator()->can_alloc(1u))
                irt_bad_return(status::model_queue_full);

            queue.emplace_back(t + default_ta, msg[0], msg[1], msg[2], msg[3]);
        }

        if (!queue.empty()) {
            sigma = queue.front().real[0] - t;
            sigma = sigma <= 0. ? 0. : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return status::success;
    }

    status lambda() noexcept
    {
        if (!queue.empty()) {
            auto it = queue.begin();
            auto end = queue.end();
            const auto t = it->real[0];

            for (; it != end && it->real[0] <= t; ++it)
                y[0].messages.emplace_front(
                  it->real[1], it->real[2], it->real[3], it->real[4]);
        }

        return status::success;
    }
};

struct dynamic_queue
{
    port x[1];
    port y[1];
    time sigma;
    flat_double_list<dated_message> queue;

    simulation* sim = nullptr;
    source default_source_ta;
    bool stop_on_error = false;

    status initialize() noexcept
    {
        sigma = time_domain<time>::infinity;
        queue.clear();

        if (stop_on_error)
            irt_return_if_bad(initialize_source(*sim, default_source_ta));
        else
            (void)initialize_source(*sim, default_source_ta);

        return status::success;
    }

    status transition(time t, time /*e*/, time /*r*/) noexcept
    {
        while (!queue.empty() && queue.front().real[0] <= t)
            queue.pop_front();

        for (const auto& msg : x[0].messages) {
            if (!queue.get_allocator()->can_alloc(1u))
                irt_bad_return(status::model_dynamic_queue_full);

            double ta;
            if (stop_on_error) {
                irt_return_if_bad(update_source(*sim, default_source_ta, ta));
                queue.emplace_back(t + ta, msg[0], msg[1], msg[2], msg[3]);
            } else {
                if (is_success(update_source(*sim, default_source_ta, ta)))
                    queue.emplace_back(t + ta, msg[0], msg[1], msg[2], msg[3]);
            }
        }

        if (!queue.empty()) {
            sigma = queue.front().real[0] - t;
            sigma = sigma <= 0. ? 0. : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return status::success;
    }

    status lambda() noexcept
    {
        if (!queue.empty()) {
            auto it = queue.begin();
            auto end = queue.end();
            const auto t = it->real[0];

            for (; it != end && it->real[0] <= t; ++it)
                y[0].messages.emplace_front(
                  it->real[1], it->real[2], it->real[3], it->real[4]);
        }

        return status::success;
    }
};

struct priority_queue
{
    port x[1];
    port y[1];
    time sigma;
    flat_double_list<dated_message> queue;
    double default_ta = 1.0;

    simulation* sim = nullptr;
    source default_source_ta;
    bool stop_on_error = false;

private:
    status try_to_insert(const time t, const message& msg) noexcept
    {
        if (!queue.get_allocator()->can_alloc(1u))
            irt_bad_return(status::model_priority_queue_source_is_null);

        if (queue.empty() || queue.begin()->real[0] > t) {
            queue.emplace_front(t, msg[0], msg[1], msg[2], msg[3]);
        } else {
            auto it = queue.begin();
            auto end = queue.end();
            ++it;

            for (; it != end; ++it) {
                if (it->real[0] > t) {
                    queue.emplace(it, t, msg[0], msg[1], msg[2], msg[3]);
                    return status::success;
                }
            }
        }

        return status::success;
    }

public:
    status initialize() noexcept
    {
        if (!queue.get_allocator())
            irt_bad_return(status::model_priority_queue_empty_allocator);

        if (stop_on_error)
            irt_return_if_bad(initialize_source(*sim, default_source_ta));
        else
            (void)initialize_source(*sim, default_source_ta);

        sigma = time_domain<time>::infinity;
        queue.clear();

        return status::success;
    }

    status transition(time t, time /*e*/, time /*r*/) noexcept
    {
        while (!queue.empty() && queue.front().real[0] <= t)
            queue.pop_front();

        for (const auto& msg : x[0].messages) {
            double value;

            if (stop_on_error) {
                irt_return_if_bad(
                  update_source(*sim, default_source_ta, value));

                if (auto ret = try_to_insert(value + t, msg); is_bad(ret))
                    irt_bad_return(status::model_priority_queue_full);
            } else {
                if (is_success(update_source(*sim, default_source_ta, value))) {
                    if (auto ret = try_to_insert(value + t, msg); is_bad(ret))
                        irt_bad_return(status::model_priority_queue_full);
                }
            }
        }

        if (!queue.empty()) {
            sigma = queue.front().real[0] - t;
            sigma = sigma <= 0. ? 0. : sigma;
        } else {
            sigma = time_domain<time>::infinity;
        }

        return status::success;
    }

    status lambda() noexcept
    {
        if (!queue.empty()) {
            auto it = queue.begin();
            auto end = queue.end();
            const auto t = it->real[0];

            for (; it != end && it->real[0] <= t; ++it)
                y[0].messages.emplace_front(
                  it->real[1], it->real[2], it->real[3], it->real[4]);
        }

        return status::success;
    }
};

constexpr sz
max(sz a)
{
    return a;
}

template<typename... Args>
constexpr sz
max(sz a, Args... args)
{
    return std::max(max(args...), a);
}

constexpr sz
max_size_in_bytes() noexcept
{
    return max(sizeof(none),
               sizeof(qss1_integrator),
               sizeof(qss1_multiplier),
               sizeof(qss1_cross),
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
               sizeof(flow));
}

struct model
{
    double tl = 0.0;
    double tn = time_domain<time>::infinity;
    heap::handle handle = nullptr;

    observer_id obs_id = observer_id{ 0 };
    dynamics_type type{ dynamics_type::none };

    std::byte dyn[max_size_in_bytes()];
};

/*****************************************************************************
 *
 * scheduller
 *
 ****************************************************************************/

class scheduller
{
private:
    using list = flat_list<model_id>;
    using allocator = typename list::allocator_type;

    allocator m_list_allocator;
    list m_list;
    heap m_heap;

public:
    scheduller() = default;

    status init(size_t capacity) noexcept
    {
        irt_return_if_bad(m_heap.init(capacity));
        irt_return_if_bad(m_list_allocator.init(capacity));

        m_list.set_allocator(&m_list_allocator);

        return status::success;
    }

    void clear()
    {
        m_heap.clear();
        m_list.clear();
    }

    /**
     * @brief Insert a newly model into the scheduller.
     */
    void insert(model& mdl, model_id id, time tn) noexcept
    {
        irt_assert(mdl.handle == nullptr);

        mdl.handle = m_heap.insert(tn, id);
    }

    /**
     * @brief Reintegrate or reinsert an old popped model into the
     * scheduller.
     */
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

    const list& pop() noexcept
    {
        time t = tn();

        m_list.clear();

        m_list.emplace_front(m_heap.pop()->id);

        while (!m_heap.empty() && t == tn())
            m_list.emplace_front(m_heap.pop()->id);

        return m_list;
    }

    const list& list_model_id() const noexcept
    {
        return m_list;
    }

    time tn() const noexcept
    {
        return m_heap.top()->tn;
    }

    bool empty() const noexcept
    {
        return m_heap.empty();
    }

    size_t size() const noexcept
    {
        return m_heap.size();
    }
};

/*****************************************************************************
 *
 * simulation
 *
 ****************************************************************************/

template<typename Dynamics>
static constexpr dynamics_type
dynamics_typeof() noexcept
{
    if constexpr (std::is_same_v<Dynamics, none>)
        return dynamics_type::none;

    if constexpr (std::is_same_v<Dynamics, qss1_integrator>)
        return dynamics_type::qss1_integrator;
    if constexpr (std::is_same_v<Dynamics, qss1_multiplier>)
        return dynamics_type::qss1_multiplier;
    if constexpr (std::is_same_v<Dynamics, qss1_cross>)
        return dynamics_type::qss1_cross;
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
    if constexpr (std::is_same_v<Dynamics, flow>)
        return dynamics_type::flow;

    return dynamics_type::none;
}

template<typename Dynamics>
Dynamics&
get_dyn(model& mdl) noexcept
{
    irt_assert(dynamics_typeof<Dynamics>() == mdl.type);
    return *reinterpret_cast<Dynamics*>(&mdl.dyn);
}

template<typename Dynamics>
const Dynamics&
get_dyn(const model& mdl) noexcept
{
    irt_assert(dynamics_typeof<Dynamics>() == mdl.type);
    return *reinterpret_cast<const Dynamics*>(&mdl.dyn);
}

template<typename Dynamics>
constexpr const model&
get_model(const Dynamics& d) noexcept
{
    const Dynamics* __mptr = &d;
    return *(const model*)((const char*)__mptr - offsetof(model, dyn));
}

template<typename Dynamics>
constexpr model&
get_model(Dynamics& d) noexcept
{
    Dynamics* __mptr = &d;
    return *(model*)((char*)__mptr - offsetof(model, dyn));
}

struct simulation
{
    flat_list<model_id>::allocator_type model_list_allocator;
    flat_list<message>::allocator_type message_list_allocator;
    shared_flat_list<node>::allocator_type node_list_allocator;
    flat_list<port*>::allocator_type emitting_output_port_allocator;
    flat_double_list<dated_message>::allocator_type dated_message_allocator;
    flat_double_list<record>::allocator_type flat_double_list_shared_allocator;

    data_array<model, model_id> models;
    data_array<message, message_id> messages;
    data_array<observer, observer_id> observers;

    scheduller sched;

    /**
     * @brief Use initialize, generate or finalize data from a source.
     *
     * See the @c external_source class for an implementation.
     */
    function_ref<status(source&, const source::operation_type)> source_dispatch;

    time begin = time_domain<time>::zero;
    time end = time_domain<time>::infinity;

    template<typename Dynamics>
    constexpr model_id get_id(const Dynamics& dyn) const
    {
        return models.get_id(get_model(dyn));
    }

    template<typename Function, typename... Args>
    constexpr auto dispatch(model& mdl, Function&& f, Args... args) noexcept
    {
        switch (mdl.type) {
        case dynamics_type::none:
            return f(*reinterpret_cast<none*>(&mdl.dyn), args...);

        case dynamics_type::qss1_integrator:
            return f(*reinterpret_cast<qss1_integrator*>(&mdl.dyn), args...);
        case dynamics_type::qss1_multiplier:
            return f(*reinterpret_cast<qss1_multiplier*>(&mdl.dyn), args...);
        case dynamics_type::qss1_cross:
            return f(*reinterpret_cast<qss1_cross*>(&mdl.dyn), args...);
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
        case dynamics_type::flow:
            return f(*reinterpret_cast<flow*>(&mdl.dyn), args...);
        }

        irt_unreachable();
    }

    template<typename Function, typename... Args>
    constexpr auto dispatch(const model& mdl,
                            Function&& f,
                            Args... args) const noexcept
    {
        switch (mdl.type) {
        case dynamics_type::none:
            return f(*reinterpret_cast<const none*>(&mdl.dyn), args...);

        case dynamics_type::qss1_integrator:
            return f(*reinterpret_cast<const qss1_integrator*>(&mdl.dyn),
                     args...);
        case dynamics_type::qss1_multiplier:
            return f(*reinterpret_cast<const qss1_multiplier*>(&mdl.dyn),
                     args...);
        case dynamics_type::qss1_cross:
            return f(*reinterpret_cast<const qss1_cross*>(&mdl.dyn), args...);
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
            return f(*reinterpret_cast<const qss2_integrator*>(&mdl.dyn),
                     args...);
        case dynamics_type::qss2_multiplier:
            return f(*reinterpret_cast<const qss2_multiplier*>(&mdl.dyn),
                     args...);
        case dynamics_type::qss2_cross:
            return f(*reinterpret_cast<const qss2_cross*>(&mdl.dyn), args...);
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
            return f(*reinterpret_cast<const qss3_integrator*>(&mdl.dyn),
                     args...);
        case dynamics_type::qss3_multiplier:
            return f(*reinterpret_cast<const qss3_multiplier*>(&mdl.dyn),
                     args...);
        case dynamics_type::qss3_cross:
            return f(*reinterpret_cast<const qss3_cross*>(&mdl.dyn), args...);
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
            return f(*reinterpret_cast<const dynamic_queue*>(&mdl.dyn),
                     args...);
        case dynamics_type::priority_queue:
            return f(*reinterpret_cast<const priority_queue*>(&mdl.dyn),
                     args...);
        case dynamics_type::generator:
            return f(*reinterpret_cast<const generator*>(&mdl.dyn), args...);
        case dynamics_type::constant:
            return f(*reinterpret_cast<const constant*>(&mdl.dyn), args...);
        case dynamics_type::cross:
            return f(*reinterpret_cast<const cross*>(&mdl.dyn), args...);
        case dynamics_type::accumulator_2:
            return f(*reinterpret_cast<const accumulator_2*>(&mdl.dyn),
                     args...);
        case dynamics_type::time_func:
            return f(*reinterpret_cast<const time_func*>(&mdl.dyn), args...);
        case dynamics_type::flow:
            return f(*reinterpret_cast<const flow*>(&mdl.dyn), args...);
        }

        irt_unreachable();
    }

public:
    status init(size_t model_capacity, size_t messages_capacity)
    {
        constexpr size_t ten{ 10 };

        irt_return_if_bad(model_list_allocator.init(model_capacity * ten));
        irt_return_if_bad(message_list_allocator.init(messages_capacity * ten));
        irt_return_if_bad(node_list_allocator.init(model_capacity * ten));
        irt_return_if_bad(dated_message_allocator.init(model_capacity * ten * ten));
        irt_return_if_bad(emitting_output_port_allocator.init(model_capacity));

        irt_return_if_bad(sched.init(model_capacity));

        irt_return_if_bad(models.init(model_capacity));
        irt_return_if_bad(messages.init(messages_capacity));
        irt_return_if_bad(observers.init(model_capacity));
        irt_return_if_bad(
          flat_double_list_shared_allocator.init(model_capacity * ten));

        return status::success;
    }

    bool can_alloc() const noexcept
    {
        return models.can_alloc();
    }


    bool can_alloc(size_t place) const noexcept
    {
        return models.can_alloc(place);
    }

    /**
     * @brief cleanup simulation object
     *
     * Clean scheduller and input/output port from message.
     */
    void clean() noexcept
    {
        sched.clear();

        model* mdl = nullptr;
        while (models.next(mdl)) {
            dispatch(
              *mdl,
              []<typename Dynamics>([[maybe_unused]] Dynamics& dyn) -> void {
                  if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                      for (sz i = 0; i < std::size(dyn.x); ++i)
                          dyn.x[i].messages.clear();
                  }

                  if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                      for (sz i = 0; i < std::size(dyn.y); ++i)
                          dyn.y[i].messages.clear();
                  }
              });
        }
    }

    /**
     * @brief cleanup simulation and destroy all models and connections
     */
    void clear() noexcept
    {
        clean();

        model_list_allocator.reset();
        message_list_allocator.reset();
        node_list_allocator.reset();
        dated_message_allocator.reset();

        emitting_output_port_allocator.reset();

        models.clear();
        messages.clear();
        observers.clear();

        begin = time_domain<time>::zero;
        end = time_domain<time>::infinity;
    }

    /** @brief This function allocates dynamics and models.
     *
     */
    template<typename Dynamics>
    Dynamics& alloc() noexcept
    {
        /* Use can_alloc before using this function. */
        irt_assert(!models.full());

        auto& mdl = models.alloc();
        mdl.type = dynamics_typeof<Dynamics>();

        mdl.handle = nullptr;

        new (&mdl.dyn) Dynamics{};
        Dynamics& dyn = get_dyn<Dynamics>(mdl);

        if constexpr (std::is_same_v<Dynamics, integrator>)
            dyn.archive.set_allocator(&flat_double_list_shared_allocator);
        if constexpr (std::is_same_v<Dynamics, quantifier>)
            dyn.archive.set_allocator(&flat_double_list_shared_allocator);

        if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
            for (size_t i = 0, e = std::size(dyn.x); i != e; ++i) {
                dyn.x[i].messages.set_allocator(&message_list_allocator);
            }
        }

        if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
            for (size_t i = 0, e = std::size(dyn.y); i != e; ++i) {
                dyn.y[i].messages.set_allocator(&message_list_allocator);
            }
        }

        return dyn;
    }

    /** @brief This function allocates dynamics and models.
     *
     */
    model& alloc(dynamics_type type) noexcept
    {
        /* Use can_alloc before using this function. */
        irt_assert(!models.full());

        auto& mdl = models.alloc();
        mdl.type = type;
        mdl.handle = nullptr;

        dispatch(mdl, [this]<typename Dynamics>(Dynamics& dyn) -> void {
            new (&dyn) Dynamics{};

            if constexpr (std::is_same_v<Dynamics, integrator>)
                dyn.archive.set_allocator(&flat_double_list_shared_allocator);
            if constexpr (std::is_same_v<Dynamics, quantifier>)
                dyn.archive.set_allocator(&flat_double_list_shared_allocator);

            if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                for (size_t i = 0, e = std::size(dyn.x); i != e; ++i) {
                    dyn.x[i].messages.set_allocator(&message_list_allocator);
                }
            }

            if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                for (size_t i = 0, e = std::size(dyn.y); i != e; ++i) {
                    dyn.y[i].messages.set_allocator(&message_list_allocator);
                }
            }
        });

        return mdl;
    }

    void observe(model& mdl, observer& obs) noexcept
    {
        mdl.obs_id = observers.get_id(obs);
        obs.model = models.get_id(mdl);
    }

    status deallocate(model_id id)
    {
        auto* mdl = models.try_to_get(id);
        irt_return_if_fail(mdl, status::unknown_dynamics);

        if (auto* obs = observers.try_to_get(mdl->obs_id); obs) {
            obs->model = static_cast<model_id>(0);
            mdl->obs_id = static_cast<observer_id>(0);
            observers.free(*obs);
        }

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
        if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
            auto& mdl_src = get_model(dyn);

            for (int i = 0, e = (int)std::size(dyn.y); i != e; ++i) {
                while (!dyn.y[i].connections.empty()) {
                    auto* mdl_dst =
                      models.try_to_get(dyn.y[i].connections.front().model);
                    if (mdl_dst) {
                        disconnect(mdl_src,
                                   i,
                                   *mdl_dst,
                                   dyn.y[i].connections.front().port_index);
                    }
                }

                dyn.y[i].connections.clear(node_list_allocator);
                dyn.y[i].messages.clear();
            }
        }

        if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
            auto& mdl_dst = get_model(dyn);

            for (int i = 0, e = (int)std::size(dyn.x); i != e; ++i) {
                while (!dyn.x[i].connections.empty()) {
                    auto* mdl_src =
                      models.try_to_get(dyn.x[i].connections.front().model);
                    if (mdl_src) {
                        disconnect(*mdl_src,
                                   dyn.x[i].connections.front().port_index,
                                   mdl_dst,
                                   i);
                    }
                }

                dyn.x[i].connections.clear(node_list_allocator);
                dyn.x[i].messages.clear();
            }
        }

        dyn.~Dynamics();
    }

    bool is_ports_compatible(const model& mdl_src,
                             [[maybe_unused]] const int o_port_index,
                             const model& mdl_dst,
                             const int i_port_index) const noexcept
    {
        if (&mdl_src == &mdl_dst)
            return false;

        switch (mdl_src.type) {
        case dynamics_type::none:
            return false;

        case dynamics_type::quantifier:
            if (mdl_dst.type == dynamics_type::integrator &&
                i_port_index ==
                  static_cast<int>(integrator::port_name::port_quanta))
                return true;

            return false;

        case dynamics_type::qss1_integrator:
        case dynamics_type::qss1_multiplier:
        case dynamics_type::qss1_cross:
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
        case dynamics_type::qss2_cross:
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
        case dynamics_type::qss3_cross:
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
        case dynamics_type::constant:
        case dynamics_type::cross:
        case dynamics_type::time_func:
        case dynamics_type::flow:
        case dynamics_type::accumulator_2:
            if (mdl_dst.type == dynamics_type::integrator &&
                i_port_index ==
                  static_cast<int>(integrator::port_name::port_quanta))
                return false;

            return true;
        }

        return false;
    }

    bool can_connect(size_t number) const noexcept
    {
        return node_list_allocator.can_alloc(number);
    }

    status get_input_port(model& src, int port_src, port*& p)
    {
        return dispatch(
          src, [port_src, &p]<typename Dynamics>(Dynamics& dyn) -> status {
              if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                  if (port_src >= 0 && port_src < (int)std::size(dyn.x)) {
                      p = &dyn.x[port_src];
                      return status::success;
                  }
              }

              return status::model_connect_output_port_unknown;
          });
    }

    status get_output_port(model& dst, int port_dst, port*& p)
    {
        return dispatch(
          dst, [port_dst, &p]<typename Dynamics>(Dynamics& dyn) -> status {
              if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                  if (port_dst >= 0 && port_dst < (int)std::size(dyn.y)) {
                      p = &dyn.y[port_dst];
                      return status::success;
                  }
              }

              return status::model_connect_output_port_unknown;
          });
    }

    status connect(model& src, int port_src, model& dst, int port_dst) noexcept
    {
        port* src_port = nullptr;
        irt_return_if_bad(get_output_port(src, port_src, src_port));

        port* dst_port = nullptr;
        irt_return_if_bad(get_input_port(dst, port_dst, dst_port));

        auto model_src_id = models.get_id(src);
        auto model_dst_id = models.get_id(dst);

        auto it = src_port->connections.begin();
        auto et = src_port->connections.end();

        while (it != et) {
            irt_return_if_fail(
              !(it->model == model_dst_id && it->port_index == port_dst),
              status::model_connect_already_exist);

            ++it;
        };

        irt_return_if_fail(is_ports_compatible(src, port_src, dst, port_dst),
                           status::model_connect_bad_dynamics);

        src_port->connections.emplace_front(
          node_list_allocator, model_dst_id, port_dst);
        dst_port->connections.emplace_front(
          node_list_allocator, model_src_id, port_src);

        return status::success;
    }

    template<typename DynamicsSrc, typename DynamicsDst>
    status connect(DynamicsSrc& src,
                   int port_src,
                   DynamicsDst& dst,
                   int port_dst) noexcept
    {
        model& src_model = get_model(src);
        model& dst_model = get_model(dst);
        model_id model_src_id = get_id(src);
        model_id model_dst_id = get_id(dst);

        auto it = std::begin(src.y[port_src].connections);
        auto et = std::end(src.y[port_src].connections);

        while (it != et) {
            irt_return_if_fail(
              !(it->model == model_dst_id && it->port_index == port_dst),
              status::model_connect_already_exist);

            ++it;
        }

        irt_return_if_fail(
          is_ports_compatible(src_model, port_src, dst_model, port_dst),
          status::model_connect_bad_dynamics);

        src.y[port_src].connections.emplace_front(
          node_list_allocator, model_dst_id, port_dst);
        dst.x[port_dst].connections.emplace_front(
          node_list_allocator, model_src_id, port_src);

        return status::success;
    }

    status disconnect(model& src,
                      int port_src,
                      model& dst,
                      int port_dst) noexcept
    {
        port* src_port = nullptr;
        irt_return_if_bad(get_output_port(src, port_src, src_port));

        port* dst_port = nullptr;
        irt_return_if_bad(get_input_port(dst, port_dst, dst_port));

        {
            const auto end = std::end(src_port->connections);
            auto it = std::begin(src_port->connections);

            if (it->model == models.get_id(dst) && it->port_index == port_dst) {
                src_port->connections.pop_front(node_list_allocator);
            } else {
                auto prev = it++;
                while (it != end) {
                    if (it->model == models.get_id(dst) &&
                        it->port_index == port_dst) {
                        src_port->connections.erase_after(node_list_allocator,
                                                          prev);
                        break;
                    }
                    prev = it++;
                }
            }
        }

        {
            const auto end = std::end(dst_port->connections);
            auto it = std::begin(dst_port->connections);

            if (it->model == models.get_id(src) && it->port_index == port_src) {
                dst_port->connections.pop_front(node_list_allocator);
            } else {
                auto prev = it++;
                while (it != end) {
                    if (it->model == models.get_id(src) &&
                        it->port_index == port_src) {
                        dst_port->connections.erase_after(node_list_allocator,
                                                          prev);
                        break;
                    }
                    prev = it++;
                }
            }
        }

        return status::success;
    }

    status initialize(time t) noexcept
    {
        clean();

        irt::model* mdl = nullptr;
        while (models.next(mdl))
            irt_return_if_bad(make_initialize(*mdl, t));

        irt::observer* obs = nullptr;
        while (observers.next(obs)) {
            if (auto* mdl = models.try_to_get(obs->model); mdl) {
                obs->msg.reset();
                obs->cb(
                  *obs, mdl->type, mdl->tl, t, observer::status::initialize);
            }
        }

        return status::success;
    }

    status run(time& t) noexcept
    {
        if (sched.empty()) {
            t = time_domain<time>::infinity;
            return status::success;
        }

        t = sched.tn();

        const auto& bag = sched.pop();

        flat_list<port*> emitting_output_ports(&emitting_output_port_allocator);

        for (const auto id : bag)
            if (auto* mdl = models.try_to_get(id); mdl)
                irt_return_if_bad(
                  make_transition(*mdl, t, emitting_output_ports));

        for (port* port_src : emitting_output_ports) {
            for (node& dst : port_src->connections) {
                if (auto* mdl = models.try_to_get(dst.model); mdl) {
                    port* port_dst = nullptr;
                    irt_return_if_bad(
                      get_input_port(*mdl, dst.port_index, port_dst));

                    for (const message& msg : port_src->messages) {
                        port_dst->messages.emplace_front(msg);
                        sched.update(*mdl, t);
                    }
                }
            }

            port_src->messages.clear();
        }

        return status::success;
    }

    template<typename Dynamics>
    status make_initialize(model& mdl, Dynamics& dyn, time t) noexcept
    {
        if constexpr (std::is_same_v<Dynamics, queue> ||
                      std::is_same_v<Dynamics, dynamic_queue> ||
                      std::is_same_v<Dynamics, priority_queue>)
            dyn.queue.set_allocator(&dated_message_allocator);

        if constexpr (std::is_same_v<Dynamics, generator> ||
                      std::is_same_v<Dynamics, dynamic_queue> ||
                      std::is_same_v<Dynamics, priority_queue>)
            dyn.sim = this;

        if constexpr (is_detected_v<initialize_function_t, Dynamics>)
            irt_return_if_bad(dyn.initialize());

        mdl.tl = t;
        mdl.tn = t + dyn.sigma;
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
    status make_transition(model& mdl,
                           Dynamics& dyn,
                           time t,
                           flat_list<port*>& emitting_output_ports) noexcept
    {
        if constexpr (is_detected_v<observation_function_t, Dynamics>) {
            if (mdl.obs_id != static_cast<observer_id>(0)) {
                if (auto* obs = observers.try_to_get(mdl.obs_id); obs) {
                    obs->msg = dyn.observation(t - mdl.tl);
                    obs->cb(*obs, mdl.type, mdl.tl, t, observer::status::run);
                } else {
                    mdl.obs_id = static_cast<observer_id>(0);
                }
            }
        }

        if (mdl.tn == mdl.handle->tn) {
            if constexpr (is_detected_v<lambda_function_t, Dynamics>) {
                if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                    irt_return_if_bad(dyn.lambda());

                    for (size_t i = 0, e = std::size(dyn.y); i != e; ++i)
                        if (!dyn.y[i].messages.empty())
                            emitting_output_ports.emplace_front(&dyn.y[i]);
                }
            }
        }

        if constexpr (is_detected_v<transition_function_t, Dynamics>)
            irt_return_if_bad(dyn.transition(t, t - mdl.tl, mdl.tn - t));

        if constexpr (is_detected_v<has_input_port_t, Dynamics>)
            for (size_t i = 0, e = std::size(dyn.x); i != e; ++i)
                dyn.x[i].messages.clear();

        irt_assert(mdl.tn >= t);

        mdl.tl = t;
        mdl.tn = t + dyn.sigma;
        if (dyn.sigma && mdl.tn == t)
            mdl.tn = std::nextafter(t, t + 1.);

        sched.reintegrate(mdl, mdl.tn);

        return status::success;
    }

    status make_transition(model& mdl, time t, flat_list<port*>& o) noexcept
    {
        return dispatch(mdl,
                        [this, &mdl, t, &o]<typename Dynamics>(Dynamics& dyn) {
                            return this->make_transition(mdl, dyn, t, o);
                        });
    }

    template<typename Dynamics>
    void make_finalize(model& mdl,
                       Dynamics& dyn,
                       observer& obs,
                       time t) noexcept
    {
        if constexpr (is_detected_v<observation_function_t, Dynamics>) {
            obs.msg = dyn.observation(t - mdl.tl);
            obs.cb(obs, mdl.type, mdl.tl, t, observer::status::finalize);
        }

        if constexpr (std::is_same_v<Dynamics, dynamic_queue> ||
                      std::is_same_v<Dynamics, priority_queue>)
            source_dispatch(dyn.default_source_ta,
                            source::operation_type::finalize);

        if constexpr (std::is_same_v<Dynamics, generator>) {
            source_dispatch(dyn.default_source_ta,
                            source::operation_type::finalize);
            source_dispatch(dyn.default_source_value,
                            source::operation_type::finalize);
        }
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
    void finalize(time t) noexcept
    {
        model* mdl = nullptr;
        while (models.next(mdl)) {
            if (mdl->obs_id != static_cast<observer_id>(0)) {
                if (auto* obs = observers.try_to_get(mdl->obs_id); obs) {
                    dispatch(
                      *mdl,
                      [this, mdl, obs, t]<typename Dynamics>(Dynamics& dyn) {
                          this->make_finalize(*mdl, dyn, *obs, t);
                      });
                }
            }
        }
    }
};

inline status
initialize_source(simulation& sim, source& src) noexcept
{
    return sim.source_dispatch(src, source::operation_type::initialize);
}

inline status
update_source(simulation& sim, source& src, double& val) noexcept
{
    if (src.next(val))
        return status::success;

    if (auto ret = sim.source_dispatch(src, source::operation_type::update);
        is_bad(ret))
        return ret;

    return src.next(val) ? status::success : status::source_empty;
}

} // namespace irt

#endif
