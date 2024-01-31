// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/container.hpp>

#include <errno.h>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace irt {

constexpr inline bool is_alignment(std::size_t value) noexcept
{
    return (value > 0) && ((value & (value - 1)) == 0);
}

#if defined(_MSC_VER)
void* malloc_memory_resource_allocate_win32(std::size_t bytes,
                                            std::size_t alignment) noexcept
{
    debug::ensure(is_alignment(alignment));
    debug::ensure((bytes % alignment) == 0);

    using fn = void* (*)(std::size_t, std::size_t) noexcept;
    fn call  = reinterpret_cast<fn>(::_aligned_malloc);

    return call(bytes, alignment);
}
#elif defined(__APPLE__)
void* malloc_memory_resource_allocate_apple(std::size_t bytes,
                                            std::size_t alignment) noexcept
{
    alignment = alignment < sizeof(void*) ? sizeof(void*) : alignment;

    debug::ensure(is_alignment(alignment));

    void* p;
    if (auto ret = ::posix_memalign(&p, alignment, bytes); ret != 0) {
        p = nullptr;

        if (ret == EINVAL)
            std::fprintf(stderr,
                         "The value of the alignment parameter is not a power "
                         "of two or a multiple of sizeof(void *).\n");
        if (ret == ENOMEM)
            std::fprintf(stderr,
                         "There is insufficient memory available with the "
                         "requested alignment.\n");
    }

    return p;
}
#else
void* malloc_memory_resource_allocate_posix(std::size_t bytes,
                                            std::size_t alignment) noexcept
{
    debug::ensure(is_alignment(alignment));
    debug::ensure((bytes % alignment) == 0);

    using fn = void* (*)(std::size_t, std::size_t) noexcept;
    fn call  = reinterpret_cast<fn>(std::aligned_alloc);

    return call(alignment, bytes);
}
#endif

void* malloc_memory_resource::do_allocate(std::size_t bytes,
                                          std::size_t alignment) noexcept
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    std::fprintf(stderr,
                 "malloc_memory_resource::need-allocate [%zu %zu]\n",
                 bytes,
                 alignment);
#endif

#if defined(_MSC_VER)
    auto* p = malloc_memory_resource_allocate_win32(bytes, alignment);
#elif defined(__APPLE__)
    auto* p = malloc_memory_resource_allocate_apple(bytes, alignment);
#else
    auto* p = malloc_memory_resource_allocate_posix(bytes, alignment);
#endif

    if (not p) {
#if defined(IRRITATOR_ENABLE_DEBUG)
        std::fprintf(
          stderr,
          "Irritator shutdown: Unable to allocate memory %zu alignment %zu\n",
          bytes,
          alignment);
#endif
        std::abort();
    }

#if defined(IRRITATOR_ENABLE_DEBUG)
    std::fprintf(stderr,
                 "malloc_memory_resource::allocate [%p %zu %zu]\n",
                 p,
                 bytes,
                 alignment);
#endif

    return p;
}

void malloc_memory_resource::do_deallocate(
  void*                        pointer,
  [[maybe_unused]] std::size_t bytes,
  [[maybe_unused]] std::size_t alignment) noexcept
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    std::fprintf(stderr,
                 "malloc_memory_resource::do_deallocate [%p %zu %zu]\n",
                 pointer,
                 bytes,
                 alignment);
#endif

#if defined(_WIN32)
    if (pointer)
        ::_aligned_free(pointer);
#else
    if (pointer)
        std::free(pointer);
#endif
}

fixed_linear_memory_resource::fixed_linear_memory_resource(
  std::byte*  data,
  std::size_t size) noexcept
  : m_start{ data }
  , m_total_size{ size }
{
    debug::ensure(m_start);
    debug::ensure(m_total_size);
}

void* fixed_linear_memory_resource::allocate(size_t bytes,
                                             size_t alignment) noexcept
{
    std::size_t padding = 0;

    const auto current_address =
      reinterpret_cast<std::uintptr_t>(m_start) + m_offset;

    if (alignment != 0 && m_offset % alignment != 0)
        padding = calculate_padding(current_address, alignment);

    if (m_offset + padding + bytes > m_total_size) {
        std::abort();
    }

    m_offset += padding;
    const auto next_address = current_address + padding;
    m_offset += bytes;

    return reinterpret_cast<void*>(next_address);
}

