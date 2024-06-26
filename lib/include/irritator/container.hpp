// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_CONTAINER_2023
#define ORG_VLEPROJECT_IRRITATOR_CONTAINER_2023

#include <irritator/macros.hpp>

#include <algorithm>
#include <bitset>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

#include <cstddef>

namespace irt {

////////////////////////////////////////////////////////////////////////
//                                                                    //
// Helpers functions................................................. //
//                                                                    //
////////////////////////////////////////////////////////////////////////

//! Compute the best size to fit the small storage size.
//!
//! This function is used into @c small_string, @c small_vector and @c
//! small_ring_buffer to determine the @c capacity and/or @c size type
//! (unsigned, short unsigned, long unsigned, long long unsigned.
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

////////////////////////////////////////////////////////////////////////
//                                                                    //
// Allocator: default menory_resource or specific................... //
//                                                                    //
////////////////////////////////////////////////////////////////////////

class memory_resource
{
public:
    memory_resource() noexcept          = default;
    virtual ~memory_resource() noexcept = default;

    [[gnu::malloc]] void* allocate(
      std::size_t bytes,
      std::size_t alignment = alignof(std::max_align_t)) noexcept
    {
        debug::ensure(bytes > 0);
        debug::ensure(bytes % alignment == 0);

        return do_allocate(bytes, alignment);
    }

    void deallocate(void*       pointer,
                    std::size_t bytes,
                    std::size_t alignment = alignof(std::max_align_t)) noexcept
    {
        debug::ensure(bytes > 0u);

        if (pointer)
            do_deallocate(pointer, bytes, alignment);
    }

    bool can_alloc(
      std::size_t bytes,
      std::size_t alignment = alignof(std::max_align_t)) const noexcept
    {
        return do_can_alloc(bytes, alignment);
    }

protected:
    virtual void* do_allocate(std::size_t bytes,
                              std::size_t alignment) noexcept = 0;

    virtual void do_deallocate(void*       pointer,
                               std::size_t bytes,
                               std::size_t alignment) noexcept = 0;

    virtual bool do_can_alloc(std::size_t bytes,
                              std::size_t alignment) const noexcept = 0;
};

//! A dynamic heap `memory_resource` using `std::aligned_alloc` or
//! `std::align_memory` to allocate aligned memory .
//!
//! @note `malloc_memory_resource` is the default `memory_resource` provides by
//! the global function `get_malloc_memory_resource()`.
class malloc_memory_resource final : public memory_resource
{
public:
    malloc_memory_resource() noexcept  = default;
    ~malloc_memory_resource() noexcept = default;

protected:
    void* do_allocate(std::size_t bytes,
                      std::size_t alignment) noexcept override;

    void do_deallocate(void*       pointer,
                       std::size_t bytes,
                       std::size_t alignment) noexcept override;

    bool do_can_alloc(std::size_t /*bytes*/,
                      std::size_t /*alignment*/) const noexcept override
    {
        return true;
    }
};

//! A stack `memory_resource` using an `std::array` to store aligned memory.
//!
//! @note `static_memory_resource` mainly used in unit tests.
template<size_t Bytes>
class static_memory_resource final : public memory_resource
{
private:
    alignas(std::max_align_t) std::byte buffer[Bytes];
    std::size_t position{};

public:
    static_memory_resource() noexcept = default;

    ~static_memory_resource() noexcept = default;

    std::size_t capacity() const noexcept { return std::size(buffer); }

    void reset() noexcept { position = 0; }

protected:
    void* do_allocate(std::size_t bytes,
                      std::size_t alignment) noexcept override
    {
        debug::mem_log("static_memory_resource<",
                       Bytes,
                       ">::need - allocate[",
                       bytes,
                       " ",
                       alignment,
                       "]\n");

        debug::ensure(bytes > 0);
        debug::ensure((bytes % alignment) == 0);

        if (Bytes < (position + bytes)) {
            debug::log("Irritator shutdown: Unable to allocate memory ",
                       bytes,
                       " alignment ",
                       alignment,
                       "\n");
            std::abort();
        }

        const auto old_position = position;
        position += bytes;
        auto* p = static_cast<void*>(std::data(buffer) + old_position);

        debug::mem_log("static_memory_resource<",
                       Bytes,
                       ">::allocate[",
                       p,
                       " ",
                       bytes,
                       " ",
                       alignment,
                       "]\n");

        return p;
    }

    void do_deallocate([[maybe_unused]] void*       pointer,
                       [[maybe_unused]] std::size_t bytes,
                       [[maybe_unused]] std::size_t alignment) noexcept override
    {
        debug::mem_log("static_memory_resource<",
                       Bytes,
                       ">::do_deallocate[",
                       pointer,
                       " ",
                       bytes,
                       " ",
                       alignment,
                       "]\n");

        [[maybe_unused]] const auto pos =
          reinterpret_cast<std::uintptr_t>(pointer);
        [[maybe_unused]] const auto first =
          reinterpret_cast<std::uintptr_t>(std::data(buffer));
        [[maybe_unused]] const auto last = reinterpret_cast<std::uintptr_t>(
          std::data(buffer) + std::size(buffer));

        debug::ensure(first <= pos and pos <= last);
    }

    bool do_can_alloc(std::size_t bytes,
                      std::size_t /*alignment*/) const noexcept override
    {
        return bytes < Bytes;
    }
};

inline malloc_memory_resource* get_malloc_memory_resource() noexcept
{
    static malloc_memory_resource mem;

    return &mem;
}

//! @brief A wrapper to the @c std::aligned_alloc and @c std::free cstdlib
//! function to (de)allocate memory.
//!
//! This allocator is @c std::is_empty and use the default memory resource. Use
//! this allocator in container to ensure allocator does not have size.
class default_allocator
{
public:
    using size_type         = std::size_t;
    using difference_type   = std::ptrdiff_t;
    using memory_resource_t = void;

    template<typename T>
    T* allocate(size_type n) noexcept
    {
        const auto bytes     = sizeof(T) * n;
        const auto alignment = alignof(T);

        debug::ensure(bytes > 0);
        debug::ensure((bytes % alignment) == 0);

        return reinterpret_cast<T*>(
          get_malloc_memory_resource()->allocate(bytes, alignment));
    }

    template<typename T>
    void deallocate(T* p, size_type n) noexcept
    {
        debug::ensure(p);
        debug::ensure(n > 0);

        get_malloc_memory_resource()->deallocate(p, n);
    }
};

//! @brief Use a @c irt::memory_resource in member to (de)allocate memory.
//!
//! Use this allocator and a @c irt::memory_resource_t to (de)allocate memory in
//! container.
template<typename MR>
class mr_allocator
{
public:
    using memory_resource_t = MR;
    using size_type         = std::size_t;
    using difference_type   = std::ptrdiff_t;

private:
    memory_resource_t* m_mr;

public:
    mr_allocator() noexcept = default;

    mr_allocator(memory_resource_t* mr) noexcept
      : m_mr(mr)
    {}

    template<typename T>
    T* allocate(size_type n) noexcept
    {
        return static_cast<T*>(m_mr->allocate(sizeof(T) * n, alignof(T)));
    }

    template<typename T>
    void deallocate(T* p, size_type n) noexcept
    {
        m_mr->deallocate(p, sizeof(T) * n, alignof(T));
    }

