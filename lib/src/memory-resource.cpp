// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/container.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>

namespace irt {

struct human_readable_length_t {
    constexpr human_readable_length_t(double           size_,
                                      std::string_view type_) noexcept
      : size(size_)
      , type(type_)
    {}

    double           size;
    std::string_view type;
};

/**
   Print a human readable version of number of bytes. It shows xxx B, xxx
   MB, xxx GB etc. using the std::fprintf stream.
 */
static constexpr auto make_human_readable_bytes(
  const std::size_t bytes) noexcept -> human_readable_length_t
{
    const auto b  = static_cast<double>(bytes);
    const auto kb = b / 1024.0;
    const auto mb = b / (1024.0 * 1024.0);
    const auto gb = b / (1024.0 * 1024.0 * 1024.0);

    if (gb > 1)
        return human_readable_length_t(gb, "GB");
    else if (mb > 1.0)
        return human_readable_length_t(mb, "MB");
    else if (kb > 1.0)
        return human_readable_length_t(kb, "KB");
    else
        return human_readable_length_t(b, "B");
}

} // namespace irt

template<>
struct fmt::formatter<::irt::human_readable_length_t> {

    constexpr auto parse(format_parse_context& ctx) noexcept
      -> format_parse_context::iterator
    {
        return ctx.begin();
    }

    auto format(const ::irt::human_readable_length_t& hr,
                format_context& ctx) const noexcept -> format_context::iterator
    {
        return format_to(ctx.out(), "{}{}", hr.size, hr.type);
    }
};

namespace irt {

void* new_delete_memory_resource::data::debug_allocate(
  std::size_t bytes,
  std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "new-delete::allocate   {},{} {},{}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));

    auto ptr = std::pmr::new_delete_resource()->allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
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
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        std::pmr::new_delete_resource()->deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));
}

template<std::size_t max_blocks_per_chunk,
         std::size_t largest_required_pool_block>
void* synchronized_pool_resource<max_blocks_per_chunk,
                                 largest_required_pool_block>::data::
  debug_allocate(std::size_t bytes, std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "sync-pools::allocate   {},{} {},{}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));

    auto ptr = mr.allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
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
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        mr.deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));
}

template<std::size_t max_blocks_per_chunk,
         std::size_t largest_required_pool_block>
void* unsynchronized_pool_resource<max_blocks_per_chunk,
                                   largest_required_pool_block>::data::
  debug_allocate(std::size_t bytes, std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "unsync-pools::allocate   {},{} {},{}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));

    auto ptr = mr.allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
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
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        mr.deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));
}

template<std::size_t Length, int ID>
void* monotonic_small_buffer<Length, ID>::data::debug_allocate(
  std::size_t bytes,
  std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "mono-small-size-{}::allocate   {},{} {},{}\n",
               id,
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));

    auto ptr = mr.allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
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
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        mr.deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));
}

template<int ID>
void* monotonic_buffer<ID>::data::debug_allocate(std::size_t bytes,
                                                 std::size_t alignment) noexcept
{
    fmt::print(debug::mem_file(),
               "mono-size-{}::allocate   {},{} {},{}\n",
               id,
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));

    auto ptr = mr.allocate(bytes, alignment);
    if (ptr)
        allocated += bytes;

    fmt::print(debug::mem_file(),
               "                       {},{} {},{} = {}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
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
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated),
               ptr);

    if (ptr) {
        deallocated += bytes;
        mr.deallocate(ptr, bytes, alignment);
    }

    fmt::print(debug::mem_file(),
               "                       {},{} {},{}\n",
               make_human_readable_bytes(bytes),
               make_human_readable_bytes(alignment),
               make_human_readable_bytes(allocated),
               make_human_readable_bytes(deallocated));
}

} // namespace irt
