// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_BINARY_HPP
#define ORG_VLEPROJECT_IRRITATOR_BINARY_HPP

#include <irritator/core.hpp>
#include <irritator/file.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstring>
#include <limits>
#include <span>
#include <type_traits>
#include <utility>

#include <cstdio>

/// Binary serialization in memory: @c binary_writer builds a buffer of bytes,
/// @c binary_reader reads a span of bytes. Neither knows @c irt::file: a dump
/// in a file is a @c write_all of the buffer and a @c read_entire_file in a
/// reader (see the end of this file).
///
/// Principles
/// - The same function describes a type for the writing and for the reading
///   (@c binary_serialize), so the two directions can not drift apart.
/// - Errors are sticky: a writer or a reader that failed ignores the following
///   operations, and @c ok() is tested once, at the end.
/// - A reader never reads outside its span and never allocates more than the
///   size of its input: every count read in a stream is checked against the
///   remaining bytes before a container is resized.
/// - The format is little endian, with fixed-width types only (see
///   @c binary_scalar).
namespace irt {

static_assert(std::endian::native == std::endian::little,
              "the binary format is little endian: a byte swap is needed on "
              "big endian platforms");
static_assert(sizeof(float) == 4 and sizeof(double) == 8 and
                std::numeric_limits<float>::is_iec559 and
                std::numeric_limits<double>::is_iec559);

/// Fixed-width arithmetic types. @c bool (stored as 0 or 1), @c long,
/// @c long long (platform dependent) and @c long double are rejected.
template<typename T>
concept binary_scalar =
  std::same_as<T, i8> or std::same_as<T, i16> or std::same_as<T, i32> or
  std::same_as<T, i64> or std::same_as<T, u8> or std::same_as<T, u16> or
  std::same_as<T, u32> or std::same_as<T, u64> or std::same_as<T, float> or
  std::same_as<T, double>;

/// Enumerations (ids...) stored as their underlying type.
template<typename T>
concept binary_enum =
  std::is_enum_v<T> and binary_scalar<std::underlying_type_t<T>>;

/// Opt-in for a trivial structure that contains a floating point or that
/// has padding bytes. The author of the type guarantees that it has no
/// padding (the file is deterministic) and no pointer:
/// @code
/// template<> inline constexpr bool irt::binary_raw_layout<cell> = true;
/// @endcode
template<typename T>
inline constexpr bool binary_raw_layout = false;

/// Opt-out for a trivial type that must not be dumped as a block of bytes
/// because not every bit pattern is a valid value (a string with its size, a
/// type with an invariant) or because it holds bytes that are not part of the
/// value. Such a type needs its own @c binary_serialize, see @c small_string
/// in @c binary-containers.hpp.
template<typename T>
inline constexpr bool binary_no_raw = false;

/// Types that can be dumped as a block of bytes (arrays of @c float, of ids,
/// of trivial structures without padding...).
template<typename T>
concept binary_raw =
  binary_scalar<T> or binary_enum<T> or
  (std::is_class_v<T> and std::is_trivially_copyable_v<T> and
   std::is_standard_layout_v<T> and not binary_no_raw<T> and
   (std::has_unique_object_representations_v<T> or binary_raw_layout<T>));

/// Contiguous containers with a dynamic size (@c irt::vector).
template<typename C>
concept binary_dynamic_array = requires(C& c, const C& cc, std::size_t n) {
    { cc.size() } -> std::integral;
    { c.data() };
    { c.resize(n) } -> std::convertible_to<bool>;
};

namespace details {

template<typename C>
constexpr std::size_t max_container_size() noexcept
{
    using size_type = decltype(std::declval<const C&>().size());

    return static_cast<std::size_t>(std::numeric_limits<size_type>::max());
}

template<typename C>
bool resize_container(C& c, const std::size_t n) noexcept
{
    using size_type = decltype(std::declval<const C&>().size());

    if (n > max_container_size<C>())
        return false;

    return static_cast<bool>(c.resize(static_cast<size_type>(n)));
}

/// After a failed read: no half filled container.
template<typename C>
void clear_container(C& c) noexcept
{
    if constexpr (requires { c.clear(); })
        c.clear();
}

} // namespace details

/// Builds a buffer of bytes. @c Buffer is a contiguous container of bytes
/// with @c size(), @c data() and @c resize() (an @c irt::vector<u8> with the
/// allocator of your choice): the writer only grows it, by steps of 1.5, and
/// @c clear() keeps its capacity: reusing a writer for each snapshot does not
/// allocate.
template<typename Buffer>
class basic_binary_writer
{
public:
    static constexpr bool is_writer = true;
    static constexpr bool is_reader = false;