    std::byte* allocate_bytes(
      size_type bytes,
      size_type alignment = alignof(std::max_align_t)) noexcept
    {
        return static_cast<std::byte*>(
          m_mr->allocate(sizeof(std::byte) * bytes, alignment));
    }

    void deallocate_bytes(
      std::byte* pointer,
      size_type  bytes,
      size_type  alignment = alignof(std::max_align_t)) noexcept
    {
        m_mr->deallocate(pointer, bytes, alignment);
    }

    bool can_alloc_bytes(
      size_type bytes,
      size_type alignment = alignof(std::max_align_t)) noexcept
    {
        return m_mr->can_alloc(bytes, alignment);
    }

    template<typename T>
    bool can_alloc(std::size_t element_count) const noexcept
    {
        return m_mr->can_alloc(sizeof(T) * element_count, alignof(T));
    }
};

////////////////////////////////////////////////////////////////////////
//                                                                    //
// Memory resource: linear, stack, stack-list........................ //
//                                                                    //
////////////////////////////////////////////////////////////////////////

//! Return the smallest closest value of `v` divisible by `alignment`.
//!
//! ~~~{.cpp}
//! assert(make_divisible_to(123, 16) == 112);
//! assert(make_divisible_to(17, 16) == 16);
//! assert(make_divisible_to(161, 16) == 160);
//! assert(make_divisible_to(7, 16) == 0);
//! assert(make_divisible_to(15, 16) == 0);
//! assert(make_divisible_to(123, 8) == 120);
//! assert(make_divisible_to(17, 8) == 16);
//! assert(make_divisible_to(161, 8) == 160);
//! assert(make_divisible_to(7, 8) == 0);
//! assert(make_divisible_to(15, 8) == 8);
//! ~~~
inline constexpr auto make_divisible_to(
  const std::integral auto v,
  const std::size_t        alignment = alignof(std::max_align_t)) noexcept
  -> decltype(v)
{
    using return_type = decltype(v);

    if constexpr (std::is_signed_v<return_type>)
        return static_cast<return_type>(
          static_cast<std::intptr_t>(v) &
          static_cast<std::intptr_t>(~(alignment - 1)));
    else
        return static_cast<return_type>(static_cast<std::size_t>(v) &
                                        ~(alignment - 1));
}

inline constexpr auto calculate_padding(const std::uintptr_t address,
                                        const std::size_t    alignment) noexcept
  -> std::size_t
{
    const auto multiplier      = (address / alignment) + 1u;
    const auto aligned_address = multiplier * alignment;
    const auto padding         = aligned_address - address;

    return static_cast<std::size_t>(padding);
}

inline constexpr auto calculate_padding_with_header(
  const std::uintptr_t address,
  const std::size_t    alignment,
  const std::size_t    header_size) noexcept -> std::size_t
{
    auto padding      = calculate_padding(address, alignment);
    auto needed_space = header_size;

    if (padding < needed_space) {
        needed_space -= padding;

        if (needed_space % alignment > 0) {
            padding += alignment * (1 + (needed_space / alignment));
        } else {
            padding += alignment * (needed_space / alignment);
        }
    }

    return padding;
}

//! A non-thread-safe allocator: allocation are linear, no de-allocation.
//!
//! This is a non-thread-safe, fast, special-purpose resource that gets
//! memory  from a preallocated buffer, but doesn’t release it with
//! deallocation. It can only grow. The main idea is to keep a pointer at the
//! first memory address of your memory chunk and move it every time an
//! allocation is done. In this memory resource, the internal fragmentation is
//! kept to a minimum because all elements are sequentially (spatial locality)
//! inserted and the only fragmentation between them is the alignment. Due to
//! its simplicity, this allocator doesn't allow specific positions of memory to
//! be freed. Usually, all memory is freed together.
class fixed_linear_memory_resource
{
public:
    static constexpr bool is_relocatable = false;

    fixed_linear_memory_resource() noexcept = default;
    fixed_linear_memory_resource(std::byte* data, std::size_t size) noexcept;

    void* allocate(size_t bytes, size_t alignment) noexcept;
    void  deallocate(void* /*p*/,
                     size_t /*bytes*/,
                     size_t /*alignment*/) noexcept
    {}

    /** Release memory provides in contructor or in release memory. */
    void destroy() noexcept;

    //! @brief Reset the use of the chunk of memory.
    void reset() noexcept;

    //! @brief Assign a chunk of memory.
    //!
    //! @attention Use this function only when no chunk of memory are allocated
    //! (ie the default constructor was called).
    //! @param data The new buffer. Must be not null.
    //! @param size The size of the buffer. Must be not null.
    void reset(std::byte* data, std::size_t size) noexcept;

    //! Check if the resource can allocate @c bytes with @c alignment.
    //!
    //! @attention Use this function before using @c do_allocate to be sure
    //!     the @c memory_resource can allocate enough memory bcause @c
    //!     do_allocate will use @c std::quick_exit or @c std::terminate to stop
    //!     the application.
    bool can_alloc(std::size_t bytes, std::size_t alignment) noexcept;

    std::byte* head() noexcept { return m_start; }

private:
    std::byte*  m_start{};
    std::size_t m_total_size{};
    std::size_t m_offset{};
};

//! A non-thread-safe allocator: node specific memory resource.
//!
//! This pool allocator splits the big memory chunk in smaller chunks of the
//! same size and keeps track of which of them are free. When an allocation is
//! requested it returns the free chunk size. When a freed is done, it just
//! stores it to be used in the next allocation. This way, allocations work
//! super fast and the fragmentation is still very low.
class pool_memory_resource
{
private:
    template<class T>
    struct list {
        struct node {
            T     data;
            node* next;
        };

        node* head = {};

        list() noexcept = default;

        void push(node* new_node) noexcept
        {
            new_node->next = head;
            head           = new_node;
        }

        node* pop() noexcept
        {
            node* top = head;
            head      = head->next;
            return top;
        }
    };

    struct header {};

    using node = list<header>::node;

    list<header> m_free_list;

    std::byte*  m_start_ptr       = nullptr;
    std::size_t m_total_size      = {};
    std::size_t m_chunk_size      = {};
    std::size_t m_total_allocated = {};

public:
    pool_memory_resource() noexcept = default;

    pool_memory_resource(std::byte*  data,
                         std::size_t size,
                         std::size_t chunk_size) noexcept;

    void* allocate(size_t bytes, size_t /*alignment*/) noexcept;

    void deallocate(void* p, size_t /*bytes*/, size_t /*alignment*/) noexcept;

    //! @brief Reset the use of the chunk of memory.
    void reset() noexcept;

    //! @brief Assign a chunk of memory.
    //!
    //! @attention Use this function only when no chunk of memory are allocated
    //! (ie the default constructor was called).
    //! @param data The new buffer. Must be not null.
    //! @param size The size of the buffer. Must be not null.
    //! @param chunk_size The size of the chun
    void reset(std::byte*  data,
               std::size_t size,
               std::size_t chunk_size) noexcept;

    //! Check if the resource can allocate @c bytes with @c alignment.
    //!
    //! @attention Use this function before using @c do_allocate to be sure
    //!     the @c memory_resource can allocate enough memory bcause @c
    //!     do_allocate will use @c std::quick_exit or @c std::terminate to stop
    //!     the application.
    bool can_alloc(std::size_t bytes, std::size_t /*alignment*/) noexcept;

