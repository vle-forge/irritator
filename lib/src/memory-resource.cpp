// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/container.hpp>
#include <irritator/format.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

namespace irt {

human_readable_length_t::human_readable_length_t(
  const std::size_t bytes) noexcept
{
    const auto b  = static_cast<double>(bytes);
    const auto kb = b / 1024.0;
    const auto mb = b / (1024.0 * 1024.0);
    const auto gb = b / (1024.0 * 1024.0 * 1024.0);

    if (gb > 1) {
        size = gb;
        type = human_readable_length_t::display_type::GB;
    } else if (mb > 1.0) {
        size = mb;
        type = human_readable_length_t::display_type::MB;
    } else if (kb > 1.0) {
        size = kb;
        type = human_readable_length_t::display_type::KB;
    } else {
        size = b;
        type = human_readable_length_t::display_type::B;
    }
}

void* new_delete_memory_resource::data::debug_allocate(
  std::size_t bytes,
  std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "new-delete::allocate   {},{} {},{}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));

    auto ptr = std::pmr::new_delete_resource()->allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    return ptr;
}

void new_delete_memory_resource::data::debug_deallocate(
  void*       ptr,
  std::size_t bytes,
  std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "new-delete::deallocate {},{} {},{} {}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        std::pmr::new_delete_resource()->deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));
}

template<std::size_t max_blocks_per_chunk,
         std::size_t largest_required_pool_block>
void* synchronized_pool_resource<max_blocks_per_chunk,
                                 largest_required_pool_block>::data::
  debug_allocate(std::size_t bytes, std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "sync-pools::allocate   {},{} {},{}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));

    auto ptr = mr.allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    return ptr;
}

template<std::size_t max_blocks_per_chunk,
         std::size_t largest_required_pool_block>
void synchronized_pool_resource<max_blocks_per_chunk,
                                largest_required_pool_block>::data::
  debug_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "sync-pools::deallocate {},{} {},{} {}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        mr.deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));
}

template<std::size_t max_blocks_per_chunk,
         std::size_t largest_required_pool_block>
void* unsynchronized_pool_resource<max_blocks_per_chunk,
                                   largest_required_pool_block>::data::
  debug_allocate(std::size_t bytes, std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "unsync-pools::allocate   {},{} {},{}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));

    auto ptr = mr.allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    return ptr;
}

template<std::size_t max_blocks_per_chunk,
         std::size_t largest_required_pool_block>
void unsynchronized_pool_resource<max_blocks_per_chunk,
                                  largest_required_pool_block>::data::
  debug_deallocate(void* ptr, std::size_t bytes, std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "unsync-pools::deallocate {},{} {},{} {}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        mr.deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));
}

template<std::size_t Length, int ID>
void* monotonic_small_buffer<Length, ID>::data::debug_allocate(
  std::size_t bytes,
  std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "mono-small-size-{}::allocate   {},{} {},{}\n",
               id,
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));

    auto ptr = mr.allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    return ptr;
}

template<std::size_t Length, int ID>
void monotonic_small_buffer<Length, ID>::data::debug_deallocate(
  void*       ptr,
  std::size_t bytes,
  std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "mono-small-size-{}::deallocate {},{} {},{} {}\n",
               id,
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        mr.deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));
}

template<int ID>
void* monotonic_buffer<ID>::data::debug_allocate(std::size_t bytes,
                                                 std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "mono-size-{}::allocate   {},{} {},{}\n",
               id,
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));

    auto ptr = mr.allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    return ptr;
}

template<int ID>
void monotonic_buffer<ID>::data::debug_deallocate(
  void*       ptr,
  std::size_t bytes,
  std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "mono-size-{}::deallocate {},{} {},{} {}\n",
               id,
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        mr.deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               human_readable_length_t(bytes),
               human_readable_length_t(alignment),
               human_readable_length_t(allocated),
               human_readable_length_t(deallocated));
}

} // namespace irt