    basic_binary_writer() noexcept = default;

    /// Use an already allocated buffer (its content is ignored).
    explicit basic_binary_writer(Buffer&& buffer) noexcept
      : buf(std::move(buffer))
    {}

    /// @return false after a failure (allocation) or @c fail().
    bool ok() const noexcept { return not failed; }

    /// Marks the writer as failed (a value that can not be dumped...).
    void fail() noexcept { failed = true; }

    /// The number of written bytes.
    std::size_t size() const noexcept { return used; }

    /// The number of bytes that can be written without allocation.
    std::size_t capacity() const noexcept
    {
        return static_cast<std::size_t>(buf.size());
    }

    /// The written bytes.
    std::span<const u8> bytes() const noexcept
    {
        return std::span<const u8>(buf.data(), used);
    }

    /// Forgets the content and the error, keeps the allocated memory.
    void clear() noexcept
    {
        used   = 0;
        failed = false;
    }

    /// Allocates for @c n bytes in total (for example the size of the
    /// previous snapshot).
    bool reserve(const std::size_t n) noexcept
    {
        if (failed)
            return false;

        if (n <= capacity())
            return true;

        return details::resize_container(buf, n);
    }

    void write_bytes(const void* data, const std::size_t length) noexcept
    {
        if (failed or length == 0)
            return;

        debug::ensure(data != nullptr);

        if (length > details::max_container_size<Buffer>() - used) {
            failed = true;
            return;
        }

        const auto needed = used + length;
        if (needed > capacity() and not grow(needed)) {
            failed = true;
            return;
        }

        std::memcpy(buf.data() + used, data, length);
        used = needed;
    }

    template<binary_scalar T>
    void write(const T value) noexcept
    {
        write_bytes(std::addressof(value), sizeof(T));
    }

    template<binary_enum E>
    void write(const E value) noexcept
    {
        write(static_cast<std::underlying_type_t<E>>(value));
    }

    /// A boolean is stored in one byte, 0 or 1.
    template<std::same_as<bool> Bool>
    void write(const Bool value) noexcept
    {
        write(static_cast<u8>(value ? 1 : 0));
    }

    /// The elements, without a count.
    template<binary_raw T>
    void write_span(const std::span<const T> elements) noexcept
    {
        write_bytes(elements.data(), elements.size() * sizeof(T));
    }

    /// The count (u64), the size of an element (u32) and the elements.
    template<binary_raw T>
    void write_array(const std::span<const T> elements) noexcept
    {
        write(static_cast<u64>(elements.size()));
        write(static_cast<u32>(sizeof(T)));
        write_span(elements);
    }

    /// Writes the values with the type dispatch: scalars, enumerations,
    /// booleans, @c binary_raw structures (one block) and every other type
    /// with a @c binary_serialize function. A @c binary_raw type is always
    /// dumped as a block: it can not have its own @c binary_serialize.
    template<typename... Ts>
    void operator()(const Ts&... values) noexcept
    {
        (io(values), ...);
    }

private:
    template<typename T>
    void io(const T& value) noexcept
    {
        if constexpr (std::is_same_v<T, bool> or binary_scalar<T> or
                      binary_enum<T>)
            write(value);
        else if constexpr (binary_raw<T>)
            write_bytes(std::addressof(value), sizeof(T));
        else
            binary_serialize(*this, const_cast<T&>(value)); // not modified
    }

    bool grow(const std::size_t needed) noexcept
    {
        const auto cap       = capacity();
        const auto geometric = std::max({ needed, cap + cap / 2,
                                          std::size_t{ 256 } });

        if (details::resize_container(buf, geometric))
            return true;

        return details::resize_container(buf, needed);
    }

    static_assert(sizeof(u8) == 1);

    Buffer      buf{};
    std::size_t used   = 0;
    bool        failed = false;
};

/// Reads a span of bytes (a file loaded in memory, the bytes of a writer...).
/// The reader does not own the bytes.
class binary_reader
{
public:
    static constexpr bool is_writer = false;
    static constexpr bool is_reader = true;

    explicit binary_reader(const std::span<const u8> input) noexcept
      : in(input)
    {}

    /// @return false after a read outside the input or a @c fail().
    bool ok() const noexcept { return not failed; }

    /// Marks the reader as failed (a value read in the stream is not valid:
    /// an unknown id, a count that is too big...).
    void fail() noexcept { failed = true; }

    std::size_t position() const noexcept { return pos; }
    std::size_t remaining() const noexcept { return in.size() - pos; }

