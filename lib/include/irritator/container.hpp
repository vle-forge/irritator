// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_CONTAINER_2023
#define ORG_VLEPROJECT_IRRITATOR_CONTAINER_2023

#include <memory>
#include <memory_resource>
#include <type_traits>
#include <utility>

#include <cassert>
#include <cstdint>
#include <cstring>

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

//! @brief Use the @c std::pmr::get_default_resource to (de)allocate memory.
//!
//! This allocator is @c std::is_empty and use the default memory resource. Use
//! this allocator in container to ensure allocator does not have size.
class default_allocator
{
public:
    using size_type                              = std::size_t;
    using difference_type                        = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    template<typename T>
    T* allocate(size_type n) noexcept
    {
        return reinterpret_cast<T*>(std::pmr::get_default_resource()->allocate(
          n * sizeof(T), alignof(T)));
    }

    template<typename T>
    void deallocate(T* p, size_type n) noexcept
    {
        std::pmr::get_default_resource()->deallocate(
          p, n * sizeof(T), alignof(T));
    }
};

//! @brief Use a @c std::pmr::memory_resource in member to (de)allocate memory.
//!
//! Use this allocator and a subclass to @c std::pmr::memory_resource to
//! (de)allocate memory in container.
class mr_allocator
{
public:
    using size_type                              = std::size_t;
    using difference_type                        = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    mr_allocator() noexcept = default;

    mr_allocator(std::pmr::memory_resource* mem) noexcept
      : m_mem(mem)
    {}

    template<typename T>
    T* allocate(size_type n) noexcept
    {
        return reinterpret_cast<T*>(m_mem->allocate(n * sizeof(T), alignof(T)));
    }

    template<typename T>
    void deallocate(T* p, size_type n) noexcept
    {
        m_mem->deallocate(p, n * sizeof(T), alignof(T));
    }

private:
    std::pmr::memory_resource* m_mem{};
};

//! @brief Same allocator than @c mr_allocator and store debug variables.
//!
//! Use this allocator and a subclass to @c std::pmr::memory_resource to
//! (de)allocate memory in container. The total amount of memory (de)allocated
//! in bytes are stored in member variable and also the number of
//! (de)allocation.
class debug_allocator
{
public:
    using size_type                              = std::size_t;
    using difference_type                        = std::ptrdiff_t;
    using propagate_on_container_move_assignment = std::true_type;

    debug_allocator() noexcept = default;

    debug_allocator(std::pmr::memory_resource* mem) noexcept
      : m_mem(mem)
    {}

    template<typename T>
    T* allocate(size_type n) noexcept
    {
        m_total_allocated += n * sizeof(T);
        m_number_allocation++;

        return reinterpret_cast<T*>(m_mem->allocate(n * sizeof(T), alignof(T)));
    }

    template<typename T>
    void deallocate(T* p, size_type n) noexcept
    {
        m_total_deallocated += n * sizeof(T);
        m_number_deallocation++;

        m_mem->deallocate(p, n * sizeof(T), alignof(T));
    }

    std::size_t m_total_allocated{};
    std::size_t m_total_deallocated{};
    std::size_t m_number_allocation{};
    std::size_t m_number_deallocation{};

private:
    std::pmr::memory_resource* m_mem{};
};

////////////////////////////////////////////////////////////////////////
//                                                                    //
// Memory resource: linear, stack, stack-list........................ //
//                                                                    //
////////////////////////////////////////////////////////////////////////

inline auto calculate_padding(const std::uintptr_t address,
                              const std::size_t    alignment) noexcept
  -> std::size_t
{
    const auto multiplier      = (address / alignment) + 1u;
    const auto aligned_address = multiplier * alignment;
    const auto padding         = aligned_address - address;

    return static_cast<std::size_t>(padding);
}

