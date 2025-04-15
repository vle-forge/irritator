// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_CONTAINER_2023
#define ORG_VLEPROJECT_IRRITATOR_CONTAINER_2023

#include <irritator/macros.hpp>

#include <algorithm>
#include <bitset>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory_resource>
#include <optional>
#include <type_traits>
#include <utility>

#include <cstddef>
#include <cstdint>

namespace irt {

////////////////////////////////////////////////////////////////////////
//                                                                    //
// Helpers functions................................................. //
//                                                                    //
////////////////////////////////////////////////////////////////////////

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

template<typename T>
concept not_integral = not std::is_integral_v<T>;

/**
   tag to allow small_vector<T, S> and vector<T, A> to reserve memory without
   resize.

   @code
   irt::vector<int> vec(10, reserve_tag{});
   irt::debug::ensure(vec.capacity() > 10);
   irt::debug::ensure(vec.size() == 0);
   @endcode
 */
struct reserve_tag {};

/**
   Returns true if the integer @c t is greater or equal to @c min_include and
   less or equal to @c max_include.
 */
template<std::integral T, std::integral R1, std::integral R2>
constexpr bool is_included(const T  t,
                           const R1 min_include,
                           const R2 max_include) noexcept
{
    return std::cmp_greater_equal(t, min_include) and
           std::cmp_less_equal(t, max_include);
}

inline constexpr auto u8s_to_u16(u8 a, u8 b) noexcept -> u16
{
    return static_cast<u16>((static_cast<u16>(a) << 8) | static_cast<u16>(b));
}

inline constexpr auto u16_to_u8s(u16 halfword) noexcept -> std::pair<u8, u8>
{
    return std::make_pair(static_cast<u8>((halfword >> 8) & 0xff),
                          static_cast<u8>(halfword & 0xff));
}

inline constexpr auto u16s_to_u32(u16 a, u16 b) noexcept
{
    return static_cast<u32>((static_cast<u32>(a) << 16) | static_cast<u32>(b));
}

inline constexpr auto u32_to_u16s(u32 word) noexcept -> std::pair<u16, u16>
{
    return std::make_pair(static_cast<u16>((word >> 16) & 0xffff),
                          static_cast<u16>(word & 0xffff));
}

inline constexpr auto u32s_to_u64(u32 a, u32 b) noexcept
{
    return static_cast<u64>((static_cast<u64>(a) << 32) | static_cast<u64>(b));
}

inline constexpr auto u64_to_u32s(u64 doubleword) noexcept
  -> std::pair<u32, u32>
{
    return std::make_pair(static_cast<u32>((doubleword >> 32) & 0xffffffff),
                          static_cast<u32>(doubleword & 0xffffffff));
}

inline constexpr auto left(std::unsigned_integral auto v) noexcept
{
    using type = std::decay_t<decltype(v)>;

    static_assert(std::is_same_v<u16, type> or std::is_same_v<u32, type> or
                  std::is_same_v<u64, type>);

    if constexpr (std::is_same_v<type, u16>)
        return static_cast<u8>(v >> 8);
    else if constexpr (std::is_same_v<type, u32>)
        return static_cast<u16>(v >> 16);
    else
        return static_cast<u32>(v >> 32);
}

inline constexpr auto right(std::unsigned_integral auto v) noexcept
{
    using type = std::decay_t<decltype(v)>;

    static_assert(std::is_same_v<u16, type> or std::is_same_v<u32, type> or
                  std::is_same_v<u64, type>);

    if constexpr (std::is_same_v<type, u16>)
        return static_cast<u8>(v & 0xff);
    else if constexpr (std::is_same_v<type, u32>)
        return static_cast<u16>(v & 0xffff);
    else
        return static_cast<u32>(v & 0xffffffff);
}

template<typename T>
concept is_identifier_type =
  std::is_enum_v<T> and
  (std::is_same_v<std::underlying_type_t<T>, std::uint32_t> or
   std::is_same_v<std::underlying_type_t<T>, std::uint64_t>);

template<typename T>
concept is_index_type =
  (std::is_same_v<T, std::uint16_t> or std::is_same_v<T, std::uint32_t>);

template<typename Identifier>
    requires(is_identifier_type<Identifier>)
constexpr Identifier undefined() noexcept
{
    return static_cast<Identifier>(0);
}

template<typename Identifier>
    requires(is_identifier_type<Identifier>)
constexpr bool is_undefined(Identifier id) noexcept
{
    return id == undefined<Identifier>();
}

template<typename Identifier>
    requires(is_identifier_type<Identifier>)
constexpr bool is_defined(Identifier id) noexcept
{
    return id != undefined<Identifier>();
}

template<typename Index>
    requires(is_index_type<Index>)
inline constexpr auto g_make_id(Index key, Index index) noexcept
{
    if constexpr (std::is_same_v<std::uint16_t, Index>)
        return (static_cast<std::uint32_t>(key) << 16) |
               static_cast<std::uint32_t>(index);
    else
        return (static_cast<std::uint64_t>(key) << 32) |
               static_cast<std::uint64_t>(index);
}

template<typename Index>
    requires(is_index_type<Index>)
inline constexpr auto g_make_next_key(Index key) noexcept
{
    if constexpr (std::is_same_v<std::uint16_t, Index>)
        return key == 0xffffffff ? 1u : key + 1u;
    else
        return key == 0xffffffffffffffff ? 1u : key + 1u;
}

template<typename Identifier>
    requires(is_identifier_type<Identifier>)
inline constexpr auto g_get_key(Identifier id) noexcept
{
    using underlying_type = std::underlying_type_t<Identifier>;

    if constexpr (std::is_same_v<std::uint32_t, underlying_type>)
        return static_cast<std::uint16_t>(
          (static_cast<std::uint32_t>(id) >> 16) & 0xffff);
    else
        return static_cast<std::uint32_t>(
          (static_cast<std::uint64_t>(id) >> 32) & 0xffffffff);
}

template<typename Identifier>
    requires(is_identifier_type<Identifier>)
inline constexpr auto g_get_index(Identifier id) noexcept
{
    using underlying_type = std::underlying_type_t<Identifier>;

    if constexpr (std::is_same_v<std::uint32_t, underlying_type>)
        return static_cast<std::uint16_t>(static_cast<std::uint16_t>(id) &
                                          0xffff);
    else
        return static_cast<std::uint32_t>(static_cast<std::uint32_t>(id) &
                                          0xffffffff);
}

/**
   Compute the best size to fit the small storage size.

   This function is used into @c small_string, @c small_vector and @c
   small_ring_buffer to determine the @c capacity and/or @c size type
   (unsigned, short unsigned, long unsigned, long long unsigned.
*/
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

template<class T, class M>
constexpr std::ptrdiff_t offset_of(const M T::* member)
{
    return reinterpret_cast<std::ptrdiff_t>(
      &(reinterpret_cast<T*>(0)->*member));
}

/**
   A helper function to get a pointer to the parent container from a member:

   @code
   struct point { float x; float y; };
   struct line { point p1, p2; };
   line l;

   void fn(point& p) {
       line& ptr = container_of(&p, &line::p1);
       ...
   }
   @endcode
*/
template<class T, class M>
constexpr T& container_of(M* ptr, const M T::* member) noexcept
{
    return *reinterpret_cast<T*>(reinterpret_cast<intptr_t>(ptr) -
                                 offset_of(member));
}

/**
   A helper function to get a constant pointer to the parent container from a
   member:

   @code
   struct point { float x; float y; };
   struct line { point p1, p2; };
   line l;

   void fn(const point& p) {
       const line& ptr = container_of(&p, &line::p1);
       ...
   }
   @endcode
*/
template<class T, class M>
constexpr const T& container_of(const M* ptr, const M T::* member) noexcept
{
    return *reinterpret_cast<const T*>(reinterpret_cast<intptr_t>(ptr) -
                                       offset_of(member));
}

/**
   Build an human readable version of a number of bytes.

   According to the number of bytes, it will produce an easy to read value of
   byte, kilobytes, megabytes etc.

   A fmt::formatter is provide in the file @c format.hpp
 */
struct human_readable_bytes {
    enum class display_type { B, KB, MB, GB };

    explicit human_readable_bytes(std::size_t bytes) noexcept;

    double       size;
    display_type type;
};

/**
   Build  an human readable version of a duration.

   According to the number of nanoseconds, producte an easy to read value of the
   duration (ms, s, mn etc.).

   A fmt::formatter is provide in the file @c format.hpp
*/
struct human_readable_time {
    enum class display_type {
        nanoseconds,
        microseconds,
        milliseconds,
        seconds,
        minutes,
        hours,
    };

    explicit human_readable_time(const std::size_t nanoseconds) noexcept;

    double       value;
    display_type type;
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

class new_delete_memory_resource
{
public:
    class data
    {
    private:
        std::size_t allocated   = 0;
        std::size_t deallocated = 0;

    public:
        void* allocate(
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            if constexpr (debug::enable_memory_log == true and
                          debug::enable_ensure == true) {
                return debug_allocate(bytes, alignment);
            } else {
                allocated += bytes;
                return std::pmr::new_delete_resource()->allocate(bytes,
                                                                 alignment);
            }
        }

        void deallocate(
          void*       p,
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            if constexpr (debug::enable_memory_log == true and
                          debug::enable_ensure == true) {
                debug_deallocate(p, bytes, alignment);
            } else {
                deallocated += bytes;
                return std::pmr::new_delete_resource()->deallocate(
                  p, bytes, alignment);
            }
        }

        void release() noexcept {}

        std::pair<std::size_t, std::size_t> get_memory_usage() noexcept
        {
            return std::make_pair(allocated, deallocated);
        }

    private:
        void* debug_allocate(std::size_t bytes, std::size_t alignment) noexcept;
        void  debug_deallocate(void*       p,
                               std::size_t bytes,
                               std::size_t alignment) noexcept;
    };

    static data& instance() noexcept
    {
        static data d;
        return d;
    }
};

template<std::size_t max_blocks_per_chunk,
         std::size_t largest_required_pool_block>
class synchronized_pool_resource
{
public:
    class data
    {
    private:
        std::pmr::synchronized_pool_resource mr{ std::pmr::pool_options{
          max_blocks_per_chunk,
          largest_required_pool_block } };

        std::size_t allocated   = 0;
        std::size_t deallocated = 0;

    public:
        void* allocate(
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            allocated += bytes;
            return mr.allocate(bytes, alignment);
        }

        void deallocate(
          void*       p,
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            deallocated += bytes;
            return mr.deallocate(p, bytes, alignment);
        }

        void release() noexcept {}

        std::pair<std::size_t, std::size_t> get_memory_usage() noexcept
        {
            return std::make_pair(allocated, deallocated);
        }

    private:
        void* debug_allocate(std::size_t bytes, std::size_t alignment) noexcept;
        void  debug_deallocate(void*       p,
                               std::size_t bytes,
                               std::size_t alignment) noexcept;
    };

    static data& instance() noexcept
    {
        static data d;
        return d;
    }
};

template<std::size_t max_blocks_per_chunk,
         std::size_t largest_required_pool_block>
class unsynchronized_pool_resource
{
public:
    class data
    {
    private:
        std::pmr::unsynchronized_pool_resource mr{ std::pmr::pool_options{
          max_blocks_per_chunk,
          largest_required_pool_block } };

        std::size_t allocated   = 0;
        std::size_t deallocated = 0;

    public:
        void* allocate(
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            allocated += bytes;
            return mr.allocate(bytes, alignment);
        }

        void deallocate(
          void*       p,
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            deallocated += bytes;
            return mr.deallocate(p, bytes, alignment);
        }

        void release() noexcept {}

        std::pair<std::size_t, std::size_t> get_memory_usage() noexcept
        {
            return std::make_pair(allocated, deallocated);
        }

    private:
        void* debug_allocate(std::size_t bytes, std::size_t alignment) noexcept;
        void  debug_deallocate(void*       p,
                               std::size_t bytes,
                               std::size_t alignment) noexcept;
    };

    static data& instance() noexcept
    {
        static data d;
        return d;
    }
};

template<std::size_t Length = 1024u * 1024u, int ID = 0>
class monotonic_small_buffer
{
public:
    static inline constexpr std::size_t size = Length;
    static inline constexpr int         id   = ID;

    static_assert(Length > 0);

    class data
    {
    private:
        std::array<std::byte, Length> buffer;

        std::pmr::monotonic_buffer_resource mr{
            buffer.data(),
            buffer.size(),
            std::pmr::new_delete_resource()
        };

        std::size_t allocated   = 0;
        std::size_t deallocated = 0;

    public:
        void* allocate(
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            allocated += bytes;
            return mr.allocate(bytes, alignment);
        }

        void deallocate(
          void*       p,
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            deallocated += bytes;

            return mr.deallocate(p, bytes, alignment);
        }

        void release() noexcept { mr.release(); }

        std::pair<std::size_t, std::size_t> get_memory_usage() noexcept
        {
            return std::make_pair(allocated, deallocated);
        }

    private:
        void* debug_allocate(std::size_t bytes, std::size_t alignment) noexcept;
        void  debug_deallocate(void*       p,
                               std::size_t bytes,
                               std::size_t alignment) noexcept;
    };

    static data& instance() noexcept
    {
        static data d;
        return d;
    }
};

template<int ID = 0>
class monotonic_buffer
{
public:
    static inline constexpr int id = ID;

    class data
    {
    private:
        std::pmr::monotonic_buffer_resource mr{
            std::pmr::new_delete_resource()
        };

        std::size_t allocated   = 0;
        std::size_t deallocated = 0;

    public:
        void* allocate(
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            allocated += bytes;
            return mr.allocate(bytes, alignment);
        }

        void deallocate(
          void*       p,
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            deallocated += bytes;

            return mr.deallocate(p, bytes, alignment);
        }

        void release() noexcept { mr.release(); }

        std::pair<std::size_t, std::size_t> get_memory_usage() noexcept
        {
            return std::make_pair(allocated, deallocated);
        }

    private:
        void* debug_allocate(std::size_t bytes, std::size_t alignment) noexcept;
        void  debug_deallocate(void*       p,
                               std::size_t bytes,
                               std::size_t alignment) noexcept;
    };

    static data& instance() noexcept
    {
        static data d;
        return d;
    }
};

/**
   A stateless allocator class to wrap static memory resource.

   @verbatim
                                   +----------+
                                   |new-delete|
                                   +----^-----+
   +-----------+        +---------+        |
   |vector<T,A>+------->|allocator+--------+
   +-----------+ static +---------+ static |
                                   +----v-----+
                                   |fixed-size|
                                   +----------+
   @endverbatim

   Enable the @c IRRITATOR_ENABLE_DEBUG preprocessor variable to enable a debug
   for all allocation/deallocation for each memory resource.
 */
template<typename MemoryResource = new_delete_memory_resource>
struct allocator {
    using size_type            = std::size_t;
    using difference_type      = std::ptrdiff_t;
    using memory_resource_type = MemoryResource;

    static void* allocate(
      std::size_t bytes,
      std::size_t alignment = alignof(std::max_align_t)) noexcept
    {
        debug::ensure(bytes != 0);

        return bytes
                 ? memory_resource_type::instance().allocate(bytes, alignment)
                 : nullptr;
    }

    static void deallocate(
      void*       p,
      std::size_t bytes,
      std::size_t alignment = alignof(std::max_align_t)) noexcept
    {
        debug::ensure((p != nullptr and bytes > 0) or
                      (p == nullptr and bytes == 0));

        if (p)
            memory_resource_type::instance().deallocate(p, bytes, alignment);
    }

    static void release() noexcept
    {
        return memory_resource_type::instance().release();
    }

    static std::pair<std::size_t, std::size_t> get_memory_usage() noexcept
    {
        return memory_resource_type::instance().get_memory_usage();
    }
};

////////////////////////////////////////////////////////////////////////
//                                                                    //
// Container: vector, data_array and list............................ //
//                                                                    //
////////////////////////////////////////////////////////////////////////

/**
   @brief A vector like class with dynamic allocation.

   @tparam T Any type (trivial or not).
 */
template<typename T, typename A = allocator<new_delete_memory_resource>>
class vector
{
public:
    using value_type       = T;
    using size_type        = std::uint32_t;
    using index_type       = std::make_signed_t<size_type>;
    using iterator         = T*;
    using const_iterator   = const T*;
    using reference        = T&;
    using const_reference  = const T&;
    using rvalue_reference = T&&;
    using pointer          = T*;
    using const_pointer    = const T*;
    using allocator_type   = A;
    using this_container   = vector<T, A>;

private:
    static_assert(std::is_nothrow_destructible_v<T> ||
                  std::is_trivially_destructible_v<T>);

    T* m_data = nullptr;

    index_type m_size     = 0;
    index_type m_capacity = 0;

public:
    constexpr vector() noexcept = default;
    explicit vector(std::integral auto size) noexcept;

    vector(std::integral auto capacity, const reserve_tag) noexcept;

    vector(std::integral auto size, const_reference value) noexcept;

    vector(std::integral auto capacity,
           std::integral auto size,
           const T&           default_value) noexcept;

    template<not_integral InputIterator>
    vector(InputIterator first, InputIterator last) noexcept;

    /**
       Equivalent to vector(init.begin(), init.end()).

       @code
       irt::vector<int> x{ 1, 2, 3, 4 };
       irt::debug::ensure(x.size() == 4u);
       irt::debug::ensure(x[0] == 1);
       irt::debug::ensure(x[1] == 2);
       irt::debug::ensure(x[2] == 3);
       irt::debug::ensure(x[3] == 4);
       @endcode

       @param init
     */
    explicit vector(std::initializer_list<T> init) noexcept;

    ~vector() noexcept;

    constexpr vector(const vector& other) noexcept;
    constexpr vector& operator=(const vector& other) noexcept;
    constexpr vector(vector&& other) noexcept;
    constexpr vector& operator=(vector&& other) noexcept;

    bool resize(std::integral auto size) noexcept;
    bool resize(std::integral auto size, const_reference value) noexcept;

    bool reserve(std::integral auto new_capacity) noexcept;
    template<int Num, int Denum>
    bool grow() noexcept;

    void destroy() noexcept; // clear all elements and free memory (size
                             // = 0, capacity = 0 after).

    template<not_integral InputIterator>
    constexpr void assign(InputIterator first, InputIterator last) noexcept;

    constexpr void assign(std::integral auto size,
                          const_reference    value) noexcept;

    constexpr iterator insert(const_iterator  it,
                              const_reference value) noexcept;
    constexpr iterator insert(const_iterator   it,
                              rvalue_reference value) noexcept;