    constexpr std::byte* head() noexcept;
};

//! A non-thread-safe allocator: a general purpose allocator.
//!
//! This memory resource doesn't impose any restriction. It allows
//! allocations and deallocations to be done in any order. For this reason,
//! its performance is not as good as its predecessors.
class freelist_memory_resource
{
public:
    enum class find_policy { find_first, find_best };

private:
    struct FreeHeader {
        std::size_t block_size;
    };

    struct AllocationHeader {
        std::size_t  block_size;
        std::uint8_t padding;
    };

    template<class T>
    class list
    {
    public:
        struct node {
            T     data;
            node* next;
        };

        node* head = nullptr;

        list() noexcept = default;

        void insert(node* previous, node* new_node) noexcept
        {
            if (previous == nullptr) {
                if (head != nullptr) {
                    new_node->next = head;
                } else {
                    new_node->next = nullptr;
                }
                head = new_node;
            } else {
                if (previous->next == nullptr) {
                    previous->next = new_node;
                    new_node->next = nullptr;
                } else {
                    new_node->next = previous->next;
                    previous->next = new_node;
                }
            }
        }

        void remove(node* previous, node* to_delete) noexcept
        {
            if (previous == nullptr) {
                if (to_delete->next == nullptr) {
                    head = nullptr;
                } else {
                    head = to_delete->next;
                }
            } else {
                previous->next = to_delete->next;
            }
        }
    };

    using node = list<FreeHeader>::node;

    static constexpr auto allocation_header_size = sizeof(AllocationHeader);
    static constexpr auto free_header_size       = sizeof(FreeHeader);

    list<FreeHeader> m_freeList;
    std::byte*       m_start_ptr   = nullptr;
    std::size_t      m_total_size  = 0;
    std::size_t      m_used        = 0;
    std::size_t      m_peak        = 0;
    find_policy      m_find_policy = find_policy::find_first;

    struct find_t {
        node*       previous  = nullptr;
        node*       allocated = nullptr;
        std::size_t padding   = 0u;
    };

public:
    freelist_memory_resource() noexcept = default;

    freelist_memory_resource(std::byte* data, std::size_t size) noexcept;

    void* allocate(size_t size, size_t alignment) noexcept;

    void deallocate(void* ptr, size_t /*bytes*/, size_t /*alignment*/) noexcept;

    /** Release memory provides in contructor or in release memory. */
    void destroy() noexcept;

    /** Reset the use of the chunk of memory. If the chunk is undefined do
     * nothing.
     * @attention Release of all memory of container/memory_resource using old
     * chunk. */
    void reset() noexcept;

    /** Assign a new chunk of memory to the memory resource. If the new chunk is
     * undefined do nothing.
     * @attention Release of all memory of container/memory_resource using old
     * chunk.
     * @param data The new buffer. Must be not null.
     * @param size The size of the buffer. Must be not null.
     */
    void reset(std::byte* data, std::size_t size) noexcept;

    //! Get the pointer to the allocated memory provided in `constructor` or in
    //! `reset` functions.
    //!
    //! @return Can return `nullptr` or a valid pointer.
    constexpr std::byte* head() const noexcept { return m_start_ptr; }

    //! Get the size of the allocated memory provided in `constructor` or in
    //! `reset` functions.
    //!
    //! @return Can return `0` or a valid size.
    constexpr std::size_t capacity() const noexcept { return m_total_size; }

private:
    void merge(node* previous, node* free_node) noexcept;

    auto find_best(const std::size_t size,
                   const std::size_t alignment) const noexcept -> find_t;

    auto find_first(const std::size_t size,
                    const std::size_t alignment) const noexcept -> find_t;
};

using freelist_allocator = mr_allocator<freelist_memory_resource>;
using pool_allocator     = mr_allocator<pool_memory_resource>;
using fixed_allocator    = mr_allocator<fixed_linear_memory_resource>;

////////////////////////////////////////////////////////////////////////
//                                                                    //
// Container: vector, data_array and list............................ //
//                                                                    //
////////////////////////////////////////////////////////////////////////

//! @brief A vector like class with dynamic allocation.
//! @tparam T Any type (trivial or not).
template<typename T, typename A = default_allocator>
class vector
{
public:
    using value_type        = T;
    using size_type         = std::uint32_t;
    using index_type        = std::make_signed_t<size_type>;
    using iterator          = T*;
    using const_iterator    = const T*;
    using reference         = T&;
    using const_reference   = const T&;
    using pointer           = T*;
    using const_pointer     = const T*;
    using allocator_type    = A;
    using memory_resource_t = typename A::memory_resource_t;
    using this_container    = vector<T, A>;

private:
    static_assert(std::is_nothrow_destructible_v<T> ||
                  std::is_trivially_destructible_v<T>);

    T* m_data = nullptr;

    [[no_unique_address]] allocator_type m_alloc;

    index_type m_size     = 0;
    index_type m_capacity = 0;

public:
    constexpr vector() noexcept;
    vector(std::integral auto capacity) noexcept;
    vector(std::integral auto capacity, std::integral auto size) noexcept;
    vector(std::integral auto capacity,
           std::integral auto size,
           const T&           default_value) noexcept;

    constexpr vector(memory_resource_t* mem) noexcept
        requires(!std::is_empty_v<A>);

    vector(memory_resource_t* mem, std::integral auto capacity) noexcept
        requires(!std::is_empty_v<A>);
    vector(memory_resource_t* mem,
           std::integral auto capacity,
           std::integral auto size) noexcept
        requires(!std::is_empty_v<A>);
    vector(memory_resource_t* mem,
           std::integral auto capacity,
           std::integral auto size,
           const T&           default_value) noexcept
        requires(!std::is_empty_v<A>);

    ~vector() noexcept;

    constexpr vector(const vector& other) noexcept;
    constexpr vector& operator=(const vector& other) noexcept;
    constexpr vector(vector&& other) noexcept;
    constexpr vector& operator=(vector&& other) noexcept;

    bool resize(std::integral auto size) noexcept;
    bool reserve(std::integral auto new_capacity) noexcept;

    void destroy() noexcept; // clear all elements and free memory (size =
                             // 0, capacity = 0 after).

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
    static_assert(std::is_enum<Identifier>::value,
                  "Identifier must be a enumeration");

    if constexpr (std::is_same_v<std::uint32_t,
                                 std::underlying_type_t<Identifier>>)
        return static_cast<std::uint16_t>(static_cast<std::uint32_t>(id) &
                                          0xffff);
    else
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(id) &
                                          0xffffffff);
}

template<typename Identifier>
constexpr auto get_key(Identifier id) noexcept
{
    static_assert(std::is_enum<Identifier>::value,
                  "Identifier must be a enumeration");

    if constexpr (std::is_same_v<std::uint32_t,
                                 std::underlying_type_t<Identifier>>)
        return static_cast<std::uint16_t>(
          (static_cast<std::uint32_t>(id) >> 16) & 0xffff);
    else
        return static_cast<std::uint32_t>(
          (static_cast<std::uint64_t>(id) >> 32) & 0xffffffff);
}

template<typename Identifier>
constexpr bool is_valid(Identifier id) noexcept
{
    return get_key(id) > 0;
}