void fixed_linear_memory_resource::reset() noexcept { m_offset = { 0 }; }

void fixed_linear_memory_resource::reset(std::byte*  data,
                                         std::size_t size) noexcept
{
    debug::ensure(data != nullptr);
    debug::ensure(size != 0u);
    debug::ensure(m_start == nullptr);
    debug::ensure(m_total_size == 0u);

    m_start      = data;
    m_total_size = size;

    reset();
}

bool fixed_linear_memory_resource::can_alloc(std::size_t bytes,
                                             std::size_t alignment) noexcept
{
    std::size_t padding = 0;

    const auto current_address =
      reinterpret_cast<std::uintptr_t>(m_start) + m_offset;

    if (alignment != 0 && m_offset % alignment != 0)
        padding = calculate_padding(current_address, alignment);

    return m_offset + padding + bytes <= m_total_size;
}

pool_memory_resource::pool_memory_resource(std::byte*  data,
                                           std::size_t size,
                                           std::size_t chunk_size) noexcept
  : m_start_ptr{ data }
  , m_total_size{ size }
  , m_chunk_size{ chunk_size }
  , m_total_allocated{ 0 }
{
    debug::ensure(chunk_size >= std::alignment_of_v<std::max_align_t>);
    debug::ensure(size % chunk_size == 0);

    reset();
}

void* pool_memory_resource::allocate(size_t bytes,
                                     size_t /*alignment*/) noexcept
{
    debug::ensure(bytes == m_chunk_size &&
                  "Allocation size must be equal to chunk size");

    node* free_position = m_free_list.pop();
    m_total_allocated += m_chunk_size;

    if (free_position == nullptr)
        std::abort();

    return reinterpret_cast<void*>(free_position);
}

void pool_memory_resource::deallocate(void* p,
                                      size_t /*bytes*/,
                                      size_t /*alignment*/) noexcept
{
    m_total_allocated -= m_chunk_size;
    m_free_list.push(reinterpret_cast<node*>(p));
}

void pool_memory_resource::reset() noexcept
{
    const auto start = reinterpret_cast<std::uintptr_t>(m_start_ptr);

    for (auto n = m_total_size / m_chunk_size; n > 0; --n) {
        auto address = start + ((n - 1) * m_chunk_size);
        m_free_list.push(reinterpret_cast<node*>(address));
    }
}

void pool_memory_resource::reset(std::byte*  data,
                                 std::size_t size,
                                 std::size_t chunk_size) noexcept
{
    debug::ensure(m_start_ptr == nullptr);
    debug::ensure(m_total_size == 0u);
    debug::ensure(data);
    debug::ensure(chunk_size >= std::alignment_of_v<std::max_align_t>);
    debug::ensure(size % chunk_size == 0);

    m_start_ptr       = data;
    m_total_size      = size;
    m_chunk_size      = chunk_size;
    m_total_allocated = 0;

    reset();
}

bool pool_memory_resource::can_alloc(std::size_t bytes,
                                     std::size_t /*alignment*/) noexcept
{
    debug::ensure((bytes % m_chunk_size) == 0 &&
                  "Allocation size must be equal to chunk size");

    return (m_total_size - m_total_allocated) > bytes;
}

freelist_memory_resource::freelist_memory_resource(std::byte*  data,
                                                   std::size_t size) noexcept
  : m_start_ptr{ data }
  , m_total_size{ size }
{
    reset();
}

void* freelist_memory_resource::allocate(size_t size, size_t alignment) noexcept
{
    // debug::ensure("Allocation size must be bigger" && size >=
    // sizeof(Node)); debug::ensure("Alignment must be 8 at least" &&
    // alignment >= 8);

    auto found = find_policy::find_first == m_find_policy
                   ? find_first(size, alignment)
                   : find_best(size, alignment);

    if (found.allocated == nullptr)
        std::abort();

    const auto alignmentPadding = found.padding - allocation_header_size;
    const auto requiredSize     = size + found.padding;
    const auto rest = found.allocated->data.block_size - requiredSize;

    if (rest > 0) {
        node* newFreeNode =
          (node*)((std::size_t)found.allocated + requiredSize);
        newFreeNode->data.block_size = rest;
        m_freeList.insert(found.allocated, newFreeNode);
    }

    m_freeList.remove(found.previous, found.allocated);

    const std::size_t headerAddress =
      (std::size_t)found.allocated + alignmentPadding;
    const std::size_t dataAddress = headerAddress + allocation_header_size;
    ((freelist_memory_resource::AllocationHeader*)headerAddress)->block_size =
      requiredSize;
    ((freelist_memory_resource::AllocationHeader*)headerAddress)->padding =
      static_cast<std::uint8_t>(alignmentPadding);

    m_used += requiredSize;
    m_peak = std::max(m_peak, m_used);

    return (void*)dataAddress;
}