    /// Limits the memory that the containers may allocate to hold the data
    /// (the capacity of a @c data_array can be far greater than the size of
    /// its dump). The default is no limit, which is fine for a snapshot made
    /// by the program itself. Set it before reading a file from an untrusted
    /// source.
    void set_allocation_limit(const std::size_t bytes) noexcept
    {
        limit = bytes;
    }

    /// An opaque pointer to the environment of the reader, for the types that
    /// can not be rebuilt from their own bytes (a pointer to a buffer that
    /// belongs to another object, see @c irt::source). The reader does not
    /// own it and never uses it.
    void set_context(void* p) noexcept { ctx = p; }

    template<typename T>
    T* context() const noexcept
    {
        return static_cast<T*>(ctx);
    }

    /// Called by the containers before an allocation of @c bytes bytes.
    /// @return false and marks the reader as failed if the limit is exceeded.
    bool can_allocate(const std::size_t bytes) noexcept
    {
        if (failed or bytes > limit) {
            failed = true;
            return false;
        }

        return true;
    }

    /// Reads @c length bytes. On error, the destination is filled with zeros.
    void read_bytes(void* out, const std::size_t length) noexcept
    {
        if (length == 0)
            return;

        debug::ensure(out != nullptr);

        if (failed or length > remaining()) {
            failed = true;
            std::memset(out, 0, length);
            return;
        }

        std::memcpy(out, in.data() + pos, length);
        pos += length;
    }

    template<binary_scalar T>
    void read(T& value) noexcept
    {
        read_bytes(std::addressof(value), sizeof(T));
    }

    template<binary_enum E>
    void read(E& value) noexcept
    {
        std::underlying_type_t<E> raw{};
        read(raw);
        value = static_cast<E>(raw);
    }

    /// A boolean is 0 or 1: any other value is an error.
    void read(bool& value) noexcept
    {
        u8 raw = 0;
        read(raw);

        if (raw > 1)
            failed = true;

        value = raw == 1;
    }

    /// The elements, without a count.
    template<binary_raw T>
    void read_span(const std::span<T> elements) noexcept
    {
        read_bytes(elements.data(), elements.size() * sizeof(T));
    }

    /// Reads the header written by @c write_array and checks it: the size of
    /// the element and the count (the elements must be in the remaining
    /// bytes, so a corrupted count never allocates).
    /// @return The number of elements to read with @c read_span, 0 on error.
    template<binary_raw T>
    std::size_t read_array_header() noexcept
    {
        u64 count = 0;
        u32 size  = 0;

        read(count);
        read(size);

        if (failed)
            return 0;

        if (size != sizeof(T) or count > remaining() / sizeof(T)) {
            failed = true;
            return 0;
        }

        return static_cast<std::size_t>(count);
    }

    /// Reads the values with the type dispatch (see the writer).
    template<typename... Ts>
    void operator()(Ts&... values) noexcept
    {
        (io(values), ...);
    }

private:
    template<typename T>
    void io(T& value) noexcept
    {
        if constexpr (std::is_same_v<T, bool> or binary_scalar<T> or
                      binary_enum<T>)
            read(value);
        else if constexpr (binary_raw<T>)
            read_bytes(std::addressof(value), sizeof(T));
        else
            binary_serialize(*this, value);
    }