//! @brief An optimized array to store unique identifier.
//!
//! A container to handle only identifier.
//! - linear memory/iteration
//! - O(1) alloc/free
//! - stable indices
//! - weak references
//! - zero overhead dereferences
//!
//! @tparam Identifier A enum class identifier to store identifier unsigned
//!     number.
//! @todo Make Identifier split key|id automatically for all unsigned.
template<typename Identifier, typename A = default_allocator>
class id_array
{
    static_assert(std::is_enum_v<Identifier>,
                  "Identifier must be a enumeration: enum class id : "
                  "std::uint32_t or std::uint64_t;");

    static_assert(
      std::is_same_v<std::underlying_type_t<Identifier>, std::uint32_t> ||
        std::is_same_v<std::underlying_type_t<Identifier>, std::uint64_t>,
      "Identifier underlying type must be std::uint32_t or std::uint64_t");

    using underlying_id_type = std::conditional_t<
      std::is_same_v<std::uint32_t, std::underlying_type_t<Identifier>>,
      std::uint32_t,
      std::uint64_t>;

    using index_type =
      std::conditional_t<std::is_same_v<std::uint32_t, underlying_id_type>,
                         std::uint16_t,
                         std::uint32_t>;

    using identifier_type   = Identifier;
    using value_type        = Identifier;
    using this_container    = id_array<Identifier, A>;
    using allocator_type    = A;
    using memory_resource_t = typename A::memory_resource_t;

    static constexpr index_type none = std::numeric_limits<index_type>::max();

private:
    vector<Identifier, A> m_items;

    index_type m_valid_item_number = 0; /**< Number of valid item. */
    index_type m_next_key          = 1; /**< [1..2^[16|32] - 1 (never == 0). */
    index_type m_free_head         = none; // index of first free entry

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
    constexpr id_array() noexcept = default;

    constexpr id_array(memory_resource_t* mem) noexcept
        requires(!std::is_empty_v<A>);

    constexpr id_array(std::integral auto capacity) noexcept
        requires(std::is_empty_v<A>);

    constexpr id_array(memory_resource_t* mem,
                       std::integral auto capacity) noexcept
        requires(!std::is_empty_v<A>);

    constexpr ~id_array() noexcept = default;

    constexpr Identifier                alloc() noexcept;
    constexpr std::optional<Identifier> try_alloc() noexcept;

    constexpr bool reserve(std::integral auto capacity) noexcept;
    constexpr void free(const Identifier id) noexcept;
    constexpr void clear() noexcept;
    constexpr void destroy() noexcept;

    constexpr std::optional<index_type> get(const Identifier id) const noexcept;
    constexpr bool exists(const Identifier id) const noexcept;

    //! @brief Checks if a `id` exists in the underlying array at index `index`.
    //!
    //! @attention This function can check the version of the identifier only
    //! the `is_defined<Identifier>` is used to detect the `Identifier`.
    //!
    //! @param index A integer.
    //! @return The `Identifier` found at index `index` or `std::nullopt`
    //! otherwise.
    constexpr identifier_type get_from_index(
      std::integral auto index) const noexcept;

    constexpr bool next(const Identifier*& idx) const noexcept;

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