inline auto calculate_padding_with_header(
  const std::uintptr_t address,
  const std::size_t    alignment,
  const std::size_t    header_size) noexcept -> std::size_t
{
    std::size_t padding      = calculate_padding(address, alignment);
    std::size_t needed_space = header_size;

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

//! @brief An handler type when a @c std::pmr::memory resource fail to allocate
//! memory.
//!
//! @param mr Reference @c the std::pmr::memory_resource that fail.
using error_not_enough_memory_handler =
  void(std::pmr::memory_resource& mr) noexcept;

inline error_not_enough_memory_handler* on_error_not_enough_memory = nullptr;

//! Thread unsafe allocator: allocation are linear, no de-allocation.
//!
//! The main idea is to keep a pointer at the first memory address of your
//! memory chunk and move it every time an allocation is done. In this memory
//! resource, the internal fragmentation is kept to a minimum because all
//! elements are sequentially (spatial locality) inserted and the only
//! fragmentation between them is the alignment.
//! Due to its simplicity, this allocator doesn't allow specific positions of
//! memory to be freed. Usually, all memory is freed together.
class fixed_linear_memory_resource : public std::pmr::memory_resource
{
public:
    fixed_linear_memory_resource(void* data, std::size_t size) noexcept
      : m_start{ data }
      , m_total_size{ size }
    {}

    void* do_allocate(size_t bytes, size_t alignment) override
    {
        std::size_t padding = 0;

        const auto current_address =
          reinterpret_cast<std::uintptr_t>(m_start) + m_offset;

        if (alignment != 0 && m_offset % alignment != 0)
            padding = calculate_padding(current_address, alignment);

        if (m_offset + padding + bytes > m_total_size) {
            if (on_error_not_enough_memory)
                on_error_not_enough_memory(*this);

            std::abort();
        }

        m_offset += padding;
        const auto next_address = current_address + padding;
        m_offset += bytes;

        return reinterpret_cast<void*>(next_address);
    }

    void do_deallocate(void* /*p*/,
                       size_t /*bytes*/,
                       size_t /*alignment*/) override
    {}

    bool do_is_equal(const memory_resource& other) const noexcept override
    {
        return this == &other;
    }

    void reset() noexcept { m_offset = { 0 }; }

private:
    void*       m_start{};
    std::size_t m_total_size{};
    std::size_t m_offset{};
};

//! Thread unsafe allocator: node specific memory resource.
//!
//! This pool allocator splits the big memory chunk in smaller chunks of the
//! same size and keeps track of which of them are free. When an allocation is
//! requested it returns the free chunk size. When a freed is done, it just
//! stores it to be used in the next allocation. This way, allocations work
//! super fast and the fragmentation is still very low.
class pool_memory_resource : public std::pmr::memory_resource
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

    void*       m_start_ptr  = nullptr;
    std::size_t m_total_size = {};
    std::size_t m_chunk_size = {};

public:
    pool_memory_resource(void*       data,
                         std::size_t size,
                         std::size_t chunk_size) noexcept
      : m_start_ptr{ data }
      , m_total_size{ size }
      , m_chunk_size{ chunk_size }
    {
        assert(chunk_size >= std::alignment_of_v<std::max_align_t>);
        assert(size % chunk_size == 0);
    }

    void* do_allocate(size_t bytes, size_t /*alignment*/) override
    {
        assert(bytes == m_chunk_size &&
               "Allocation size must be equal to chunk size");

        node* free_position = m_free_list.pop();

        if (free_position == nullptr) {
            if (on_error_not_enough_memory)
                on_error_not_enough_memory(*this);

            std::abort();
        }

        return reinterpret_cast<void*>(free_position);
    }

    void do_deallocate(void* p, size_t /*bytes*/, size_t /*alignment*/) override
    {
        m_free_list.push(reinterpret_cast<node*>(p));
    }

    void reset() noexcept
    {
        const std::size_t n = m_total_size / m_chunk_size;
        for (std::size_t i = 0; i < n; ++i) {
            auto address =
              reinterpret_cast<std::uintptr_t>(m_start_ptr) + i * m_chunk_size;
            m_free_list.push(reinterpret_cast<node*>(address));
        }
    }
};

//! Thread unsafe allocator: 5 size-node specific memory resource.
//!
//! This pool allocator splits the big memory chunk in smaller chunks of the
//! same size and keeps track of which of them are free. When an allocation is
//! requested it returns the free chunk size. When a freed is done, it just
//! stores it to be used in the next allocation. This way, allocations work
//! super fast and the fragmentation is still very low.
class multi_pool_memory_resource : public std::pmr::memory_resource
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

    list<header>   m_free_lists[5]{};
    std::uintptr_t m_start_pos[5]{};
    std::size_t    m_total_sizes[5]{};
    std::size_t    m_chunk_size{};

public:
    multi_pool_memory_resource(void*       data,
                               std::size_t size,
                               std::size_t chunk_size) noexcept
      : m_chunk_size{ chunk_size }
    {
        assert(chunk_size >= std::alignment_of_v<std::max_align_t>);
        assert(size % 1024u == 0);
        assert(size % chunk_size == 0);

        const auto start      = reinterpret_cast<std::uintptr_t>(data);
        const auto total_size = size * chunk_size;

        m_start_pos[0] = start;
        m_start_pos[1] = m_start_pos[0] + total_size / 5;
        m_start_pos[2] = m_start_pos[1] + total_size / 5;
        m_start_pos[3] = m_start_pos[2] + total_size / 5;
        m_start_pos[4] = m_start_pos[3] + total_size / 5;
    }

    void* do_allocate(size_t bytes, size_t /*alignment*/) override
    {
        assert(bytes % m_chunk_size == 0);
        assert(bytes / m_chunk_size < 5);

        const auto id = bytes / m_chunk_size;

        node* free_position = m_free_lists[id].pop();

        if (free_position == nullptr) {
            if (on_error_not_enough_memory)
                on_error_not_enough_memory(*this);

            std::abort();
        }

        return reinterpret_cast<void*>(free_position);
    }

    void do_deallocate(void* p, size_t bytes, size_t /*alignment*/) override
    {
        assert(bytes % m_chunk_size == 0);
        assert(bytes / m_chunk_size < 5);

        const auto id = bytes / m_chunk_size;

        m_free_lists[id].push(reinterpret_cast<node*>(p));
    }

    void reset() noexcept
    {
        for (std::size_t id = 0; id < 5u; ++id) {
            const std::size_t n = m_total_sizes[id] / (m_chunk_size * id);
            for (std::size_t i = 0; i < n; ++i) {
                auto address = m_start_pos[id] + i * id * m_chunk_size;
                m_free_lists[id].push(reinterpret_cast<node*>(address));
            }
        }
    }
};