    constexpr void clear() noexcept; // clear all elements (size = 0 after).
    constexpr void swap(vector<T, A>& other) noexcept;

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
    constexpr const_iterator cbegin() const noexcept;
    constexpr const_iterator cend() const noexcept;

    constexpr bool     can_alloc(std::integral auto number = 1) const noexcept;
    constexpr unsigned size() const noexcept;
    constexpr int      ssize() const noexcept;
    constexpr int      capacity() const noexcept;
    constexpr bool     empty() const noexcept;
    constexpr bool     full() const noexcept;

    template<typename... Args>
    constexpr reference emplace_back(Args&&... args) noexcept;

    constexpr reference push_back(const T& args) noexcept;

    constexpr int find(const T& t) const noexcept;

    constexpr void pop_back() noexcept;
    constexpr void swap_pop_back(std::integral auto index) noexcept;

    constexpr iterator erase(iterator it) noexcept;
    constexpr iterator erase(iterator begin, iterator end) noexcept;

    constexpr int  index_from_ptr(const T* data) const noexcept;
    constexpr bool is_iterator_valid(const_iterator it) const noexcept;

private:
    constexpr bool make(std::integral auto capacity) noexcept;
    constexpr bool make(std::integral auto capacity,
                        std::integral auto size) noexcept;
    constexpr bool make(std::integral auto capacity,
                        std::integral auto size,
                        const T&           default_value) noexcept;

    int compute_new_capacity(int new_capacity) const;
};

template<typename Identifier>
constexpr auto get_index(Identifier id) noexcept
{
    static_assert(std::is_enum_v<Identifier>);

    using utype = std::underlying_type_t<Identifier>;

    static_assert(std::is_same_v<utype, u32> or std::is_same_v<utype, u64>);

    return right(static_cast<utype>(id));
}

template<typename Identifier>
constexpr auto get_key(Identifier id) noexcept
{
    static_assert(std::is_enum_v<Identifier>);

    using utype = std::underlying_type_t<Identifier>;

    static_assert(std::is_same_v<utype, u32> or std::is_same_v<utype, u64>);

    return left(static_cast<utype>(id));
}

template<typename Identifier>
constexpr bool is_valid(Identifier id) noexcept
{
    return get_key(id) > 0;
}

/**
   An optimized array to store unique identifier.

   A container to handle only identifier.
   - linear memory/iteration
   - O(1) alloc/free
   - stable indices
   - weak references
   - zero overhead dereferences

   @tparam Identifier A enum class identifier to store identifier unsigned
   number.
*/
template<typename Identifier,
         typename A = allocator<new_delete_memory_resource>>
class id_array
{
    static_assert(is_identifier_type<Identifier>,
                  "Identifier must be a enumeration: enum class id : "
                  "std::uint32_t or std::uint64_t;");

    using underlying_id_type = std::conditional_t<
      std::is_same_v<std::uint32_t, std::underlying_type_t<Identifier>>,
      std::uint32_t,
      std::uint64_t>;

    using index_type =
      std::conditional_t<std::is_same_v<std::uint32_t, underlying_id_type>,
                         std::uint16_t,
                         std::uint32_t>;

    using identifier_type = Identifier;
    using value_type      = Identifier;
    using this_container  = id_array<Identifier, A>;
    using allocator_type  = A;

    static constexpr index_type none = std::numeric_limits<index_type>::max();

private:
    Identifier* m_items = nullptr; // items array

    index_type m_max_size  = 0;    //!< Number of valid item.
    index_type m_max_used  = 0;    //!< highest index ever allocated
    index_type m_capacity  = 0;    //!< capacity of the array
    index_type m_next_key  = 1;    /**< [1..2^[16|32] - 1 (never == 0). */
    index_type m_free_head = none; // index of first free entry

    //! Build a new identifier merging m_next_key and the best free
    //! index.
    static constexpr identifier_type make_id(index_type key,
                                             index_type index) noexcept;

    //! Build a new m_next_key and avoid key 0.
    static constexpr index_type make_next_key(index_type key) noexcept;

    //! Get the index part of the identifier
    static constexpr index_type get_index(Identifier id) noexcept;

    //! Get the key part of the identifier
    static constexpr index_type get_key(Identifier id) noexcept;

public:
    constexpr id_array() noexcept = default;

    constexpr id_array(const id_array& other) noexcept;
    constexpr id_array(id_array&& other) noexcept;
    constexpr id_array& operator=(const id_array& other) noexcept;
    constexpr id_array& operator=(id_array&& other) noexcept;

    constexpr explicit id_array(std::integral auto capacity) noexcept;

    constexpr ~id_array() noexcept;

    constexpr Identifier                alloc() noexcept;
    constexpr std::optional<Identifier> try_alloc() noexcept;

    constexpr bool reserve(std::integral auto capacity) noexcept;
    constexpr void free(const Identifier id) noexcept;
    constexpr void clear() noexcept;
    constexpr void destroy() noexcept;

    /**
     * Use @a reserve function with a @a factor to grow the capacity. If the @a
     * capacity equals zero, default size is @a 8 otherwise, the capactiy will
     * be @a capacity() multiples @a grow_factor.
     * @param grow_factor The growing factor.
     * @return true is success, false otherwise.
     */
    template<int Num, int Denum = 1>
    constexpr bool grow() noexcept;

    constexpr std::optional<index_type> get(const Identifier id) const noexcept;
    constexpr bool exists(const Identifier id) const noexcept;

    //! @brief Checks if a `id` exists in the underlying array at index
    //! `index`.
    //!
    //! @attention This function can check the version of the identifier
    //! only the `is_defined<Identifier>` is used to detect the
    //! `Identifier`.
    //!
    //! @param index A integer.
    //! @return The `Identifier` found at index `index` or
    //! `std::nullopt` otherwise.
    constexpr identifier_type get_from_index(
      std::integral auto index) const noexcept;

    constexpr bool next(const Identifier*& idx) const noexcept;

    constexpr bool       empty() const noexcept;
    constexpr bool       full() const noexcept;
    constexpr unsigned   size() const noexcept;
    constexpr int        ssize() const noexcept;
    constexpr bool       can_alloc(std::integral auto nb) const noexcept;
    constexpr int        max_used() const noexcept;
    constexpr int        capacity() const noexcept;
    constexpr index_type next_key() const noexcept;
    constexpr bool       is_free_list_empty() const noexcept;

    template<bool is_const>
    struct iterator_base {
        using iterator_concept = std::bidirectional_iterator_tag;
        using difference_type  = std::ptrdiff_t;
        using element_type =
          std::conditional_t<is_const, const value_type, value_type>;
        using pointer   = element_type*;
        using reference = element_type;
        using container_type =
          std::conditional_t<is_const, const this_container, this_container>;

        iterator_base() noexcept = default;

        template<typename Container>
        iterator_base(Container* self_, identifier_type id_) noexcept
            requires(!std::is_const_v<Container> && !is_const)
          : self{ self_ }
          , id{ id_ }
        {}

        template<typename Container>
        iterator_base(Container* self_, identifier_type id_) noexcept
            requires(std::is_const_v<Container> && is_const)
          : self{ self_ }
          , id{ id_ }
        {}

        reference operator*() const noexcept { return id; }
        reference operator*() noexcept { return id; }
        pointer   operator->() const noexcept { return &id; }
        pointer   operator->() noexcept { return &id; }

        iterator_base& operator--() noexcept
        {
            if (auto index = get_index(id); index) {
                do {
                    --index;
                    if (is_valid(self->m_items[index])) {
                        id = self->m_items[index];
                        return *this;
                    }
                } while (index);
            }

            id = identifier_type{};
            return *this;
        }

        iterator_base& operator++() noexcept
        {
            auto index = get_index(id);
            ++index;

            for (; index < self->m_max_used; ++index) {
                if (is_valid(self->m_items[index])) {
                    id = self->m_items[index];
                    return *this;
                }
            }

            id = identifier_type{};
            return *this;
        }

        iterator_base operator--(int) noexcept
        {
            const auto old_id = id;

            if (auto index = get_index(id); index) {
                do {
                    --index;
                    if (is_valid(self->m_items[index])) {
                        id = self->m_items[index];
                        return iterator_base{ .self = self, .id = old_id };
                    }
                } while (index);
            }

            id = identifier_type{};
            return iterator_base{ .self = self, .id = old_id };
        }

        iterator_base operator++(int) noexcept
        {
            const auto old_id = id;
            auto       index  = get_index(id);
            ++index;

            for (; index < self->max_used; ++index) {
                if (is_valid(self->m_items[index])) {
                    id = self->m_items[index];
                    return iterator_base{ .self = self, .id = old_id };
                }
            }

            id = identifier_type{};
            return iterator_base{ .self = self, .id = old_id };
        }

        auto operator<=>(const iterator_base&) const noexcept = default;

        container_type* self = {};
        identifier_type id   = { 0 };
    };

    using iterator       = iterator_base<false>;
    using const_iterator = iterator_base<true>;

    constexpr iterator       begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;
};

/**
   An optimized SOA structure to store unique identifier and mutiples vector.

   A container to handle only identifier.
   - linear memory/iteration
   - O(1) alloc/free
   - stable indices
   - weak references
   - zero overhead dereferences

   @code
   struct pos3d {
       float x, y, z;
   };

   struct color {
       std::uint32_t rgba;
   };

   using name = irt::small_string<15>;

   enum class ex1_id : uint32_t;

   irt::id_data_array<ex1_id,
        irt::allocator<new_delete_memory_resource>, pos3d, color, name>
     d;
   d.reserve(1024);
   expect(ge(d.capacity(), 1024u));
   expect(fatal(d.can_alloc(1)));

   const auto id =
     d.alloc([](const auto id, auto& p, auto& c, auto& n) noexcept
   {
       p = pos3d(1.f, 2.f, 3.f);
       c = color{ 123u };
       n = "HelloWorld!";
   });
   @endcode

   @tparam Identifier A enum class identifier to store identifier unsigned
   number.
*/
template<typename Identifier,
         typename A = allocator<new_delete_memory_resource>,
         class... Ts>
class id_data_array
{
    static_assert(is_identifier_type<Identifier>,
                  "Identifier must be a enumeration: enum class id : "
                  "std::uint32_t or std::uint64_t;");

    using underlying_id_type = std::conditional_t<
      std::is_same_v<std::uint32_t, std::underlying_type_t<Identifier>>,
      std::uint32_t,
      std::uint64_t>;

    using index_type =
      std::conditional_t<std::is_same_v<std::uint32_t, underlying_id_type>,
                         std::uint16_t,
                         std::uint32_t>;

    using identifier_type = Identifier;
    using value_type      = Identifier;
    using allocator_type  = A;

    id_array<identifier_type, A> m_ids;
    std::tuple<Ts*...>           m_col;

private:
    template<typename Function, std::size_t... Is>
    void do_call_fn(Function&&            fn,
                    const identifier_type id,
                    std::index_sequence<Is...>) noexcept
    {
        const auto idx = get_index(id);

        fn(id, std::get<Is>(m_col)[(idx)]...);
    }

    template<typename Function, std::size_t... Is>
    void do_call_fn(Function&&            fn,
                    const identifier_type id,
                    std::index_sequence<Is...>) const noexcept
    {
        const auto idx = get_index(id);

        fn(id, std::get<Is>(m_col)[(idx)]...);
    }

    template<typename Function, std::size_t... Is>
    void do_call_alloc_fn(Function&&            fn,
                          const identifier_type id,
                          std::index_sequence<Is...>) noexcept
    {
        const auto idx = get_index(id);

        fn(id, std::get<Is>(m_col)[(idx)]...);
    }

    template<typename T>
    void do_construct(T* ptr, const std::integral auto len) noexcept
    {
        debug::ensure(ptr);
        debug::ensure(len > 0);

        std::uninitialized_default_construct_n(ptr, len);
    }

    template<std::size_t... Is>
    void do_construct(const std::integral auto len,
                      std::index_sequence<Is...>) noexcept
    {
        debug::ensure(len > 0);

        (do_construct(std::get<Is>(m_col), len), ...);
    }

    template<typename T>
    void do_alloc(T*& ptr, const std::integral auto len) noexcept
    {
        debug::ensure(ptr == nullptr);
        debug::ensure(len > 0);

        ptr = reinterpret_cast<T*>(A::allocate(sizeof(T) * len));
    }

    /**
     * Allocate a uninitialised buffer for each element in the @a std::tuple @a
     * m_col.
     */
    template<std::size_t... Is>
    void do_alloc(const std::integral auto len,
                  std::index_sequence<Is...>) noexcept
    {
        debug::ensure(len > 0);

        (do_alloc(std::get<Is>(m_col), len), ...);
    }

    template<typename T>
    void do_destroy(const std::integral auto len, T*& ptr) noexcept
    {
        if (ptr) {
            std::destroy_n(ptr, len);
            A::deallocate(ptr, sizeof(T) * len);
        }

        ptr = nullptr;
    }

    /**
     * Destroy all objects for each element in the @a std::tuple @a m_col and
     * deallocate the memory used.
     */
    template<std::size_t... Is>
    void do_destroy(const std::integral auto len,
                    std::index_sequence<Is...>) noexcept
    {
        (do_destroy(len, std::get<Is>(m_col)), ...);
    }

    template<typename T>
    void do_uninitialised_copy(const T*           src,
                               T*                 dst,
                               std::integral auto len) noexcept
    {
        debug::ensure(src and dst);

        std::uninitialized_copy_n(src, len, dst);
    }

    template<std::size_t... Is>
    void do_uninitialised_copy(const auto& other_m_col,
                               const auto  len,
                               std::index_sequence<Is...>) noexcept
    {
        (do_uninitialised_copy(
           std::get<Is>(other_m_col), std::get<Is>(m_col), len),
         ...);
    }

    /**
     * Affect nullptr to any element in the @a std::tuple @a other_m_col.
     * @param other_m_col
     */
    template<std::size_t... Is>
    void do_reset(auto& other_m_col, std::index_sequence<Is...>) noexcept
    {
        ((std::get<Is>(other_m_col) = nullptr), ...);
    }

    template<typename T>
    void do_resize(std::integral auto old,
                   std::integral auto req,
                   T*&                ptr) noexcept
    {
        auto* new_ptr = reinterpret_cast<T*>(A::allocate(sizeof(T) * req));

        if (ptr) {
            if constexpr (std::is_trivially_copy_constructible_v<T>) {
                std::uninitialized_copy_n(ptr, old, new_ptr);
            } else if constexpr (std::is_trivially_move_constructible_v<T>) {
                std::uninitialized_move_n(ptr, old, new_ptr);
            } else {
                std::uninitialized_copy_n(ptr, old, new_ptr);
            }
            A::deallocate(ptr, sizeof(T) * old);
        }

        std::uninitialized_value_construct_n(new_ptr + old, req - old);
        ptr = new_ptr;
    }

    template<std::size_t... Is>
    bool do_resize(std::integral auto old,
                   std::integral auto req,
                   std::index_sequence<Is...>) noexcept
    {
        (do_resize(old, req, std::get<Is>(m_col)), ...);
        return ((std::get<Is>(m_col) != nullptr) and ... and true);
    }

public:
    using iterator       = id_array<identifier_type, A>::iterator;
    using const_iterator = id_array<identifier_type, A>::const_iterator;

    id_data_array() noexcept = default;
    ~id_data_array() noexcept;

    id_data_array(const id_data_array& other) noexcept;
    id_data_array(id_data_array&& other) noexcept;
    id_data_array& operator=(const id_data_array& other) noexcept;
    id_data_array& operator=(id_data_array&& other) noexcept;

    identifier_type alloc() noexcept;

    template<typename Function>
    identifier_type alloc(Function&& fn) noexcept;

    /** Return the identifier type at index `idx` if and only if `idx`
     * is in range `[0, size()[` and if the value `is_defined` otherwise
     * return `undefined<identifier_type>()`. */
    identifier_type get_id(std::integral auto idx) const noexcept;

    /** Release the @c identifier from the @c id_array. @attention To
     * improve memory access, the destructors of underlying objects in
     * @c std::tuple of
     * @c vector are not called. If you need to realease memory use it
     * before releasing the identifier but these objects can be reuse in
     * future. In any case all destructor will free the memory in @c
     * id_data_array destructor or during the @c reserve() operation.
     *
     * @example
     * id_data_array<obj_id, allocator<new_delete_memory_resource,
     * std::string, position> m;
     *
     * if (m.exists(id))
     *     std::swap(m.get<std::string>()[get_index(id)],
     * std::string{});
     * @endexample */
    void free(const identifier_type id) noexcept;

    /** Get the underlying @c vector in @c tuple using an index. (read
     * @c std::get). */
    template<std::size_t Index>
    auto& get() noexcept;

    /** Get the underlying @c vector in @c tuple using a type (read @c
     * std::get). */
    template<typename Type>
    auto& get() noexcept;

    /** Get the underlying object at position @c id @c vector in @c
     * tuple using an index. (read @c std::get). */
    template<std::size_t Index>
    auto& get(const identifier_type id) noexcept;

    /** Get the underlying object at position @c id in @c vector in @c
     * tuple using a type (read @c std::get). */
    template<typename Type>
    auto& get(const identifier_type id) noexcept;

    /** Get the underlying @c vector in @c tuple using an index. (read
     * @c std::get). */
    template<std::size_t Index>
    auto& get() const noexcept;

    /** Get the underlying @c vector in @c tuple using a type (read @c
     * std::get). */
    template<typename Type>
    auto& get() const noexcept;

    /** Get the underlying object at position @c id @c vector in @c
     * tuple using an index. (read @c std::get). */
    template<std::size_t Index>
    auto& get(const identifier_type id) const noexcept;

    /** Get the underlying object at position @c id in @c vector in @c
     * tuple using a type (read @c std::get). */
    template<typename Type>
    auto& get(const identifier_type id) const noexcept;

    //! Call the @c fn function if @c id is valid.
    //!
    //! The function take one value from Index (integer or type).
    template<typename Index, typename Function>
    void if_exists_do(const identifier_type id, Function&& fn) noexcept;

    //! Call the @c fn function if @c id is valid.
    //!
    //! The function take values from all vectors.
    template<typename Function>
    void if_exists_do(const identifier_type id, Function&& fn) noexcept;

    //! Call the @c fn function for each valid identifier.
    //! The function take one identifier_type.
    template<typename Function>
    void for_each_id(Function&& fn) noexcept;

    //! Call the @c fn function for each valid identifier.
    //!
    //! The function take one identifier_type.
    template<typename Function>
    void for_each_id(Function&& fn) const noexcept;

    //! Call the @c fn function for each valid identifier.
    template<typename Index, typename Function>
    void for_each(Function&& fn) noexcept;