            for (const auto e = self->m_items.size(); index < e; ++index) {
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

            for (const auto e = self->m_items.size(); index < e; ++index) {
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

//! @brief An optimized SOA structure to store unique identifier and mutiples
//! vector.
//!
//! A container to handle only identifier.
//! - linear memory/iteration
//! - O(1) alloc/free
//! - stable indices
//! - weak references
//! - zero overhead dereferences
//!
//! @code
//! struct pos3d {
//!     float x, y, z;
//! };
//!
//! struct color {
//!     std::uint32_t rgba;
//! };
//!
//! using name = irt::small_string<15>;
//!
//! enum class ex1_id : uint32_t;
//!
//! irt::id_data_array<ex1_id, irt::default_allocator, pos3d, color, name>
//!   d;
//! d.reserve(1024);
//! expect(ge(d.capacity(), 1024u));
//! expect(fatal(d.can_alloc(1)));
//!
//! const auto id =
//!   d.alloc([](const auto /*id*/, auto& p, auto& c, auto& n) noexcept {
//!       p = pos3d(1.f, 2.f, 3.f);
//!       c = color{ 123u };
//!       n = "HelloWorld!";
//!   });
//! @endcode
//!
//! @tparam Identifier A enum class identifier to store identifier unsigned
//!     number.
template<typename Identifier, typename A = irt::default_allocator, class... Ts>
class id_data_array
{
    static_assert(std::is_enum_v<Identifier>,
                  "Identifier must be a enumeration: enum class id : "
                  "std::uint32_t or std::uint64_t;");

    static_assert(
      std::is_same_v<std::underlying_type_t<Identifier>, std::uint32_t> ||
        std::is_same_v<std::underlying_type_t<Identifier>, std::uint64_t>,
      "Identifier underlying type must be std::uint32_t or std::uint64_t");

    using underlying_id_type = std::conditional_t<
      std::is_same_v<std::uint32_t, std::underlying_type_t<Identifier>>,
      std::uint32_t,
      std::uint64_t>;

    using index_type =
      std::conditional_t<std::is_same_v<std::uint32_t, underlying_id_type>,
                         std::uint16_t,
                         std::uint32_t>;

    using identifier_type   = Identifier;
    using value_type        = Identifier;
    using allocator_type    = A;
    using memory_resource_t = typename A::memory_resource_t;

    id_array<identifier_type, A> m_ids;
    std::tuple<vector<Ts, A>...> m_col;

private:
    template<typename Function, std::size_t... Is>
    void do_call_fn(Function&&            fn,
                    const identifier_type id,
                    std::index_sequence<Is...>) noexcept
    {
        const auto idx = get_index(id);

        fn(id, std::get<Is>(m_col).operator[](idx)...);
    }

    template<typename Function, std::size_t... Is>
    void do_call_fn(Function&&            fn,
                    const identifier_type id,
                    std::index_sequence<Is...>) const noexcept
    {
        const auto idx = get_index(id);

        fn(id, std::get<Is>(m_col).operator[](idx)...);
    }

    template<typename Function, std::size_t... Is>
    void do_call_alloc_fn(Function&&            fn,
                          const identifier_type id,
                          std::index_sequence<Is...>) noexcept
    {
        const auto idx = get_index(id);

        fn(id, std::get<Is>(m_col).operator[](idx)...);
    }

    template<std::size_t... Is>
    void do_resize(std::index_sequence<Is...>,
                   const std::integral auto len) noexcept
    {
        (std::get<Is>(m_col).resize(len), ...);
    }

public:
    identifier_type alloc() noexcept;

    template<typename Function>
    identifier_type alloc(Function&& fn) noexcept;

    /** Return the identifier type at index `idx` if and only if `idx` is in
     * range `[0, size()[` and if the value `is_defined` otherwise return
     * `undefined<identifier_type>()`. */
    identifier_type get_id(std::integral auto idx) const noexcept;

    /** Release the @c identifier from the @c id_array. @attention To improve
     * memory access, the destructors of underlying objects in @c std::tuple of
     * @c vector are not called. If you need to realease memory use it before
     * releasing the identifier but these objects can be reuse in future. In any
     * case all destructor will free the memory in @c id_data_array destructor
     * or during the @c reserve() operation.
     *
     * @example
     * id_data_array<obj_id, default_allocator, std::string, position> m;
     *
     * if (m.exists(id))
     *     std::swap(m.get<std::string>()[get_index(id)], std::string{});
     * @endexample */
    void free(const identifier_type id) noexcept;

    /** Get the underlying @c vector in @c tuple using an index. (read @c
     * std::get). */
    template<std::size_t Index>
    auto& get() noexcept;

    /** Get the underlying @c vector in @c tuple using a type (read @c
     * std::get). */
    template<typename Type>
    auto& get() noexcept
        requires(not std::is_integral_v<Type>);

    /** Get the underlying object at position @c id @c vector in @c tuple using
     * an index. (read @c std::get). */
    template<std::size_t Index>
    auto& get(const identifier_type id) noexcept;

    /** Get the underlying object at position @c id in @c vector in @c tuple
     * using a type (read @c std::get). */
    template<typename Type>
    auto& get(const identifier_type id) noexcept
        requires(not std::is_integral_v<Type>);

    /** Get the underlying @c vector in @c tuple using an index. (read @c
     * std::get). */
    template<std::size_t Index>
    auto& get() const noexcept;

    /** Get the underlying @c vector in @c tuple using a type (read @c
     * std::get). */
    template<typename Type>
    auto& get() const noexcept
        requires(not std::is_integral_v<Type>);

    /** Get the underlying object at position @c id @c vector in @c tuple using
     * an index. (read @c std::get). */
    template<std::size_t Index>
    auto& get(const identifier_type id) const noexcept;

    /** Get the underlying object at position @c id in @c vector in @c tuple
     * using a type (read @c std::get). */
    template<typename Type>
    auto& get(const identifier_type id) const noexcept
        requires(not std::is_integral_v<Type>);

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

    void clear() noexcept;
    void reserve(std::integral auto len) noexcept;

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
//! @tparam Identifier A enum class identifier to store identifier unsigned
//!     number.
//! @todo Make Identifier split key|id automatically for all unsigned.
template<typename T, typename Identifier, typename A = default_allocator>
class data_array
{
public:
    static_assert(std::is_enum_v<Identifier>,
                  "Identifier must be a enumeration: enum class id : "
                  "std::uint32_t or std::uint64_t;");

    static_assert(
      std::is_same_v<std::underlying_type_t<Identifier>, std::uint32_t> ||
        std::is_same_v<std::underlying_type_t<Identifier>, std::uint64_t>,
      "Identifier underlying type must be std::uint32_t or std::uint64_t");

    using underlying_id_type = std::conditional_t<
      std::is_same_v<std::uint32_t, std::underlying_type_t<Identifier>>,
      std::uint32_t,
      std::uint64_t>;

    using index_type =
      std::conditional_t<std::is_same_v<std::uint32_t, underlying_id_type>,
                         std::uint16_t,
                         std::uint32_t>;

    using identifier_type   = Identifier;
    using value_type        = T;
    using this_container    = data_array<T, Identifier, A>;
    using allocator_type    = A;
    using memory_resource_t = typename A::memory_resource_t;

private:
    struct item {
        T          item;
        Identifier id;
    };

public:
    using internal_value_type = item;

    item* m_items = nullptr; // items array

    [[no_unique_address]] allocator_type m_alloc;

    index_type m_max_size  = 0;    // number of valid item
    index_type m_max_used  = 0;    // highest index ever allocated
    index_type m_capacity  = 0;    // capacity of the array
    index_type m_next_key  = 1;    // [1..2^32-64] (never == 0)
    index_type m_free_head = none; // index of first free entry

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

    constexpr data_array() noexcept = default;

    constexpr data_array(memory_resource_t* mem) noexcept
        requires(!std::is_empty_v<A>);

    constexpr data_array(std::integral auto capacity) noexcept
        requires(std::is_empty_v<A>);

    constexpr data_array(memory_resource_t* mem,
                         std::integral auto capacity) noexcept
        requires(!std::is_empty_v<A>);

    constexpr ~data_array() noexcept;

    //! Try to reserve more memory than current capacity.
    //!
    //! @param capacity The new capacity. Do nothing if this @c capacity
    //! parameter is less or equal than the current @c capacity().
    //!
    //! @attention  If the `reserve()` succedded a reallocation takes
    //! place, in which case all iterators (including the end() iterator)
    //! and all references to the elements are invalidated.
    //!
    //! @attention Use `can_alloc()` to be sure `reserve()` succeed.
    void reserve(std::integral auto capacity) noexcept;

    //! Try to grow the capacity of memory.
    //!
    //! - current capacity equals 0 then tries to reserve 8 elements.
    //! - current capacity equals size then tries to reserve capacity * 2
    //!   elements.
    //! - else tries to reserve capacity * 3 / 2 elements.
    //!
    //! @attention  If the `grow()` succedded a reallocation takes
    //! place, in which case all iterators (including the end() iterator)
    //! and all references to the elements are invalidated.
    //!
    //! @attention Use `can_alloc()` to be sure `grow()` succeed.
    void grow() noexcept;

    //! @brief Destroy all items in the data_array but keep memory
    //!  allocation.
    //!
    //! @details Run the destructor if @c T is not trivial on outstanding
    //!  items and re-initialize the size.
    void clear() noexcept;

    //! @brief Destroy all items in the data_array and release memory.
    //!
    //! @details Run the destructor if @c T is not trivial on outstanding
    //!  items and re-initialize the size.
    void destroy() noexcept;

    //! @brief Alloc a new element.
    //!
    //! If m_max_size == m_capacity then this function will abort. Before
    //! using this function, tries @c !can_alloc() for example otherwise use
    //! the @c try_alloc function.
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
    //! @return A pair with a boolean if the allocation success and a
    //! pointer to the newly element.
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
            pointer next    = self->try_to_get(id);
            bool    success = self->next(next);
            id              = success ? self->get_id(*next) : identifier_type{};

            return *this;
        }

        iterator_base operator++(int) noexcept
        {
            auto    old_id  = id;
            pointer next    = self->try_to_get(id);
            bool    success = self->next(next);
            id              = success ? self->get_id(*next) : identifier_type{};

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
template<typename T, typename A = default_allocator>
class ring_buffer
{
public:
    using value_type        = T;
    using size_type         = std::uint32_t;
    using index_type        = std::make_signed_t<size_type>;
    using reference         = T&;
    using const_reference   = const T&;
    using pointer           = T*;
    using const_pointer     = const T*;
    using this_container    = ring_buffer<T, A>;
    using allocator_type    = A;
    using memory_resource_t = typename A::memory_resource_t;

    static_assert((std::is_nothrow_constructible_v<T> ||
                   std::is_nothrow_move_constructible_v<
                     T>)&&std::is_nothrow_destructible_v<T>);

private:
    T*                                   buffer = nullptr;
    [[no_unique_address]] allocator_type m_alloc;
    index_type                           m_head     = 0;
    index_type                           m_tail     = 0;
    index_type                           m_capacity = 0;

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
    constexpr ring_buffer(std::integral auto capacity) noexcept;
    constexpr ring_buffer(memory_resource_t* mem) noexcept
        requires(!std::is_empty_v<A>);

    constexpr ring_buffer(memory_resource_t* mem,
                          std::integral auto capacity) noexcept
        requires(!std::is_empty_v<A>);

    constexpr ~ring_buffer() noexcept;

    constexpr ring_buffer(const ring_buffer& rhs) noexcept;
    constexpr ring_buffer& operator=(const ring_buffer& rhs) noexcept;
    constexpr ring_buffer(ring_buffer&& rhs) noexcept;
    constexpr ring_buffer& operator=(ring_buffer&& rhs) noexcept;

    constexpr void swap(ring_buffer& rhs) noexcept;
    constexpr void clear() noexcept;
    constexpr void destroy() noexcept;
    constexpr void reserve(std::integral auto capacity) noexcept;

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

    constexpr bool operator==(const std::string_view rhs) const noexcept;
    constexpr bool operator!=(const std::string_view rhs) const noexcept;
    constexpr bool operator>(const std::string_view rhs) const noexcept;
    constexpr bool operator<(const std::string_view rhs) const noexcept;
    constexpr bool operator==(const char* rhs) const noexcept;
    constexpr bool operator!=(const char* rhs) const noexcept;
    constexpr bool operator>(const char* rhs) const noexcept;
    constexpr bool operator<(const char* rhs) const noexcept;
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
    constexpr ~small_vector() noexcept;

    constexpr small_vector(const small_vector& other) noexcept;
    constexpr small_vector& operator=(const small_vector& other) noexcept;
    constexpr small_vector(small_vector&& other) noexcept
        requires(std::is_move_constructible_v<T>);
    constexpr small_vector& operator=(small_vector&& other) noexcept
        requires(std::is_move_assignable_v<T>);

    constexpr void resize(std::integral auto capacity) noexcept;
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

    constexpr iterator erase(std::integral auto index) noexcept;
    constexpr iterator erase(iterator to_del) noexcept;
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
template<typename T, int length>
class small_ring_buffer
{
public:
    static_assert(length >= 1);
    static_assert((std::is_nothrow_constructible_v<T> ||
                   std::is_nothrow_move_constructible_v<
                     T>)&&std::is_nothrow_destructible_v<T>);

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
    constexpr bitflags(unsigned long long val) noexcept;

    constexpr bitflags(std::same_as<EnumT> auto... args) noexcept;

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
    if constexpr (std::is_same_v<std::uint16_t, index_type>)
        return static_cast<identifier_type>(
          (static_cast<std::uint32_t>(key) << 16) |
          static_cast<std::uint32_t>(index));
    else
        return static_cast<identifier_type>(
          (static_cast<std::uint64_t>(key) << 32) |
          static_cast<std::uint64_t>(index));
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::index_type
id_array<Identifier, A>::make_next_key(index_type key) noexcept
{
    if constexpr (std::is_same_v<std::uint16_t, index_type>)
        return key == 0xffffffff ? 1u : key + 1u;
    else
        return key == 0xffffffffffffffff ? 1u : key + 1;
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::index_type
id_array<Identifier, A>::get_key(Identifier id) noexcept
{
    if constexpr (std::is_same_v<std::uint16_t, index_type>)
        return static_cast<std::uint16_t>(
          (static_cast<std::uint32_t>(id) >> 16) & 0xffff);
    else
        return static_cast<std::uint32_t>(
          (static_cast<std::uint64_t>(id) >> 32) & 0xffffffff);
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::index_type
id_array<Identifier, A>::get_index(Identifier id) noexcept
{
    if constexpr (std::is_same_v<std::uint16_t, index_type>)
        return static_cast<std::uint16_t>(static_cast<std::uint32_t>(id) &
                                          0xffff);
    else
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(id) &
                                          0xffffffff);
}

template<typename Identifier, typename A>
constexpr id_array<Identifier, A>::id_array(memory_resource_t* mem) noexcept
    requires(!std::is_empty_v<A>)
  : m_items{ mem }
{}

template<typename Identifier, typename A>
constexpr id_array<Identifier, A>::id_array(
  std::integral auto capacity) noexcept
    requires(std::is_empty_v<A>)
  : m_items{ capacity }
{}

template<typename Identifier, typename A>
constexpr id_array<Identifier, A>::id_array(
  memory_resource_t* mem,
  std::integral auto capacity) noexcept
    requires(!std::is_empty_v<A>)
  : m_items{ mem, capacity }
{}

template<typename Identifier, typename A>
constexpr Identifier id_array<Identifier, A>::alloc() noexcept
{
    debug::ensure(can_alloc(1) &&
                  "check alloc() with full() before using use.");

    if (m_free_head != none) {
        index_type new_index = m_free_head;
        if (is_valid(m_items[m_free_head]))
            m_free_head = none;
        else
            m_free_head = get_index(m_items[m_free_head]);

        m_items[new_index] = make_id(m_next_key, new_index);
        m_next_key         = make_next_key(m_next_key);
        ++m_valid_item_number;
        return m_items[new_index];
    } else {
        m_items.emplace_back(
          make_id(m_next_key, static_cast<index_type>(m_items.size())));
        m_next_key = make_next_key(m_next_key);
        ++m_valid_item_number;
        return m_items.back();
    }
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
    return m_items.reserve(capacity);
}

template<typename Identifier, typename A>
constexpr void id_array<Identifier, A>::free(const Identifier id) noexcept
{
    const auto index = get_index(id);

    debug::ensure(std::cmp_less_equal(0, index));
    debug::ensure(std::cmp_less(index, m_items.size()));
    debug::ensure(m_items[index] == id);
    debug::ensure(is_valid(id));

    m_items[index] = static_cast<Identifier>(m_free_head);
    m_free_head    = static_cast<index_type>(index);

    --m_valid_item_number;
}

template<typename Identifier, typename A>
constexpr void id_array<Identifier, A>::clear() noexcept
{
    m_items.clear();
    m_valid_item_number = 0;
    m_free_head         = none;
}

template<typename Identifier, typename A>
constexpr void id_array<Identifier, A>::destroy() noexcept
{
    m_items.clear();
    m_valid_item_number = 0;
    m_free_head         = none;

    m_items.destroy();
}

template<typename Identifier, typename A>
constexpr std::optional<typename id_array<Identifier, A>::index_type>
id_array<Identifier, A>::get(const Identifier id) const noexcept
{
    const auto index = get_index(id);

    debug::ensure(std::cmp_less_equal(0, index));
    debug::ensure(std::cmp_less(index, m_items.size()));

    return m_items[index] == id ? std::make_optional(index) : std::nullopt;
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::exists(
  const Identifier id) const noexcept
{
    const auto index = get_index(id);

    return std::cmp_greater_equal(index, 0u) and
           std::cmp_less(index, m_items.size()) and m_items[index] == id;
}

template<typename Identifier, typename A>
constexpr typename id_array<Identifier, A>::identifier_type
id_array<Identifier, A>::get_from_index(std::integral auto index) const noexcept
{
    if (std::cmp_greater_equal(index, 0u) and
        std::cmp_less(index, m_items.size()) and is_defined(m_items[index]) and
        std::cmp_equal(get_index(m_items[index]), index))
        return m_items[index];

    return undefined<identifier_type>();
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::next(
  const Identifier*& id) const noexcept
{
    if (id) {
        auto index = get_index(*id);
        ++index;

        for (const auto e = m_items.size(); index < e; ++index) {
            if (is_valid(m_items[index])) {
                id = &m_items[index];
                return true;
            }
        }
    } else {
        for (auto index = 0, e = m_items.ssize(); index < e; ++index) {
            if (is_valid(m_items[index])) {
                id = &m_items[index];
                return true;
            }
        }
    }

    return false;
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::full() const noexcept
{
    return m_free_head == none && m_items.ssize() == m_items.capacity();
}

template<typename Identifier, typename A>
constexpr unsigned id_array<Identifier, A>::size() const noexcept
{
    return m_valid_item_number;
}

template<typename Identifier, typename A>
constexpr int id_array<Identifier, A>::ssize() const noexcept
{
    return m_valid_item_number;
}

template<typename Identifier, typename A>
constexpr bool id_array<Identifier, A>::can_alloc(
  std::integral auto nb) const noexcept
{
    return std::cmp_greater_equal(m_items.capacity() - m_valid_item_number, nb);
}

template<typename Identifier, typename A>
constexpr int id_array<Identifier, A>::max_used() const noexcept
{
    return m_items.size();
}

template<typename Identifier, typename A>
constexpr int id_array<Identifier, A>::capacity() const noexcept
{
    return m_items.capacity();
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
    for (index_type index = 0; index < m_items.size(); ++index)
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
    for (index_type index = 0; index < m_items.size(); ++index)
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
    requires(not std::is_integral_v<Type>)
{
    return std::get<vector<Type>>(m_col);
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
    requires(not std::is_integral_v<Type>)
{
    debug::ensure(m_ids.exists(id));

    return std::get<vector<Type>>(m_col)[get_index(id)];
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
    requires(not std::is_integral_v<Type>)
{
    return std::get<vector<Type>>(m_col);
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
    requires(not std::is_integral_v<Type>)
{
    debug::ensure(m_ids.exists(id));

    return std::get<vector<Type>>(m_col)[get_index(id)];
}

template<typename Identifier, typename A, class... Ts>
template<typename Index, typename Function>
void id_data_array<Identifier, A, Ts...>::if_exists_do(const identifier_type id,
                                                       Function&& fn) noexcept
{
    if constexpr (std::is_integral_v<Index>) {
        if (m_ids.exists(id))
            fn(id, std::get<Index>(m_col)[get_index(id)]);
    } else {
        if (m_ids.exists(id))
            fn(id, std::get<vector<Index>>(m_col)[get_index(id)]);
    }
}

template<typename Identifier, typename A, class... Ts>
template<typename Index, typename Function>
void id_data_array<Identifier, A, Ts...>::for_each(Function&& fn) noexcept
{
    if constexpr (std::is_integral_v<Index>) {
        for (const auto id : m_ids)
            fn(id, std::get<Index>(m_col).operator[](get_index(id)));
    } else {
        for (const auto id : m_ids)
            fn(id, std::get<vector<Index>>(m_col).operator[](get_index(id)));
    }
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
            fn(id, std::get<Index>(m_col).operator[](get_index(id)));
    } else {
        for (const auto id : m_ids)
            fn(id, std::get<vector<Index>>(m_col).operator[](get_index(id)));
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
void id_data_array<Identifier, A, Ts...>::clear() noexcept
{
    m_ids.clear();
}

template<typename Identifier, typename A, class... Ts>
void id_data_array<Identifier, A, Ts...>::reserve(
  std::integral auto len) noexcept
{
    if (std::cmp_less(capacity(), len)) {
        m_ids.reserve(len);
        do_resize(std::index_sequence_for<Ts...>(), len);
    }
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
    if constexpr (std::is_same_v<std::uint16_t, index_type>)
        return static_cast<identifier_type>(
          (static_cast<std::uint32_t>(key) << 16) |
          static_cast<std::uint32_t>(index));
    else
        return static_cast<identifier_type>(
          (static_cast<std::uint64_t>(key) << 32) |
          static_cast<std::uint64_t>(index));
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::index_type
data_array<T, Identifier, A>::make_next_key(index_type key) noexcept
{
    if constexpr (std::is_same_v<std::uint16_t, index_type>)
        return key == 0xffffffff ? 1u : key + 1u;
    else
        return key == 0xffffffffffffffff ? 1u : key + 1;
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::index_type
data_array<T, Identifier, A>::get_key(Identifier id) noexcept
{
    if constexpr (std::is_same_v<std::uint16_t, index_type>)
        return static_cast<std::uint16_t>(
          (static_cast<std::uint32_t>(id) >> 16) & 0xffff);
    else
        return static_cast<std::uint32_t>(
          (static_cast<std::uint64_t>(id) >> 32) & 0xffffffff);
}

template<typename T, typename Identifier, typename A>
constexpr typename data_array<T, Identifier, A>::index_type
data_array<T, Identifier, A>::get_index(Identifier id) noexcept
{
    if constexpr (std::is_same_v<std::uint16_t, index_type>)
        return static_cast<std::uint16_t>(static_cast<std::uint32_t>(id) &
                                          0xffff);
    else
        return static_cast<std::uint32_t>(static_cast<std::uint64_t>(id) &
                                          0xffffffff);
}

template<typename T, typename Identifier, typename A>
constexpr data_array<T, Identifier, A>::data_array(
  memory_resource_t* mem) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc{ mem }
{}

template<typename T, typename Identifier, typename A>
constexpr data_array<T, Identifier, A>::data_array(
  std::integral auto capacity) noexcept
    requires(std::is_empty_v<A>)
{
    debug::ensure(std::cmp_greater(capacity, 0));
    debug::ensure(
      std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    m_items     = m_alloc.template allocate<item>(capacity);
    m_max_size  = 0;
    m_max_used  = 0;
    m_capacity  = static_cast<index_type>(capacity);
    m_next_key  = 1;
    m_free_head = none;
}

template<typename T, typename Identifier, typename A>
constexpr data_array<T, Identifier, A>::data_array(
  memory_resource_t* mem,
  std::integral auto capacity) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{
    debug::ensure(std::cmp_greater(capacity, 0));
    debug::ensure(
      std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    m_items     = m_alloc.template allocate<item>(capacity);
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
        m_alloc.deallocate(m_items, m_capacity);
}

template<typename T, typename Identifier, typename A>
void data_array<T, Identifier, A>::reserve(std::integral auto capacity) noexcept
{
    static_assert(std::is_trivial_v<T> or std::is_move_constructible_v<T> or
                  std::is_copy_constructible_v<T>);

    debug::ensure(
      std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    if (std::cmp_equal(capacity, 0) or
        std::cmp_less_equal(capacity, m_capacity))
        return;

    item* new_buffer = m_alloc.template allocate<item>(capacity);
    if (new_buffer == nullptr)
        return;

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
        m_alloc.template deallocate<item>(m_items, m_capacity);

    m_items    = new_buffer;
    m_capacity = static_cast<index_type>(capacity);
}

template<typename T, typename Identifier, typename A>
void data_array<T, Identifier, A>::grow() noexcept
{
    if (m_capacity <= 4) {
        reserve(8);
    } else {
        const auto capacity =
          m_max_size == m_capacity ? m_capacity * 2 : m_capacity * 3 / 2;

        if (std::cmp_less(capacity, std::numeric_limits<index_type>::max()))
            reserve(capacity);
        else
            reserve(std::numeric_limits<index_type>::max());
    }
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
    m_next_key  = 1;
    m_free_head = none;
}

template<typename T, typename Identifier, typename A>
void data_array<T, Identifier, A>::destroy() noexcept
{
    clear();

    if (m_items)
        m_alloc.deallocate(m_items, m_capacity);

    m_items    = nullptr;
    m_capacity = 0;
}

template<typename T, typename Identifier, typename A>
template<typename... Args>
typename data_array<T, Identifier, A>::value_type&
data_array<T, Identifier, A>::alloc(Args&&... args) noexcept
{
    debug::ensure(can_alloc(1) &&
                  "check alloc() with full() before using use.");

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
std::pair<bool, T*> data_array<T, Identifier, A>::try_alloc(
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
    return m_items[get_index(id)].item;
}

template<typename T, typename Identifier, typename A>
const T& data_array<T, Identifier, A>::get(Identifier id) const noexcept
{
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
T* data_array<T, Identifier, A>::try_to_get(
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
constexpr vector<T, A>::vector() noexcept
{}

template<typename T, typename A>
constexpr vector<T, A>::vector(memory_resource_t* mem) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{}

template<typename T, typename A>
constexpr bool vector<T, A>::make(std::integral auto capacity) noexcept
{
    debug::ensure(std::cmp_greater(capacity, 0));
    debug::ensure(std::cmp_less(sizeof(T) * capacity,
                                std::numeric_limits<index_type>::max()));

    if (std::cmp_greater(capacity, 0)) {
        m_data = m_alloc.template allocate<T>(capacity);

        if (m_data) {
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
inline vector<T, A>::vector(std::integral auto capacity) noexcept
{
    make(capacity);
}

template<typename T, typename A>
inline vector<T, A>::vector(std::integral auto capacity,
                            std::integral auto size) noexcept
{
    make(capacity, size);
}

template<typename T, typename A>
inline vector<T, A>::vector(std::integral auto capacity,
                            std::integral auto size,
                            const T&           default_value) noexcept
{
    make(capacity, size, default_value);
}

template<typename T, typename A>
inline vector<T, A>::vector(memory_resource_t* mem,
                            std::integral auto capacity) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{
    make(capacity);
}

template<typename T, typename A>
inline vector<T, A>::vector(memory_resource_t* mem,
                            std::integral auto capacity,
                            std::integral auto size) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{
    make(capacity, size);
}

template<typename T, typename A>
inline vector<T, A>::vector(memory_resource_t* mem,
                            std::integral auto capacity,
                            std::integral auto size,
                            const T&           default_value) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{
    make(capacity, size, default_value);
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
inline void vector<T, A>::destroy() noexcept
{
    clear();

    if (m_data)
        m_alloc.template deallocate<T>(m_data, m_capacity);

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

    if (std::cmp_greater(size, m_capacity)) {
        if (!reserve(compute_new_capacity(static_cast<index_type>(size))))
            return false;
    }

    std::uninitialized_value_construct_n(data() + m_size, size - m_size);
    // std::uninitialized_default_construct_n(data() + m_size, size);

    m_size = static_cast<index_type>(size);

    return true;
}

template<typename T, typename A>
bool vector<T, A>::reserve(std::integral auto new_capacity) noexcept
{
    debug::ensure(
      std::cmp_less(new_capacity, std::numeric_limits<index_type>::max()));

    if (std::cmp_greater(new_capacity, m_capacity)) {
        T* new_data = m_alloc.template allocate<T>(new_capacity);
        if (!new_data)
            return false;

        if constexpr (std::is_move_constructible_v<T>)
            std::uninitialized_move_n(data(), m_size, new_data);
        else
            std::uninitialized_copy_n(data(), m_size, new_data);

        if (m_data)
            m_alloc.template deallocate<T>(m_data, m_capacity);

        m_data     = new_data;
        m_capacity = static_cast<index_type>(new_capacity);
    }

    return true;
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
constexpr small_vector<T, length>::small_vector(
  small_vector<T, length>&& other) noexcept
    requires(std::is_move_constructible_v<T>)
{
    std::uninitialized_move_n(other.data(), other.m_size, data());

    m_size = std::exchange(other.m_size, 0);
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
constexpr void small_vector<T, length>::resize(
  std::integral auto default_size) noexcept
{
    static_assert(std::is_nothrow_default_constructible_v<T>,
                  "T must be nothrow default constructible to use "
                  "init() function");

    debug::ensure(std::cmp_greater_equal(default_size, 0) &&
                  std::cmp_less(default_size, length));

    if (!std::cmp_greater_equal(default_size, 0))
        default_size = 0;

    if (!std::cmp_less(default_size, length))
        default_size = length - 1;

    const auto new_default_size = static_cast<size_type>(default_size);

    if (new_default_size > m_size)
        std::uninitialized_value_construct_n(data() + m_size,
                                             new_default_size - m_size);
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

    debug::ensure(can_alloc(1) &&
                  "check alloc() with full() before using use.");

    std::construct_at(&(data()[m_size]), std::forward<Args>(args)...);
    ++m_size;

    return data()[m_size - 1];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::push_back(const T& arg) noexcept
{
    debug::ensure(can_alloc(1) &&
                  "check alloc() with full() before using use.");

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
        buffer     = m_alloc.template allocate<T>(capacity);
        m_capacity = static_cast<index_type>(capacity);
    }

    return buffer != nullptr;
}

template<class T, typename A>
constexpr ring_buffer<T, A>::ring_buffer(memory_resource_t* mem) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{}

template<class T, typename A>
constexpr ring_buffer<T, A>::ring_buffer(memory_resource_t* mem,
                                         std::integral auto capacity) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{
    make(capacity);
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
            if (buffer)
                m_alloc.template deallocate<T>(buffer, m_capacity);

            if (rhs.m_capacity) {
                buffer     = m_alloc.template allocate<T>(rhs.m_capacity);
                m_capacity = rhs.m_capacity;
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
            if (buffer)
                m_alloc.template deallocate<T>(buffer, m_capacity);

            if (rhs.m_capacity > 0) {
                buffer     = m_alloc.template allocate<T>(rhs.m_capacity);
                m_capacity = rhs.m_capacity;
            }
        }

        for (auto it = rhs.begin(), et = rhs.end(); it != et; ++it)
            push_tail(*it);
    }

    return *this;
}

template<class T, typename A>
constexpr ring_buffer<T, A>::ring_buffer(ring_buffer&& rhs) noexcept
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

template<class T, typename A>
constexpr ring_buffer<T, A>& ring_buffer<T, A>::operator=(
  ring_buffer&& rhs) noexcept
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
    if (buffer)
        m_alloc.template deallocate<T>(buffer, m_capacity);
    buffer = nullptr;
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::reserve(std::integral auto capacity) noexcept
{
    debug::ensure(
      std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    if (m_capacity < capacity) {
        ring_buffer<T, A> tmp(capacity);

        if constexpr (std::is_move_constructible_v<T>)
            for (auto it = begin(), et = end(); it != et; ++it)
                tmp.emplace_tail(std::move(*it));
        else
            for (auto it = begin(), et = end(); it != et; ++it)
                tmp.push_tail(*it);

        swap(tmp);
    }
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
    std::uninitialized_move_n(rhs.data(), rhs.m_size, data());

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
{
    m_bits = val;
}

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

} // namespace irt

#endif