//! Thread unsafe allocator: a general purpose allocator.
//!
//! This memory resource doesn't impose any restriction. It allows allocations
//! and deallocations to be done in any order. For this reason, its performance
//! is not as good as its predecessors.
class freelist_memory_resource : public std::pmr::memory_resource
{
private:
    template<class T>
    struct list {
        struct node {
            T     data;
            node* next;
        };

        node* head{};

        list() noexcept = default;

        void insert(node* previous_node, node* new_node) noexcept
        {
            if (previous_node == nullptr) {
                if (head != nullptr) {
                    new_node->next = head;
                } else {
                    new_node->next = nullptr;
                }
                head = new_node;
            } else {
                if (previous_node->next == nullptr) {
                    previous_node->next = new_node;
                    new_node->next      = nullptr;
                } else {
                    new_node->next      = previous_node->next;
                    previous_node->next = new_node;
                }
            }
        }

        void remove(node* previous_node, node* delete_node) noexcept
        {
            if (previous_node == nullptr) {
                if (delete_node->next == nullptr) {
                    head = nullptr;
                } else {
                    head = delete_node->next;
                }
            } else {
                previous_node->next = delete_node->next;
            }
        }
    };

private:
    enum class search_policy { find_first, find_best };

    struct header {
        std::size_t block_size;
    };

    struct allocation_header {
        std::size_t block_size;
        char        padding;
    };

    using node = typename list<header>::node;

    void*         m_start_ptr{};
    std::size_t   m_total_size{};
    list<header>  m_free_list;
    search_policy m_policy = search_policy::find_first;

public:
    freelist_memory_resource(void*               data,
                             std::size_t         size,
                             const search_policy policy) noexcept
      : m_start_ptr{ data }
      , m_total_size{ size }
      , m_policy{ policy }
    {}

    void* do_allocate(size_t bytes, size_t alignment) override
    {
        const auto allocation_header_size =
          sizeof(freelist_memory_resource::allocation_header);

        assert(bytes >= sizeof(node));
        assert(alignment >= std::alignment_of_v<std::max_align_t>);

        std::size_t padding{};
        node*       affected_node{};
        node*       previous_node{};

        find(bytes, alignment, padding, previous_node, affected_node);
        if (affected_node == nullptr) {
            if (on_error_not_enough_memory)
                on_error_not_enough_memory(*this);

            std::abort();
        }

        const auto alignment_padding = padding - allocation_header_size;
        const auto required_size     = bytes + padding;
        const auto rest = affected_node->data.block_size - required_size;

        if (rest > 0) {
            node* new_free_node = reinterpret_cast<node*>(
              reinterpret_cast<std::uintptr_t>(affected_node) + required_size);
            new_free_node->data.block_size = rest;
            m_free_list.insert(affected_node, new_free_node);
        }

        m_free_list.remove(previous_node, affected_node);

        const auto header_address =
          reinterpret_cast<std::uintptr_t>(affected_node) + alignment_padding;
        const auto dataAddress = header_address + allocation_header_size;

        reinterpret_cast<allocation_header*>(header_address)->block_size =
          required_size;

        reinterpret_cast<allocation_header*>(header_address)->padding =
          static_cast<char>(alignment_padding);

        return (void*)dataAddress;
    }

    void do_deallocate(void* p, size_t /*bytes*/, size_t /*alignment*/) override
    {
        const auto current_address = reinterpret_cast<std::uintptr_t>(p);
        const auto header_address = current_address - sizeof(allocation_header);
        const auto* allocationHeader =
          reinterpret_cast<allocation_header*>(header_address);

        node* freeNode = reinterpret_cast<node*>(header_address);
        freeNode->data.block_size =
          allocationHeader->block_size + allocationHeader->padding;
        freeNode->next = nullptr;

        node* it      = m_free_list.head;
        node* it_prev = nullptr;

        while (it != nullptr) {
            if (p < it) {
                m_free_list.insert(it_prev, freeNode);
                break;
            }

            it_prev = it;
            it      = it->next;
        }

        merge(it_prev, freeNode);
    }