    //! Call the @c fn function for each valid identifier.
    //
    //! The function take one vector from Index (integer or type).
    template<typename Function>
    void for_each(Function&& fn) noexcept;

    //! Call the @c fn function for each valid identifier.
    template<typename Index, typename Function>
    void for_each(Function&& fn) const noexcept;

    //! Call the @c fn function for each valid identifier.
    //
    //! The function take one vector from Index (integer or type).
    template<typename Function>
    void for_each(Function&& fn) const noexcept;

    constexpr iterator       begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;

    void clear() noexcept;
    bool reserve(std::integral auto len) noexcept;
    template<int Num, int Denum = 1>
    bool grow() noexcept;

    bool     exists(const identifier_type id) const noexcept;
    bool     can_alloc(std::integral auto nb = 1) const noexcept;
    bool     empty() const noexcept;
    unsigned size() const noexcept;
    int      ssize() const noexcept;
    unsigned capacity() const noexcept;
};

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
//! @tparam Identifier A enum class identifier to store identifier
//! unsigned
//!     number.
//! @todo Make Identifier split key|id automatically for all unsigned.
template<typename T,
         typename Identifier,
         typename A = allocator<new_delete_memory_resource>>
class data_array
{
public:
    static_assert(std::is_enum_v<Identifier>,
                  "Identifier must be a enumeration: enum class id : "
                  "std::uint32_t or std::uint64_t;");

    static_assert(
      std::is_same_v<std::underlying_type_t<Identifier>, std::uint32_t> ||
        std::is_same_v<std::underlying_type_t<Identifier>, std::uint64_t>,
      "Identifier underlying type must be std::uint32_t or "
      "std::uint64_t");

    using underlying_id_type = std::conditional_t<
      std::is_same_v<std::uint32_t, std::underlying_type_t<Identifier>>,
      std::uint32_t,
      std::uint64_t>;

    using index_type =
      std::conditional_t<std::is_same_v<std::uint32_t, underlying_id_type>,
                         std::uint16_t,
                         std::uint32_t>;

    using identifier_type = Identifier;
    using value_type      = T;
    using this_container  = data_array<T, Identifier, A>;
    using allocator_type  = A;

private:
    struct item {
        T          item;
        Identifier id;
    };

public:
    using internal_value_type = item;

    item* m_items = nullptr; // items array

    index_type m_max_size  = 0;    // number of valid item
    index_type m_max_used  = 0;    // highest index ever allocated
    index_type m_capacity  = 0;    // capacity of the array
    index_type m_next_key  = 1;    // [1..2^32-64] (never == 0)
    index_type m_free_head = none; // index of first free entry

    //! Build a new identifier merging m_next_key and the best free
    //! index.
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

    constexpr data_array() noexcept = default;

    constexpr data_array(std::integral auto capacity) noexcept;

    constexpr ~data_array() noexcept;

    //! Try to reserve more memory than current capacity.
    //!
    //! @param capacity The new capacity. Do nothing if this @c capacity
    //! parameter is less or equal than the current @c capacity().
    //!
    //! @attention  If the `reserve()` succedded a reallocation takes
    //! place, in which case all iterators (including the end()
    //! iterator) and all references to the elements are invalidated.
    //!
    //! @attention Use `can_alloc()` to be sure `reserve()` succeed.
    bool reserve(std::integral auto capacity) noexcept;

    //! Try to grow the capacity of memory.
    //!
    //! - current capacity equals 0 then tries to reserve 8 elements.
    //! - current capacity equals size then tries to reserve capacity *
    //! 2
    //!   elements.
    //! - else tries to reserve capacity * 3 / 2 elements.
    //!
    //! @attention  If the `grow()` succedded a reallocation takes
    //! place, in which case all iterators (including the end()
    //! iterator) and all references to the elements are invalidated.
    //!
    //! @attention Use `can_alloc()` to be sure `grow()` succeed.
    template<int Num, int Denum = 1>
    bool grow() noexcept;

    //! @brief Destroy all items in the data_array but keep memory
    //!  allocation.
    //!
    //! @details Run the destructor if @c T is not trivial on
    //! outstanding
    //!  items and re-initialize the size.
    void clear() noexcept;

    //! @brief Destroy all items in the data_array and release memory.
    //!
    //! @details Run the destructor if @c T is not trivial on
    //! outstanding
    //!  items and re-initialize the size.
    void destroy() noexcept;

    //! @brief Alloc a new element.
    //!
    //! If m_max_size == m_capacity then this function will abort.
    //! Before using this function, tries @c !can_alloc() for example
    //! otherwise use the @c try_alloc function.
    //!
    //! Use @c m_free_head if not empty or use a new items from buffer
    //! (@m_item[max_used++]). The id is set to from @c next_key++ <<
    //! 32) | index to build unique identifier.
    //!
    //! @return A reference to the newly allocated element.
    template<typename... Args>
    T& alloc(Args&&... args) noexcept;

    //! @brief Alloc a new element.
    //!
    //! Use @c m_free_head if not empty or use a new items from buffer
    //! (@m_item[max_used++]). The id is set to from @c next_key++ <<
    //! 32) | index to build unique identifier.
    //!
    //! @return A pair with a boolean if the allocation success and a
    //! pointer to the newly element.
    template<typename... Args>
    T* try_alloc(Args&&... args) noexcept;

    //! @brief Free the element @c t.
    //!
    //! Internally, puts the elelent @c t entry on free list and use id
    //! to store next.
    void free(T& t) noexcept;

    //! @brief Free the element pointer by @c id.
    //!
    //! Internally, puts the element @c id entry on free list and use id
    //! to store next.
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
    //! @details Validates ID, then returns item, returns null if
    //! invalid. For cases like AI references and others where 'the
    //! thing might have been deleted out from under me.
    //!
    //! @param id Identifier to get.
    //! @return T or nullptr
    T* try_to_get(Identifier id) const noexcept;

    //! @brief Get a T directly from the index in the array.
    //!
    //! @param index The array.
    //! @return T or nullptr.
    T* try_to_get_from_pos(std::integral auto index) const noexcept;

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
    //! Loop item where @c id & 0xffffffff00000000 != 0 (i.e. items not
    //! on free list).
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
    //! Loop item where @c id & 0xffffffff00000000 != 0 (i.e. items not
    //! on free list).
    //!
    //! @return true if the paramter @c t is valid false otherwise.
    bool next(const T*& t) const noexcept;

    template<bool is_const>
    struct iterator_base {
        using iterator_concept = std::forward_iterator_tag;
        using difference_type  = std::ptrdiff_t;
        using element_type =
          std::conditional_t<is_const, const value_type, value_type>;
        using pointer   = element_type*;
        using reference = element_type&;
        using container_type =
          std::conditional_t<is_const, const this_container, this_container>;

        iterator_base() noexcept = default;

        template<typename Container>
        iterator_base(Container* self_, identifier_type id_) noexcept
            requires(!std::is_const_v<Container> && !is_const)
          : self{ self_ }
          , id{ id_ }
        {}

        template<typename Container>
        iterator_base(Container* self_, identifier_type id_) noexcept
            requires(std::is_const_v<Container> && is_const)
          : self{ self_ }
          , id{ id_ }
        {}

        reference operator*() const noexcept { return *self->try_to_get(id); }
        pointer   operator->() const noexcept { return self->try_to_get(id); }

        iterator_base& operator++() noexcept
        {
            pointer ptr     = self->try_to_get(id);
            bool    success = self->next(ptr);
            id              = success ? self->get_id(*ptr) : identifier_type{};

            return *this;
        }

        iterator_base operator++(int) noexcept
        {
            auto    old_id  = id;
            pointer ptr     = self->try_to_get(id);
            bool    success = self->next(ptr);
            id              = success ? self->get_id(*ptr) : identifier_type{};

            return iterator_base{ .self = self, .id = old_id };
        }

        auto operator<=>(const iterator_base&) const noexcept = default;

        container_type* self = {};
        identifier_type id   = { 0 };
    };

    using iterator       = iterator_base<false>;
    using const_iterator = iterator_base<true>;

    constexpr iterator       begin() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr bool       empty() const noexcept;
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

//! A ring-buffer based on a fixed size container. m_head point to
//! the first element can be dequeue while m_tail point to the first
//! constructible element in the ring.
//!
//!     --+----+----+----+----+----+--
//!       |    |    |    |    |    |
//!       |    |    |    |    |    |
//!       |    |    |    |    |    |
//!     --+----+----+----+----+----+--
//!       head                tail
//!
//!       ----->              ----->
//!       dequeue()           enqueue()
//!
//! @tparam T Any type (trivial or not).
template<typename T, typename A = allocator<new_delete_memory_resource>>
class ring_buffer
{
public:
    using value_type      = T;
    using size_type       = std::uint32_t;
    using index_type      = std::make_signed_t<size_type>;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using this_container  = ring_buffer<T, A>;
    using allocator_type  = A;

    static_assert((std::is_nothrow_constructible_v<T> ||
                   std::is_nothrow_move_constructible_v<T>) &&
                  std::is_nothrow_destructible_v<T>);

private:
    T*         buffer     = nullptr;
    index_type m_head     = 0;
    index_type m_tail     = 0;
    index_type m_capacity = 0;

    constexpr index_type advance(index_type position) const noexcept;
    constexpr index_type back(index_type position) const noexcept;

public:
    template<bool is_const>
    struct iterator_base {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::conditional_t<is_const, const T, T>;
        using pointer           = value_type*;
        using reference         = value_type&;
        using container_type =
          std::conditional_t<is_const, const this_container, this_container>;

        friend ring_buffer;

    private:
        container_type* ring{};
        index_type      i{};

        template<typename Container>
        iterator_base(Container* ring_, index_type i_) noexcept
            requires(!std::is_const_v<Container> && !is_const);

        template<typename Container>
        iterator_base(Container* ring_, index_type i_) noexcept
            requires(std::is_const_v<Container> && is_const);

    public:
        container_type* buffer() noexcept { return ring; }
        index_type      index() noexcept { return i; }

        iterator_base() noexcept = default;
        iterator_base(const iterator_base& other) noexcept;
        iterator_base& operator=(const iterator_base& other) noexcept;

        reference operator*() const noexcept;
        pointer   operator->() const noexcept;

        iterator_base& operator++() noexcept;
        iterator_base  operator++(int) noexcept;
        iterator_base& operator--() noexcept;
        iterator_base  operator--(int) noexcept;
        template<bool R>
        bool operator==(const iterator_base<R>& other) const;
        void reset() noexcept;
    };

    using iterator       = iterator_base<false>;
    using const_iterator = iterator_base<true>;

    constexpr ring_buffer() noexcept = default;
    explicit constexpr ring_buffer(std::integral auto capacity) noexcept;

    constexpr ~ring_buffer() noexcept;

    constexpr ring_buffer(const ring_buffer& rhs) noexcept;
    constexpr ring_buffer& operator=(const ring_buffer& rhs) noexcept;
    constexpr ring_buffer(ring_buffer&& rhs) noexcept;
    constexpr ring_buffer& operator=(ring_buffer&& rhs) noexcept;

    constexpr void swap(ring_buffer& rhs) noexcept;
    constexpr void clear() noexcept;
    constexpr void destroy() noexcept;
    constexpr bool reserve(std::integral auto capacity) noexcept;

    template<typename Compare>
    constexpr void sort(Compare fn) noexcept;

    template<typename... Args>
    constexpr bool emplace_head(Args&&... args) noexcept;
    template<typename... Args>
    constexpr bool emplace_tail(Args&&... args) noexcept;
    template<typename... Args>
    constexpr void force_emplace_tail(Args&&... args) noexcept;

    constexpr bool push_head(const T& item) noexcept;
    constexpr void pop_head() noexcept;
    constexpr bool push_tail(const T& item) noexcept;
    constexpr void pop_tail() noexcept;
    constexpr void erase_after(iterator not_included) noexcept;
    constexpr void erase_before(iterator not_included) noexcept;

    template<typename... Args>
    constexpr bool emplace_enqueue(Args&&... args) noexcept;

    template<typename... Args>
    constexpr reference force_emplace_enqueue(Args&&... args) noexcept;

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
    constexpr int      capacity() const noexcept;
    constexpr int      available() const noexcept;
    constexpr bool     empty() const noexcept;
    constexpr bool     full() const noexcept;
    constexpr int      index_from_begin(int index) const noexcept;
    constexpr int      head_index() const noexcept;
    constexpr int      tail_index() const noexcept;

private:
    constexpr bool make(std::integral auto capacity) noexcept;
};

//! @brief A small_string without heap allocation.
template<int length = 8>
class small_string
{
public:
    static_assert(length >= 2);

    using size_type  = small_storage_size_t<length>;
    using index_type = std::make_signed_t<size_type>;

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
    constexpr char*              data() noexcept;
    constexpr const char*        data() const noexcept;

    constexpr iterator       begin() noexcept;
    constexpr iterator       end() noexcept;
    constexpr const_iterator begin() const noexcept;
    constexpr const_iterator end() const noexcept;

    constexpr auto operator<=>(const small_string& rhs) const noexcept;

    constexpr bool operator==(const small_string<length>& rhs) const noexcept;
    constexpr bool operator!=(const small_string<length>& rhs) const noexcept;
    constexpr bool operator>(const small_string<length>& rhs) const noexcept;
    constexpr bool operator>=(const small_string<length>& rhs) const noexcept;
    constexpr bool operator<(const small_string<length>& rhs) const noexcept;
    constexpr bool operator<=(const small_string<length>& rhs) const noexcept;

    constexpr bool operator==(const std::string_view rhs) const noexcept;
    constexpr bool operator!=(const std::string_view rhs) const noexcept;
    constexpr bool operator>(const std::string_view rhs) const noexcept;
    constexpr bool operator<(const std::string_view rhs) const noexcept;
    constexpr bool operator==(const char* rhs) const noexcept;
    constexpr bool operator!=(const char* rhs) const noexcept;
    constexpr bool operator>(const char* rhs) const noexcept;
    constexpr bool operator<(const char* rhs) const noexcept;

    template<int Size>
    friend std::istream& operator>>(std::istream& is, small_string<Size>& s);
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

    using value_type = T;
    using size_type  = small_storage_size_t<length>;
    using index_type = std::make_signed_t<size_type>;

private:
    alignas(std::max_align_t) std::byte m_buffer[length * sizeof(T)];
    size_type m_size = 0;

public:
    using iterator        = T*;
    using const_iterator  = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    constexpr small_vector() noexcept = default;
    constexpr ~small_vector() noexcept;

    constexpr small_vector(std::integral auto size) noexcept;
    constexpr small_vector(std::integral auto size,
                           const_reference    value) noexcept;

    template<not_integral InputIterator>
    constexpr small_vector(InputIterator first, InputIterator last) noexcept;

    explicit constexpr small_vector(std::initializer_list<T> init) noexcept;

    constexpr small_vector(const small_vector& other) noexcept;
    constexpr small_vector& operator=(const small_vector& other) noexcept;
    constexpr small_vector(small_vector&& other) noexcept
        requires(std::is_move_constructible_v<T>);
    constexpr small_vector& operator=(small_vector&& other) noexcept
        requires(std::is_move_assignable_v<T>);

    template<not_integral InputIterator>
    constexpr void assign(InputIterator first, InputIterator last) noexcept;

    constexpr void assign(std::integral auto size,
                          const_reference    value) noexcept;

    constexpr void resize(std::integral auto capacity) noexcept;
    constexpr void resize(std::integral auto capacity,
                          const_reference    value) noexcept;
    constexpr void clear() noexcept;

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
    constexpr reference push_back(const T& arg) noexcept;

    constexpr void pop_back() noexcept;
    constexpr void swap_pop_back(std::integral auto index) noexcept;
    constexpr void swap_pop_back(const_iterator it) noexcept;

    constexpr iterator erase(std::integral auto index) noexcept;
    constexpr iterator erase(iterator to_del) noexcept;
};

/**
   A ring-buffer based on a fixed size container. m_head point to
   the first element can be dequeue while m_tail point to the first
   constructible element in the ring.

       --+----+----+----+----+----+--
         |    |    |    |    |    |
         |    |    |    |    |    |
         |    |    |    |    |    |
       --+----+----+----+----+----+--
         head                tail

         ----->              ----->
         dequeue()           enqueue()

   @tparam T Any type (trivial or not).
 */
template<typename T, int length>
class small_ring_buffer
{
public:
    static_assert(length >= 1);
    static_assert((std::is_nothrow_constructible_v<T> ||
                   std::is_nothrow_move_constructible_v<T>) &&
                  std::is_nothrow_destructible_v<T>);

    using value_type      = T;
    using size_type       = small_storage_size_t<length>;
    using index_type      = std::make_signed_t<size_type>;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using this_container  = small_ring_buffer<T, length>;

    alignas(std::alignment_of_v<std::max_align_t>)
      std::byte m_buffer[length * sizeof(T)];

private:
    index_type m_head = 0;
    index_type m_tail = 0;

    constexpr index_type advance(index_type position) const noexcept;
    constexpr index_type back(index_type position) const noexcept;

public:
    template<bool is_const>
    struct iterator_base {
    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = std::conditional_t<is_const, const T, T>;
        using pointer           = value_type*;
        using reference         = value_type&;
        using container_type =
          std::conditional_t<is_const, const this_container, this_container>;

        friend small_ring_buffer;

    private:
        container_type* ring{};
        index_type      i{};

        template<typename Container>
        iterator_base(Container* ring_, index_type i_) noexcept
            requires(!std::is_const_v<Container> && !is_const);

        template<typename Container>
        iterator_base(Container* ring_, index_type i_) noexcept
            requires(std::is_const_v<Container> && is_const);

    public:
        container_type* buffer() noexcept { return ring; }
        index_type      index() noexcept { return i; }

        iterator_base() noexcept = default;
        iterator_base(const iterator_base& other) noexcept;
        iterator_base& operator=(const iterator_base& other) noexcept;

        reference operator*() const noexcept;
        pointer   operator->() const noexcept;

        iterator_base& operator++() noexcept;
        iterator_base  operator++(int) noexcept;
        iterator_base& operator--() noexcept;
        iterator_base  operator--(int) noexcept;

        template<bool R>
        bool operator==(const iterator_base<R>& other) const;
        void reset() noexcept;
    };

    using iterator       = iterator_base<false>;
    using const_iterator = iterator_base<true>;

    constexpr small_ring_buffer() noexcept = default;
    constexpr ~small_ring_buffer() noexcept;

