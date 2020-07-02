// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2020
#define ORG_VLEPROJECT_IRRITATOR_2020

#include <algorithm>
#include <limits>
#include <string_view>

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <vector>

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

    dynamics_unknown_id,
    dynamics_unknown_port_id,
    dynamics_not_enough_memory,

    model_connect_output_port_unknown,
    model_connect_input_port_unknown,
    model_connect_already_exist,
    model_connect_bad_dynamics,

    model_adder_empty_init_message,
    model_adder_bad_init_message,
    model_adder_bad_external_message,

    model_mult_empty_init_message,
    model_mult_bad_init_message,
    model_mult_bad_external_message,

    model_integrator_dq_error,
    model_integrator_X_error,

    model_integrator_internal_error,
    model_integrator_output_error,
    model_integrator_running_without_x_dot,
    model_integrator_ta_with_bad_x_dot,
    model_integrator_bad_external_message,

    model_quantifier_bad_quantum_parameter,
    model_quantifier_bad_archive_length_parameter,
    model_quantifier_shifting_value_neg,
    model_quantifier_shifting_value_less_1,
    model_quantifier_bad_external_message,

    model_cross_bad_external_message,

    model_time_func_bad_init_message,

    model_accumulator_bad_external_message,

    gui_not_enough_memory,

    io_file_format_error,
    io_file_format_model_error,
    io_file_format_model_number_error,
    io_file_format_model_unknown,
    io_file_format_dynamics_unknown,
    io_file_format_dynamics_limit_reach,
    io_file_format_dynamics_init_error
};

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

template<typename T, typename... Args>
constexpr bool
match(const T& s, Args... args) noexcept
{
    return ((s == args) || ... || false);
}

#ifndef NDEBUG
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) &&         \
  __GNUC__ >= 2
#define irt_breakpoint()                                                       \
    do {                                                                       \
        __asm__ __volatile__("int $03");                                       \
    } while (0)
#elif (defined(_MSC_VER) || defined(__DMC__)) && defined(_M_IX86)
#define irt_breakpoint()                                                       \
    do {                                                                       \
        __asm int 3h                                                           \
    } while (0)
#elif defined(_MSC_VER)
#define irt_breakpoint()                                                       \
    do {                                                                       \
        __debugbreak();                                                        \
    } while (0)
#elif defined(__alpha__) && !defined(__osf__) && defined(__GNUC__) &&          \
  __GNUC__ >= 2
#define irt_breakpoint()                                                       \
    do {                                                                       \
        __asm__ __volatile__("bpt");                                           \
    } while (0)
#elif defined(__APPLE__)
#define irt_breakpoint()                                                       \
    do {                                                                       \
        __builtin_trap();                                                      \
    } while (0)