    void reset() noexcept
    {
        node* first            = reinterpret_cast<node*>(m_start_ptr);
        first->data.block_size = m_total_size;
        first->next            = nullptr;
        m_free_list.head       = nullptr;

        m_free_list.insert(nullptr, first);
    }

private:
    void merge(node* previous_node, node* free_node) noexcept
    {
        if (free_node->next != nullptr &&
            reinterpret_cast<std::uintptr_t>(free_node) +
                free_node->data.block_size ==
              reinterpret_cast<std::uintptr_t>(free_node->next)) {
            free_node->data.block_size += free_node->next->data.block_size;
            m_free_list.remove(free_node, free_node->next);
        }

        if (previous_node != nullptr &&
            reinterpret_cast<std::uintptr_t>(previous_node) +
                previous_node->data.block_size ==
              reinterpret_cast<std::uintptr_t>(free_node)) {
            previous_node->data.block_size += free_node->data.block_size;
            m_free_list.remove(previous_node, free_node);
        }
    }

    void find(const std::size_t size,
              const std::size_t alignment,
              std::size_t&      padding,
              node*&            previous_node,
              node*&            found_node) noexcept
    {
        switch (m_policy) {
        case search_policy::find_first:
            find_first(size, alignment, padding, previous_node, found_node);
            break;
        case search_policy::find_best:
            find_best(size, alignment, padding, previous_node, found_node);
            break;
        }
    }

    void find_first(const std::size_t size,
                    const std::size_t alignment,
                    std::size_t&      padding,
                    node*&            previous_node,
                    node*&            found_node) const noexcept
    {
        node* it      = m_free_list.head;
        node* it_prev = nullptr;

        while (it != nullptr) {
            padding = calculate_padding_with_header(
              (std::size_t)it,
              alignment,
              sizeof(freelist_memory_resource::allocation_header));

            const std::size_t required_space = size + padding;
            if (it->data.block_size >= required_space)
                break;

            it_prev = it;
            it      = it->next;
        }

        previous_node = it_prev;
        found_node    = it;
    }

    void find_best(const std::size_t size,
                   const std::size_t alignment,
                   std::size_t&      padding,
                   node*&            previous_node,
                   node*&            found_node) const noexcept
    {
        std::size_t smallestDiff = std::numeric_limits<std::size_t>::max();
        node*       best_block   = nullptr;
        node*       it           = m_free_list.head;
        node*       it_prev      = nullptr;

        while (it != nullptr) {
            padding = calculate_padding_with_header(
              reinterpret_cast<std::uintptr_t>(it),
              alignment,
              sizeof(freelist_memory_resource::allocation_header));

            const auto required_space = size + padding;
            if (it->data.block_size >= required_space &&
                (it->data.block_size - required_space < smallestDiff)) {
                best_block = it;
            }

            it_prev = it;
            it      = it->next;
        }

        previous_node = it_prev;
        found_node    = best_block;
    }
};

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
    using value_type      = T;
    using size_type       = std::uint32_t;
    using index_type      = std::make_signed_t<size_type>;
    using iterator        = T*;
    using const_iterator  = const T*;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;
    using allocator_type  = A;
    using this_container  = vector<T, A>;

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

    constexpr vector(std::pmr::memory_resource* mem) noexcept
        requires(!std::is_empty_v<A>);
    vector(std::pmr::memory_resource* mem, std::integral auto capacity) noexcept
        requires(!std::is_empty_v<A>);
    vector(std::pmr::memory_resource* mem,
           std::integral auto         capacity,
           std::integral auto         size) noexcept
        requires(!std::is_empty_v<A>);
    vector(std::pmr::memory_resource* mem,
           std::integral auto         capacity,
           std::integral auto         size,
           const T&                   default_value) noexcept
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

    constexpr int find(const T& t) const noexcept;

    constexpr void pop_back() noexcept;
    constexpr void swap_pop_back(std::integral auto index) noexcept;

    constexpr void erase(iterator it) noexcept;
    constexpr void erase(iterator begin, iterator end) noexcept;

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

    using identifier_type = Identifier;
    using value_type      = T;
    using this_container  = data_array<T, Identifier>;
    using allocator_type  = A;

private:
    struct item {
        T          item;
        Identifier id;
    };

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

    data_array(std::integral auto capacity) noexcept
        requires(std::is_empty_v<A>);

    ~data_array() noexcept;

    data_array(std::pmr::memory_resource* mem,
               std::integral auto         capacity) noexcept
        requires(!std::is_empty_v<A>);

    //! @brief Destroy all items in the data_array but keep memory
    //!  allocation.
    //!
    //! @details Run the destructor if @c T is not trivial on outstanding
    //!  items and re-initialize the size.
    void clear() noexcept;

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