    std::span<const u8> in;
    std::size_t         pos    = 0;
    std::size_t         limit  = std::numeric_limits<std::size_t>::max();
    void*               ctx    = nullptr;
    bool                failed = false;
};

using binary_writer = basic_binary_writer<vector<u8>>;

/* * * * * * * * * *
 *
 * binary_serialize: one description for both directions
 *
 * A type is described with a function found by argument dependent lookup:
 *
 *   template<typename Ar>
 *   void binary_serialize(Ar& ar, my_type& x) noexcept
 *   {
 *       ar(x.a, x.b);                       // scalars, enums, bool, types
 *       if constexpr (Ar::is_reader) { ... } // checks, resize, rebuild
 *   }
 *
 * The writer calls it with a const_cast: the function must not modify the
 * object when @c Ar::is_writer.
 *
 * * * * * * * * * */

/// std::array: the elements, without a count.
template<typename Ar, typename T, std::size_t N>
void binary_serialize(Ar& ar, std::array<T, N>& a) noexcept
{
    if constexpr (binary_raw<T>) {
        if constexpr (Ar::is_writer)
            ar.write_span(std::span<const T>(a));
        else
            ar.read_span(std::span<T>(a));
    } else {
        for (auto& element : a)
            ar(element);
    }
}

/// C arrays (@c input_port x[4]): like @c std::array, the elements without a
/// count.
template<typename Ar, typename T, std::size_t N>
void binary_serialize(Ar& ar, T (&a)[N]) noexcept
{
    if constexpr (binary_raw<T>) {
        if constexpr (Ar::is_writer)
            ar.write_span(std::span<const T>(a, N));
        else
            ar.read_span(std::span<T>(a, N));
    } else {
        for (auto& element : a)
            ar(element);
    }
}

/// Dynamic arrays (irt::vector): the count, then the elements.
///
/// - elements @c binary_raw: the header of @c write_array and one block.
/// - other elements: the count (u64) and the elements one by one; an element
///   must write at least one byte, so a count that is greater than the
///   remaining bytes is an error.
template<typename Ar, binary_dynamic_array C>
void binary_serialize(Ar& ar, C& c) noexcept
{
    using value_type = std::remove_cvref_t<decltype(*c.data())>;

    if constexpr (Ar::is_writer) {
        const auto size = static_cast<std::size_t>(c.size());

        if constexpr (binary_raw<value_type>) {
            ar.write_array(std::span<const value_type>(c.data(), size));
        } else {
            ar.write(static_cast<u64>(size));
            for (std::size_t i = 0; i != size; ++i)
                ar(c.data()[i]);
        }
    } else {
        std::size_t size = 0;

        if constexpr (binary_raw<value_type>) {
            size = ar.template read_array_header<value_type>();
        } else {
            u64 count = 0;
            ar.read(count);

            if (not ar.ok() or count > ar.remaining()) {
                ar.fail();
                details::clear_container(c);
                return;
            }

            size = static_cast<std::size_t>(count);
        }

        if (not ar.ok()) {
            details::clear_container(c);
            return;
        }

        if (not details::resize_container(c, size)) { // a small_vector is full
            ar.fail();
            details::clear_container(c);
            return;
        }

        if constexpr (binary_raw<value_type>) {
            ar.read_span(std::span<value_type>(c.data(), size));
        } else {
            for (std::size_t i = 0; i != size and ar.ok(); ++i)
                ar(c.data()[i]);
        }

        if (not ar.ok())
            details::clear_container(c);
    }
}

/* * * * * * * * * *
 *
 * file header
 *
 * * * * * * * * * */

/// A magic number from four characters: binary_magic('I', 'R', 'T', 'D').
constexpr u32 binary_magic(const char a,
                           const char b,
                           const char c,
                           const char d) noexcept
{
    return static_cast<u32>(static_cast<u8>(a)) |
           (static_cast<u32>(static_cast<u8>(b)) << 8) |
           (static_cast<u32>(static_cast<u8>(c)) << 16) |
           (static_cast<u32>(static_cast<u8>(d)) << 24);
}

inline constexpr u32 binary_byte_order_mark = 0x01020304u;

/// Writes the magic number, the version of the format and a byte order mark
/// (12 bytes).
template<typename Buffer>
void write_header(basic_binary_writer<Buffer>& w,
                  const u32                    magic,
                  const u32                    version) noexcept
{
    w(magic, version, binary_byte_order_mark);
}

/// Reads and checks the header.
/// @return The version, or 0 and a failed reader if the magic number or the
///     byte order mark are wrong (or the input too short). The caller decides
///     which versions it can read.
inline u32 read_header(binary_reader& r, const u32 magic) noexcept
{
    u32 file_magic = 0;
    u32 version    = 0;
    u32 order      = 0;

    r(file_magic, version, order);

    if (file_magic != magic or order != binary_byte_order_mark)
        r.fail();

    return r.ok() ? version : 0u;
}

/* * * * * * * * * *
 *
 * dumps in files
 *
 * * * * * * * * * */

/// Writes all the bytes in the file.
/// @return false if the file is closed or if the write failed (the buffered
///     data are written by @c file::close(): check its result).
inline bool write_all(const file& f, const std::span<const u8> bytes) noexcept
{
    if (not f.is_open())
        return false;

    if (bytes.empty())
        return true;

    return std::fwrite(bytes.data(), 1, bytes.size(), f.to_file()) ==
           bytes.size();
}

/// The bytes of a buffer returned by @c file::read_entire_file().
inline std::span<const u8> bytes_of(const vector<char>& buffer) noexcept
{
    return std::span<const u8>(reinterpret_cast<const u8*>(buffer.data()),
                               static_cast<std::size_t>(buffer.size()));
}

} // namespace irt

#endif