#else /* !__i386__ && !__alpha__ */
#define irt_breakpoint()                                                       \
    do {                                                                       \
        raise(SIGTRAP);                                                        \
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

inline status
check_return(status s) noexcept
{
    if (s != status::success)
        irt_breakpoint();

    return s;
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

        size_ = static_cast<unsigned char>(copy_length);
    }

    constexpr std::string_view sv() const noexcept
    {
        return { buffer_, size_ };
    }

    constexpr void append(const std::string_view str) noexcept
    {
        const size_t remaining = length - size_;

        if (remaining) {
            size_t copy = std::min(remaining - 1, str.size());
            std::strncpy(buffer_ + size_, str.data(), copy);
            copy += size_;
            size_ = static_cast<unsigned char>(copy);
            assert(size_ < length);

            buffer_[size_] = '\0';
        }
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

enum class value_type : i8
{
    none,
    integer_8,
    integer_32,
    integer_64,
    real_32,
    real_64
};

template<typename T, size_t length>
struct span
{
public:
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;
    using const_reference = T&;
    using iterator = T*;
    using const_iterator = const T*;

private:
    const T* data_;

public:
    constexpr span(const T* ptr)
      : data_(ptr)
    {}

    constexpr T* data() noexcept
    {
        return data_;
    }

    constexpr const T* data() const noexcept
    {
        return data_;
    }

    constexpr size_t size() const noexcept
    {
        return length;
    }

    constexpr T operator[](size_t i) const noexcept
    {
        assert(i < length);
        return data_[i];
    }

    constexpr iterator begin() noexcept
    {
        return iterator{ data_ };
    }

    constexpr iterator end() noexcept
    {
        return iterator{ data_ + length };
    }

    constexpr const_iterator begin() const noexcept
    {
        return iterator{ data_ };
    }

    constexpr const_iterator end() const noexcept
    {
        return iterator{ data_ + length };
    }
};

template<class T, class... Rest>
constexpr bool
are_all_same() noexcept
{
    return (std::is_same_v<T, Rest> && ...);
}

struct message
{
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;

    union
    {
        i8 integer_8[32];  // 32 bytes
        i32 integer_32[8]; // 8 * 4 bytes
        i64 integer_64[4]; // 4 * 8 bytes
        float real_32[8];  // 8 * 4 bytes
        double real_64[4]; // 4 * 8 bytes
    };

    u8 length;
    value_type type;

    constexpr std::size_t size() const noexcept
    {
        return length;
    }

    constexpr message() noexcept
      : integer_8{ 0 }
      , length{ 0 }
      , type{ value_type::none }
    {}

    template<typename... T>
    constexpr message(T... args) noexcept
    {
        if constexpr (are_all_same<i8, T...>()) {
            static_assert(sizeof...(args) <= 32, "i8 message limited to 32");
            using unused = i8[];
            length = 0;
            (void)unused{ i8(0), (integer_8[length++] = args, i8(0))... };
            type = value_type::integer_8;
        } else if constexpr (are_all_same<i32, T...>()) {
            static_assert(sizeof...(args) <= 8, "i32 message limited to 32");
            using unused = i32[];
            length = 0;
            (void)unused{ i32(0), (integer_32[length++] = args, i32(0))... };
            type = value_type::integer_32;
        } else if constexpr (are_all_same<i64, T...>()) {
            static_assert(sizeof...(args) <= 4, "i64 message limited to 32");
            using unused = i64[];
            length = 0;
            (void)unused{ i64(0), (integer_64[length++] = args, i64(0))... };
            type = value_type::integer_64;
        } else if constexpr (are_all_same<float, T...>()) {
            static_assert(sizeof...(args) <= 8, "float message limited to 8");
            using unused = float[];
            length = 0;
            (void)unused{ 0.0f, (real_32[length++] = args, 0.0f)... };
            type = value_type::real_32;
            return;
        } else if constexpr (are_all_same<double, T...>()) {
            static_assert(sizeof...(args) <= 4, "double message limited to 4");
            using unused = double[];
            length = 0;
            (void)unused{ 0.0, (real_64[length++] = args, 0.0)... };
            type = value_type::real_64;
        }
    }

    constexpr span<i8, 32> to_integer_8() const
    {
        assert(type == value_type::integer_8);
        return span<i8, 32>(integer_8);
    }

    constexpr span<i32, 8> to_integer_32() const
    {
        assert(type == value_type::integer_32);
        return span<i32, 8>(integer_32);
    }

    constexpr span<i64, 4> to_integer_64() const
    {
        assert(type == value_type::integer_64);
        return span<i64, 4>(integer_64);
    }

    constexpr span<float, 8> to_real_32() const
    {
        assert(type == value_type::real_32);
        return span<float, 8>(real_32);
    }

    constexpr span<double, 4> to_real_64() const
    {
        assert(type == value_type::real_64);
        return span<double, 4>(real_64);
    }

    template<typename T>
    constexpr i8 to_integer_8(T i) const
    {
        static_assert(std::is_integral_v<T>, "need [unsigned] integer");

        if constexpr (std::is_signed_v<T>)
            assert(i >= 0);

        assert(type == value_type::integer_8);
        assert(i < static_cast<T>(length));

        return integer_8[i];
    }

    template<typename T>
    constexpr i32 to_integer_32(T i) const
    {
        static_assert(std::is_integral_v<T>, "need [unsigned] integer");

        if constexpr (std::is_signed_v<T>)
            assert(i >= 0);

        assert(type == value_type::integer_32);
        assert(i < static_cast<T>(length));

        return integer_32[i];
    }

    template<typename T>
    constexpr i64 to_integer_64(T i) const
    {
        static_assert(std::is_integral_v<T>, "need [unsigned] integer");

        if constexpr (std::is_signed_v<T>)
            assert(i >= 0);

        assert(type == value_type::integer_64);
        assert(i < static_cast<T>(length));

        return integer_64[i];
    }

    template<typename T>
    constexpr float to_real_32(T i) const
    {
        static_assert(std::is_integral_v<T>, "need [unsigned] integer");

        if constexpr (std::is_signed_v<T>)
            assert(i >= 0);

        assert(type == value_type::real_32);
        assert(i < static_cast<T>(length));

        return real_32[i];
    }

    template<typename T>
    constexpr double to_real_64(T i) const
    {
        static_assert(std::is_integral_v<T>, "need [unsigned] integer");

        if constexpr (std::is_signed_v<T>)
            assert(i >= 0);

        assert(type == value_type::real_64);
        assert(i < static_cast<T>(length));

        return real_64[i];
    }

    template<typename T>
    constexpr double cast_to_real_64(T i) const
    {
        if constexpr (std::is_signed_v<T>)
            assert(i >= 0);

        switch (type) {
        case value_type::integer_8:
            return static_cast<double>(to_integer_8(i));
        case value_type::integer_32:
            return static_cast<double>(to_integer_32(i));
        case value_type::integer_64:
            return static_cast<double>(to_integer_64(i));
        case value_type::real_32:
            return static_cast<double>(to_real_32(i));
        case value_type::real_64:
            return static_cast<double>(to_real_64(i));
        default:
            return 0.0;
        }

        return 0.0;
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
    static_assert(std::is_trivially_destructible_v<T>,
                  "block_allocator is only for POD object");

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
            std::free(blocks);
    }

    status init(std::size_t new_capacity) noexcept
    {
        if (new_capacity == 0)
            return status::block_allocator_bad_capacity;

        if (new_capacity != capacity) {
            if (blocks)
                std::free(blocks);
            blocks =
              static_cast<block*>(std::malloc(new_capacity * sizeof(block)));
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

    std::pair<bool, T*> try_alloc() noexcept
    {
        if (free_head == nullptr && max_size >= capacity)
            return { false, nullptr };

        return { true, alloc() };
    }

    T* alloc() noexcept
    {
        ++size;
        block* new_block = nullptr;

        if (free_head != nullptr) {
            new_block = free_head;
            free_head = free_head->next;
        } else {
            assert(max_size < capacity);
            new_block = reinterpret_cast<block*>(&blocks[max_size++]);
        }

        return reinterpret_cast<T*>(new_block);
    }

    void free(T* n) noexcept
    {
        assert(n);

        block* ptr = reinterpret_cast<block*>(n);

        ptr->next = free_head;
        free_head = ptr;

        --size;

        if (size == 0) {         // A special part: if it no longer exists
            max_size = 0;        // we reset the free list and the number
            free_head = nullptr; // of elements allocated.
        }
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
        assert(!empty());

        return node->value;
    }

    const_reference front() const noexcept
    {
        assert(!empty());

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
        assert(allocator);

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

template<typename T>
class vector
{
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

private:
    value_type* m_items{ nullptr };
    unsigned m_capacity = 0u;
    unsigned m_size = 0u;

public:
    vector() = default;

    ~vector() noexcept
    {
        clear();

        if (m_items)
            std::free(m_items);
    }

    vector(const vector&) noexcept = delete;
    vector& operator=(const vector&) noexcept = delete;
    vector(vector&&) noexcept = delete;
    vector& operator=(vector&&) noexcept = delete;

    status init(std::size_t new_capacity_) noexcept
    {
        if (new_capacity_ > std::numeric_limits<unsigned>::max())
            return status::vector_init_capacity_too_big;

        if (new_capacity_ == 0)
            return status::vector_init_capacity_zero;

        clear();

        const auto new_capacity = static_cast<unsigned int>(new_capacity_);

        if (new_capacity > m_capacity) {
            if (m_items)
                std::free(m_items);

            m_items = static_cast<value_type*>(
              std::malloc(new_capacity * sizeof(value_type)));

            if (m_items == nullptr)
                return status::vector_init_not_enough_memory;

            m_capacity = new_capacity;
        }

        return status::success;
    }

    status resize(std::size_t new_capacity_) noexcept
    {
        static_assert(std::is_trivial_v<T>,
                      "vector::resize only for trivial data");

        irt_return_if_bad(init(new_capacity_));

        m_size = m_capacity;

        return status::success;
    }

    void clear() noexcept
    {
        if constexpr (!std::is_trivial_v<T>)
            for (auto i = 0u; i != m_size; ++i)
                m_items[i].~T();

        m_size = 0;
    }

    bool can_alloc(size_type number) const noexcept
    {
        return number + m_size <= m_capacity;
    }

    template<typename... Args>
    std::pair<bool, iterator> try_emplace_back(Args&&... args) noexcept
    {
        if (!can_alloc(1u))
            return std::make_pair(false, end());

        unsigned ret = m_size++;
        new (&m_items[ret]) T(std::forward<Args>(args)...);

        return std::pair(true, begin() + ret);
    }

    template<typename... Args>
    iterator emplace_back(Args&&... args) noexcept
    {
        assert(m_size < m_capacity);

        unsigned ret = m_size++;
        new (&m_items[ret]) T(std::forward<Args>(args)...);

        return begin() + ret;
    }

    void pop_back() noexcept
    {
        if (m_size == 0)
            return;

        --m_size;

        if constexpr (!std::is_trivial_v<T>)
            m_items[m_size].~T();
    }

    iterator erase_and_swap(const_iterator it) noexcept
    {
        auto diff = std::distance(cbegin(), it);
        assert(diff < std::numeric_limits<unsigned>::max());

        auto i = static_cast<unsigned>(diff);
        assert(i < m_size);

        if (m_size - 1 == i) {
            pop_back();
            return end();
        } else {
            using std::swap;
            swap(m_items[m_size - 1], m_items[i]);
            pop_back();
            return begin() + i;
        }
    }

    reference operator[](size_type i) noexcept
    {
        assert(i < m_size);
        return m_items[i];
    }

    const_reference operator[](size_type i) const noexcept
    {
        assert(i < m_size);
        return m_items[i];
    }

    pointer data() noexcept
    {
        return m_items;
    }

    const_pointer data() const noexcept
    {
        return m_items;
    }

    iterator begin() noexcept
    {
        return m_items;
    }

    iterator end() noexcept
    {
        return m_items + m_size;
    }

    const_iterator begin() const noexcept
    {
        return m_items;
    }

    const_iterator end() const noexcept
    {
        return m_items + m_size;
    }

    const_iterator cbegin() const noexcept
    {
        return m_items;
    }

    const_iterator cend() const noexcept
    {
        return m_items + m_size;
    }

    size_t size() const noexcept
    {
        return m_size;
    }

    size_type capacity() const noexcept
    {
        return m_capacity;
    }

    reference front() noexcept
    {
        assert(m_size > 0);
        return m_items[0];
    }

    const_reference front() const noexcept
    {
        assert(m_size > 0);
        return m_items[0];
    }

    reference back() noexcept
    {
        assert(m_size > 0);
        return m_items[m_size - 1];
    }

    const_reference back() const noexcept
    {
        assert(m_size > 0);
        return m_items[m_size - 1];
    }
};

template<typename T>
class array
{
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

private:
    value_type* m_items{ nullptr };
    unsigned m_capacity{ 0 };

public:
    array() = default;

    ~array() noexcept
    {
        clear();

        if (m_items)
            std::free(m_items);
    }

    array(const array&) noexcept = delete;
    array& operator=(const array&) noexcept = delete;
    array(array&&) noexcept = delete;
    array& operator=(array&&) noexcept = delete;

    status init(std::size_t new_capacity) noexcept
    {
        if (new_capacity > std::numeric_limits<unsigned>::max())
            return status::array_init_capacity_too_big;

        if (new_capacity == 0)
            return status::array_init_capacity_zero;

        if (new_capacity != m_capacity) {
            if (m_items) {
                if constexpr (!std::is_trivial_v<T>)
                    for (auto i = 0u; i != m_capacity; ++i)
                        m_items[i].~T();

                std::free(m_items);
            }

            m_items = static_cast<value_type*>(
              std::malloc(new_capacity * sizeof(value_type)));

            if constexpr (!std::is_trivial_v<T>)
                for (auto i = 0u; i != m_capacity; ++i)
                    new (&m_items[i]) T();

            if (m_items == nullptr)
                return status::array_init_not_enough_memory;
        }

        m_capacity = static_cast<unsigned>(new_capacity);

        return status::success;
    }

    void clear() noexcept
    {
        if constexpr (!std::is_trivial_v<T>)
            for (auto i = 0u; i != m_capacity; ++i)
                m_items[i].~T();
    }

    reference operator[](size_type i) noexcept
    {
        assert(i < m_capacity);
        return m_items[i];
    }

    const_reference operator[](size_type i) const noexcept
    {
        assert(i < m_capacity);
        return m_items[i];
    }

    pointer data() noexcept
    {
        return m_items;
    }

    const_pointer data() const noexcept
    {
        return m_items;
    }

    iterator begin() noexcept
    {
        return m_items;
    }

    iterator end() noexcept
    {
        return m_items + m_capacity;
    }

    const_iterator begin() const noexcept
    {
        return m_items;
    }

    const_iterator end() const noexcept
    {
        return m_items + m_capacity;
    }

    const_iterator cbegin() const noexcept
    {
        return m_items;
    }

    const_iterator cend() const noexcept
    {
        return m_items + m_capacity;
    }

    size_t size() const noexcept
    {
        return m_capacity;
    }

    size_t capacity() const noexcept
    {
        return m_capacity;
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
enum class input_port_id : std::uint64_t;
enum class output_port_id : std::uint64_t;
enum class init_port_id : std::uint64_t;
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
            std::free(m_items);
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

        m_items = static_cast<item*>(std::malloc(capacity * sizeof(item)));
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

        assert(&m_items[index] == static_cast<void*>(&t));
        assert(m_items[index].id == id);
        assert(is_valid(id));

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

        assert(m_items[index].id == id);
        assert(is_valid(id));

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
        assert(t != nullptr);

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

template<typename T, typename Identifier>
class data_array_archive
{
private:
    flat_double_list<record>::allocator_type shared_allocator;
    data_array<T, Identifier> data;

public:
    using identifier_type = Identifier;
    using value_type = T;

    data_array_archive() = default;

    ~data_array_archive() noexcept = default;

    status init(std::size_t capacity, std::size_t archive_capacity)
    {
        irt_return_if_bad(data.init(capacity));
        irt_return_if_bad(shared_allocator.init(archive_capacity));

        return status::success;
    }

    void clear() noexcept
    {
        data.clear();
    }

    template<typename... Args>
    T& alloc(Args&&... args) noexcept
    {
        assert(can_alloc(1) && "check alloc() with full() before using use.");

        auto& ret = data.alloc(std::forward<Args>(args)...);
        ret.archive.set_allocator(&shared_allocator);

        return ret;
    }

    template<typename... Args>
    std::pair<bool, T*> try_alloc(Args&&... args) noexcept
    {
        auto ret = data.try_alloc(std::forward<Args>(args)...);
        if (ret.first)
            ret.second->set_allocator(&shared_allocator);

        return ret;
    }

    void free(T& t) noexcept
    {
        data.free(t);
    }

    /**
     * @brief Free the element pointer by @c id.
     *
     * Internally, puts the elelent @c t entry on free list and use id to store
     * next.
     */
    void free(Identifier id) noexcept
    {
        data.free(id);
    }

    /**
     * @brief Accessor to the id part of the item
     *
     * @return @c Identifier.
     */
    Identifier get_id(const T& t) const noexcept
    {
        return data.get_id(t);
    }

    /**
     * @brief Accessor to the item part of the id.
     *
     * @return @c T
     */
    T& get(Identifier id) noexcept
    {
        return data.get(id);
    }

    /**
     * @brief Accessor to the item part of the id.
     *
     * @return @c T
     */
    const T& get(Identifier id) const noexcept
    {
        return data.get(id);
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
        return data.try_to_get(id);
    }

    constexpr bool can_alloc(std::size_t place) const noexcept
    {
        return data.can_alloc(place);
    }
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

public:
    using handle = node*;

    heap() = default;

    ~heap() noexcept
    {
        if (nodes)
            std::free(nodes);
    }

    status init(size_t new_capacity) noexcept
    {
        if (new_capacity == 0)
            return status::head_allocator_bad_capacity;

        if (new_capacity != capacity) {
            if (nodes)
                std::free(nodes);
            nodes =
              static_cast<node*>(std::malloc(new_capacity * sizeof(node)));
            if (nodes == nullptr)
                return status::head_allocator_not_enough_memory;
        }

        m_size = 0;
        max_size = 0;
        capacity = new_capacity;
        root = nullptr;

        return status::success;
    }

    void clear()
    {
        m_size = 0;
        max_size = 0;
        root = nullptr;
    }

    handle insert(time tn, model_id id) noexcept
    {
        node* new_node = &nodes[max_size++];
        new_node->tn = tn;
        new_node->id = id;
        new_node->prev = nullptr;
        new_node->next = nullptr;
        new_node->child = nullptr;

        ++m_size;

        if (root == nullptr)
            root = new_node;
        else
            root = merge(new_node, root);

        return new_node;
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
        if (elem == root) {
            pop();
            return;
        }

        if (m_size > 0) {
            m_size--;
            detach_subheap(elem);
            elem = merge_subheaps(elem);
            root = merge(root, elem);
        } else {
            root = nullptr;
        }

        // assert(m_size > 0);

        // m_size--;
        // detach_subheap(elem);
        // elem = merge_subheaps(elem);
        // root = merge(root, elem);
    }

    void pop() noexcept
    {
        assert(m_size > 0);

        m_size--;

        auto* top = root;

        if (top->child == nullptr)
            root = nullptr;
        else
            root = merge_subheaps(top);
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

/*****************************************************************************
 *
 * DEVS Model / Simulation entities
 *
 ****************************************************************************/

enum class dynamics_type : i8
{
    none,

    qss1_integrator,

    qss2_integrator,
    qss2_multiplier,
    qss2_sum_2,
    qss2_sum_3,
    qss2_sum_4,
    qss2_wsum_2,
    qss2_wsum_3,
    qss2_wsum_4,

    integrator,
    quantifier,
    adder_2,
    adder_3,
    adder_4,
    mult_2,
    mult_3,
    mult_4,
    counter,
    generator,
    constant,
    cross,
    time_func,
    accumulator_2,
    flow
};

struct model
{
    double tl{ 0 };
    double tn{ time_domain<time>::infinity };
    heap::handle handle{ nullptr };

    dynamics_id id{ 0 };
    observer_id obs_id{ 0 };

    dynamics_type type{ dynamics_type::none };

    small_string<7> name;
};

struct observer
{
    observer() noexcept = default;

    observer(const time time_step_,
             const char* name_,
             void* user_data_) noexcept
      : time_step(std::clamp(time_step_, 0.0, time_domain<time>::infinity))
      , name(name_)
      , user_data(user_data_)
    {}

    observer(const time time_step_,
             const char* name_,
             void* user_data_,
             void (*initialize_)(const observer& obs, const time t) noexcept,
             void (*observe_)(const observer& obs,
                              const time t,
                              const message& msg) noexcept,
             void (*free_)(const observer& obs, const time t) noexcept)
      : time_step(std::clamp(time_step_, 0.0, time_domain<time>::infinity))
      , name(name_)
      , user_data(user_data_)
      , initialize(initialize_)
      , observe(observe_)
      , free(free_)
    {}

    double tl = 0.0;
    double time_step = 0.0;
    small_string<8> name;
    model_id model = static_cast<model_id>(0);

    void* user_data = nullptr;

    void (*initialize)(const observer& obs, const time t) noexcept = nullptr;

    void (*observe)(const observer& obs,
                    const time t,
                    const message& msg) noexcept = nullptr;

    void (*free)(const observer& obs, const time t) noexcept = nullptr;
};

struct init_message
{
    message_id msg;
    flat_list<std::pair<model_id, int>> models;
    small_string<7> name;
};

struct input_port
{
    model_id model;
    flat_list<output_port_id> connections;
    flat_list<message> messages;
    small_string<8> name;
};

struct output_port
{
    model_id model;
    flat_list<input_port_id> connections;
    flat_list<message> messages;
    small_string<8> name;
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
using lambda_function_t = decltype(
  detail::helper<status (T::*)(data_array<output_port, output_port_id>&),
                 &T::lambda>{});

template<class T>
using transition_function_t = decltype(
  detail::helper<
    status (T::*)(data_array<input_port, input_port_id>&, time, time, time),
    &T::transition>{});

template<class T>
using observation_function_t =
  decltype(detail::helper<message (T::*)(time) const, &T::observation>{});

template<class T>
using initialize_function_t =
  decltype(detail::helper<status (T::*)(data_array<message, message_id>&),
                          &T::initialize>{});

template<typename T>
using has_input_port_t = decltype(&T::x);

template<typename T>
using has_output_port_t = decltype(&T::y);

template<typename T>
using has_init_port_t = decltype(&T::init);

struct simulation;

struct none
{
    model_id id;
    time sigma = time_domain<time>::infinity;
};

struct integrator
{
    model_id id;
    input_port_id x[3];
    output_port_id y[1];
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

    status initialize(data_array<message, message_id>& /*init*/) noexcept
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

    status external(input_port& port_quanta,
                    input_port& port_x_dot,
                    input_port& port_reset,
                    time t) noexcept
    {
        for (const auto& msg : port_quanta.messages) {
            irt_return_if_fail(msg.type == value_type::real_64 &&
                                 msg.size() == 2,
                               status::model_integrator_bad_external_message);

            up_threshold = msg.to_real_64(0);
            down_threshold = msg.to_real_64(1);

            if (st == state::wait_for_quanta)
                st = state::running;

            if (st == state::wait_for_both)
                st = state::wait_for_x_dot;
        }

        for (const auto& msg : port_x_dot.messages) {
            irt_return_if_fail(msg.type == value_type::real_64 &&
                                 msg.size() == 1,
                               status::model_integrator_bad_external_message);

            archive.emplace_back(msg.to_real_64(0), t);

            if (st == state::wait_for_x_dot)
                st = state::running;

            if (st == state::wait_for_both)
                st = state::wait_for_quanta;
        }

        for (const auto& msg : port_reset.messages) {
            irt_return_if_fail(msg.type == value_type::real_64 &&
                                 msg.size() == 1,
                               status::model_integrator_bad_external_message);

            reset_value = msg.to_real_64(0);
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

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time t,
                      time /*e*/,
                      time r) noexcept
    {
        auto& port_1 = input_ports.get(x[port_quanta]);
        auto& port_2 = input_ports.get(x[port_x_dot]);
        auto& port_3 = input_ports.get(x[port_reset]);

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

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        auto& port = output_ports.get(y[0]);
        switch (st) {
        case state::running:
            port.messages.emplace_front(expected_value);
            return status::success;
        case state::init:
            port.messages.emplace_front(current_value);
            return status::success;
        default:
            return status::model_integrator_output_error;
        }

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(last_output_value);
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

        return (up_threshold < val ) ? reset_value : val;
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

struct qss1_integrator
{
    model_id id;
    input_port_id x[2];
    output_port_id y[1];
    double default_X = 0.;
    double default_dQ = 0.01;
    double X;
    double dQ;
    double q;
    double u;
    time sigma = time_domain<time>::zero;

    enum port_name
    {
        port_x_dot,
        port_reset
    };

    qss1_integrator() = default;

    qss1_integrator(const qss1_integrator& other) noexcept
      : default_X(other.default_X)
      , default_dQ(other.default_dQ)
      , X(other.X)
      , dQ(other.dQ)
      , q(other.q)
      , u(other.u)
      , sigma(other.sigma)
    {}

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        irt_return_if_fail(std::isfinite(default_X),
                           status::model_integrator_X_error);

        irt_return_if_fail(std::isfinite(default_dQ) && default_dQ > 0.,
                           status::model_integrator_X_error);

        X = default_X;
        dQ = default_dQ;
        q = std::floor(X / dQ) * dQ;
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
                sigma = (q + dQ - X) / u;
            else
                sigma = (q - dQ - X) / u;
        }

        return status::success;
    }

    status internal() noexcept
    {
        if (u == 0.) {
            sigma = time_domain<time>::infinity;
        } else {
            X += sigma * u;
            sigma = dQ / std::abs(u);
        }

        q = X;

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time e,
                      time r) noexcept
    {
        auto& port_x = input_ports.get(x[port_x_dot]);
        auto& port_r = input_ports.get(x[port_reset]);
        double value_x = 0.;
        bool reset = false;

        for (const auto& msg : port_x.messages) {
            irt_return_if_fail(msg.type == value_type::real_64 &&
                                 msg.size() == 1,
                               status::model_integrator_bad_external_message);

            value_x = msg.to_real_64(0);
        }

        for (const auto& msg : port_r.messages) {
            irt_return_if_fail(msg.type == value_type::real_64 &&
                                 msg.size() == 1,
                               status::model_integrator_bad_external_message);

            X = msg.to_real_64(0);
            reset = true;
        }

        if (port_x.messages.empty() && !reset) {
            irt_return_if_bad(internal());
        } else {
            if (time_domain<time>::is_zero(r))
                irt_return_if_bad(internal());

            if (!reset)
                irt_return_if_bad(external(value_x, e));
        }

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        auto& port = output_ports.get(y[0]);

        port.messages.emplace_front(u == 0 ? q : q + dQ * u / std::abs(u));

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(X);
    }
};

/*****************************************************************************
 *
 * Qss2 part
 *
 ****************************************************************************/

struct qss2_integrator
{
    model_id id;
    input_port_id x[2];
    output_port_id y[1];
    double default_X = 0.;
    double default_dQ = 0.;
    double X;
    double dQ;
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

    qss2_integrator() = default;

    qss2_integrator(const qss2_integrator& other) noexcept
      : default_X(other.default_X)
      , default_dQ(other.default_dQ)
      , X(other.X)
      , dQ(other.dQ)
      , u(other.u)
      , mu(other.mu)
      , q(other.q)
      , mq(other.mq)
      , sigma(other.sigma)
    {}

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        irt_return_if_fail(std::isfinite(default_X),
                           status::model_integrator_X_error);

        irt_return_if_fail(std::isfinite(default_dQ) && default_dQ > 0.,
                           status::model_integrator_X_error);

        X = default_X;
        dQ = default_dQ;

        u = 0.;
        mu = 0.;
        q = default_X;
        mq = 0.;

        sigma = time_domain<time>::zero;

        return status::success;
    }

    status external(const double value_x,
                    const double value_slope,
                    const time e) noexcept
    {
        X += u * e + mu / 2.0 * e * e;
        u = value_x;
        mu = value_slope;

        if (sigma != 0) {
            q += mq * e;
            const double a = mu / 2;
            const double b = u - mq;
            double c = X - q + dQ;
            double s;
            sigma = time_domain<time>::infinity;

            if (a == 0) {
                if (b != 0) {
                    s = -c / b;
                    if (s > 0)
                        sigma = s;

                    c = X - q - dQ;
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

                c = X - q - dQ;
                s = (-b + std::sqrt(b * b - 4. * a * c)) / 2. / a;
                if ((s > 0.) && (s < sigma))
                    sigma = s;

                s = (-b - std::sqrt(b * b - 4. * a * c)) / 2. / a;
                if ((s > 0.) && (s < sigma))
                    sigma = s;
            }

            if (((X - q) > dQ) || ((q - X) > dQ))
                sigma = 0;
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
                         : std::sqrt(2. * dQ / std::abs(mu));

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time e,
                      time r) noexcept
    {
        auto& port_x = input_ports.get(x[port_x_dot]);
        auto& port_r = input_ports.get(x[port_reset]);
        double value_x = 0.;
        double value_slope = 0.;
        bool reset = false;

        for (const auto& msg : port_x.messages) {
            irt_return_if_fail(msg.type == value_type::real_64 &&
                                 msg.size() == 2,
                               status::model_integrator_bad_external_message);

            value_x = msg.to_real_64(0);
            value_slope = msg.to_real_64(1);
        }

        for (const auto& msg : port_r.messages) {
            irt_return_if_fail(msg.type == value_type::real_64 &&
                                 msg.size() == 1,
                               status::model_integrator_bad_external_message);

            X = msg.to_real_64(0);
            reset = true;
        }

        if (port_x.messages.empty() && !reset) {
            irt_return_if_bad(internal());
        } else {
            if (time_domain<time>::is_zero(r))
                irt_return_if_bad(internal());

            if (!reset)
                irt_return_if_bad(external(value_x, value_slope, e));
        }

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        auto& port = output_ports.get(y[0]);

        port.messages.emplace_front(X + u * sigma + mu * sigma * sigma / 2.,
                                    u + mu * sigma);

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(X);
    }
};

template<size_t PortNumber>
struct qss2_sum
{
    static_assert(PortNumber > 1,
                  "qss2_sum model need at least two input port");

    model_id id;
    input_port_id x[PortNumber];
    output_port_id y[1];
    time sigma;

    double values[PortNumber];
    double slopes[PortNumber];

    qss2_sum() noexcept = default;

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        std::fill_n(values, PortNumber, 0.);
        std::fill_n(slopes, PortNumber, 0.);

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        double value = 0.;
        double slope = 0.;

        for (size_t i = 0; i != PortNumber; ++i) {
            value += values[i];
            slope += slopes[i];
        }

        output_ports.get(y[0]).messages.emplace_front(value, slope);

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time e,
                      time /*r*/) noexcept
    {
        bool message = false;

        for (size_t i = 0; i != PortNumber; ++i) {
            auto& i_port = input_ports.get(x[i]);

            if (i_port.messages.empty()) {
                values[i] += slopes[i] * e;
            } else {
                for (const auto& msg : input_ports.get(x[i]).messages) {
                    values[i] = msg.to_real_64(0);
                    slopes[i] = msg.size() > 1 ? msg.to_real_64(1) : 0.0;
                    message = true;
                }
            }
        }

        sigma = message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        double value = 0.;
        double slope = 0.;

        for (size_t i = 0; i != PortNumber; ++i) {
            value += values[i];
            slope += slopes[i];
        }

        return message{ value, slope };
    }
};

struct qss2_multiplier
{
    model_id id;
    input_port_id x[2];
    output_port_id y[1];
    time sigma;

    double values[2];
    double slopes[2];

    qss2_multiplier() noexcept = default;

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        values[0] = values[1] = slopes[0] = slopes[1] = 0.;

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        output_ports.get(y[0]).messages.emplace_front(
          values[0] * values[1], slopes[0] * values[1] + slopes[1] * values[0]);

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time e,
                      time /*r*/) noexcept
    {
        bool message_port_0 = false;
        bool message_port_1 = false;
        sigma = time_domain<time>::infinity;

        for (const auto& msg : input_ports.get(x[0]).messages) {
            values[0] = msg.to_real_64(0);
            slopes[0] = msg.size() > 1 ? msg.to_real_64(1) : 0.0;
            message_port_0 = true;
            sigma = time_domain<time>::zero;
        }

        for (const auto& msg : input_ports.get(x[1]).messages) {
            values[1] = msg.to_real_64(0);
            slopes[1] = msg.size() > 1 ? msg.to_real_64(1) : 0.0;
            message_port_1 = true;
            sigma = time_domain<time>::zero;
        }

        if (!message_port_0)
            values[0] += e * slopes[0];

        if (!message_port_1)
            values[1] += e * slopes[1];

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return { values[0] * values[1],
                 slopes[0] * values[1] + slopes[1] * values[0] };
    }
};

template<size_t PortNumber>
struct qss2_wsum
{
    static_assert(PortNumber > 1,
                  "qss2_wsum model need at least two input port");

    model_id id;
    input_port_id x[PortNumber];
    output_port_id y[1];
    time sigma;

    double default_input_coeffs[PortNumber] = { 0 };
    double values[PortNumber];
    double slopes[PortNumber];

    qss2_wsum() noexcept = default;

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        std::fill_n(values, PortNumber, 0.);
        std::fill_n(slopes, PortNumber, 0.);

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        double value = 0.;
        double slope = 0.;

        for (size_t i = 0; i != PortNumber; ++i) {
            value += default_input_coeffs[i] * values[i];
            slope += default_input_coeffs[i] * slopes[i];
        }

        output_ports.get(y[0]).messages.emplace_front(value, slope);

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time e,
                      time /*r*/) noexcept
    {
        bool message = false;

        for (size_t i = 0; i != PortNumber; ++i) {
            auto& i_port = input_ports.get(x[i]);

            if (i_port.messages.empty()) {
                values[i] += slopes[i] * e;
            } else {
                for (const auto& msg : input_ports.get(x[i]).messages) {
                    values[i] = msg.to_real_64(0);
                    slopes[i] = msg.size() > 1 ? msg.to_real_64(1) : 0.0;
                    message = true;
                }
            }
        }

        sigma = message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        double value = 0.;
        double slope = 0.;

        for (size_t i = 0; i != PortNumber; ++i) {
            value += default_input_coeffs[i] * values[i];
            slope += default_input_coeffs[i] * slopes[i];
        }

        return message{ value, slope };
    }
};

struct quantifier
{
    model_id id;
    input_port_id x[1];
    output_port_id y[1];
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

    status initialize(data_array<message, message_id>& /*init*/) noexcept
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

    status external(input_port& port, time t) noexcept
    {
        double val = 0.0, shifting_factor = 0.0;

        {
            double sum = 0.0;
            double nb = 0.0;
            for (const auto& msg : port.messages) {
                irt_return_if_fail(
                  msg.type == value_type::real_64 && msg.size() == 1,
                  status::model_quantifier_bad_external_message);
                sum += msg.to_real_64(0);
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

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time t,
                      time /*e*/,
                      time r) noexcept
    {
        auto& port = input_ports.get(x[0]);

        if (port.messages.empty()) {
            irt_return_if_bad(internal());
        } else {
            if (time_domain<time>::is_zero(r))
                irt_return_if_bad(internal());

            irt_return_if_bad(external(port, t));
        }

        return ta();
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        if (auto* port = output_ports.try_to_get(y[0]); port)
            port->messages.emplace_front(m_upthreshold, m_downthreshold);

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(m_upthreshold, m_downthreshold);
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

    model_id id;
    input_port_id x[PortNumber];
    output_port_id y[1];
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

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        std::copy_n(std::begin(default_values), PortNumber, std::begin(values));

        std::copy_n(std::begin(default_input_coeffs),
                    PortNumber,
                    std::begin(input_coeffs));

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        double to_send = 0.0;

        for (size_t i = 0; i != PortNumber; ++i)
            to_send += input_coeffs[i] * values[i];

        output_ports.get(y[0]).messages.emplace_front(to_send);

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        bool have_message = false;

        for (size_t i = 0; i != PortNumber; ++i) {
            for (const auto& msg : input_ports.get(x[i]).messages) {
                irt_return_if_fail(msg.type == value_type::real_64,
                                   status::model_adder_bad_external_message);
                irt_return_if_fail(msg.size() == 1,
                                   status::model_adder_bad_external_message);

                values[i] = msg.to_real_64(0);

                have_message = true;
            }
        }

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        double ret = 0.0;

        for (size_t i = 0; i != PortNumber; ++i)
            ret += input_coeffs[i] * values[i];

        return message(ret);
    }
};

template<size_t PortNumber>
struct mult
{
    static_assert(PortNumber > 1, "mult model need at least two input port");

    model_id id;
    input_port_id x[PortNumber];
    output_port_id y[1];
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

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        std::copy_n(std::begin(default_values), PortNumber, std::begin(values));

        std::copy_n(std::begin(default_input_coeffs),
                    PortNumber,
                    std::begin(input_coeffs));

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        double to_send = 1.0;

        for (size_t i = 0; i != PortNumber; ++i)
            to_send *= std::pow(values[i], input_coeffs[i]);

        output_ports.get(y[0]).messages.emplace_front(to_send);

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        bool have_message = false;
        for (size_t i = 0; i != PortNumber; ++i) {
            for (const auto& msg : input_ports.get(x[i]).messages) {
                irt_return_if_fail(msg.type == value_type::real_64,
                                   status::model_mult_bad_external_message);
                irt_return_if_fail(msg.size() == 1,
                                   status::model_mult_bad_external_message);

                values[i] = msg.to_real_64(0);
                have_message = true;
            }
        }

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        double ret = 1.0;

        for (size_t i = 0; i != PortNumber; ++i)
            ret *= std::pow(values[i], input_coeffs[i]);

        return message(ret);
    }
};

struct counter
{
    model_id id;
    input_port_id x[1];
    time sigma;
    i64 number;

    status initialize(
      data_array<message, message_id>& /*init_messages*/) noexcept
    {
        number = { 0 };
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        auto& port = input_ports.get(x[0]);

        const auto diff =
          std::distance(std::begin(port.messages), std::end(port.messages));

        number += static_cast<i64>(diff);

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(number);
    }
};

struct generator
{
    model_id id;
    output_port_id y[1];
    time sigma;
    double default_value = 0.0;
    double default_period = 1.0;
    double default_offset = 1.0;
    double value = 0.0;
    double period = 1.0;
    double offset = 1.0;

    status initialize(
      data_array<message, message_id>& /*init_messages*/) noexcept
    {
        value = default_value;
        period = default_period;
        offset = default_offset;

        sigma = offset;

        return status::success;
    }
    status transition(data_array<input_port, input_port_id>& /*input_ports*/,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        sigma = period;
        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        output_ports.get(y[0]).messages.emplace_front(value);

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(value);
    }
};

struct constant
{
    model_id id;
    output_port_id y[1];
    time sigma;

    double default_value = 0.0;

    double value = 0.0;

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        sigma = time_domain<time>::zero;

        value = default_value;

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& /*input_ports*/,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        output_ports.get(y[0]).messages.emplace_front(value);

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(value);
    }
};

struct flow
{
    model_id id;
    output_port_id y[1];
    time sigma;

    double default_value = 0.0;
    std::vector<double> default_data;
    double default_samplerate = 44100.0;

    double value = 0.0;
    std::vector<double> data;
    double samplerate = 44100.0;

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {

        sigma = 1.0/samplerate;

        value = default_value;
        data = default_data;
        samplerate = default_samplerate;

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& /*input_ports*/,
                      time t,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        value = data[static_cast<int>(t*samplerate)];

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {     
        output_ports.get(y[0]).messages.emplace_front(value);

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(value);
    }
};

template<size_t PortNumber>
struct accumulator
{
    model_id id;
    input_port_id x[2 * PortNumber];
    time sigma;
    double number;
    double numbers[PortNumber];

    status initialize(
      data_array<message, message_id>& /*init_messages*/) noexcept
    {
        number = 0.0;
        std::fill_n(numbers, PortNumber, 0.0);

        sigma = time_domain<time>::infinity;

        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {

        for (size_t i = 0; i != PortNumber; ++i) {
            auto& port = input_ports.get(x[i + PortNumber]);
            for (const auto& msg : port.messages) {
                irt_return_if_fail(
                  msg.type == value_type::real_64,
                  status::model_accumulator_bad_external_message);
                irt_return_if_fail(
                  msg.size() == 1,
                  status::model_accumulator_bad_external_message);

                numbers[i] = msg.to_real_64(0);
            }
        }

        for (size_t i = 0; i != PortNumber; ++i) {
            auto& port = input_ports.get(x[i]);
            for (const auto& msg : port.messages) {
                irt_return_if_fail(
                  msg.type == value_type::real_64,
                  status::model_accumulator_bad_external_message);
                irt_return_if_fail(
                  msg.size() == 1,
                  status::model_accumulator_bad_external_message);

                if (msg.to_real_64(0) != 0.0)
                    number += numbers[i];
            }
        }

        return status::success;
    }
};

struct cross
{
    model_id id;
    input_port_id x[3];
    output_port_id y[2];
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
        port_else_value
    };

    status initialize(data_array<message, message_id>& /*init*/) noexcept
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

    status transition(data_array<input_port, input_port_id>& input_ports,
                      time /*t*/,
                      time /*e*/,
                      time /*r*/) noexcept
    {
        bool have_message = false;
        bool have_message_value = false;

        for (const auto& msg : input_ports.get(x[port_value]).messages) {
            irt_return_if_fail(msg.type == value_type::real_64,
                               status::model_cross_bad_external_message);
            irt_return_if_fail(msg.size() == 1,
                               status::model_cross_bad_external_message);

            value = msg.to_real_64(0);
            have_message_value = true;
            have_message = true;
        }

        for (const auto& msg : input_ports.get(x[port_if_value]).messages) {
            irt_return_if_fail(msg.type == value_type::real_64,
                               status::model_cross_bad_external_message);
            irt_return_if_fail(msg.size() == 1,
                               status::model_cross_bad_external_message);

            if_value = msg.to_real_64(0);
            have_message = true;
        }

        for (const auto& msg : input_ports.get(x[port_else_value]).messages) {
            irt_return_if_fail(msg.type == value_type::real_64,
                               status::model_cross_bad_external_message);
            irt_return_if_fail(msg.size() == 1,
                               status::model_cross_bad_external_message);

            else_value = msg.to_real_64(0);
            have_message = true;
        }

        if (have_message_value) {
            if (value >= threshold) {
                else_value = if_value;
                event = 1.0;
            } else {
                event = 0.0;
            }
        }

        result = else_value;

        sigma =
          have_message ? time_domain<time>::zero : time_domain<time>::infinity;

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        output_ports.get(y[0]).messages.emplace_front(result);
        output_ports.get(y[1]).messages.emplace_front(event);

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(value, if_value, else_value);
    }
};

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
    model_id id;
    output_port_id y[1];
    time sigma;

    double (*default_f)(double) = &time_function;

    double value;
    double (*f)(double) = nullptr;

    status initialize(data_array<message, message_id>& /*init*/) noexcept
    {
        f = default_f;
        sigma = 1.0;
        value = sigma;
        return status::success;
    }

    status transition(data_array<input_port, input_port_id>& /*input_ports*/,
                      time t,
                      time /*e*/,
                      time /*r*/) noexcept
    {

        sigma = (*f)(t);
        value = sigma;

        return status::success;
    }

    status lambda(
      data_array<output_port, output_port_id>& output_ports) noexcept
    {
        output_ports.get(y[0]).messages.emplace_front(value);

        return status::success;
    }

    message observation(time /*t*/) const noexcept
    {
        return message(value);
    }
};

using adder_2 = adder<2>;
using adder_3 = adder<3>;
using adder_4 = adder<4>;

using mult_2 = mult<2>;
using mult_3 = mult<3>;
using mult_4 = mult<4>;

using qss2_sum_2 = qss2_sum<2>;
using qss2_sum_3 = qss2_sum<3>;
using qss2_sum_4 = qss2_sum<4>;
using qss2_wsum_2 = qss2_wsum<2>;
using qss2_wsum_3 = qss2_wsum<3>;
using qss2_wsum_4 = qss2_wsum<4>;

using accumulator_2 = accumulator<2>;

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
        assert(mdl.handle == nullptr);

        mdl.handle = m_heap.insert(tn, id);
    }

    /**
     * @brief Reintegrate or reinsert an old popped model into the scheduller.
     */
    void reintegrate(model& mdl, time tn) noexcept
    {
        assert(mdl.handle != nullptr);

        mdl.handle->tn = tn;

        m_heap.insert(mdl.handle);
    }

    void erase(model& mdl) noexcept
    {
        if (mdl.handle) {
            m_heap.remove(mdl.handle);
            mdl.handle = nullptr;
        }
    }

    void update(model& mdl, time tn) noexcept
    {
        assert(mdl.handle != nullptr);

        mdl.handle->tn = tn;

        if (tn < mdl.tn)
            m_heap.decrease(mdl.handle);
        else if (tn > mdl.tn)
            m_heap.increase(mdl.handle);
    }

    const list& pop() noexcept
    {
        time t = tn();

        m_list.clear();

        m_list.emplace_front(m_heap.top()->id);
        m_heap.pop();

        auto nb = 1u;
        while (!m_heap.empty() && t == tn()) {
            m_list.emplace_front(m_heap.top()->id);
            m_heap.pop();
            nb++;
        }

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

struct simulation
{
    flat_list<model_id>::allocator_type model_list_allocator;
    flat_list<message>::allocator_type message_list_allocator;
    flat_list<input_port_id>::allocator_type input_port_list_allocator;
    flat_list<output_port_id>::allocator_type output_port_list_allocator;

    flat_list<output_port_id>::allocator_type emitting_output_port_allocator;

    data_array<model, model_id> models;

    data_array<init_message, message_id> init_messages;
    data_array<message, message_id> messages;
    data_array<input_port, input_port_id> input_ports;
    data_array<output_port, output_port_id> output_ports;

    data_array<none, dynamics_id> none_models;

    data_array<qss1_integrator, dynamics_id> qss1_integrator_models;

    data_array<qss2_integrator, dynamics_id> qss2_integrator_models;
    data_array<qss2_multiplier, dynamics_id> qss2_multiplier_models;
    data_array<qss2_sum_2, dynamics_id> qss2_sum_2_models;
    data_array<qss2_sum_3, dynamics_id> qss2_sum_3_models;
    data_array<qss2_sum_4, dynamics_id> qss2_sum_4_models;
    data_array<qss2_wsum_2, dynamics_id> qss2_wsum_2_models;
    data_array<qss2_wsum_3, dynamics_id> qss2_wsum_3_models;
    data_array<qss2_wsum_4, dynamics_id> qss2_wsum_4_models;

    data_array_archive<integrator, dynamics_id> integrator_models;
    data_array_archive<quantifier, dynamics_id> quantifier_models;
    data_array<adder_2, dynamics_id> adder_2_models;
    data_array<adder_3, dynamics_id> adder_3_models;
    data_array<adder_4, dynamics_id> adder_4_models;
    data_array<mult_2, dynamics_id> mult_2_models;
    data_array<mult_3, dynamics_id> mult_3_models;
    data_array<mult_4, dynamics_id> mult_4_models;
    data_array<counter, dynamics_id> counter_models;
    data_array<generator, dynamics_id> generator_models;
    data_array<constant, dynamics_id> constant_models;
    data_array<cross, dynamics_id> cross_models;
    data_array<time_func, dynamics_id> time_func_models;
    data_array<accumulator_2, dynamics_id> accumulator_2_models;
    data_array<flow, dynamics_id> flow_models;

    data_array<observer, observer_id> observers;

    scheduller sched;

    time begin = time_domain<time>::zero;
    time end = time_domain<time>::infinity;

    template<typename Function>
    status dispatch(const dynamics_type type, Function f) noexcept
    {
        switch (type) {
        case dynamics_type::none:
            return f(none_models);

        case dynamics_type::qss1_integrator:
            return f(qss1_integrator_models);

        case dynamics_type::qss2_integrator:
            return f(qss2_integrator_models);
        case dynamics_type::qss2_multiplier:
            return f(qss2_multiplier_models);
        case dynamics_type::qss2_sum_2:
            return f(qss2_sum_2_models);
        case dynamics_type::qss2_sum_3:
            return f(qss2_sum_3_models);
        case dynamics_type::qss2_sum_4:
            return f(qss2_sum_4_models);
        case dynamics_type::qss2_wsum_2:
            return f(qss2_wsum_2_models);
        case dynamics_type::qss2_wsum_3:
            return f(qss2_wsum_3_models);
        case dynamics_type::qss2_wsum_4:
            return f(qss2_wsum_4_models);

        case dynamics_type::integrator:
            return f(integrator_models);
        case dynamics_type::quantifier:
            return f(quantifier_models);
        case dynamics_type::adder_2:
            return f(adder_2_models);
        case dynamics_type::adder_3:
            return f(adder_3_models);
        case dynamics_type::adder_4:
            return f(adder_4_models);
        case dynamics_type::mult_2:
            return f(mult_2_models);
        case dynamics_type::mult_3:
            return f(mult_3_models);
        case dynamics_type::mult_4:
            return f(mult_4_models);
        case dynamics_type::counter:
            return f(counter_models);
        case dynamics_type::generator:
            return f(generator_models);
        case dynamics_type::constant:
            return f(constant_models);
        case dynamics_type::cross:
            return f(cross_models);
        case dynamics_type::accumulator_2:
            return f(accumulator_2_models);
        case dynamics_type::time_func:
            return f(time_func_models);
        case dynamics_type::flow:
            return f(flow_models);
        }

        irt_bad_return(status::unknown_dynamics);
    }

    template<typename Function>
    status dispatch(const dynamics_type type, Function f) const noexcept
    {
        switch (type) {
        case dynamics_type::none:
            return f(none_models);

        case dynamics_type::qss1_integrator:
            return f(qss1_integrator_models);

        case dynamics_type::qss2_integrator:
            return f(qss2_integrator_models);
        case dynamics_type::qss2_multiplier:
            return f(qss2_multiplier_models);
        case dynamics_type::qss2_sum_2:
            return f(qss2_sum_2_models);
        case dynamics_type::qss2_sum_3:
            return f(qss2_sum_3_models);
        case dynamics_type::qss2_sum_4:
            return f(qss2_sum_4_models);
        case dynamics_type::qss2_wsum_2:
            return f(qss2_wsum_2_models);
        case dynamics_type::qss2_wsum_3:
            return f(qss2_wsum_3_models);
        case dynamics_type::qss2_wsum_4:
            return f(qss2_wsum_4_models);

        case dynamics_type::integrator:
            return f(integrator_models);
        case dynamics_type::quantifier:
            return f(quantifier_models);
        case dynamics_type::adder_2:
            return f(adder_2_models);
        case dynamics_type::adder_3:
            return f(adder_3_models);
        case dynamics_type::adder_4:
            return f(adder_4_models);
        case dynamics_type::mult_2:
            return f(mult_2_models);
        case dynamics_type::mult_3:
            return f(mult_3_models);
        case dynamics_type::mult_4:
            return f(mult_4_models);
        case dynamics_type::counter:
            return f(counter_models);
        case dynamics_type::generator:
            return f(generator_models);
        case dynamics_type::constant:
            return f(constant_models);
        case dynamics_type::cross:
            return f(cross_models);
        case dynamics_type::accumulator_2:
            return f(accumulator_2_models);
        case dynamics_type::time_func:
            return f(time_func_models);
        case dynamics_type::flow:
            return f(flow_models);
        }

        irt_bad_return(status::unknown_dynamics);
    }

    status get_output_port_index(const model& mdl,
                                 const output_port_id port,
                                 int* index) const noexcept
    {
        return dispatch(
          mdl.type,
          [ dyn_id = mdl.id, port,
            index ]<typename DynamicsM>(DynamicsM & dyn_models)
            ->status {
                using Dynamics = typename DynamicsM::value_type;

                if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                    auto* dyn = dyn_models.try_to_get(dyn_id);
                    irt_return_if_fail(dyn, status::dynamics_unknown_id);

                    for (size_t i = 0, e = std::size(dyn->y); i != e; ++i) {
                        if (dyn->y[i] == port) {
                            *index = static_cast<int>(i);
                            return status::success;
                        }
                    }
                }

                return status::dynamics_unknown_port_id;
            });
    }

    template<typename Function>
    void for_all_input_port(const model& mdl, Function f)
    {
        dispatch(
          mdl.type,
          [ this, &f, dyn_id = mdl.id ]<typename DynamicsM>(DynamicsM &
                                                            dyn_models) {
              using Dynamics = typename DynamicsM::value_type;

              if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                  if (auto* dyn = dyn_models.try_to_get(dyn_id); dyn)
                      for (size_t i = 0, e = std::size(dyn->x); i != e; ++i)
                          if (auto* port = input_ports.try_to_get(dyn->x[i]);
                              port)
                              f(*port, dyn->x[i]);
              }
              return status::success;
          });
    }

    template<typename Function>
    void for_all_output_port(const model& mdl, Function f)
    {
        dispatch(
          mdl.type,
          [ this, &f, dyn_id = mdl.id ]<typename DynamicsM>(DynamicsM &
                                                            dyn_models) {
              using Dynamics = typename DynamicsM::value_type;

              if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                  if (auto* dyn = dyn_models.try_to_get(dyn_id); dyn)
                      for (size_t i = 0, e = std::size(dyn->y); i != e; ++i)
                          if (auto* port = output_ports.try_to_get(dyn->y[i]);
                              port)
                              f(*port, dyn->y[i]);
              }

              return status::success;
          });
    }

    status get_input_port_index(const model& mdl,
                                const input_port_id port,
                                int* index) const noexcept
    {
        return dispatch(
          mdl.type,
          [ dyn_id = mdl.id, port,
            index ]<typename DynamicsM>(DynamicsM & dyn_models)
            ->status {
                using Dynamics = typename DynamicsM::value_type;

                if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                    auto* dyn = dyn_models.try_to_get(dyn_id);
                    irt_return_if_fail(dyn, status::dynamics_unknown_id);

                    for (size_t i = 0, e = std::size(dyn->x); i != e; ++i) {
                        if (dyn->x[i] == port) {
                            *index = static_cast<int>(i);
                            return status::success;
                        }
                    }
                }

                return status::dynamics_unknown_port_id;
            });
    }

    status get_output_port_id(const model& mdl,
                              int index,
                              output_port_id* port) const noexcept
    {
        return dispatch(
          mdl.type,
          [ dyn_id = mdl.id, index,
            port ]<typename DynamicsM>(DynamicsM & dyn_models)
            ->status {
                using Dynamics = typename DynamicsM::value_type;

                if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                    auto* dyn = dyn_models.try_to_get(dyn_id);
                    irt_return_if_fail(dyn, status::dynamics_unknown_id);

                    irt_return_if_fail(0 <= index &&
                                         static_cast<size_t>(index) <
                                           std::size(dyn->y),
                                       status::dynamics_unknown_port_id);

                    *port = dyn->y[index];
                    return status::success;
                }

                return status::dynamics_unknown_port_id;
            });
    }

    status get_input_port_id(const model& mdl,
                             int index,
                             input_port_id* port) const noexcept
    {
        return dispatch(
          mdl.type,
          [ dyn_id = mdl.id, index,
            port ]<typename DynamicsM>(DynamicsM & dyn_models)
            ->status {
                using Dynamics = typename DynamicsM::value_type;

                if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
                    auto* dyn = dyn_models.try_to_get(dyn_id);
                    irt_return_if_fail(dyn, status::dynamics_unknown_id);

                    irt_return_if_fail(0 <= index &&
                                         static_cast<size_t>(index) <
                                           std::size(dyn->x),
                                       status::dynamics_unknown_port_id);

                    *port = dyn->x[index];
                    return status::success;
                }

                return status::dynamics_unknown_port_id;
            });
    }

public:
    status init(size_t model_capacity, size_t messages_capacity)
    {
        constexpr size_t ten{ 10 };

        irt_return_if_bad(model_list_allocator.init(model_capacity * ten));
        irt_return_if_bad(message_list_allocator.init(messages_capacity * ten));
        irt_return_if_bad(input_port_list_allocator.init(model_capacity * ten));
        irt_return_if_bad(output_port_list_allocator.init(model_capacity * ten));
        irt_return_if_bad(emitting_output_port_allocator.init(model_capacity));

        irt_return_if_bad(sched.init(model_capacity));

        irt_return_if_bad(models.init(model_capacity));
        irt_return_if_bad(init_messages.init(model_capacity ));
        irt_return_if_bad(messages.init(messages_capacity));
        irt_return_if_bad(input_ports.init(model_capacity ));
        irt_return_if_bad(output_ports.init(model_capacity ));

        irt_return_if_bad(none_models.init(model_capacity));

        irt_return_if_bad(qss1_integrator_models.init(model_capacity));

        irt_return_if_bad(qss2_integrator_models.init(model_capacity));
        irt_return_if_bad(qss2_multiplier_models.init(model_capacity));
        irt_return_if_bad(qss2_sum_2_models.init(model_capacity));
        irt_return_if_bad(qss2_sum_3_models.init(model_capacity));
        irt_return_if_bad(qss2_sum_4_models.init(model_capacity));
        irt_return_if_bad(qss2_wsum_2_models.init(model_capacity));
        irt_return_if_bad(qss2_wsum_3_models.init(model_capacity));
        irt_return_if_bad(qss2_wsum_4_models.init(model_capacity));

        irt_return_if_bad(integrator_models.init(
          model_capacity, model_capacity * ten  ));
        irt_return_if_bad(quantifier_models.init(
          model_capacity, model_capacity * ten  ));
        irt_return_if_bad(adder_2_models.init(model_capacity));
        irt_return_if_bad(adder_3_models.init(model_capacity));
        irt_return_if_bad(adder_4_models.init(model_capacity));
        irt_return_if_bad(mult_2_models.init(model_capacity));
        irt_return_if_bad(mult_3_models.init(model_capacity));
        irt_return_if_bad(mult_4_models.init(model_capacity));
        irt_return_if_bad(counter_models.init(model_capacity));
        irt_return_if_bad(generator_models.init(model_capacity));
        irt_return_if_bad(constant_models.init(model_capacity));
        irt_return_if_bad(cross_models.init(model_capacity));
        irt_return_if_bad(time_func_models.init(model_capacity));
        irt_return_if_bad(accumulator_2_models.init(model_capacity));
        irt_return_if_bad(flow_models.init(model_capacity));

        irt_return_if_bad(observers.init(model_capacity));

        return status::success;
    }

    bool can_alloc(size_t place) const noexcept
    {
        return models.can_alloc(place);
    }

    /**
     * @brief cleanup simulation object
     *
     * Clean scheduller and input/output port from message. This function
     * must be call at the end of the simulation.
     */
    void clean() noexcept
    {
        sched.clear();

        output_port* out = nullptr;
        while (output_ports.next(out))
            out->messages.clear();

        input_port* in = nullptr;
        while (input_ports.next(in))
            in->messages.clear();
    }

    /**
     * @brief cleanup simulation and destroy all models and connections
     */
    void clear() noexcept
    {
        clean();

        model_list_allocator.reset();
        message_list_allocator.reset();
        input_port_list_allocator.reset();
        output_port_list_allocator.reset();

        emitting_output_port_allocator.reset();

        models.clear();

        init_messages.clear();
        messages.clear();
        input_ports.clear();
        output_ports.clear();

        none_models.clear();

        qss1_integrator_models.clear();

        qss2_integrator_models.clear();
        qss2_multiplier_models.clear();
        qss2_sum_2_models.clear();
        qss2_sum_3_models.clear();
        qss2_sum_4_models.clear();
        qss2_wsum_2_models.clear();
        qss2_wsum_3_models.clear();
        qss2_wsum_4_models.clear();

        integrator_models.clear();
        quantifier_models.clear();
        adder_2_models.clear();
        adder_3_models.clear();
        adder_4_models.clear();
        mult_2_models.clear();
        mult_3_models.clear();
        mult_4_models.clear();
        counter_models.clear();
        generator_models.clear();
        constant_models.clear();
        cross_models.clear();
        time_func_models.clear();
        accumulator_2_models.clear();
        flow_models.clear();

        observers.clear();

        begin = time_domain<time>::zero;
        end = time_domain<time>::infinity;
    }

    template<typename Dynamics>
    status alloc(Dynamics& dynamics,
                 dynamics_id id,
                 const char* name = nullptr) noexcept
    {
        irt_return_if_fail(!models.full(), status::simulation_not_enough_model);

        model& mdl = models.alloc();
        model_id mdl_id = models.get_id(mdl);
        mdl.handle = nullptr;
        mdl.id = id;

        if (name)
            mdl.name.assign(name);

        dynamics.id = mdl_id;

        if constexpr (std::is_same_v<Dynamics, none>)
            mdl.type = dynamics_type::none;

        else if constexpr (std::is_same_v<Dynamics, qss1_integrator>)
            mdl.type = dynamics_type::qss1_integrator;

        else if constexpr (std::is_same_v<Dynamics, qss2_integrator>)
            mdl.type = dynamics_type::qss2_integrator;
        else if constexpr (std::is_same_v<Dynamics, qss2_multiplier>)
            mdl.type = dynamics_type::qss2_multiplier;
        else if constexpr (std::is_same_v<Dynamics, qss2_sum_2>)
            mdl.type = dynamics_type::qss2_sum_2;
        else if constexpr (std::is_same_v<Dynamics, qss2_sum_3>)
            mdl.type = dynamics_type::qss2_sum_3;
        else if constexpr (std::is_same_v<Dynamics, qss2_sum_4>)
            mdl.type = dynamics_type::qss2_sum_4;
        else if constexpr (std::is_same_v<Dynamics, qss2_wsum_2>)
            mdl.type = dynamics_type::qss2_wsum_2;
        else if constexpr (std::is_same_v<Dynamics, qss2_wsum_3>)
            mdl.type = dynamics_type::qss2_wsum_3;
        else if constexpr (std::is_same_v<Dynamics, qss2_wsum_4>)
            mdl.type = dynamics_type::qss2_wsum_4;

        else if constexpr (std::is_same_v<Dynamics, integrator>)
            mdl.type = dynamics_type::integrator;
        else if constexpr (std::is_same_v<Dynamics, quantifier>)
            mdl.type = dynamics_type::quantifier;
        else if constexpr (std::is_same_v<Dynamics, adder_2>)
            mdl.type = dynamics_type::adder_2;
        else if constexpr (std::is_same_v<Dynamics, adder_3>)
            mdl.type = dynamics_type::adder_3;
        else if constexpr (std::is_same_v<Dynamics, adder_4>)
            mdl.type = dynamics_type::adder_4;
        else if constexpr (std::is_same_v<Dynamics, mult_2>)
            mdl.type = dynamics_type::mult_2;
        else if constexpr (std::is_same_v<Dynamics, mult_3>)
            mdl.type = dynamics_type::mult_3;
        else if constexpr (std::is_same_v<Dynamics, mult_4>)
            mdl.type = dynamics_type::mult_4;
        else if constexpr (std::is_same_v<Dynamics, counter>)
            mdl.type = dynamics_type::counter;
        else if constexpr (std::is_same_v<Dynamics, generator>)
            mdl.type = dynamics_type::generator;
        else if constexpr (std::is_same_v<Dynamics, constant>)
            mdl.type = dynamics_type::constant;
        else if constexpr (std::is_same_v<Dynamics, cross>)
            mdl.type = dynamics_type::cross;
        else if constexpr (std::is_same_v<Dynamics, accumulator_2>)
            mdl.type = dynamics_type::accumulator_2;
        else if constexpr (std::is_same_v<Dynamics, flow>)
            mdl.type = dynamics_type::flow;
        else
            mdl.type = dynamics_type::time_func;

        if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
            for (size_t i = 0, e = std::size(dynamics.x); i != e; ++i) {
                auto& port = input_ports.alloc();
                port.model = dynamics.id;
                port.connections.set_allocator(&output_port_list_allocator);
                port.messages.set_allocator(&message_list_allocator);
                dynamics.x[i] = input_ports.get_id(port);
            }
        }

        if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
            for (size_t i = 0, e = std::size(dynamics.y); i != e; ++i) {
                auto& port = output_ports.alloc();
                port.model = dynamics.id;
                port.connections.set_allocator(&input_port_list_allocator);
                port.messages.set_allocator(&message_list_allocator);
                dynamics.y[i] = output_ports.get_id(port);
            }
        }

        return status::success;
    }

    void observe(model& mdl, observer& obs) noexcept
    {
        mdl.obs_id = observers.get_id(obs);
        obs.model = models.get_id(mdl);
    }

    status deallocate(model_id id)
    {
        auto* mdl = models.try_to_get(id);
        if (!mdl)
            return status::success;

        if (auto* obs = observers.try_to_get(mdl->obs_id); obs) {
            obs->model = static_cast<model_id>(0);
            mdl->obs_id = static_cast<observer_id>(0);
            observers.free(*obs);
        }

        auto ret = dispatch(mdl->type, [&](auto& d_array) {
            do_deallocate(d_array.get(mdl->id));
            d_array.free(mdl->id);
            return status::success;
        });

        sched.erase(*mdl);
        models.free(*mdl);

        return ret;
    }

    template<typename Dynamics>
    void do_deallocate([[maybe_unused]] Dynamics& dyn) noexcept
    {
        if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
            for (size_t i = 0, e = std::size(dyn.y); i != e; ++i) {
                auto& src = output_ports.get(dyn.y[i]);

                while (!src.connections.empty())
                    disconnect(dyn.y[i], src.connections.front());

                src.connections.clear();
                src.messages.clear();
            }
        }

        if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
            for (size_t i = 0, e = std::size(dyn.x); i != e; ++i) {
                auto& dst = input_ports.get(dyn.x[i]);

                while (!dst.connections.empty())
                    disconnect(dst.connections.front(), dyn.x[i]);

                dst.connections.clear();
                dst.messages.clear();
            }
        }

        if constexpr (is_detected_v<has_output_port_t, Dynamics>)
            for (size_t i = 0, e = std::size(dyn.y); i != e; ++i)
                output_ports.free(dyn.y[i]);

        if constexpr (is_detected_v<has_input_port_t, Dynamics>)
            for (size_t i = 0, e = std::size(dyn.x); i != e; ++i)
                input_ports.free(dyn.x[i]);
    }

    bool is_ports_compatible(const output_port_id src_id,
                             const output_port& src,
                             const input_port_id dst_id,
                             const input_port& dst) const noexcept
    {
        auto* mdl_src = models.try_to_get(src.model);
        auto* mdl_dst = models.try_to_get(dst.model);

        if (mdl_src == mdl_dst)
            return false;

        int o_port_index, i_port_index;

        if (is_bad(get_output_port_index(*mdl_src, src_id, &o_port_index)) ||
            is_bad(get_input_port_index(*mdl_dst, dst_id, &i_port_index)))
            return false;

        switch (mdl_src->type) {
        case dynamics_type::none:
            return false;

        case dynamics_type::quantifier:
            if (mdl_dst->type == dynamics_type::integrator &&
                i_port_index ==
                  static_cast<int>(integrator::port_name::port_quanta))
                return true;

            return false;

        case dynamics_type::qss1_integrator:
        case dynamics_type::qss2_integrator:
        case dynamics_type::qss2_multiplier:
        case dynamics_type::qss2_sum_2:
        case dynamics_type::qss2_sum_3:
        case dynamics_type::qss2_sum_4:
        case dynamics_type::qss2_wsum_2:
        case dynamics_type::qss2_wsum_3:
        case dynamics_type::qss2_wsum_4:
        case dynamics_type::integrator:
        case dynamics_type::adder_2:
        case dynamics_type::adder_3:
        case dynamics_type::adder_4:
        case dynamics_type::mult_2:
        case dynamics_type::mult_3:
        case dynamics_type::mult_4:
        case dynamics_type::counter:
        case dynamics_type::generator:
        case dynamics_type::constant:
        case dynamics_type::cross:
        case dynamics_type::time_func:
        case dynamics_type::accumulator_2:
            if (mdl_dst->type == dynamics_type::integrator &&
                i_port_index ==
                  static_cast<int>(integrator::port_name::port_quanta))
                return false;

            return true;
        }

        return false;
    }

    status connect(output_port_id src, input_port_id dst) noexcept
    {
        auto* src_port = output_ports.try_to_get(src);
        irt_return_if_fail(src_port, status::model_connect_output_port_unknown);

        auto* dst_port = input_ports.try_to_get(dst);
        irt_return_if_fail(dst_port, status::model_connect_input_port_unknown);

        irt_return_if_fail(std::find(std::begin(src_port->connections),
                                     std::end(src_port->connections),
                                     dst) == std::end(src_port->connections),
                           status::model_connect_already_exist);

        irt_return_if_fail(is_ports_compatible(src, *src_port, dst, *dst_port),
                           status::model_connect_bad_dynamics);

        src_port->connections.emplace_front(dst);
        dst_port->connections.emplace_front(src);

        return status::success;
    }

    status disconnect(output_port_id src, input_port_id dst) noexcept
    {
        auto* src_port = output_ports.try_to_get(src);
        if (!src_port)
            return status::model_connect_output_port_unknown;

        auto* dst_port = input_ports.try_to_get(dst);
        if (!dst_port)
            return status::model_connect_input_port_unknown;

        {
            const auto end = std::end(src_port->connections);
            auto it = std::begin(src_port->connections);

            if (*it == dst) {
                src_port->connections.pop_front();
            } else {
                auto prev = it++;
                while (it != end) {
                    if (*it == dst) {
                        src_port->connections.erase_after(prev);
                        break;
                    }
                    prev = it++;
                }
            }
        }

        {
            const auto end = std::end(dst_port->connections);
            auto it = std::begin(dst_port->connections);

            if (*it == src) {
                dst_port->connections.pop_front();
            } else {
                auto prev = it++;
                while (it != end) {
                    if (*it == src) {
                        dst_port->connections.erase_after(prev);
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
            obs->tl = t;
            if (obs->initialize)
                obs->initialize(*obs, t);
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

        flat_list<output_port_id> emitting_output_ports(
          &emitting_output_port_allocator);

        for (const auto id : bag)
            if (auto* mdl = models.try_to_get(id); mdl)
                irt_return_if_bad(
                  make_transition(*mdl, t, emitting_output_ports));

        for (const output_port_id src : emitting_output_ports) {
            if (auto* port_src = output_ports.try_to_get(src); port_src) {
                for (const message& msg : port_src->messages) {
                    for (const input_port_id dst : port_src->connections) {
                        if (auto* port_dst = input_ports.try_to_get(dst);
                            port_dst) {
                            port_dst->messages.emplace_front(msg);

                            if (auto* mdl = models.try_to_get(port_dst->model);
                                mdl) {
                                sched.update(*mdl, t);
                            }
                        }
                    }
                }

                port_src->messages.clear();
            }
        }

        return status::success;
    }

    template<typename Dynamics>
    status make_initialize(model& mdl, Dynamics& dyn, time t) noexcept
    {
        if constexpr (is_detected_v<initialize_function_t, Dynamics>)
            irt_return_if_bad(dyn.initialize(messages));

        mdl.tl = t;
        mdl.tn = t + dyn.sigma;
        mdl.handle = nullptr;

        sched.insert(mdl, dyn.id, mdl.tn);

        return status::success;
    }

    status make_initialize(model& mdl, time t) noexcept
    {
        return dispatch(
          mdl.type,
          [ this, &mdl,
            t ]<typename DynamicsModels>(DynamicsModels & dyn_models) {
              return this->make_initialize(mdl, dyn_models.get(mdl.id), t);
          });
    }

    template<typename Dynamics>
    status make_transition(
      model& mdl,
      Dynamics& dyn,
      time t,
      flat_list<output_port_id>& emitting_output_ports) noexcept
    {
        if (mdl.tn == mdl.handle->tn) {
            if constexpr (is_detected_v<lambda_function_t, Dynamics>) {
                if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
                    irt_return_if_bad(dyn.lambda(output_ports));

                    for (size_t i = 0, e = std::size(dyn.y); i != e; ++i) {
                        if (auto* port_src = output_ports.try_to_get(dyn.y[i]);
                            port_src) {
                            if (!port_src->messages.empty())
                                emitting_output_ports.emplace_front(dyn.y[i]);
                        }
                    }
                }
            }
        }

        if constexpr (is_detected_v<transition_function_t, Dynamics>)
            irt_return_if_bad(
              dyn.transition(input_ports, t, t - mdl.tl, mdl.tn - t));

        if constexpr (is_detected_v<has_input_port_t, Dynamics>)
            for (size_t i = 0, e = std::size(dyn.x); i != e; ++i)
                if (auto* port = input_ports.try_to_get(dyn.x[i]); port)
                    port->messages.clear();

        if constexpr (is_detected_v<observation_function_t, Dynamics>) {
            if (mdl.obs_id != static_cast<observer_id>(0)) {
                if (auto* observer = observers.try_to_get(mdl.obs_id);
                    observer && observer->observe) {
                    if (observer->time_step == 0.0 ||
                        t - observer->tl >= observer->time_step) {
                        observer->observe(*observer, t, dyn.observation(t));
                        observer->tl = t;
                    }
                } else {
                    mdl.obs_id = static_cast<observer_id>(0);
                }
            }
        }

        mdl.tl = t;
        mdl.tn = t + dyn.sigma;

        assert(mdl.tn >= t);

        sched.reintegrate(mdl, mdl.tn);

        return status::success;
    }

    status make_transition(model& mdl,
                           time t,
                           flat_list<output_port_id>& o) noexcept
    {
        return dispatch(
          mdl.type,
          [ this, &mdl, t, &
            o ]<typename DynamicsModels>(DynamicsModels & dyn_models) {
              return this->make_transition(mdl, dyn_models.get(mdl.id), t, o);
          });
    }
};

} // namespace irt

#endif