    constexpr small_ring_buffer(const small_ring_buffer& rhs) noexcept;
    constexpr small_ring_buffer& operator=(
      const small_ring_buffer& rhs) noexcept;
    constexpr small_ring_buffer(small_ring_buffer&& rhs) noexcept;
    constexpr small_ring_buffer& operator=(small_ring_buffer&& rhs) noexcept;

    constexpr void clear() noexcept;
    constexpr void destroy() noexcept;

    template<typename... Args>
    constexpr bool emplace_head(Args&&... args) noexcept;
    template<typename... Args>
    constexpr bool emplace_tail(Args&&... args) noexcept;

    constexpr bool push_head(const T& item) noexcept;
    constexpr void pop_head() noexcept;
    constexpr bool push_tail(const T& item) noexcept;
    constexpr void pop_tail() noexcept;
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

    constexpr unsigned   size() const noexcept;
    constexpr int        ssize() const noexcept;
    constexpr int        capacity() const noexcept;
    constexpr int        available() const noexcept;
    constexpr bool       empty() const noexcept;
    constexpr bool       full() const noexcept;
    constexpr index_type index_from_begin(
      std::integral auto index) const noexcept;
    constexpr index_type head_index() const noexcept;
    constexpr index_type tail_index() const noexcept;
};

template<typename EnumT>
class bitflags
{
    static_assert(std::is_enum_v<EnumT>,
                  "irt::flags can only be used with enum types");

public:
    using value_type      = EnumT;
    using underlying_type = typename std::make_unsigned_t<
      typename std::underlying_type_t<value_type>>;

    constexpr bitflags() noexcept = default;
    explicit constexpr bitflags(unsigned long long val) noexcept;

    explicit constexpr bitflags(std::same_as<EnumT> auto... args) noexcept;

    constexpr bitflags& set(value_type e, bool value = true) noexcept;
    constexpr bitflags& reset(value_type e) noexcept;
    constexpr bitflags& reset() noexcept;

    bool all() const noexcept;
    bool any() const noexcept;
    bool none() const noexcept;

    constexpr std::size_t size() const noexcept;
    constexpr std::size_t count() const noexcept;
    std::size_t           to_unsigned() const noexcept;

    constexpr bool operator[](value_type e) const;

private:
    static constexpr underlying_type underlying(value_type e) noexcept;