//! @brief A ring-buffer based on a fixed size container. m_head point to
//! the first element can be dequeue while m_tail point to the first
//! constructible element in the ring.
//! @tparam T Any type (trivial or not).
template<typename T, typename A = default_allocator>
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
                   std::is_nothrow_move_constructible_v<
                     T>)&&std::is_nothrow_destructible_v<T>);

public:
    T* buffer = nullptr;

private:
    [[no_unique_address]] allocator_type m_alloc;

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
            requires(!std::is_const_v<Container> && !is_const)
          : ring(ring_)
          , i(i_)
        {}

        template<typename Container>
        iterator_base(Container* ring_, index_type i_) noexcept
            requires(std::is_const_v<Container> && is_const)
          : ring(ring_)
          , i(i_)
        {}

    public:
        container_type* buffer() noexcept { return ring; }
        index_type      index() noexcept { return i; }

        iterator_base() noexcept = default;

        iterator_base(const iterator_base& other) noexcept
          : ring(other.ring)
          , i(other.i)
        {}

        iterator_base& operator=(const iterator_base& other) noexcept
        {
            if (this != &other) {
                ring = other.ring;
                i    = other.i;
            }

            return *this;
        }

        reference operator*() const noexcept { return ring->buffer[i]; }
        pointer   operator->() const noexcept { return &ring->buffer[i]; }

        iterator_base& operator++() noexcept
        {
            if (ring) {
                i = ring->advance(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        iterator_base operator++(int) noexcept
        {
            iterator_base orig(*this);
            ++(*this);

            return orig;
        }

        iterator_base& operator--() noexcept
        {
            if (ring) {
                i = ring->back(i);

                if (i == ring->m_tail)
                    reset();
            }

            return *this;
        }

        iterator_base operator--(int) noexcept
        {
            iterator_base orig(*this);
            --(*orig);

            return orig;
        }

        template<bool R>
        bool operator==(const iterator_base<R>& other) const
        {
            return other.ring == ring && other.i == i;
        }

        void reset() noexcept
        {
            ring = nullptr;
            i    = 0;
        }
    };

    using iterator       = iterator_base<false>;
    using const_iterator = iterator_base<true>;

    constexpr ring_buffer() noexcept = default;
    constexpr ring_buffer(std::integral auto capacity) noexcept;
    constexpr ring_buffer(std::pmr::memory_resource* mem) noexcept
        requires(!std::is_empty_v<A>);

    constexpr ring_buffer(std::pmr::memory_resource* mem,
                          std::integral auto         capacity) noexcept
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

    constexpr bool operator==(const small_string& rhs) const noexcept;
    constexpr bool operator!=(const small_string& rhs) const noexcept;
    constexpr bool operator>(const small_string& rhs) const noexcept;
    constexpr bool operator<(const small_string& rhs) const noexcept;
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
    constexpr small_vector(small_vector&& other) noexcept;
    constexpr small_vector& operator=(small_vector&& other) noexcept;

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
    constexpr void      pop_back() noexcept;
    constexpr void      swap_pop_back(std::integral auto index) noexcept;
};

//! @brief A ring-buffer based on a fixed size container. m_head point to
//! the first element can be dequeue while m_tail point to the first
//! constructible element in the ring.
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

// template<typeanem T, typename Identifier>
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
data_array<T, Identifier, A>::data_array(std::integral auto capacity) noexcept
    requires(std::is_empty_v<A>)
{
    assert(std::cmp_greater(capacity, 0));
    assert(std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    m_items     = m_alloc.template allocate<item>(capacity);
    m_max_size  = 0;
    m_max_used  = 0;
    m_capacity  = static_cast<index_type>(capacity);
    m_next_key  = 1;
    m_free_head = none;
}

template<typename T, typename Identifier, typename A>
data_array<T, Identifier, A>::data_array(std::pmr::memory_resource* mem,
                                         std::integral auto capacity) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{
    assert(std::cmp_greater(capacity, 0));
    assert(std::cmp_less(capacity, std::numeric_limits<index_type>::max()));

    m_items     = m_alloc.template allocate<item>(capacity);
    m_max_size  = 0;
    m_max_used  = 0;
    m_capacity  = static_cast<index_type>(capacity);
    m_next_key  = 1;
    m_free_head = none;
}

template<typename T, typename Identifier, typename A>
data_array<T, Identifier, A>::~data_array() noexcept
{
    clear();

    if (m_items)
        m_alloc.deallocate(m_items, m_capacity);
}

template<typename T, typename Identifier, typename A>
void data_array<T, Identifier, A>::clear() noexcept
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

template<typename T, typename Identifier, typename A>
template<typename... Args>
typename data_array<T, Identifier, A>::value_type&
data_array<T, Identifier, A>::alloc(Args&&... args) noexcept
{
    assert(can_alloc(1) && "check alloc() with full() before using use.");

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

    assert(&m_items[index] == static_cast<void*>(&t));
    assert(m_items[index].id == id);
    assert(is_valid(id));

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
    assert(t != nullptr);

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
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, m_max_used));

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
    const std::uint64_t capacity = m_capacity;
    const std::uint64_t max_size = m_max_size;

    return std::cmp_greater_equal(capacity - max_size, nb);
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

// implementation
// vector<T, A>

template<typename T, typename A>
constexpr vector<T, A>::vector() noexcept
{}

template<typename T, typename A>
constexpr vector<T, A>::vector(std::pmr::memory_resource* mem) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{}

template<typename T, typename A>
constexpr bool vector<T, A>::make(std::integral auto capacity) noexcept
{
    assert(std::cmp_greater(capacity, 0));
    assert(std::cmp_less(sizeof(T) * capacity,
                         std::numeric_limits<index_type>::max()));

    m_data     = m_alloc.template allocate<T>(capacity);
    m_capacity = capacity;
    m_size     = 0;

    return m_data != nullptr;
}

template<typename T, typename A>
constexpr bool vector<T, A>::make(std::integral auto capacity,
                                  std::integral auto size) noexcept
{
    assert(std::cmp_greater(capacity, 0));
    assert(std::cmp_greater_equal(size, 0));
    assert(std::cmp_greater_equal(size, capacity));
    assert(std::cmp_less(sizeof(T) * capacity,
                         std::numeric_limits<index_type>::max()));
    assert(
      std::cmp_less(sizeof(T) * size, std::numeric_limits<index_type>::max()));

    static_assert(std::is_constructible_v<T>,
                  "T must be nothrow or trivially default constructible");

    capacity = std::max(capacity, size);

    if (capacity > 0) {
        m_data = m_alloc.template allocate<T>(capacity);

        std::uninitialized_value_construct_n(m_data, size);
        // std::uninitialized_default_construct_n(m_data, size);

        m_capacity = capacity;
        m_size     = size;
    }

    return m_data != nullptr;
}

template<typename T, typename A>
constexpr bool vector<T, A>::make(std::integral auto capacity,
                                  std::integral auto size,
                                  const T&           default_value) noexcept
{
    assert(std::cmp_greater(capacity, 0));
    assert(std::cmp_greater_equal(size, 0));
    assert(std::cmp_greater_equal(size, capacity));
    assert(std::cmp_less(sizeof(T) * capacity,
                         std::numeric_limits<index_type>::max()));
    assert(
      std::cmp_less(sizeof(T) * size, std::numeric_limits<index_type>::max()));

    static_assert(std::is_copy_constructible_v<T>,
                  "T must be nothrow or trivially default constructible");

    capacity = std::max(capacity, size);

    if (capacity > 0) {
        m_data = m_alloc.template allocate<T>(capacity);

        std::uninitialized_fill_n(m_data, size, default_value);

        m_capacity = capacity;
        m_size     = size;
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
inline vector<T, A>::vector(std::pmr::memory_resource* mem,
                            std::integral auto         capacity) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{
    make(capacity);
}

template<typename T, typename A>
inline vector<T, A>::vector(std::pmr::memory_resource* mem,
                            std::integral auto         capacity,
                            std::integral auto         size) noexcept
    requires(!std::is_empty_v<A>)
  : m_alloc(mem)
{
    make(capacity, size);
}

template<typename T, typename A>
inline vector<T, A>::vector(std::pmr::memory_resource* mem,
                            std::integral auto         capacity,
                            std::integral auto         size,
                            const T&                   default_value) noexcept
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
    operator=(other);
}

template<typename T, typename A>
inline constexpr vector<T, A>& vector<T, A>::operator=(
  const vector& other) noexcept
{
    clear();

    resize(other.m_size);
    std::copy_n(other.m_data, other.m_size, m_data);
    return *this;
}

template<typename T, typename A>
inline constexpr vector<T, A>::vector(vector&& other) noexcept
  : m_data(other.m_data)
  , m_size(other.m_size)
  , m_capacity(other.m_capacity)
{
    other.m_data     = nullptr;
    other.m_size     = 0;
    other.m_capacity = 0;
}

template<typename T, typename A>
inline constexpr vector<T, A>& vector<T, A>::operator=(vector&& other) noexcept
{
    destroy();

    m_data     = std::exchange(other.m_data, nullptr);
    m_size     = std::exchange(other.m_size, 0);
    m_capacity = std::exchange(other.m_capacity, 0);

    return *this;
}

template<typename T, typename A>
inline void vector<T, A>::destroy() noexcept
{
    clear();

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

    assert(
      std::cmp_less(size, std::numeric_limits<decltype(m_capacity)>::max()));

    if (std::cmp_greater(size, m_capacity)) {
        if (!reserve(
              compute_new_capacity(static_cast<decltype(m_capacity)>(size))))
            return false;
    }

    std::uninitialized_value_construct_n(data() + m_size, size - m_size);
    // std::uninitialized_default_construct_n(data() + m_size, size);

    m_size = static_cast<decltype(m_size)>(size);

    return true;
}

template<typename T, typename A>
bool vector<T, A>::reserve(std::integral auto new_capacity) noexcept
{
    assert(std::cmp_less(new_capacity,
                         std::numeric_limits<decltype(m_capacity)>::max()));

    if (std::cmp_greater(new_capacity, m_capacity)) {
        T* new_data = m_alloc.template allocate<T>(new_capacity);
        if (!new_data)
            return false;

        if constexpr (std::is_move_constructible_v<T>)
            std::uninitialized_move_n(data(), m_size, new_data);
        else
            std::uninitialized_copy_n(data(), m_size, new_data);

        m_alloc.template deallocate<T>(m_data, m_capacity);

        m_data     = new_data;
        m_capacity = static_cast<decltype(m_capacity)>(new_capacity);
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
    assert(m_size > 0);
    return m_data[0];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_reference vector<T, A>::front()
  const noexcept
{
    assert(m_size > 0);
    return m_data[0];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::reference vector<T, A>::back() noexcept
{
    assert(m_size > 0);
    return m_data[m_size - 1];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_reference vector<T, A>::back()
  const noexcept
{
    assert(m_size > 0);
    return m_data[m_size - 1];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::reference vector<T, A>::operator[](
  std::integral auto index) noexcept
{
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, typename A>
inline constexpr typename vector<T, A>::const_reference
vector<T, A>::operator[](std::integral auto index) const noexcept
{
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, m_size));

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
inline constexpr void vector<T, A>::pop_back() noexcept
{
    static_assert(std::is_nothrow_destructible_v<T> ||
                    std::is_trivially_destructible_v<T>,
                  "T must be nothrow or trivially destructible to use "
                  "pop_back() function");

    assert(m_size);

    if (m_size) {
        std::destroy_at(data() + m_size - 1);
        --m_size;
    }
}

template<typename T, typename A>
inline constexpr void vector<T, A>::swap_pop_back(
  std::integral auto index) noexcept
{
    assert(std::cmp_less(index, m_size));

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
inline constexpr void vector<T, A>::erase(iterator it) noexcept
{
    assert(it >= data() && it < data() + m_size);

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

template<typename T, typename A>
inline constexpr void vector<T, A>::erase(iterator first,
                                          iterator last) noexcept
{
    assert(first >= data() && first < data() + m_size && last > first &&
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

    m_size -= static_cast<int>(count);
}

template<typename T, typename A>
inline constexpr int vector<T, A>::index_from_ptr(const T* data) const noexcept
{
    assert(is_iterator_valid(const_iterator(data)));

    const auto off = data - m_data;

    assert(0 <= off && off < INT32_MAX);

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
constexpr small_vector<T, length>::small_vector(
  small_vector<T, length>&& other) noexcept
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
        std::destroy_n(data(), m_size);
        std::uninitialized_copy_n(other.data(), other.m_size, data());

        m_size = other.m_size;
    }

    return *this;
}

template<typename T, int length>
constexpr small_vector<T, length>& small_vector<T, length>::operator=(
  small_vector<T, length>&& other) noexcept
{
    if (&other != this) {
        std::destroy_n(data(), m_size);
        std::uninitialized_copy_n(other.data(), other.m_size, data());

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

    assert(std::cmp_greater_equal(default_size, 0) &&
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
    assert(m_size > 0);
    return data()[0];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::front() const noexcept
{
    assert(m_size > 0);
    return data()[0];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::back() noexcept
{
    assert(m_size > 0);
    return data()[m_size - 1];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::back() const noexcept
{
    assert(m_size > 0);
    return data()[m_size - 1];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::reference
small_vector<T, length>::operator[](std::integral auto index) noexcept
{
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, m_size));

    return data()[index];
}

template<typename T, int length>
constexpr typename small_vector<T, length>::const_reference
small_vector<T, length>::operator[](std::integral auto index) const noexcept
{
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, m_size));

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
    assert(std::cmp_greater_equal(index, 0) && std::cmp_less(index, m_size));

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
        m_size               = static_cast<std::uint8_t>(std::strlen(m_buffer));
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
    m_size               = static_cast<std::uint8_t>(std::strlen(m_buffer));
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
    assert(std::cmp_less(index, length));

    return m_buffer[index];
}

template<int length>
inline constexpr typename small_string<length>::const_reference
small_string<length>::operator[](std::integral auto index) const noexcept
{
    assert(std::cmp_less(index, m_size));

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

////////////////////////////////////////////////////////////////////////
//                                                                    //
// template<typename T, typename A>.................................. //
// class ring_buffer;                                                 //
//                                                                    //
////////////////////////////////////////////////////////////////////////

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
constexpr ring_buffer<T, A>::ring_buffer(std::pmr::memory_resource* mem,
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
            m_alloc.template deallocate<T>(buffer, m_capacity);

            if (rhs.m_capacity > 0) {
                buffer     = m_alloc.template allocate(rhs.m_capacity);
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
    m_alloc.template deallocate<T>(buffer, m_capacity);
    buffer = nullptr;
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::reserve(std::integral auto capacity) noexcept
{
    assert(std::cmp_less(capacity,
                         std::numeric_limits<decltype(m_capacity)>::max()));

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
    assert(this_it.ring != nullptr);

    if (this_it == tail())
        return;

    while (this_it != tail())
        pop_tail();
}

template<class T, typename A>
constexpr void ring_buffer<T, A>::erase_before(iterator this_it) noexcept
{
    assert(this_it.ring != nullptr);

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
constexpr void ring_buffer<T, A>::force_emplace_enqueue(Args&&... args) noexcept
{
    if (full())
        dequeue();

    std::construct_at(&buffer[m_tail], std::forward<Args>(args)...);
    m_tail = advance(m_tail);
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
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, m_capacity));

    return buffer[index];
}

template<class T, typename A>
constexpr const T& ring_buffer<T, A>::operator[](
  std::integral auto index) const noexcept
{
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, m_capacity));

    return buffer[index];
}

template<class T, typename A>
constexpr T& ring_buffer<T, A>::front() noexcept
{
    assert(!empty());

    return buffer[m_head];
}

template<class T, typename A>
constexpr const T& ring_buffer<T, A>::front() const noexcept
{
    assert(!empty());

    return buffer[m_head];
}

template<class T, typename A>
constexpr T& ring_buffer<T, A>::back() noexcept
{
    assert(!empty());

    return buffer[back(m_tail)];
}

template<class T, typename A>
constexpr const T& ring_buffer<T, A>::back() const noexcept
{
    assert(!empty());

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

        std::uninitialized_move_n(rhs.data(), rhs.m_size, data());

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
    assert(this_it.ring != nullptr);

    if (this_it == tail())
        return;

    while (this_it != tail())
        pop_tail();
}

template<class T, int length>
constexpr void small_ring_buffer<T, length>::erase_before(
  iterator this_it) noexcept
{
    assert(this_it.ring != nullptr);

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
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, length));

    return data()[index];
}

template<class T, int length>
constexpr const T& small_ring_buffer<T, length>::operator[](
  std::integral auto index) const noexcept
{
    assert(std::cmp_greater_equal(index, 0));
    assert(std::cmp_less(index, length));

    return data()[index];
}

template<class T, int length>
constexpr T& small_ring_buffer<T, length>::front() noexcept
{
    assert(!empty());

    return data()[m_head];
}

template<class T, int length>
constexpr const T& small_ring_buffer<T, length>::front() const noexcept
{
    assert(!empty());

    return data()[m_head];
}

template<class T, int length>
constexpr T& small_ring_buffer<T, length>::back() noexcept
{
    assert(!empty());

    return data()[back(m_tail)];
}

template<class T, int length>
constexpr const T& small_ring_buffer<T, length>::back() const noexcept
{
    assert(!empty());

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
    assert(is_numeric_castable<index_type>(idx));

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
small_ring_buffer<T, length>::template iterator_base<
  is_const>::template iterator_base<is_const>&
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
    return ring->buffer[i];
}

template<class T, int length>
template<bool is_const>
typename small_ring_buffer<T, length>::template iterator_base<is_const>::pointer
small_ring_buffer<T, length>::iterator_base<is_const>::operator->()
  const noexcept
{
    return &ring->buffer[i];
}

template<class T, int length>
template<bool is_const>
small_ring_buffer<T, length>::template iterator_base<
  is_const>::template iterator_base<is_const>&
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
small_ring_buffer<T, length>::template iterator_base<
  is_const>::template iterator_base<is_const>
small_ring_buffer<T, length>::iterator_base<is_const>::operator++(int) noexcept
{
    iterator_base orig(*this);
    ++(*this);

    return orig;
}

template<class T, int length>
template<bool is_const>
small_ring_buffer<T, length>::template iterator_base<
  is_const>::template iterator_base<is_const>&
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
small_ring_buffer<T, length>::template iterator_base<
  is_const>::template iterator_base<is_const>
small_ring_buffer<T, length>::iterator_base<is_const>::operator--(int) noexcept
{
    iterator_base orig(*this);
    --(*orig);

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

} // namespace irt

#endif