void freelist_memory_resource::deallocate(void* ptr,
                                          size_t /*bytes*/,
                                          size_t /*alignment*/) noexcept
{
    const auto currentAddress = (std::size_t)ptr;
    const auto headerAddress  = currentAddress - allocation_header_size;

    const freelist_memory_resource::AllocationHeader* allocationHeader{ (
      freelist_memory_resource::AllocationHeader*)headerAddress };

    node* freeNode = (node*)(headerAddress);
    freeNode->data.block_size =
      allocationHeader->block_size + allocationHeader->padding;
    freeNode->next = nullptr;

    node* it   = m_freeList.head;
    node* prev = nullptr;
    while (it != nullptr) {
        if (ptr < it) {
            m_freeList.insert(prev, freeNode);
            break;
        }
        prev = it;
        it   = it->next;
    }

    m_used -= freeNode->data.block_size;

    merge(prev, freeNode);
}

void freelist_memory_resource::reset() noexcept
{
    m_used = 0u;
    m_peak = 0u;

    node* first            = reinterpret_cast<node*>(m_start_ptr);
    first->data.block_size = m_total_size;
    first->next            = nullptr;

    m_freeList.head = nullptr;
    m_freeList.insert(nullptr, first);
}

void freelist_memory_resource::reset(std::byte* data, std::size_t size) noexcept
{
    debug::ensure(data != nullptr);
    debug::ensure(size != 0u);
    debug::ensure(m_start_ptr == nullptr);
    debug::ensure(m_total_size == 0u);

    m_start_ptr   = data;
    m_total_size  = size;
    m_used        = 0;
    m_peak        = 0;
    m_find_policy = find_policy::find_first;

    reset();
}

void freelist_memory_resource::merge(node* previous, node* free_node) noexcept
{
    if (free_node->next != nullptr &&
        (std::size_t)free_node + free_node->data.block_size ==
          (std::size_t)free_node->next) {
        free_node->data.block_size += free_node->next->data.block_size;
        m_freeList.remove(free_node, free_node->next);
    }

    if (previous != nullptr &&
        (std::size_t)previous + previous->data.block_size ==
          (std::size_t)free_node) {
        previous->data.block_size += free_node->data.block_size;
        m_freeList.remove(previous, free_node);
    }
}

auto freelist_memory_resource::find_best(
  const std::size_t size,
  const std::size_t alignment) const noexcept -> find_t
{
    node*       best         = nullptr;
    node*       it           = m_freeList.head;
    node*       prev         = nullptr;
    std::size_t best_padding = 0;
    std::size_t diff         = std::numeric_limits<std::size_t>::max();

    while (it != nullptr) {
        const auto padding = calculate_padding_with_header(
          (std::size_t)it, alignment, allocation_header_size);

        const std::size_t required = size + padding;
        if (it->data.block_size >= required &&
            (it->data.block_size - required < diff)) {
            best         = it;
            best_padding = padding;
            diff         = it->data.block_size - required;
        }

        prev = it;
        it   = it->next;
    }

    return { prev, best, best_padding };
}

auto freelist_memory_resource::find_first(
  const std::size_t size,
  const std::size_t alignment) const noexcept -> find_t
{
    node*       it      = m_freeList.head;
    node*       prev    = nullptr;
    std::size_t padding = 0;

    while (it != nullptr) {
        padding = calculate_padding_with_header(
          (std::size_t)it, alignment, allocation_header_size);

        const std::size_t requiredSpace = size + padding;
        if (it->data.block_size >= requiredSpace)
            break;

        prev = it;
        it   = it->next;
    }

    return { prev, it, padding };
}

} // namespace irt