    std::bitset<underlying(value_type::Count)> m_bits;
};

// template<typename Identifier, typename A>
// class id_array

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::identifier_type
id_array<Identifier, A>::make_id(index_type key, index_type index) noexcept
{
    return static_cast<Identifier>(g_make_id(key, index));
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::index_type
id_array<Identifier, A>::make_next_key(index_type key) noexcept
{
    return g_make_next_key(key);
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::index_type
id_array<Identifier, A>::get_key(Identifier id) noexcept
{
    return g_get_key(id);
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::index_type
id_array<Identifier, A>::get_index(Identifier id) noexcept
{
    return g_get_index(id);
}

template<typename Identifier, typename A>
constexpr id_array<Identifier, A>::id_array(
  const id_array<Identifier, A>& o) noexcept
{
    if (o.m_items != nullptr) {
        m_items = reinterpret_cast<identifier_type*>(
          A::allocate(sizeof(identifier_type) * o.m_capacity));
        if (m_items != nullptr) {
            std::copy_n(o.m_items, o.m_max_used, m_items);
            m_capacity  = o.m_capacity;
            m_max_size  = o.m_max_size;
            m_max_used  = o.m_max_used;
            m_next_key  = o.m_next_key;
            m_free_head = o.m_free_head;
        }
    } else {
        destroy();
    }
}

template<typename Identifier, typename A>
constexpr id_array<Identifier, A>::id_array(
  id_array<Identifier, A>&& o) noexcept
  : m_items{ std::exchange(o.m_items, nullptr) }
  , m_max_size{ std::exchange(o.m_max_size, 0) }
  , m_max_used{ std::exchange(o.m_max_used, 0) }
  , m_capacity{ std::exchange(o.m_capacity, 0) }
  , m_next_key{ std::exchange(o.m_next_key, 1) }
  , m_free_head{ std::exchange(o.m_free_head, none) }
{}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::this_container&
id_array<Identifier, A>::operator=(id_array<Identifier, A>&& o) noexcept
{
    if (this != &o) {
        m_items     = std::exchange(o.m_items, nullptr);
        m_max_size  = std::exchange(o.m_max_size, 0);
        m_max_used  = std::exchange(o.m_max_used, 0);
        m_capacity  = std::exchange(o.m_capacity, 0);
        m_next_key  = std::exchange(o.m_next_key, 1);
        m_free_head = std::exchange(o.m_free_head, none);
    }

    return *this;
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::this_container&
id_array<Identifier, A>::operator=(const id_array<Identifier, A>& o) noexcept
{
    if (this != &o) {
        clear();

        if (o.m_items != nullptr) {
            if (m_capacity < o.m_capacity) {
                destroy();

                m_items = reinterpret_cast<identifier_type*>(
                  A::allocate(sizeof(identifier_type) * o.m_capacity));
                if (m_items == nullptr)
                    return *this;

                m_capacity = o.m_capacity;
            }

            std::copy_n(o.m_items, o.m_max_used, m_items);
            m_max_size  = o.m_max_size;
            m_max_used  = o.m_max_used;
            m_next_key  = o.m_next_key;
            m_free_head = o.m_free_head;
        } else {
            destroy();
        }
    }

    return *this;
}

template<typename Identifier, typename A>
constexpr id_array<Identifier, A>::id_array(
  std::integral auto capacity) noexcept
{
    debug::ensure(std::cmp_greater(capacity, 0));
    debug::ensure(
      std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    if (std::cmp_greater(capacity, 0) and
        std::cmp_less(capacity, std::numeric_limits<index_type>::max())) {
        m_items = reinterpret_cast<identifier_type*>(
          A::allocate(sizeof(identifier_type) * capacity));
        m_max_size  = 0;
        m_max_used  = 0;
        m_capacity  = static_cast<index_type>(capacity);
        m_next_key  = 1;
        m_free_head = none;
    }
}

template<typename Identifier, typename A>
constexpr id_array<Identifier, A>::~id_array() noexcept
{
    clear();

    if (m_items)
        A::deallocate(m_items, sizeof(identifier_type) * m_capacity);
}

template<typename Identifier, typename A>
constexpr Identifier id_array<Identifier, A>::alloc() noexcept
{
    debug::ensure(can_alloc(1));
    index_type new_index;

    if (m_free_head != none) {
        new_index = m_free_head;
        if (is_valid(m_items[m_free_head]))
            m_free_head = none;
        else
            m_free_head = get_index(m_items[m_free_head]);
    } else {
        new_index = m_max_used++;
    }

    m_items[new_index] = make_id(m_next_key, new_index);
    m_next_key         = make_next_key(m_next_key);
    ++m_max_size;

    return m_items[new_index];
}

template<typename Identifier, typename A>
constexpr std::optional<Identifier>
id_array<Identifier, A>::try_alloc() noexcept
{
    if (not can_alloc())
        return std::nullopt;

    return alloc();
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::reserve(
  std::integral auto capacity) noexcept
{
    debug::ensure(
      std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    if (std::cmp_less_equal(capacity, m_capacity))
        return true;

    if (std::cmp_greater_equal(capacity,
                               std::numeric_limits<index_type>::max()))
        return false;

    identifier_type* new_buffer = reinterpret_cast<identifier_type*>(
      A::allocate(sizeof(identifier_type) * capacity));
    if (new_buffer == nullptr)
        return false;

    std::uninitialized_copy_n(reinterpret_cast<std::byte*>(&m_items[0]),
                              sizeof(identifier_type) * m_max_used,
                              reinterpret_cast<std::byte*>(&new_buffer[0]));

    if (m_items)
        A::deallocate(m_items, sizeof(identifier_type) * m_capacity);

    m_items    = new_buffer;
    m_capacity = static_cast<index_type>(capacity);
    return true;
}

template<typename Identifier, typename A>
constexpr void id_array<Identifier, A>::free(const Identifier id) noexcept
{
    const auto index = get_index(id);

    debug::ensure(std::cmp_less_equal(0, index));
    debug::ensure(std::cmp_less(index, m_max_used));
    debug::ensure(m_items[index] == id);
    debug::ensure(is_valid(id));

    m_items[index] = static_cast<Identifier>(m_free_head);
    m_free_head    = static_cast<index_type>(index);

    --m_max_size;
}

template<typename Identifier, typename A>
constexpr void id_array<Identifier, A>::clear() noexcept
{
    m_max_size  = 0;
    m_max_used  = 0;
    m_free_head = none;
}

template<typename Identifier, typename A>
constexpr void id_array<Identifier, A>::destroy() noexcept
{
    clear();

    if (m_items)
        A::deallocate(m_items, sizeof(identifier_type) * m_capacity);

    m_items    = nullptr;
    m_capacity = 0;
}

template<typename Identifier, typename A>
template<int Num, int Denum>
constexpr bool id_array<Identifier, A>::grow() noexcept
{
    static_assert(Num > 0 and Denum > 0 and Num > Denum);

    const auto nb  = m_capacity * Num / Denum;
    const auto req = std::cmp_equal(m_capacity, nb) ? m_capacity + 8 : nb;

    return reserve(req);
}

template<typename Identifier, typename A>
constexpr std::optional<typename id_array<Identifier, A>::index_type>
id_array<Identifier, A>::get(const Identifier id) const noexcept
{
    const auto index = get_index(id);

    debug::ensure(std::cmp_less_equal(0, index));
    debug::ensure(std::cmp_less(index, m_max_used));

    return m_items[index] == id ? std::make_optional(index) : std::nullopt;
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::exists(
  const Identifier id) const noexcept
{
    const auto index = get_index(id);

    return std::cmp_greater_equal(index, 0u) and
           std::cmp_less(index, m_max_used) and m_items[index] == id;
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::identifier_type
id_array<Identifier, A>::get_from_index(std::integral auto index) const noexcept
{
    if (std::cmp_greater_equal(index, 0u) and
        std::cmp_less(index, m_max_used) and is_defined(m_items[index]) and
        std::cmp_equal(get_index(m_items[index]), index))
        return m_items[index];

    return static_cast<identifier_type>(0);
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::next(
  const Identifier*& id) const noexcept
{
    if (id) {
        auto index = get_index(*id);
        ++index;

        for (; index < m_max_used; ++index) {
            if (is_valid(m_items[index])) {
                id = &m_items[index];
                return true;
            }
        }
    } else {
        for (auto index = 0; index < m_max_used; ++index) {
            if (is_valid(m_items[index])) {
                id = &m_items[index];
                return true;
            }
        }
    }

    return false;
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::empty() const noexcept
{
    return m_max_size == 0;
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::full() const noexcept
{
    return m_free_head == none && m_max_size >= m_capacity;
}

template<typename Identifier, typename A>
constexpr unsigned id_array<Identifier, A>::size() const noexcept
{
    return m_max_size;
}

template<typename Identifier, typename A>
constexpr int id_array<Identifier, A>::ssize() const noexcept
{
    return m_max_size;
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::can_alloc(
  std::integral auto nb) const noexcept
{
    return std::cmp_greater_equal(m_capacity - m_max_size, nb);
}

template<typename Identifier, typename A>
constexpr int id_array<Identifier, A>::max_used() const noexcept
{
    return m_max_used;
}

template<typename Identifier, typename A>
constexpr int id_array<Identifier, A>::capacity() const noexcept
{
    return m_capacity;
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::index_type
id_array<Identifier, A>::next_key() const noexcept
{
    return m_next_key;
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::is_free_list_empty() const noexcept
{
    return m_free_head == none;
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::iterator
id_array<Identifier, A>::begin() noexcept
{
    for (index_type index = 0; index < m_max_used; ++index)
        if (is_valid(m_items[index]))
            return iterator(this, m_items[index]);

    return iterator(this, identifier_type{});
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::iterator
id_array<Identifier, A>::end() noexcept
{
    return iterator(this, identifier_type{});
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::const_iterator
id_array<Identifier, A>::begin() const noexcept
{
    for (index_type index = 0; m_max_used; ++index)
        if (is_valid(m_items[index]))
            return const_iterator(this, m_items[index]);

    return const_iterator(this, identifier_type{});
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::const_iterator
id_array<Identifier, A>::end() const noexcept
{
    return const_iterator(this, identifier_type{});
}

// template<typename Identifier, typename A, class... Ts>
// class id_data_array

template<typename Identifier, typename A, class... Ts>
id_data_array<Identifier, A, Ts...>::~id_data_array() noexcept
{
    do_destroy(capacity(), std::index_sequence_for<Ts...>());
}

template<typename Identifier, typename A, class... Ts>
id_data_array<Identifier, A, Ts...>::id_data_array(
  const id_data_array& other) noexcept
  : m_ids(other.m_ids)
{
    do_alloc(other.capacity(), std::index_sequence_for<Ts...>());
    do_uninitialised_copy(
      other.m_col, other.capacity(), std::index_sequence_for<Ts...>());
}

template<typename Identifier, typename A, class... Ts>
id_data_array<Identifier, A, Ts...>::id_data_array(
  id_data_array&& other) noexcept
  : m_ids(std::move(other.m_ids))
  , m_col(std::move(other.m_col))
{
    do_reset(other.m_col, std::index_sequence_for<Ts...>());
}

template<typename Identifier, typename A, class... Ts>
auto id_data_array<Identifier, A, Ts...>::operator=(
  const id_data_array& other) noexcept -> id_data_array&
{
    if (this != &other) {
        do_destroy(std::index_sequence_for<Ts...>());
        do_alloc(other.capacity(), std::index_sequence_for<Ts...>());
        do_uninitialised_copy(
          other.m_col, other.capacity(), std::index_sequence_for<Ts...>());
    }

    return *this;
}

template<typename Identifier, typename A, class... Ts>
auto id_data_array<Identifier, A, Ts...>::operator=(
  id_data_array&& other) noexcept -> id_data_array&
{
    if (this != &other) {
        m_ids.destroy();
        do_destroy(std::index_sequence_for<Ts...>());
        m_ids = std::move(other.m_ids);
        std::swap(m_col, other.m_col);
        do_reset(other.m_col, std::index_sequence_for<Ts...>());
    }

    return *this;
}

template<typename Identifier, typename A, class... Ts>
auto id_data_array<Identifier, A, Ts...>::alloc() noexcept
  -> id_data_array<Identifier, A, Ts...>::identifier_type
{
    irt::debug::ensure(m_ids.can_alloc(1));

    return m_ids.alloc();
}

template<typename Identifier, typename A, class... Ts>
template<typename Function>
auto id_data_array<Identifier, A, Ts...>::alloc(Function&& fn) noexcept
  -> id_data_array<Identifier, A, Ts...>::identifier_type
{
    irt::debug::ensure(m_ids.can_alloc(1));

    const auto id = m_ids.alloc();

    do_call_alloc_fn(fn, id, std::index_sequence_for<Ts...>());

    return id;
}

template<typename Identifier, typename A, class... Ts>
void id_data_array<Identifier, A, Ts...>::free(
  const identifier_type id) noexcept
{
    if (m_ids.exists(id))
        m_ids.free(id);
}

template<typename Identifier, typename A, class... Ts>
template<std::size_t Index>
auto& id_data_array<Identifier, A, Ts...>::get() noexcept
{
    return std::get<Index>(m_col);
}

template<typename Identifier, typename A, class... Ts>
template<typename Type>
auto& id_data_array<Identifier, A, Ts...>::get() noexcept
{
    return std::get<Type*>(m_col);
}

template<typename Identifier, typename A, class... Ts>
template<std::size_t Index>
auto& id_data_array<Identifier, A, Ts...>::get(
  const identifier_type id) noexcept
{
    debug::ensure(m_ids.exists(id));

    return std::get<Index>(m_col)[get_index(id)];
}

template<typename Identifier, typename A, class... Ts>
template<typename Type>
auto& id_data_array<Identifier, A, Ts...>::get(
  const identifier_type id) noexcept
{
    debug::ensure(m_ids.exists(id));

    return std::get<Type*>(m_col)[get_index(id)];
}

template<typename Identifier, typename A, class... Ts>
template<std::size_t Index>
auto& id_data_array<Identifier, A, Ts...>::get() const noexcept
{
    return std::get<Index>(m_col);
}

template<typename Identifier, typename A, class... Ts>
template<typename Type>
auto& id_data_array<Identifier, A, Ts...>::get() const noexcept
{
    return std::get<Type*>(m_col);
}

template<typename Identifier, typename A, class... Ts>
template<std::size_t Index>
auto& id_data_array<Identifier, A, Ts...>::get(
  const identifier_type id) const noexcept
{
    debug::ensure(m_ids.exists(id));

    return std::get<Index>(m_col)[get_index(id)];
}

template<typename Identifier, typename A, class... Ts>
template<typename Type>
auto& id_data_array<Identifier, A, Ts...>::get(
  const identifier_type id) const noexcept
{
    debug::ensure(m_ids.exists(id));

    return std::get<Type*>(m_col)[get_index(id)];
}

template<typename Identifier, typename A, class... Ts>
template<typename Index, typename Function>
void id_data_array<Identifier, A, Ts...>::if_exists_do(const identifier_type id,
                                                       Function&& fn) noexcept
{
    if (m_ids.exists(id))
        fn(id, std::get<Index*>(m_col)[get_index(id)]);
}

template<typename Identifier, typename A, class... Ts>
template<typename Index, typename Function>
void id_data_array<Identifier, A, Ts...>::for_each(Function&& fn) noexcept
{
    for (const auto id : m_ids)
        fn(id, std::get<Index*>(m_col)[get_index(id)]);
}

template<typename Identifier, typename A, class... Ts>
template<typename Function>
void id_data_array<Identifier, A, Ts...>::for_each(Function&& fn) noexcept
{
    for (const auto id : m_ids)
        do_call_fn(fn, id, std::index_sequence_for<Ts...>());
}

template<typename Identifier, typename A, class... Ts>
template<typename Index, typename Function>
void id_data_array<Identifier, A, Ts...>::for_each(Function&& fn) const noexcept
{
    if constexpr (std::is_integral_v<Index>) {
        for (const auto id : m_ids)
            fn(id, std::get<Index>(m_col)[get_index(id)]);
    } else {
        for (const auto id : m_ids)
            fn(id, std::get<Index*>(m_col)[get_index(id)]);
    }
}

template<typename Identifier, typename A, class... Ts>
template<typename Function>
void id_data_array<Identifier, A, Ts...>::for_each(Function&& fn) const noexcept
{
    for (const auto id : m_ids)
        do_call_fn(fn, id, std::index_sequence_for<Ts...>());
}

template<typename Identifier, typename A, class... Ts>
template<typename Function>
void id_data_array<Identifier, A, Ts...>::for_each_id(Function&& fn) noexcept
{
    for (const auto id : m_ids)
        fn(id);
}

template<typename Identifier, typename A, class... Ts>
template<typename Function>
void id_data_array<Identifier, A, Ts...>::for_each_id(
  Function&& fn) const noexcept
{
    for (const auto id : m_ids)
        fn(id);
}

template<typename Identifier, typename A, class... Ts>
constexpr typename id_data_array<Identifier, A, Ts...>::iterator
id_data_array<Identifier, A, Ts...>::begin() noexcept
{
    return m_ids.begin();
}

template<typename Identifier, typename A, class... Ts>
constexpr typename id_data_array<Identifier, A, Ts...>::const_iterator
id_data_array<Identifier, A, Ts...>::begin() const noexcept
{
    return m_ids.begin();
}

template<typename Identifier, typename A, class... Ts>
constexpr typename id_data_array<Identifier, A, Ts...>::iterator
id_data_array<Identifier, A, Ts...>::end() noexcept
{
    return m_ids.end();
}

template<typename Identifier, typename A, class... Ts>
constexpr typename id_data_array<Identifier, A, Ts...>::const_iterator
id_data_array<Identifier, A, Ts...>::end() const noexcept
{
    return m_ids.end();
}

template<typename Identifier, typename A, class... Ts>
void id_data_array<Identifier, A, Ts...>::clear() noexcept
{
    m_ids.clear();
}

template<typename Identifier, typename A, class... Ts>
bool id_data_array<Identifier, A, Ts...>::reserve(
  std::integral auto len) noexcept
{
    if (std::cmp_less(capacity(), len)) {
        if (not do_resize(capacity(), len, std::index_sequence_for<Ts...>()))
            return false;
        return m_ids.reserve(len);
    }

    return true;
}

template<typename Identifier, typename A, class... Ts>
template<int Num, int Denum>
bool id_data_array<Identifier, A, Ts...>::grow() noexcept
{
    static_assert(Num > 0 and Denum > 0 and Num > Denum);

    const auto nb  = capacity() * Num / Denum;
    const auto req = std::cmp_equal(capacity(), nb) ? capacity() + 8 : nb;

    return reserve(req);
}

template<typename Identifier, typename A, class... Ts>
bool id_data_array<Identifier, A, Ts...>::exists(
  const identifier_type id) const noexcept
{
    return m_ids.exists(id);
}

template<typename Identifier, typename A, class... Ts>
typename id_data_array<Identifier, A, Ts...>::identifier_type
id_data_array<Identifier, A, Ts...>::get_id(
  std::integral auto idx) const noexcept
{
    return m_ids.get_from_index(idx);
}

template<typename Identifier, typename A, class... Ts>
bool id_data_array<Identifier, A, Ts...>::can_alloc(
  std::integral auto nb) const noexcept
{
    return m_ids.can_alloc(nb);
}

template<typename Identifier, typename A, class... Ts>
bool id_data_array<Identifier, A, Ts...>::empty() const noexcept
{
    return m_ids.size() == 0;
}

template<typename Identifier, typename A, class... Ts>
unsigned id_data_array<Identifier, A, Ts...>::size() const noexcept
{
    return m_ids.size();
}

template<typename Identifier, typename A, class... Ts>
int id_data_array<Identifier, A, Ts...>::ssize() const noexcept
{
    return m_ids.ssize();
}

template<typename Identifier, typename A, class... Ts>
unsigned id_data_array<Identifier, A, Ts...>::capacity() const noexcept
{
    return m_ids.capacity();
}

// template<typename T, typename Identifier, typename A>
// class data_array;

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::identifier_type
data_array<T, Identifier, A>::make_id(index_type key, index_type index) noexcept
{
    return static_cast<identifier_type>(g_make_id(key, index));
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::index_type
data_array<T, Identifier, A>::make_next_key(index_type key) noexcept
{
    return g_make_next_key(key);
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::index_type
data_array<T, Identifier, A>::get_key(Identifier id) noexcept
{
    return g_get_key(id);
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::index_type
data_array<T, Identifier, A>::get_index(Identifier id) noexcept
{
    return g_get_index(id);
}

template<typename T, typename Identifier, typename A>
constexpr data_array<T, Identifier, A>::data_array(
  std::integral auto capacity) noexcept
{
    debug::ensure(std::cmp_greater(capacity, 0));
    debug::ensure(
      std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    m_items     = reinterpret_cast<item*>(A::allocate(sizeof(item) * capacity));
    m_max_size  = 0;
    m_max_used  = 0;
    m_capacity  = static_cast<index_type>(capacity);
    m_next_key  = 1;
    m_free_head = none;
}

template<typename T, typename Identifier, typename A>
constexpr data_array<T, Identifier, A>::~data_array() noexcept
{
    clear();

    if (m_items)
        A::deallocate(m_items, sizeof(item) * m_capacity);
}

template<typename T, typename Identifier, typename A>
bool data_array<T, Identifier, A>::reserve(std::integral auto capacity) noexcept
{
    static_assert(std::is_trivial_v<T> or std::is_move_constructible_v<T> or
                  std::is_copy_constructible_v<T>);

    debug::ensure(
      std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    if (std::cmp_equal(capacity, 0) or
        std::cmp_less_equal(capacity, m_capacity))
        return false;

    item* new_buffer =
      reinterpret_cast<item*>(A::allocate(sizeof(item) * capacity));
    if (new_buffer == nullptr)
        return false;

    if constexpr (std::is_trivial_v<T>) {
        std::uninitialized_copy_n(reinterpret_cast<std::byte*>(&m_items[0]),
                                  sizeof(item) * m_max_used,
                                  reinterpret_cast<std::byte*>(&new_buffer[0]));
    } else if constexpr (std::is_move_constructible_v<T>) {
        for (index_type i = 0; i != m_max_used; ++i) {
            if (is_valid(m_items[i].id)) {
                std::construct_at(&new_buffer[i].item,
                                  std::move(m_items[i].item));
                new_buffer[i].id = m_items[i].id;
            } else {
                std::uninitialized_copy_n(
                  reinterpret_cast<std::byte*>(&m_items[i]),
                  sizeof(item),
                  reinterpret_cast<std::byte*>(&new_buffer[i]));
            }
        }
    } else {
        for (index_type i = 0; i != m_max_used; ++i) {
            if (is_valid(m_items[i].id)) {
                std::construct_at(&new_buffer[i].item, m_items[i].item);
                new_buffer[i].id = m_items[i].id;
            } else {
                std::uninitialized_copy_n(
                  reinterpret_cast<std::byte*>(&m_items[i]),
                  sizeof(item),
                  reinterpret_cast<std::byte*>(&new_buffer[i]));
            }
        }
    }

    if (m_items)
        A::deallocate(m_items, sizeof(item) * m_capacity);

    m_items    = new_buffer;
    m_capacity = static_cast<index_type>(capacity);
    return true;
}

template<typename T, typename Identifier, typename A>
template<int Num, int Denum>
bool data_array<T, Identifier, A>::grow() noexcept
{
    static_assert(Num > 0 and Denum > 0 and Num > Denum);

    const auto nb  = m_capacity * Num / Denum;
    const auto req = std::cmp_equal(nb, m_capacity) ? m_capacity + 8 : nb;

    return reserve(req);
}

template<typename T, typename Identifier, typename A>
void data_array<T, Identifier, A>::clear() noexcept
{
    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (index_type i = 0; i != m_max_used; ++i) {
            if (is_valid(m_items[i].id)) {
                if constexpr (not std::is_trivially_destructible_v<T>) {
                    std::destroy_at(&m_items[i].item);
                }
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
    m_free_head = none;
}

template<typename T, typename Identifier, typename A>
void data_array<T, Identifier, A>::destroy() noexcept
{
    clear();

    if (m_items)
        A::deallocate(m_items, sizeof(item) * m_capacity);

    m_items    = nullptr;
    m_capacity = 0;
}

template<typename T, typename Identifier, typename A>
template<typename... Args>
typename data_array<T, Identifier, A>::value_type&
data_array<T, Identifier, A>::alloc(Args&&... args) noexcept
{
    debug::ensure(can_alloc(1));

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

template<typename T, typename Identifier, typename A>
template<typename... Args>
T* data_array<T, Identifier, A>::try_alloc(Args&&... args) noexcept
{
    if (!can_alloc(1))
        return nullptr;

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

    return &m_items[new_index].item;
}

template<typename T, typename Identifier, typename A>
void data_array<T, Identifier, A>::free(T& t) noexcept
{
    auto id    = get_id(t);
    auto index = get_index(id);

    debug::ensure(&m_items[index] == static_cast<void*>(&t));
    debug::ensure(m_items[index].id == id);
    debug::ensure(is_valid(id));

    std::destroy_at(&m_items[index].item);

    m_items[index].id = static_cast<Identifier>(m_free_head);
    m_free_head       = static_cast<index_type>(index);

    --m_max_size;
}

template<typename T, typename Identifier, typename A>
void data_array<T, Identifier, A>::free(Identifier id) noexcept
{
    auto index = get_index(id);

    if (m_items[index].id == id && is_valid(id)) {
        std::destroy_at(&m_items[index].item);

        m_items[index].id = static_cast<Identifier>(m_free_head);
        m_free_head       = static_cast<index_type>(index);

        --m_max_size;
    }
}

template<typename T, typename Identifier, typename A>
Identifier data_array<T, Identifier, A>::get_id(const T* t) const noexcept
{
    debug::ensure(t != nullptr);

    auto* ptr = reinterpret_cast<const item*>(t);
    return ptr->id;
}

template<typename T, typename Identifier, typename A>
Identifier data_array<T, Identifier, A>::get_id(const T& t) const noexcept
{
    auto* ptr = reinterpret_cast<const item*>(&t);
    return ptr->id;
}

template<typename T, typename Identifier, typename A>
T& data_array<T, Identifier, A>::get(Identifier id) noexcept
{
    debug::ensure(get_key(id) and std::cmp_greater_equal(get_index(id), 0) and
                  std::cmp_less(get_index(id), m_max_used) and
                  m_items[get_index(id)].id == id);

    return m_items[get_index(id)].item;
}

template<typename T, typename Identifier, typename A>
const T& data_array<T, Identifier, A>::get(Identifier id) const noexcept
{
    debug::ensure(get_key(id) and std::cmp_greater_equal(get_index(id), 0) and
                  std::cmp_less(get_index(id), m_max_used) and
                  m_items[get_index(id)].id == id);

    return m_items[get_index(id)].item;
}

template<typename T, typename Identifier, typename A>
T* data_array<T, Identifier, A>::try_to_get(Identifier id) const noexcept
{
    if (get_key(id)) {
        auto index = get_index(id);
        if (std::cmp_greater_equal(index, 0) &&
            std::cmp_less(index, m_max_used) && m_items[index].id == id)
            return &m_items[index].item;
    }

    return nullptr;
}

template<typename T, typename Identifier, typename A>
T* data_array<T, Identifier, A>::try_to_get_from_pos(
  std::integral auto index) const noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, m_max_used));

    if (is_valid(m_items[index].id))
        return &m_items[index].item;

    return nullptr;
}

template<typename T, typename Identifier, typename A>
bool data_array<T, Identifier, A>::next(T*& t) noexcept
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

template<typename T, typename Identifier, typename A>
bool data_array<T, Identifier, A>::next(const T*& t) const noexcept
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

template<typename T, typename Identifier, typename A>
constexpr bool data_array<T, Identifier, A>::empty() const noexcept
{
    return m_max_size == 0;
}

template<typename T, typename Identifier, typename A>
constexpr bool data_array<T, Identifier, A>::full() const noexcept
{
    return m_free_head == none && m_max_used == m_capacity;
}

template<typename T, typename Identifier, typename A>
constexpr unsigned data_array<T, Identifier, A>::size() const noexcept
{
    return static_cast<unsigned>(m_max_size);
}

template<typename T, typename Identifier, typename A>
constexpr int data_array<T, Identifier, A>::ssize() const noexcept
{
    return static_cast<int>(m_max_size);
}

template<typename T, typename Identifier, typename A>
constexpr bool data_array<T, Identifier, A>::can_alloc(
  std::integral auto nb) const noexcept
{
    return std::cmp_greater_equal(m_capacity - m_max_size, nb);
}

template<typename T, typename Identifier, typename A>
constexpr bool data_array<T, Identifier, A>::can_alloc() const noexcept
{
    return std::cmp_greater(m_capacity, m_max_size);
}

template<typename T, typename Identifier, typename A>
constexpr int data_array<T, Identifier, A>::max_size() const noexcept
{
    return static_cast<int>(m_max_size);
}

template<typename T, typename Identifier, typename A>
constexpr int data_array<T, Identifier, A>::max_used() const noexcept
{
    return static_cast<int>(m_max_used);
}

template<typename T, typename Identifier, typename A>
constexpr int data_array<T, Identifier, A>::capacity() const noexcept
{
    return static_cast<int>(m_capacity);
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::index_type
data_array<T, Identifier, A>::next_key() const noexcept
{
    return m_next_key;
}

template<typename T, typename Identifier, typename A>
constexpr bool data_array<T, Identifier, A>::is_free_list_empty() const noexcept
{
    return m_free_head == none;
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::iterator
data_array<T, Identifier, A>::begin() noexcept
{
    for (index_type index = 0; index < m_max_used; ++index)
        if (is_valid(m_items[index].id))
            return iterator(this, m_items[index].id);

    return iterator(this, identifier_type{});
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::iterator
data_array<T, Identifier, A>::end() noexcept
{
    return iterator(this, identifier_type{});
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::const_iterator
data_array<T, Identifier, A>::begin() const noexcept
{
    for (index_type index = 0; index < m_max_used; ++index)
        if (is_valid(m_items[index].id))
            return const_iterator(this, m_items[index].id);

    return const_iterator(this, identifier_type{});
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::const_iterator
data_array<T, Identifier, A>::end() const noexcept
{
    return const_iterator(this, identifier_type{});
}

//
// implementation
// vector<T, A>
//

template<typename T, typename A>
constexpr bool vector<T, A>::make(std::integral auto capacity) noexcept
{
    debug::ensure(std::cmp_greater(capacity, 0));
    debug::ensure(std::cmp_less(sizeof(T) * capacity,
                                std::numeric_limits<index_type>::max()));

    if (std::cmp_greater(capacity, 0)) {
        if (auto* ret = A::allocate(sizeof(T) * capacity)) {
            m_data     = reinterpret_cast<T*>(ret);
            m_size     = static_cast<index_type>(0);
            m_capacity = static_cast<index_type>(capacity);
        }
    }

    return m_data != nullptr;
}

template<typename T, typename A>
constexpr bool vector<T, A>::make(std::integral auto capacity,
                                  std::integral auto size) noexcept
{
    debug::ensure(std::cmp_greater(capacity, 0));
    debug::ensure(std::cmp_greater_equal(size, 0));
    debug::ensure(std::cmp_greater_equal(capacity, size));
    debug::ensure(std::cmp_less(sizeof(T) * capacity,
                                std::numeric_limits<index_type>::max()));

    static_assert(std::is_constructible_v<T>,
                  "T must be nothrow or trivially default constructible");

    if (make(capacity)) {
        if (std::cmp_greater_equal(size, capacity)) {
            m_size     = static_cast<index_type>(capacity);
            m_capacity = static_cast<index_type>(capacity);
        } else {
            m_size     = static_cast<index_type>(size);
            m_capacity = static_cast<index_type>(capacity);
        }

        std::uninitialized_value_construct_n(m_data, m_size);
        // std::uninitialized_default_construct_n(m_data, size);
    }

    return m_data != nullptr;
}

template<typename T, typename A>
constexpr bool vector<T, A>::make(std::integral auto capacity,
                                  std::integral auto size,
                                  const T&           default_value) noexcept
{
    debug::ensure(std::cmp_greater(capacity, 0));
    debug::ensure(std::cmp_greater_equal(size, 0));
    debug::ensure(std::cmp_greater_equal(capacity, size));
    debug::ensure(std::cmp_less(sizeof(T) * capacity,
                                std::numeric_limits<index_type>::max()));

    static_assert(std::is_copy_constructible_v<T>,
                  "T must be nothrow or trivially default constructible");

    if (make(capacity)) {
        if (std::cmp_greater_equal(size, capacity)) {
            m_size     = static_cast<index_type>(capacity);
            m_capacity = static_cast<index_type>(capacity);
        } else {
            m_size     = static_cast<index_type>(size);
            m_capacity = static_cast<index_type>(capacity);
        }

        std::uninitialized_fill_n(m_data, m_size, default_value);
        // std::uninitialized_default_construct_n(m_data, size);
    }

    return m_data != nullptr;
}

template<typename T, typename A>
inline vector<T, A>::vector(std::integral auto size) noexcept
{
    make(size, size);
}

template<typename T, typename A>
inline vector<T, A>::vector(std::integral auto capacity,
                            const reserve_tag) noexcept
{
    make(capacity);
}

template<typename T, typename A>
inline vector<T, A>::vector(std::integral auto size,
                            const_reference    value) noexcept
{
    make(size, size, value);
}

template<typename T, typename A>
inline vector<T, A>::vector(std::integral auto capacity,
                            std::integral auto size,
                            const T&           default_value) noexcept
{
    make(capacity, size, default_value);
}

template<typename T, typename A>
inline vector<T, A>::vector(std::initializer_list<T> init) noexcept
{
    assign(init.begin(), init.end());
}

template<typename T, typename A>
template<not_integral InputIterator>
inline vector<T, A>::vector(InputIterator first, InputIterator last) noexcept
{
    assign(first, last);
}

template<typename T, typename A>
inline vector<T, A>::~vector() noexcept
{
    destroy();
}

template<typename T, typename A>
inline constexpr vector<T, A>::vector(const vector<T, A>& other) noexcept
  : m_data(nullptr)
  , m_size(0)
  , m_capacity(0)
{
    reserve(other.m_capacity);

    std::uninitialized_copy_n(other.m_data, other.m_size, m_data);
    m_size     = other.m_size;
    m_capacity = other.m_capacity;
}

template<typename T, typename A>
inline constexpr vector<T, A>& vector<T, A>::operator=(
  const vector& other) noexcept
{
    if (this != &other) {
        clear();
        reserve(other.m_capacity);
        std::uninitialized_copy_n(other.m_data, other.m_size, m_data);
        m_size     = other.m_size;
        m_capacity = other.m_capacity;
    }

    return *this;
}

template<typename T, typename A>
inline constexpr vector<T, A>::vector(vector&& other) noexcept
  : m_data(std::exchange(other.m_data, nullptr))
  , m_size(std::exchange(other.m_size, 0))
  , m_capacity(std::exchange(other.m_capacity, 0))
{}

template<typename T, typename A>
inline constexpr vector<T, A>& vector<T, A>::operator=(vector&& other) noexcept
{
    if (this != &other) {
        destroy();
        m_data     = std::exchange(other.m_data, nullptr);
        m_size     = std::exchange(other.m_size, 0);
        m_capacity = std::exchange(other.m_capacity, 0);
    }

    return *this;
}

template<typename T, typename A>
template<not_integral InputIterator>
inline constexpr void vector<T, A>::assign(InputIterator first,
                                           InputIterator last) noexcept
{
    clear();

    const auto distance = std::distance(first, last);
    if (std::cmp_less(m_capacity, distance))
        reserve(distance);

    m_size = static_cast<size_type>(distance);
    std::uninitialized_copy_n(first, m_size, data());
}

template<typename T, typename A>
inline constexpr void vector<T, A>::assign(std::integral auto size,
                                           const_reference    value) noexcept
{
    clear();

    if (std::cmp_less(m_capacity, size))
        reserve(size);

    m_size = static_cast<size_type>(size);
    std::uninitialized_fill_n(data(), m_size, value);
}

template<typename T, typename A>
constexpr typename vector<T, A>::iterator vector<T, A>::insert(
  const_iterator  it,
  const_reference value) noexcept
{
    if (it == cend()) {
        push_back(value);
        return begin() + m_size - 1;
    }

    const auto distance = std::distance(cbegin(), it);

    if (m_size < m_capacity) {
        std::move_backward(
          begin() + distance, begin() + m_size, begin() + m_size);
        std::construct_at(begin() + distance, std::move(value));
        ++m_size;
    } else {
        const auto new_capacity = compute_new_capacity(m_capacity * 2);
        if (auto* ret = A::allocate(sizeof(T) * new_capacity)) {
            auto new_data = reinterpret_cast<T*>(ret);
            std::uninitialized_move_n(m_data, distance, new_data);
            std::construct_at(new_data + distance, value);
            std::uninitialized_move_n(
              m_data + distance, m_size - distance, new_data + distance + 1);

            A::deallocate(m_data, m_capacity * sizeof(T));
            m_data     = new_data;
            m_capacity = new_capacity;
            ++m_size;
        }
    }

    return begin() + distance;
}

template<typename T, typename A>
constexpr typename vector<T, A>::iterator vector<T, A>::insert(
  const_iterator   it,
  rvalue_reference value) noexcept
{
    if (it == cend()) {
        push_back(std::move(value));
        return begin() + m_size - 1;
    }

    const auto distance = std::distance(cbegin(), it);

    if (m_size < m_capacity) {
        std::move_backward(
          begin() + distance, begin() + m_size, begin() + m_size);
        std::construct_at(begin() + distance, std::move(value));
        ++m_size;
    } else {
        const auto new_capacity = compute_new_capacity(m_capacity * 2);
        if (auto* ret = A::allocate(sizeof(T) * new_capacity)) {
            auto new_data = reinterpret_cast<T*>(ret);
            std::uninitialized_move_n(m_data, distance, new_data);
            std::construct_at(new_data + distance, std::move(value));
            std::uninitialized_move_n(
              m_data + distance, m_size - distance, new_data + distance + 1);

            A::deallocate(m_data, m_capacity * sizeof(T));
            m_data     = new_data;
            m_capacity = new_capacity;
            ++m_size;
        }
    }

    return begin() + distance;
}

template<typename T, typename A>
inline void vector<T, A>::destroy() noexcept
{
    clear();

    if (m_data)
        A::deallocate(m_data, m_capacity * sizeof(T));

    m_data     = nullptr;
    m_size     = 0;
    m_capacity = 0;
}

template<typename T, typename A>
inline constexpr void vector<T, A>::clear() noexcept
{
    std::destroy_n(data(), m_size);

    m_size = 0;
}

template<typename T, typename A>
inline constexpr void vector<T, A>::swap(vector<T, A>& other) noexcept
{
    std::swap(m_data, other.m_data);
    std::swap(m_size, other.m_size);
    std::swap(m_capacity, other.m_capacity);
}

template<typename T, typename A>
bool vector<T, A>::resize(std::integral auto size) noexcept
{
    static_assert(std::is_default_constructible_v<T> ||
                    std::is_trivially_default_constructible_v<T>,
                  "T must be default or trivially default constructible to use "
                  "resize() function");

    debug::ensure(std::cmp_less(size, std::numeric_limits<index_type>::max()));

    if (std::cmp_equal(size, m_size))
        return true;

    if (std::cmp_less(size, m_size)) {
        std::destroy_n(data() + size, m_size - size);
    } else {
        if (std::cmp_greater(size, m_capacity)) {
            if (!reserve(compute_new_capacity(static_cast<index_type>(size))))
                return false;
        }

        std::uninitialized_value_construct_n(data() + m_size, size - m_size);
    }

    m_size = static_cast<index_type>(size);

    return true;
}

template<typename T, typename A>
bool vector<T, A>::resize(std::integral auto size,
                          const_reference    value) noexcept
{
    static_assert(std::is_default_constructible_v<T> ||
                    std::is_trivially_default_constructible_v<T>,
                  "T must be default or trivially default constructible to use "
                  "resize() function");

    debug::ensure(std::cmp_less(size, std::numeric_limits<index_type>::max()));

    if (std::cmp_equal(size, m_size))
        return true;

    if (std::cmp_less(size, m_size)) {
        std::destroy_n(data() + size, m_size - size);
    } else {
        if (std::cmp_greater(size, m_capacity)) {
            if (!reserve(compute_new_capacity(static_cast<index_type>(size))))
                return false;
        }

        std::uninitialized_fill_n(data() + m_size, size - m_size, value);
    }

    m_size = static_cast<index_type>(size);

    return true;
}

template<typename T, typename A>
bool vector<T, A>::reserve(std::integral auto new_capacity) noexcept
{
    debug::ensure(
      std::cmp_less(new_capacity, std::numeric_limits<index_type>::max()));

    if (std::cmp_greater(new_capacity, m_capacity)) {
        T* new_data =
          reinterpret_cast<T*>(A::allocate(sizeof(T) * new_capacity));
        if (!new_data)
            return false;

        if constexpr (std::is_move_constructible_v<T>)
            std::uninitialized_move_n(data(), m_size, new_data);
        else
            std::uninitialized_copy_n(data(), m_size, new_data);

        if (m_data)
            A::deallocate(m_data, sizeof(T) * m_capacity);

        m_data     = new_data;
        m_capacity = static_cast<index_type>(new_capacity);
    }

    return true;
}

template<typename T, typename A>
template<int Num, int Denum>
bool vector<T, A>::grow() noexcept
{
    static_assert(Num > 0 and Denum > 0 and Num > Denum);

    const auto nb  = capacity() * Num / Denum;
    const auto req = std::cmp_equal(nb, capacity()) ? capacity() + 8 : nb;

    return reserve(req);
}

template<typename T, typename A>
inline constexpr T* vector<T, A>::data() noexcept
{
    return m_data;
}

template<typename T, typename A>
inline constexpr const T* vector<T, A>::data() const noexcept
{
    return m_data;
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::reference vector<T, A>::front() noexcept
{
    debug::ensure(m_size > 0);
    return m_data[0];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_reference vector<T, A>::front()
  const noexcept
{
    debug::ensure(m_size > 0);
    return m_data[0];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::reference vector<T, A>::back() noexcept
{
    debug::ensure(m_size > 0);
    return m_data[m_size - 1];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_reference vector<T, A>::back()
  const noexcept
{
    debug::ensure(m_size > 0);
    return m_data[m_size - 1];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::reference vector<T, A>::operator[](
  std::integral auto index) noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_reference
vector<T, A>::operator[](std::integral auto index) const noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::iterator vector<T, A>::begin() noexcept
{
    return data();
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_iterator vector<T, A>::begin()
  const noexcept
{
    return data();
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_iterator vector<T, A>::cbegin()
  const noexcept
{
    return data();
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::iterator vector<T, A>::end() noexcept
{
    return data() + m_size;
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_iterator vector<T, A>::end()
  const noexcept
{
    return data() + m_size;
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_iterator vector<T, A>::cend()
  const noexcept
{
    return data() + m_size;
}

template<typename T, typename A>
inline constexpr unsigned vector<T, A>::size() const noexcept
{
    return static_cast<unsigned>(m_size);
}

template<typename T, typename A>
inline constexpr int vector<T, A>::ssize() const noexcept
{
    return m_size;
}

template<typename T, typename A>
inline constexpr int vector<T, A>::capacity() const noexcept
{
    return static_cast<int>(m_capacity);
}

template<typename T, typename A>
inline constexpr bool vector<T, A>::empty() const noexcept
{
    return m_size == 0;
}

template<typename T, typename A>
inline constexpr bool vector<T, A>::full() const noexcept
{
    return m_size >= m_capacity;
}

template<typename T, typename A>
inline constexpr bool vector<T, A>::can_alloc(
  std::integral auto number) const noexcept
{
    return std::cmp_greater_equal(m_capacity - m_size, number);
}

template<typename T, typename A>
inline constexpr int vector<T, A>::find(const T& t) const noexcept
{
    for (auto i = 0, e = ssize(); i != e; ++i)
        if (m_data[i] == t)
            return i;

    return m_size;
}

template<typename T, typename A>
template<typename... Args>
inline constexpr typename vector<T, A>::reference vector<T, A>::emplace_back(
  Args&&... args) noexcept
{
    static_assert(std::is_constructible_v<T, Args...> ||
                    std::is_nothrow_constructible_v<T, Args...>,
                  "T must but trivially or nothrow constructible from this "
                  "argument(s)");

    if (m_size >= m_capacity)
        reserve(compute_new_capacity(m_size + 1));

    std::construct_at(data() + m_size, std::forward<Args>(args)...);

    ++m_size;

    return data()[m_size - 1];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::reference vector<T, A>::push_back(
  const T& arg) noexcept
{
    if (m_size >= m_capacity)
        reserve(compute_new_capacity(m_size + 1));

    std::construct_at(data() + m_size, arg);

    ++m_size;

    return data()[m_size - 1];
}

template<typename T, typename A>
inline constexpr void vector<T, A>::pop_back() noexcept
{
    static_assert(std::is_nothrow_destructible_v<T> ||
                    std::is_trivially_destructible_v<T>,
                  "T must be nothrow or trivially destructible to use "
                  "pop_back() function");

    debug::ensure(m_size);

    if (m_size) {
        std::destroy_at(data() + m_size - 1);
        --m_size;
    }
}

template<typename T, typename A>
inline constexpr void vector<T, A>::swap_pop_back(
  std::integral auto index) noexcept
{
    debug::ensure(std::cmp_less(index, m_size));

    if (std::cmp_equal(index, m_size - 1)) {
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

template<typename T, typename A>
inline constexpr typename vector<T, A>::iterator vector<T, A>::erase(
  iterator it) noexcept
{
    debug::ensure(it >= data() && it < data() + m_size);

    if (it == end())
        return end();

    iterator prev = it == begin() ? end() : it - 1;
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

    return prev;
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::iterator vector<T, A>::erase(
  iterator first,
  iterator last) noexcept
{
    debug::ensure(first >= data() && first < data() + m_size && last > first &&
                  last <= data() + m_size);

    iterator prev = first == begin() ? end() : first - 1;
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

    m_size -= static_cast<int>(count);

    return prev;
}

template<typename T, typename A>
inline constexpr int vector<T, A>::index_from_ptr(const T* data) const noexcept
{
    debug::ensure(is_iterator_valid(const_iterator(data)));

    const auto off = data - m_data;

    debug::ensure(0 <= off && off < INT32_MAX);

    return static_cast<int>(off);
}

template<typename T, typename A>
inline constexpr bool vector<T, A>::is_iterator_valid(
  const_iterator it) const noexcept
{
    return it >= m_data && it < m_data + m_size;
}

template<typename T, typename A>
int vector<T, A>::compute_new_capacity(int size) const
{
    int new_capacity = m_capacity ? (m_capacity + m_capacity / 2) : 8;
    return new_capacity > size ? new_capacity : size;
}

//
// template<typename T, size_type length>
// class small_vector;
//

template<typename T, int length>
constexpr small_vector<T, length>::small_vector(
  const small_vector<T, length>& other) noexcept
{
    std::uninitialized_copy_n(other.data(), other.m_size, data());

    m_size = other.m_size;
}

template<typename T, int length>
constexpr small_vector<T, length>::small_vector(
  small_vector<T, length>&& other) noexcept
    requires(std::is_move_constructible_v<T>)
{
    std::uninitialized_move_n(other.data(), other.m_size, data());

    m_size = std::exchange(other.m_size, 0);
}

template<typename T, int length>
constexpr small_vector<T, length>::small_vector(
  std::integral auto size) noexcept
{
    if (std::cmp_less(size, 0))
        m_size = 0;
    else if (std::cmp_less(length, size))
        m_size = length;
    else
        m_size = static_cast<size_type>(size);

    std::uninitialized_value_construct_n(data(), m_size);
}

template<typename T, int length>
constexpr small_vector<T, length>::small_vector(std::integral auto size,
                                                const_reference value) noexcept
{
    if (std::cmp_less(size, 0))
        m_size = 0;
    else if (std::cmp_less(length, size))
        m_size = length;
    else
        m_size = static_cast<size_type>(size);

    std::uninitialized_fill_n(data(), m_size, value);
}

template<typename T, int length>
constexpr small_vector<T, length>::small_vector(
  std::initializer_list<T> init) noexcept
{
    assign(init.begin(), init.end());
}

template<typename T, int length>
template<not_integral InputIterator>
constexpr small_vector<T, length>::small_vector(InputIterator first,
                                                InputIterator last) noexcept
{
    assign(first, last);
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
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::copy_n(other.data(), other.size(), data());
        } else {
            if (m_size <= other.m_size) {
                std::copy_n(other.data(), size(), data());
                std::uninitialized_copy_n(other.data() + m_size,
                                          other.m_size - m_size,
                                          data() + m_size);
            } else {
                std::copy_n(other.data(), other.size(), data());
                if (not std::is_trivially_destructible_v<T>) {
                    std::destroy_n(data() + other.m_size,
                                   m_size - other.m_size);
                }
            }
        }

        m_size = other.m_size;
    }

    return *this;
}

template<typename T, int length>
constexpr small_vector<T, length>& small_vector<T, length>::operator=(
  small_vector<T, length>&& other) noexcept
    requires(std::is_move_assignable_v<T>)
{
    if (&other != this) {
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::copy_n(
              std::make_move_iterator(other.data()), other.size(), data());
        } else {
            if (m_size <= other.m_size) {
                std::copy_n(
                  std::make_move_iterator(other.begin()), size(), data());
                std::uninitialized_move_n(other.data() + m_size,
                                          other.m_size - m_size,
                                          data() + m_size);
            } else {
                std::copy_n(
                  std::make_move_iterator(other.begin()), other.size(), data());
                if (not std::is_trivially_destructible_v<T>) {
                    std::destroy_n(data() + other.m_size,
                                   m_size - other.m_size);
                }
            }
        }

        m_size = std::exchange(other.m_size, 0);
    }

    return *this;
}

template<typename T, int length>
template<not_integral InputIterator>
constexpr void small_vector<T, length>::assign(InputIterator first,
                                               InputIterator last) noexcept
{
    clear();

    const auto distance = std::distance(first, last);
    if (std::cmp_less(length, distance))
        m_size = length;
    else
        m_size = static_cast<size_type>(distance);

    std::uninitialized_copy_n(first, m_size, data());
}

template<typename T, int length>
constexpr void small_vector<T, length>::assign(std::integral auto size,
                                               const_reference value) noexcept
{
    clear();

    if (std::cmp_less_equal(size, 0))
        m_size = 0;
    else if (std::cmp_less(length, size))
        m_size = length;
    else
        m_size = size;

    std::uninitialized_fill_n(data(), m_size, value);
}

template<typename T, int length>
constexpr void small_vector<T, length>::resize(
  std::integral auto default_size) noexcept
{
    static_assert(std::is_nothrow_default_constructible_v<T>,
                  "T must be nothrow default constructible to use "
                  "init() function");

    if (std::cmp_greater(0, default_size))
        default_size = 0;

    if (std::cmp_less(length, default_size))
        default_size = length;

    const auto new_default_size = static_cast<size_type>(default_size);

    if (new_default_size > m_size)
        std::uninitialized_value_construct_n(data() + m_size,
                                             new_default_size - m_size);
    else
        std::destroy_n(data() + new_default_size, m_size - new_default_size);

    m_size = new_default_size;
}

template<typename T, int length>
constexpr void small_vector<T, length>::resize(std::integral auto default_size,
                                               const_reference value) noexcept
{
    static_assert(std::is_nothrow_default_constructible_v<T>,
                  "T must be nothrow default constructible to use "
                  "init() function");

    if (std::cmp_greater(0, default_size))
        default_size = 0;

    if (std::cmp_less(length, default_size))
        default_size = length;

    const auto new_default_size = static_cast<size_type>(default_size);

    if (new_default_size > m_size)
        std::uninitialized_fill_n(
          data() + m_size, new_default_size - m_size, value);
    else
        std::destroy_n(data() + new_default_size, m_size - new_default_size);

    m_size = new_default_size;
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
    debug::ensure(m_size > 0);
    return data()[0];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::front() const noexcept
{
    debug::ensure(m_size > 0);
    return data()[0];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::back() noexcept
{
    debug::ensure(m_size > 0);
    return data()[m_size - 1];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::back() const noexcept
{
    debug::ensure(m_size > 0);
    return data()[m_size - 1];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::operator[](std::integral auto index) noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::operator[](std::integral auto index) const noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, m_size));

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

    debug::ensure(can_alloc(1));

    std::construct_at(&(data()[m_size]), std::forward<Args>(args)...);
    ++m_size;

    return data()[m_size - 1];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::push_back(const T& arg) noexcept
{
    debug::ensure(can_alloc(1));

    std::construct_at(&(data()[m_size]), arg);
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
    debug::ensure(std::cmp_greater_equal(index, 0) &&
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

template<typename T, int length>
constexpr void small_vector<T, length>::swap_pop_back(
  const_iterator to_delete) noexcept
{
    debug::ensure(m_size > 0);
    debug::ensure(begin() <= to_delete and to_delete < begin() + m_size);

    if (to_delete != end()) {
        auto last = data() + m_size - 1;

        if (to_delete == last) {
            pop_back();
        } else {
            std::destroy_at(to_delete);

            if constexpr (std::is_move_constructible_v<T>) {
                std::uninitialized_move_n(last, 1, last);
            } else {
                std::uninitialized_copy_n(last, 1, last);
                std::destroy_at(last);
            }

            --m_size;
        }
    }
}

template<typename T, int length>
constexpr typename small_vector<T, length>::iterator
small_vector<T, length>::erase(std::integral auto index) noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0) &&
                  std::cmp_less(index, m_size));

    if (m_size) {
        if (index != m_size - 1) {
            auto dst = begin() + index;
            auto src = begin() + index + 1;

            if constexpr (std::is_move_assignable_v<T>) {
                std::move(src, end(), dst);
                --m_size;
            } else if constexpr (std::is_copy_assignable_v<T>) {
                std::copy(src, end(), dst);
                pop_back();
            }

            return begin() + index;
        }
    }

    return end();
}

template<typename T, int length>
constexpr typename small_vector<T, length>::iterator
small_vector<T, length>::erase(iterator to_del) noexcept
{
    debug::ensure(begin() <= to_del and to_del < begin() + m_size);

    if (m_size) {
        if (to_del != end()) {
            auto dst = to_del;
            auto src = to_del + 1;

            if constexpr (std::is_move_assignable_v<T>) {
                std::move(src, end(), dst);
                --m_size;
            } else if constexpr (std::is_copy_assignable_v<T>) {
                std::copy(src, end(), dst);
                pop_back();
            }

            return to_del;
        }
    }

    return end();
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
    std::uninitialized_copy_n(str.m_buffer, str.m_size, m_buffer);
    m_buffer[str.m_size] = '\0';
    m_size               = str.m_size;
}

template<int length>
inline constexpr small_string<length>::small_string(
  small_string<length>&& str) noexcept
{
    std::uninitialized_copy_n(str.m_buffer, str.m_size, m_buffer);
    m_buffer[str.m_size] = '\0';
    m_size               = str.m_size;
    str.clear();
}

template<int length>
inline constexpr small_string<length>& small_string<length>::operator=(
  const small_string<length>& str) noexcept
{
    if (&str != this) {
        std::uninitialized_copy_n(str.m_buffer, str.m_size, m_buffer);
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
        std::uninitialized_copy_n(str.m_buffer, str.m_size, m_buffer);
        m_buffer[str.m_size] = '\0';
        m_size               = str.m_size;
    }

    return *this;
}

template<int length>
inline constexpr small_string<length>& small_string<length>::operator=(
  const char* str) noexcept
{
    if (m_buffer != str)
        assign(std::string_view{ str });

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
    assign(std::string_view{ str });
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

    std::uninitialized_copy_n(str.data(), m_size, &m_buffer[0]);
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
    debug::ensure(std::cmp_less(index, length));

    return m_buffer[index];
}

template<int length>
inline constexpr typename small_string<length>::const_reference
small_string<length>::operator[](std::integral auto index) const noexcept
{
    debug::ensure(std::cmp_less(index, m_size));

    return m_buffer[index];
}

template<int length>
inline constexpr const char* small_string<length>::c_str() const noexcept
{
    return m_buffer;
}

template<int length>
inline constexpr char* small_string<length>::data() noexcept
{
    return m_buffer;
}
template<int length>
inline constexpr const char* small_string<length>::data() const noexcept
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
inline constexpr auto small_string<length>::operator<=>(
  const small_string& rhs) const noexcept
{
    return sv() <=> rhs.sv();
}

template<int length>
inline constexpr bool small_string<length>::operator==(
  const small_string<length>& rhs) const noexcept
{
    return sv().compare(rhs.sv()) == 0;
}

template<int length>
inline constexpr bool small_string<length>::operator!=(
  const small_string<length>& rhs) const noexcept
{
    return sv().compare(rhs.sv()) != 0;
}

template<int length>
inline constexpr bool small_string<length>::operator>(
  const small_string<length>& rhs) const noexcept
{
    return sv().compare(rhs.sv()) > 0;
}

template<int length>
inline constexpr bool small_string<length>::operator>=(
  const small_string<length>& rhs) const noexcept
{
    return sv().compare(rhs.sv()) >= 0;
}

template<int length>
inline constexpr bool small_string<length>::operator<(
  const small_string<length>& rhs) const noexcept
{
    return sv().compare(rhs.sv()) < 0;
}

template<int length>
inline constexpr bool small_string<length>::operator<=(
  const small_string<length>& rhs) const noexcept
{
    return sv().compare(rhs.sv()) <= 0;
}

template<int length>
inline constexpr bool small_string<length>::operator==(
  const std::string_view rhs) const noexcept
{
    return sv() == rhs;
}

template<int length>
inline constexpr bool small_string<length>::operator!=(
  const std::string_view rhs) const noexcept
{
    return sv() != rhs;
}

template<int length>
inline constexpr bool small_string<length>::operator>(
  const std::string_view rhs) const noexcept
{
    return sv() > rhs;
}

template<int length>
inline constexpr bool small_string<length>::operator<(
  const std::string_view rhs) const noexcept
{
    return sv() < rhs;
}

template<int length>
inline constexpr bool small_string<length>::operator==(
  const char* rhs) const noexcept
{
    return sv() == std::string_view{ rhs };
}

template<int length>
inline constexpr bool small_string<length>::operator!=(
  const char* rhs) const noexcept
{
    return sv() != std::string_view{ rhs };
}

template<int length>
inline constexpr bool small_string<length>::operator>(
  const char* rhs) const noexcept
{
    return sv() > std::string_view{ rhs };
}

template<int length>
inline constexpr bool small_string<length>::operator<(
  const char* rhs) const noexcept
{
    return sv() < std::string_view{ rhs };
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
// template<typename T, typename A>.................................. //
// class ring_buffer;                                                 //
//                                                                    //
////////////////////////////////////////////////////////////////////////

template<class T, typename A>
template<bool is_const>
template<typename Container>
ring_buffer<T, A>::iterator_base<is_const>::iterator_base(
  Container* ring_,
  index_type i_) noexcept
    requires(!std::is_const_v<Container> && !is_const)
  : ring(ring_)
  , i(i_)
{}

template<class T, typename A>
template<bool is_const>
template<typename Container>
ring_buffer<T, A>::iterator_base<is_const>::iterator_base(
  Container* ring_,
  index_type i_) noexcept
    requires(std::is_const_v<Container> && is_const)
  : ring(ring_)
  , i(i_)
{}

template<class T, typename A>
template<bool is_const>
ring_buffer<T, A>::iterator_base<is_const>::iterator_base(
  const iterator_base& other) noexcept
  : ring(other.ring)
  , i(other.i)
{}

template<class T, typename A>
template<bool is_const>
typename ring_buffer<T, A>::template iterator_base<is_const>&
ring_buffer<T, A>::iterator_base<is_const>::operator=(
  const iterator_base& other) noexcept
{
    if (this != &other) {
        ring = other.ring;
        i    = other.i;
    }

    return *this;
}

template<class T, typename A>
template<bool is_const>
typename ring_buffer<T, A>::template iterator_base<is_const>::reference
ring_buffer<T, A>::iterator_base<is_const>::operator*() const noexcept
{
    return ring->buffer[i];
}

template<class T, typename A>
template<bool is_const>
typename ring_buffer<T, A>::template iterator_base<is_const>::pointer
ring_buffer<T, A>::iterator_base<is_const>::operator->() const noexcept
{
    return &ring->buffer[i];
}

template<class T, typename A>
template<bool is_const>
typename ring_buffer<T, A>::template iterator_base<is_const>&
ring_buffer<T, A>::iterator_base<is_const>::operator++() noexcept
{
    if (ring) {
        i = ring->advance(i);

        if (i == ring->m_tail)
            reset();
    }

    return *this;
}

template<class T, typename A>
template<bool is_const>
typename ring_buffer<T, A>::template iterator_base<is_const>
ring_buffer<T, A>::iterator_base<is_const>::operator++(int) noexcept
{
    iterator_base orig(*this);
    ++(*this);

    return orig;
}

template<class T, typename A>
template<bool is_const>
typename ring_buffer<T, A>::template iterator_base<is_const>&
ring_buffer<T, A>::iterator_base<is_const>::operator--() noexcept
{
    if (ring) {
        i = ring->back(i);

        if (i == ring->m_tail)
            reset();
    }

    return *this;
}

template<class T, typename A>
template<bool is_const>
typename ring_buffer<T, A>::template iterator_base<is_const>
ring_buffer<T, A>::iterator_base<is_const>::operator--(int) noexcept
{
    iterator_base orig(*this);
    --(*this);

    return orig;
}

template<class T, typename A>
template<bool is_const>
template<bool R>
bool ring_buffer<T, A>::iterator_base<is_const>::operator==(
  const iterator_base<R>& other) const
{
    return other.ring == ring && other.i == i;
}

template<class T, typename A>
template<bool is_const>
void ring_buffer<T, A>::iterator_base<is_const>::reset() noexcept
{
    ring = nullptr;
    i    = 0;
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::index_type ring_buffer<T, A>::advance(
  index_type position) const noexcept
{
    return (position + 1) % m_capacity;
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::index_type ring_buffer<T, A>::back(
  index_type position) const noexcept
{
    return (((position - 1) % m_capacity) + m_capacity) % m_capacity;
}

template<class T, typename A>
constexpr bool ring_buffer<T, A>::make(std::integral auto capacity) noexcept
{
    if (std::cmp_greater(capacity, 0)) {
        auto* ptr = reinterpret_cast<T*>(A::allocate(sizeof(T) * capacity));
        if (ptr) {
            buffer     = ptr;
            m_capacity = static_cast<index_type>(capacity);
        }
    }

    return buffer != nullptr;
}

template<class T, typename A>
constexpr ring_buffer<T, A>::ring_buffer(std::integral auto capacity) noexcept
{
    make(capacity);
}

template<class T, typename A>
constexpr ring_buffer<T, A>::ring_buffer(const ring_buffer& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        if (m_capacity != rhs.m_capacity) {
            auto* ptr =
              reinterpret_cast<T*>(A::allocate(sizeof(T) * rhs.m_capacity));
            if (ptr) {
                if (buffer)
                    A::deallocate(buffer, sizeof(T) * m_capacity);

                if (rhs.m_capacity) {
                    buffer     = ptr;
                    m_capacity = rhs.m_capacity;
                }
            }
        }

        for (auto it = rhs.begin(), et = rhs.end(); it != et; ++it)
            push_tail(*it);
    }
}

template<class T, typename A>
constexpr ring_buffer<T, A>& ring_buffer<T, A>::operator=(
  const ring_buffer& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        if (m_capacity != rhs.m_capacity) {
            auto* ptr =
              reinterpret_cast<T*>(A::allocate(sizeof(T) * rhs.m_capacity));
            if (ptr) {
                if (buffer)
                    A::deallocate(buffer, sizeof(T) * m_capacity);

                if (rhs.m_capacity > 0) {
                    buffer     = ptr;
                    m_capacity = rhs.m_capacity;
                }
            }
        }

        for (auto it = rhs.begin(), et = rhs.end(); it != et; ++it)
            push_tail(*it);
    }

    return *this;
}

template<class T, typename A>
constexpr ring_buffer<T, A>::ring_buffer(ring_buffer&& rhs) noexcept
  : buffer{ std::exchange(rhs.buffer, nullptr) }
  , m_head{ std::exchange(rhs.m_head, 0) }
  , m_tail{ std::exchange(rhs.m_tail, 0) }
  , m_capacity{ std::exchange(rhs.m_capacity, 0) }
{}

template<class T, typename A>
constexpr ring_buffer<T, A>& ring_buffer<T, A>::operator=(
  ring_buffer&& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        buffer     = std::exchange(rhs.buffer, nullptr);
        m_head     = std::exchange(rhs.m_head, 0);
        m_tail     = std::exchange(rhs.m_tail, 0);
        m_capacity = std::exchange(rhs.m_capacity, 0);
    }

    return *this;
}

template<class T, typename A>
constexpr ring_buffer<T, A>::~ring_buffer() noexcept
{
    destroy();
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::swap(ring_buffer& rhs) noexcept
{
    std::swap(buffer, rhs.buffer);
    std::swap(m_head, rhs.m_head);
    std::swap(m_tail, rhs.m_tail);
    std::swap(m_capacity, rhs.m_capacity);
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::clear() noexcept
{
    if (!std::is_trivially_destructible_v<T>) {
        while (!empty())
            dequeue();
    }

    m_head = 0;
    m_tail = 0;
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::destroy() noexcept
{
    clear();

    if (buffer) {
        A::deallocate(buffer, sizeof(T) * m_capacity);
        buffer = nullptr;
    }
}

template<class T, typename A>
constexpr bool ring_buffer<T, A>::reserve(
  std::integral auto new_capacity) noexcept
{
    debug::ensure(
      std::cmp_less(new_capacity, std::numeric_limits<index_type>::max()));

    if (m_capacity < new_capacity) {
        ring_buffer<T, A> tmp(new_capacity);
        if (tmp.capacity() < capacity())
            return false;

        if constexpr (std::is_move_constructible_v<T>)
            for (auto it = begin(), et = end(); it != et; ++it)
                tmp.emplace_tail(std::move(*it));
        else
            for (auto it = begin(), et = end(); it != et; ++it)
                tmp.push_tail(*it);

        swap(tmp);
    }

    return true;
}

template<class T, typename A>
template<typename Compare>
constexpr void ring_buffer<T, A>::sort(Compare fn) noexcept
{
    if (ssize() < 2)
        return;

    auto next = begin(), et = end();
    auto it = next++;

    for (; it != et; ++it) {
        next = it;

        for (; next != et; ++next) {
            if (fn(*next, *it)) {
                using std::swap;
                swap(*next, *it);
            }
        }
    }
}

template<class T, typename A>
template<typename... Args>
constexpr bool ring_buffer<T, A>::emplace_head(Args&&... args) noexcept
{
    if (full())
        return false;

    m_head = back(m_head);
    std::construct_at(&buffer[m_head], std::forward<Args>(args)...);

    return true;
}

template<class T, typename A>
template<typename... Args>
constexpr bool ring_buffer<T, A>::emplace_tail(Args&&... args) noexcept
{
    if (full())
        return false;

    std::construct_at(&buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);

    return true;
}

template<class T, typename A>
template<typename... Args>
constexpr void ring_buffer<T, A>::force_emplace_tail(Args&&... args) noexcept
{
    if (full())
        pop_head();

    std::construct_at(&buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);
}

template<class T, typename A>
constexpr bool ring_buffer<T, A>::push_head(const T& item) noexcept
{
    if (full())
        return false;

    m_head = back(m_head);
    std::construct_at(&buffer[m_head], item);

    return true;
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::pop_head() noexcept
{
    if (!empty()) {
        std::destroy_at(&buffer[m_head]);
        m_head = advance(m_head);
    }
}

template<class T, typename A>
constexpr bool ring_buffer<T, A>::push_tail(const T& item) noexcept
{
    if (full())
        return false;

    std::construct_at(&buffer[m_tail], item);
    m_tail = advance(m_tail);

    return true;
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::pop_tail() noexcept
{
    if (!empty()) {
        m_tail = back(m_tail);
        std::destroy_at(&buffer[m_tail]);
    }
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::erase_after(iterator this_it) noexcept
{
    debug::ensure(this_it.ring != nullptr);

    if (this_it == tail())
        return;

    while (this_it != tail())
        pop_tail();
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::erase_before(iterator this_it) noexcept
{
    debug::ensure(this_it.ring != nullptr);

    if (this_it == head())
        return;

    auto it = head();
    while (it != head())
        pop_head();
}

template<class T, typename A>
template<typename... Args>
constexpr bool ring_buffer<T, A>::emplace_enqueue(Args&&... args) noexcept
{
    if (full())
        return false;

    std::construct_at(&buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);

    return true;
}

template<class T, typename A>
template<typename... Args>
constexpr typename ring_buffer<T, A>::reference
ring_buffer<T, A>::force_emplace_enqueue(Args&&... args) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&buffer[m_tail], std::forward<Args>(args)...);
    const auto old = m_tail;
    m_tail         = advance(m_tail);
    return buffer[old];
}

template<class T, typename A>
constexpr bool ring_buffer<T, A>::enqueue(const T& item) noexcept
{
    if (full())
        return false;

    std::construct_at(&buffer[m_tail], item);
    m_tail = advance(m_tail);

    return true;
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::force_enqueue(const T& item) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&buffer[m_tail], item);
    m_tail = advance(m_tail);
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::dequeue() noexcept
{
    if (!empty()) {
        std::destroy_at(&buffer[m_head]);
        m_head = advance(m_head);
    }
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::iterator
ring_buffer<T, A>::head() noexcept
{
    return empty() ? iterator{} : iterator{ this, m_head };
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::const_iterator ring_buffer<T, A>::head()
  const noexcept
{
    return empty() ? const_iterator{} : const_iterator{ this, m_head };
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::iterator
ring_buffer<T, A>::tail() noexcept
{
    return empty() ? iterator{} : iterator{ this, back(m_tail) };
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::const_iterator ring_buffer<T, A>::tail()
  const noexcept
{
    return empty() ? const_iterator{} : const_iterator{ this, back(m_tail) };
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::iterator
ring_buffer<T, A>::begin() noexcept
{
    return empty() ? iterator{} : iterator{ this, m_head };
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::const_iterator ring_buffer<T, A>::begin()
  const noexcept
{
    return empty() ? const_iterator{} : const_iterator{ this, m_head };
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::iterator ring_buffer<T, A>::end() noexcept
{
    return iterator{};
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::const_iterator ring_buffer<T, A>::end()
  const noexcept
{
    return const_iterator{};
}

template<class T, typename A>
constexpr T* ring_buffer<T, A>::data() noexcept
{
    return &buffer[0];
}

template<class T, typename A>
constexpr const T* ring_buffer<T, A>::data() const noexcept
{
    return &buffer[0];
}

template<class T, typename A>
constexpr T& ring_buffer<T, A>::operator[](std::integral auto index) noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, m_capacity));

    return buffer[index];
}

template<class T, typename A>
constexpr const T& ring_buffer<T, A>::operator[](
  std::integral auto index) const noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, m_capacity));

    return buffer[index];
}

template<class T, typename A>
constexpr T& ring_buffer<T, A>::front() noexcept
{
    debug::ensure(!empty());

    return buffer[m_head];
}

template<class T, typename A>
constexpr const T& ring_buffer<T, A>::front() const noexcept
{
    debug::ensure(!empty());

    return buffer[m_head];
}

template<class T, typename A>
constexpr T& ring_buffer<T, A>::back() noexcept
{
    debug::ensure(!empty());

    return buffer[back(m_tail)];
}

template<class T, typename A>
constexpr const T& ring_buffer<T, A>::back() const noexcept
{
    debug::ensure(!empty());

    return buffer[back(m_tail)];
}

template<class T, typename A>
constexpr bool ring_buffer<T, A>::empty() const noexcept
{
    return m_head == m_tail;
}

template<class T, typename A>
constexpr bool ring_buffer<T, A>::full() const noexcept
{
    return advance(m_tail) == m_head;
}

template<class T, typename A>
constexpr unsigned ring_buffer<T, A>::size() const noexcept
{
    return static_cast<unsigned>(ssize());
}

template<class T, typename A>
constexpr int ring_buffer<T, A>::ssize() const noexcept
{
    return (m_tail >= m_head) ? m_tail - m_head
                              : m_capacity - (m_head - m_tail);
}

template<class T, typename A>
constexpr int ring_buffer<T, A>::available() const noexcept
{
    return capacity() - size();
}

template<class T, typename A>
constexpr int ring_buffer<T, A>::capacity() const noexcept
{
    return m_capacity;
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::index_type
ring_buffer<T, A>::index_from_begin(index_type idx) const noexcept
{
    return (m_head + idx) % m_capacity;
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::index_type ring_buffer<T, A>::head_index()
  const noexcept
{
    return m_head;
}

template<class T, typename A>
constexpr typename ring_buffer<T, A>::index_type ring_buffer<T, A>::tail_index()
  const noexcept
{
    return m_tail;
}

////////////////////////////////////////////////////////////////////////
//                                                                    //
// template<typename T, int length>.................................. //
// snall_ring_buffer<T, index>                                        //
//                                                                    //
////////////////////////////////////////////////////////////////////////

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::index_type
small_ring_buffer<T, length>::advance(index_type position) const noexcept
{
    return static_cast<index_type>((static_cast<int>(position) + 1) % length);
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::index_type
small_ring_buffer<T, length>::back(index_type position) const noexcept
{
    return static_cast<index_type>(
      (((static_cast<int>(position) - 1) % length) + length) % length);
}

template<class T, int length>
constexpr small_ring_buffer<T, length>::small_ring_buffer(
  const small_ring_buffer& rhs) noexcept
{
    for (auto it = rhs.begin(), et = rhs.end(); it != et; ++it)
        push_tail(*it);
}

template<class T, int length>
constexpr small_ring_buffer<T, length>& small_ring_buffer<T, length>::operator=(
  const small_ring_buffer& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        for (auto it = rhs.begin(), et = rhs.end(); it != et; ++it)
            push_tail(*it);
    }

    return *this;
}

template<class T, int length>
constexpr small_ring_buffer<T, length>::small_ring_buffer(
  small_ring_buffer&& rhs) noexcept
{
    std::uninitialized_move_n(rhs.data(), rhs.size(), data());

    m_head = std::exchange(rhs.m_head, 0);
    m_tail = std::exchange(rhs.m_tail, 0);
}

template<class T, int length>
constexpr small_ring_buffer<T, length>& small_ring_buffer<T, length>::operator=(
  small_ring_buffer&& rhs) noexcept
{
    if (this != &rhs) {
        clear();

        std::uninitialized_move_n(rhs.data(), rhs.size(), data());

        m_head = std::exchange(rhs.m_head, 0);
        m_tail = std::exchange(rhs.m_tail, 0);
    }

    return *this;
}

template<class T, int length>
constexpr small_ring_buffer<T, length>::~small_ring_buffer() noexcept
{
    destroy();
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::clear() noexcept
{
    if (!std::is_trivially_destructible_v<T>) {
        while (!empty())
            dequeue();
    }

    m_head = 0;
    m_tail = 0;
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::destroy() noexcept
{
    clear();

    m_head = 0;
    m_tail = 0;
}

template<class T, int length>
template<typename... Args>
constexpr bool small_ring_buffer<T, length>::emplace_head(
  Args&&... args) noexcept
{
    if (full())
        return false;

    m_head = back(m_head);
    std::construct_at(&data()[m_head], std::forward<Args>(args)...);

    return true;
}

template<class T, int length>
template<typename... Args>
constexpr bool small_ring_buffer<T, length>::emplace_tail(
  Args&&... args) noexcept
{
    if (full())
        return false;

    std::construct_at(&data()[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);

    return true;
}

template<class T, int length>
constexpr bool small_ring_buffer<T, length>::push_head(const T& item) noexcept
{
    if (full())
        return false;

    m_head = back(m_head);
    std::construct_at(&data()[m_head], item);

    return true;
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::pop_head() noexcept
{
    if (!empty()) {
        std::destroy_at(&data()[m_head]);
        m_head = advance(m_head);
    }
}

template<class T, int length>
constexpr bool small_ring_buffer<T, length>::push_tail(const T& item) noexcept
{
    if (full())
        return false;

    std::construct_at(&data()[m_tail], item);
    m_tail = advance(m_tail);

    return true;
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::pop_tail() noexcept
{
    if (!empty()) {
        m_tail = back(m_tail);
        std::destroy_at(&data()[m_tail]);
    }
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::erase_after(
  iterator this_it) noexcept
{
    debug::ensure(this_it.ring != nullptr);

    if (this_it == tail())
        return;

    while (this_it != tail())
        pop_tail();
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::erase_before(
  iterator this_it) noexcept
{
    debug::ensure(this_it.ring != nullptr);

    if (this_it == head())
        return;

    auto it = head();
    while (it != head())
        pop_head();
}

template<class T, int length>
template<typename... Args>
constexpr bool small_ring_buffer<T, length>::emplace_enqueue(
  Args&&... args) noexcept
{
    if (full())
        return false;

    std::construct_at(&data()[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);

    return true;
}

template<class T, int length>
template<typename... Args>
constexpr void small_ring_buffer<T, length>::force_emplace_enqueue(
  Args&&... args) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&data()[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);
}

template<class T, int length>
constexpr bool small_ring_buffer<T, length>::enqueue(const T& item) noexcept
{
    if (full())
        return false;

    std::construct_at(&data()[m_tail], item);
    m_tail = advance(m_tail);

    return true;
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::force_enqueue(
  const T& item) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&data()[m_tail], item);
    m_tail = advance(m_tail);
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::dequeue() noexcept
{
    if (!empty()) {
        std::destroy_at(&data()[m_head]);
        m_head = advance(m_head);
    }
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::iterator
small_ring_buffer<T, length>::head() noexcept
{
    return empty() ? iterator{} : iterator{ this, m_head };
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::const_iterator
small_ring_buffer<T, length>::head() const noexcept
{
    return empty() ? const_iterator{} : const_iterator{ this, m_head };
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::iterator
small_ring_buffer<T, length>::tail() noexcept
{
    return empty() ? iterator{} : iterator{ this, back(m_tail) };
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::const_iterator
small_ring_buffer<T, length>::tail() const noexcept
{
    return empty() ? const_iterator{} : const_iterator{ this, back(m_tail) };
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::iterator
small_ring_buffer<T, length>::begin() noexcept
{
    return empty() ? iterator{} : iterator{ this, m_head };
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::const_iterator
small_ring_buffer<T, length>::begin() const noexcept
{
    return empty() ? const_iterator{} : const_iterator{ this, m_head };
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::iterator
small_ring_buffer<T, length>::end() noexcept
{
    return iterator{};
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::const_iterator
small_ring_buffer<T, length>::end() const noexcept
{
    return const_iterator{};
}

template<class T, int length>
constexpr T* small_ring_buffer<T, length>::data() noexcept
{
    return reinterpret_cast<T*>(&m_buffer[0]);
}

template<class T, int length>
constexpr const T* small_ring_buffer<T, length>::data() const noexcept
{
    return reinterpret_cast<const T*>(&m_buffer[0]);
}

template<class T, int length>
constexpr T& small_ring_buffer<T, length>::operator[](
  std::integral auto index) noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, length));

    return data()[index];
}

template<class T, int length>
constexpr const T& small_ring_buffer<T, length>::operator[](
  std::integral auto index) const noexcept
{
    debug::ensure(std::cmp_greater_equal(index, 0));
    debug::ensure(std::cmp_less(index, length));

    return data()[index];
}

template<class T, int length>
constexpr T& small_ring_buffer<T, length>::front() noexcept
{
    debug::ensure(!empty());

    return data()[m_head];
}

template<class T, int length>
constexpr const T& small_ring_buffer<T, length>::front() const noexcept
{
    debug::ensure(!empty());

    return data()[m_head];
}

template<class T, int length>
constexpr T& small_ring_buffer<T, length>::back() noexcept
{
    debug::ensure(!empty());

    return data()[back(m_tail)];
}

template<class T, int length>
constexpr const T& small_ring_buffer<T, length>::back() const noexcept
{
    debug::ensure(!empty());

    return data()[back(m_tail)];
}

template<class T, int length>
constexpr bool small_ring_buffer<T, length>::empty() const noexcept
{
    return m_head == m_tail;
}

template<class T, int length>
constexpr bool small_ring_buffer<T, length>::full() const noexcept
{
    return advance(m_tail) == m_head;
}

template<class T, int length>
constexpr unsigned small_ring_buffer<T, length>::size() const noexcept
{
    return static_cast<unsigned>(ssize());
}

template<class T, int length>
constexpr int small_ring_buffer<T, length>::ssize() const noexcept
{
    return (m_tail >= m_head) ? m_tail - m_head : length - (m_head - m_tail);
}

template<class T, int length>
constexpr int small_ring_buffer<T, length>::available() const noexcept
{
    return capacity() - size();
}

template<class T, int length>
constexpr int small_ring_buffer<T, length>::capacity() const noexcept
{
    return length;
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::index_type
small_ring_buffer<T, length>::index_from_begin(
  std::integral auto idx) const noexcept
{
    debug::ensure(is_numeric_castable<index_type>(idx));

    return (m_head + static_cast<index_type>(idx)) % length;
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::index_type
small_ring_buffer<T, length>::head_index() const noexcept
{
    return m_head;
}

template<class T, int length>
constexpr typename small_ring_buffer<T, length>::index_type
small_ring_buffer<T, length>::tail_index() const noexcept
{
    return m_tail;
}

template<class T, int length>
template<bool is_const>
template<typename Container>
small_ring_buffer<T, length>::iterator_base<is_const>::iterator_base(
  Container* ring_,
  index_type i_) noexcept
    requires(!std::is_const_v<Container> && !is_const)
  : ring(ring_)
  , i(i_)
{}

template<class T, int length>
template<bool is_const>
template<typename Container>
small_ring_buffer<T, length>::iterator_base<is_const>::iterator_base(
  Container* ring_,
  index_type i_) noexcept
    requires(std::is_const_v<Container> && is_const)
  : ring(ring_)
  , i(i_)
{}

template<class T, int length>
template<bool is_const>
small_ring_buffer<T, length>::iterator_base<is_const>::iterator_base(
  const iterator_base& other) noexcept
  : ring(other.ring)
  , i(other.i)
{}

template<class T, int length>
template<bool is_const>
typename small_ring_buffer<T, length>::template iterator_base<is_const>&
small_ring_buffer<T, length>::iterator_base<is_const>::operator=(
  const iterator_base& other) noexcept
{
    if (this != &other) {
        ring = other.ring;
        i    = other.i;
    }

    return *this;
}

template<class T, int length>
template<bool is_const>
typename small_ring_buffer<T,
                           length>::template iterator_base<is_const>::reference
small_ring_buffer<T, length>::iterator_base<is_const>::operator*()
  const noexcept
{
    return ring->data()[i];
}

template<class T, int length>
template<bool is_const>
typename small_ring_buffer<T, length>::template iterator_base<is_const>::pointer
small_ring_buffer<T, length>::iterator_base<is_const>::operator->()
  const noexcept
{
    return &ring->data()[i];
}

template<class T, int length>
template<bool is_const>
typename small_ring_buffer<T, length>::template iterator_base<is_const>&
small_ring_buffer<T, length>::iterator_base<is_const>::operator++() noexcept
{
    if (ring) {
        i = ring->advance(i);

        if (i == ring->m_tail)
            reset();
    }

    return *this;
}

template<class T, int length>
template<bool is_const>
typename small_ring_buffer<T, length>::template iterator_base<is_const>
small_ring_buffer<T, length>::iterator_base<is_const>::operator++(int) noexcept
{
    iterator_base orig(*this);
    ++(*this);

    return orig;
}

template<class T, int length>
template<bool is_const>
typename small_ring_buffer<T, length>::template iterator_base<is_const>&
small_ring_buffer<T, length>::iterator_base<is_const>::operator--() noexcept
{
    if (ring) {
        i = ring->back(i);

        if (i == ring->m_tail)
            reset();
    }

    return *this;
}

template<class T, int length>
template<bool is_const>
typename small_ring_buffer<T, length>::template iterator_base<is_const>
small_ring_buffer<T, length>::iterator_base<is_const>::operator--(int) noexcept
{
    iterator_base orig(*this);
    --(*this);

    return orig;
}

template<class T, int length>
template<bool is_const>
template<bool R>
bool small_ring_buffer<T, length>::iterator_base<is_const>::operator==(
  const iterator_base<R>& other) const
{
    return other.ring == ring && other.i == i;
}

template<class T, int length>
template<bool is_const>
void small_ring_buffer<T, length>::iterator_base<is_const>::reset() noexcept
{
    ring = nullptr;
    i    = 0;
}

//
// class bitflags<EnumT>
//

template<typename EnumT>
constexpr bitflags<EnumT>::bitflags(unsigned long long val) noexcept
  : m_bits{ val }
{}

template<typename EnumT>
constexpr bitflags<EnumT>::bitflags(std::same_as<EnumT> auto... args) noexcept
{
    (set(args), ...);
}

template<typename EnumT>
constexpr bitflags<EnumT>& bitflags<EnumT>::set(value_type e,
                                                bool       value) noexcept
{
    m_bits.set(underlying(e), value);
    return *this;
}

template<typename EnumT>
constexpr bitflags<EnumT>& bitflags<EnumT>::reset(value_type e) noexcept
{
    set(e, false);
    return *this;
}

template<typename EnumT>
constexpr bitflags<EnumT>& bitflags<EnumT>::reset() noexcept
{
    m_bits.reset();
    return *this;
}

template<typename EnumT>
bool bitflags<EnumT>::all() const noexcept
{
    return m_bits.all();
}

template<typename EnumT>
bool bitflags<EnumT>::any() const noexcept
{
    return m_bits.any();
}

template<typename EnumT>
bool bitflags<EnumT>::none() const noexcept
{
    return m_bits.none();
}

template<typename EnumT>
constexpr std::size_t bitflags<EnumT>::size() const noexcept
{
    return m_bits.size();
}

template<typename EnumT>
constexpr std::size_t bitflags<EnumT>::count() const noexcept
{
    return m_bits.count();
}

template<typename EnumT>
std::size_t bitflags<EnumT>::to_unsigned() const noexcept
{
    return m_bits.to_ullong();
}

template<typename EnumT>
constexpr bool bitflags<EnumT>::operator[](value_type e) const
{
    return m_bits[underlying(e)];
}

template<typename EnumT>
constexpr typename bitflags<EnumT>::underlying_type bitflags<EnumT>::underlying(
  value_type e) noexcept
{
    return static_cast<typename bitflags<EnumT>::underlying_type>(e);
}

/**
   Equal vector operator.

   @param lhs Reference to the first vector.
   @param rhs Reference to the second vector.
   @return @c true if the arrays are equal, otherwise @c false.
 */
template<typename T>
constexpr bool operator==(const vector<T>& lhs, const vector<T>& rhs) noexcept
{
    return lhs.size() == rhs.size() and
           std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
   Equal vector operator.

   @param lhs Reference to the first vector.
   @param rhs Reference to the second vector.
   @return @c true if the arrays are equal, otherwise @c false.
 */
template<typename T, int Length>
constexpr bool operator==(const vector<T>&               lhs,
                          const small_vector<T, Length>& rhs) noexcept
{
    return lhs.size() == rhs.size() and
           std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
   Equal vector operator.

   @param lhs Reference to the first vector.
   @param rhs Reference to the second vector.
   @return @c true if the arrays are equal, otherwise @c false.
 */
template<typename T, int Length>
constexpr bool operator==(const small_vector<T, Length>& lhs,
                          const vector<T>&               rhs) noexcept
{
    return lhs.size() == rhs.size() and
           std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
   Equal vector operator.

   @param lhs Reference to the first vector.
   @param rhs Reference to the second vector.
   @return @c true if the arrays are equal, otherwise @c false.
 */
template<typename T, int LengthLhs, int LengthRhs>
constexpr bool operator==(const small_vector<T, LengthLhs>& lhs,
                          const small_vector<T, LengthRhs>& rhs) noexcept
{
    return lhs.size() == rhs.size() and
           std::equal(lhs.begin(), lhs.end(), rhs.begin());
}

/**
   Not equal vector operator.

   @param lhs Reference to the first vector.
   @param rhs Reference to the second vector.
   @return @c true if the arrays are not equal, otherwise @c false.
 */
template<typename T>
constexpr bool operator!=(const vector<T>& lhs, const vector<T>& rhs) noexcept
{
    return not(lhs == rhs);
}

/**
   Not equal vector operator.

   @param lhs Reference to the first vector.
   @param rhs Reference to the second vector.
   @return @c true if the arrays are not equal, otherwise @c false.
 */
template<typename T, int Length>
constexpr bool operator!=(const vector<T>&               lhs,
                          const small_vector<T, Length>& rhs) noexcept
{
    return not(lhs == rhs);
}

/**
   Not equal vector operator.

   @param lhs Reference to the first vector.
   @param rhs Reference to the second vector.
   @return @c true if the arrays are not equal, otherwise @c false.
 */
template<typename T, int Length>
constexpr bool operator!=(const small_vector<T, Length>& lhs,
                          const vector<T>&               rhs) noexcept
{
    return not(lhs == rhs);
}

/**
   Not equal vector operator.

   @param lhs Reference to the first vector.
   @param rhs Reference to the second vector.
   @return @c true if the arrays are not equal, otherwise @c false.
 */
template<typename T, int LengthLhs, int LengthRhs>
constexpr bool operator!=(const small_vector<T, LengthLhs>& lhs,
                          const small_vector<T, LengthRhs>& rhs) noexcept
{
    return not(lhs == rhs);
}

} // namespace irt

#endif
