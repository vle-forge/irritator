// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/error.hpp>
#include <irritator/file.hpp>
#include <irritator/macros.hpp>

#include <bit>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <utility>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef __MINGW32__
#include <Windows.h>
#else
#include <windows.h>
#endif
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace irt {

#if defined(_WIN32)
constexpr wchar_t char_empty               = L'\0';
constexpr wchar_t char_file_open_options[] = L"rwa+bx";
#else
constexpr char char_empty               = '\0';
constexpr char char_file_open_options[] = "rwa+bx";
#endif

#if defined(_WIN32)
static auto build_wchar_from_utf8(const char8_t* u8_str) noexcept
  -> vector<wchar_t>
{
    const auto* c_str = reinterpret_cast<const char*>(u8_str);

    const auto len = ::MultiByteToWideChar(CP_UTF8, 0, c_str, -1, nullptr, 0);

    vector<wchar_t> buf(len);
    ::MultiByteToWideChar(CP_UTF8, 0, c_str, -1, buf.data(), len);

    return buf;
}

static auto open_file(const wchar_t* filename, const wchar_t* mode)
  -> std::FILE*
{
    std::FILE* fp = nullptr;

    if (::_wfopen_s(&fp, filename, mode) == 0) {
        return fp;
    } else {
        return nullptr;
    }
}
#else
static auto open_file(const char* filename, const char* mode) -> std::FILE*
{
    return std::fopen(filename, mode);
}
#endif

template<typename CharType>
static constexpr auto get_mode_impl(const file_mode c) noexcept
  -> std::array<CharType, 8>
{
    std::array<CharType, 8> vec;
    vec.fill(char_empty);

    int vec_pos = 0;

    const int option_pos = c[file_open_options::read]    ? 0
                           : c[file_open_options::write] ? 1
                                                         : 2;

    vec[vec_pos++] = char_file_open_options[option_pos];

    if (c[file_open_options::extended])
        vec[vec_pos++] = char_file_open_options[3];
    if (not c[file_open_options::text])
        vec[vec_pos++] = char_file_open_options[4];
    if (c[file_open_options::fail_if_exist] and c[file_open_options::write])
        vec[vec_pos++] = char_file_open_options[5];

    return vec;
}

static auto convert_path(const char8_t* filename) noexcept
{
#if defined(_WIN32)
    return build_wchar_from_utf8(filename);
#else
    return reinterpret_cast<const char*>(filename);
#endif
}

static auto get_pointer(const auto filename) noexcept
{
#if defined(_WIN32)
    return filename.data();
#else
    return filename;
#endif
}

static constexpr auto get_mode(const file_mode c) noexcept
{
#if defined(_WIN32)
    return get_mode_impl<wchar_t>(c);
#else
    return get_mode_impl<char>(c);
#endif
}

expected<file> file::open_tmp() noexcept
{
    const auto m = file_mode(file_open_options::write,
                             file_open_options::extended);

#if defined(_WIN32)
    std::FILE* tmpf = nullptr;
    if (auto err = tmpfile_s(&tmpf); err == 0 and tmpf != nullptr)
        return file{ tmpf, m };
#else
    if (auto tmpf = std::tmpfile())
        return file{ tmpf, m };
#endif

    return new_error(file_errc::open_error);
}

template<typename File>
bool read_from_file(File& f, i8& value) noexcept
{
    return f.read(&value, 1);
}

template<typename File>
bool write_to_file(File& f, const i8 value) noexcept
{
    return f.write(&value, 1);
}

template<typename File>
bool read_from_file(File& f, i16& value) noexcept
{
    if (not(f.read(&value, 2)))
        return false;

    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        value = __builtin_bswap16(value);
#elif defined(_MSC_VER)
        value = _byteswap_ushort(value);
#endif
    }

    return true;
}

irt::vector<char> file::read_entire_file() noexcept
{
    debug::ensure(is_open());
    debug::ensure(mode[file_open_options::read] or
                  mode[file_open_options::extended]);

    vector<char> buffer;

    if (is_open() and
        (mode[file_open_options::read] or mode[file_open_options::extended])) {

        const auto end = std::fseek(to_file(), 0, SEEK_END);
        if (end >= 0) {
            const auto size = std::ftell(to_file());
            if (size >= 0) {
                const auto beg = std::fseek(to_file(), 0, SEEK_SET);
                if (beg == 0) {
                    buffer.resize(size, '\0');

                    const auto read_size = std::fread(
                      buffer.data(), 1, static_cast<size_t>(size), to_file());

                    // @TODO We must handle reading error
                    if (std::cmp_equal(read_size, buffer.size()))
                        buffer.clear();
                }
            }
        }
    }

    return buffer;
}

std::span<char> file::read_entire_file(std::span<char> buffer) noexcept
{
    debug::ensure(is_open());
    debug::ensure(mode[file_open_options::read] or
                  mode[file_open_options::extended]);

    if (is_open() and
        (mode[file_open_options::read] or mode[file_open_options::extended])) {

        const auto end = std::fseek(to_file(), 0, SEEK_END);
        if (end >= 0) {
            const auto size = std::ftell(to_file());
            if (size >= 0) {
                const auto beg = std::fseek(to_file(), 0, SEEK_SET);
                if (beg == 0) {
                    const auto real_size =
                      std::cmp_less(size, buffer.size())
                        ? static_cast<std::size_t>(size)
                      : std::cmp_greater(size, buffer.size())
                        ? buffer.size()
                        : static_cast<std::size_t>(size);

                    std::fill_n(buffer.data(), real_size, '\0');

                    const auto read_size =
                      std::fread(buffer.data(), 1, real_size, to_file());

                    buffer[read_size] = '\0';

                    return buffer.subspan(0, read_size);
                }
            }
        }
    }

    return std::span<char>();
}

template<typename File>
bool write_to_file(File& f, const i16 value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        auto temp = __builtin_bswap16(value);
#elif defined(_MSC_VER)
        auto temp = _byteswap_ushort(value);
#endif
        return f.write(&temp, 2);
    } else {
        return f.write(&value, 2);
    }
}

template<typename File>
bool read_from_file(File& f, i32& value) noexcept
{
    if (not(f.read(&value, 4)))
        return false;

    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        value = __builtin_bswap32(value);
#elif defined(_MSC_VER)
        value = _byteswap_ulong(value);
#endif
    }

    return true;
}

template<typename File>
bool write_to_file(File& f, const i32 value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        auto temp = __builtin_bswap32(value);
#elif defined(_MSC_VER)
        auto temp = _byteswap_ulong(value);
#endif
        return f.write(&temp, 4);
    } else {
        return f.write(&value, 4);
    }
}

template<typename File>
bool read_from_file(File& f, i64& value) noexcept
{
    if (not(f.read(&value, 8)))
        return false;

    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        value = __builtin_bswap64(value);
#elif defined(_MSC_VER)
        value = _byteswap_uint64(value);
#endif
    }

    return true;
}

template<typename File>
bool write_to_file(File& f, const i64 value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        auto temp = __builtin_bswap64(value);
#elif defined(_MSC_VER)
        auto temp = _byteswap_uint64(value);
#endif
        return f.write(&temp, 8);
    } else {
        return f.write(&value, 8);
    }
}

template<typename File>
bool read_from_file(File& f, u8& value) noexcept
{
    return f.read(&value, 1);
}

template<typename File>
bool write_to_file(File& f, const u8 value) noexcept
{
    return f.write(&value, 1);
}

template<typename File>
bool read_from_file(File& f, u16& value) noexcept
{
    if (not f.read(&value, 2))
        return false;

    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        value = __builtin_bswap16(value);
#elif defined(_MSC_VER)
        value = _byteswap_ushort(value);
#endif
    }

    return true;
}

template<typename File>
bool write_to_file(File& f, const u16 value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        auto temp = __builtin_bswap16(value);
#elif defined(_MSC_VER)
        auto temp = _byteswap_ushort(value);
#endif
        return f.write(&temp, 2);
    } else {
        return f.write(&value, 2);
    }
}

template<typename File>
bool read_from_file(File& f, u32& value) noexcept
{
    if (not f.read(&value, 4))
        return false;

    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        value = __builtin_bswap32(value);
#elif defined(_MSC_VER)
        value = _byteswap_ulong(value);
#endif
    }

    return true;
}

template<typename File>
bool write_to_file(File& f, const u32 value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        auto temp = __builtin_bswap32(value);
#elif defined(_MSC_VER)
        auto temp = _byteswap_ulong(value);
#endif
        return f.write(&temp, 4);
    } else {
        return f.write(&value, 4);
    }
}

template<typename File>
bool read_from_file(File& f, u64& value) noexcept
{
    if (not(f.read(&value, 8)))
        return false;

    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        value = __builtin_bswap64(value);
#elif defined(_MSC_VER)
        value = _byteswap_uint64(value);
#endif
    }

    return true;
}

template<typename File>
bool write_to_file(File& f, const u64 value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        auto temp = __builtin_bswap64(value);
#elif defined(_MSC_VER)
        auto temp = _byteswap_uint64(value);
#endif
        return f.write(&temp, 8);
    } else {
        return f.write(&value, 8);
    }
}

template<typename File>
bool read_from_file(File& f, float& value) noexcept
{
    if (not(f.read(reinterpret_cast<void*>(&value), 4)))
        return false;

    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        value = __builtin_bswap32(value);
#elif defined(_MSC_VER)
        value = _byteswap_ulong(value);
#endif
    }

    return true;
}

template<typename File>
bool write_to_file(File& f, const float value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        auto temp = __builtin_bswap32(value);
#elif defined(_MSC_VER)
        auto temp = _byteswap_ulong(value);
#endif
        return f.write(&temp, 4);
    } else {
        return f.write(&value, 4);
    }
}

template<typename File>
bool read_from_file(File& f, double& value) noexcept
{
    if (not(f.read(reinterpret_cast<void*>(&value), 8)))
        return false;

    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        value = __builtin_bswap64(value);
#elif defined(_MSC_VER)
        value = _byteswap_uint64(value);
#endif
    }

    return true;
}

template<typename File>
bool write_to_file(File& f, const double value) noexcept
{
    if constexpr (std::endian::native == std::endian::big) {
#if defined(__GNUC__) || defined(__clang__)
        auto temp = __builtin_bswap64(value);
#elif defined(_MSC_VER)
        auto temp = _byteswap_uint64(value);
#endif
        return f.write(&temp, 8);
    } else {
        return f.write(&value, 8);
    }
}

inline expected<file> file::open(const char8_t*  filename,
                                 const file_mode mode) noexcept
{
    debug::ensure(filename != nullptr);

    if (not filename)
        return new_error(file_errc::empty);

    try {
        const auto m = ::irt::get_mode(mode);
        const auto c = ::irt::convert_path(filename);
        const auto v = ::irt::get_pointer(c);

        if (auto f = ::irt::open_file(v, m.data()))
            return file{ f, mode };
        else
            return new_error(file_errc::open_error);
    } catch (...) {
        return new_error(file_errc::memory_error);
    }
}

expected<file> file::open(const std::filesystem::path& path,
                          const file_mode              mode) noexcept
{
    try {
        const auto m = ::irt::get_mode(mode);
        const auto v = path.c_str();

        if (auto f = ::irt::open_file(v, m.data()))
            return file{ f, mode };
        else
            return new_error(file_errc::open_error);
    } catch (...) {
        return new_error(file_errc::memory_error);
    }
}

expected<memory> memory::make(const i64 length) noexcept
{
    debug::ensure(1 <= length and length <= INT32_MAX);

    if (not(1 <= length and length <= INT32_MAX))
        return new_error(file_errc::memory_error);

    memory mem(length);
    if (not std::cmp_equal(mem.data.size(), length))
        return new_error(file_errc::memory_error);

    return mem;
}

file::file(void* handle, const file_mode m) noexcept
  : file_handle(handle)
  , mode(m)
{}

file::file(file&& other) noexcept
  : file_handle(std::exchange(other.file_handle, nullptr))
  , mode(other.mode)
{}

file& file::operator=(file&& other) noexcept
{
    if (this != &other) {
        if (file_handle)
            std::fclose(to_file());

        file_handle = std::exchange(other.file_handle, nullptr);
        mode        = other.mode;
    }

    return *this;
}

file::~file() noexcept
{
    if (file_handle)
        std::fclose(to_file());
}

void file::close() noexcept
{
    if (file_handle) {
        std::fclose(to_file());
        file_handle = nullptr;
    }
}

bool file::is_open() const noexcept { return file_handle != nullptr; }
bool file::is_eof() const noexcept { return std::feof(to_file()); }

i64 file::length() const noexcept
{
    debug::ensure(file_handle);

    const auto prev = std::ftell(to_file());
    std::fseek(to_file(), 0, SEEK_END);

    const auto size = std::ftell(to_file());
    std::fseek(to_file(), prev, SEEK_SET);

    return size;
}

i64 file::tell() const noexcept
{
    debug::ensure(file_handle);

    return std::ftell(to_file());
}

void file::flush() const noexcept
{
    debug::ensure(file_handle);

    std::fflush(to_file());
}

i64 file::seek(i64 offset, seek_origin origin) noexcept
{
    debug::ensure(file_handle);

    const auto offset_good = static_cast<long int>(offset);
    const auto origin_good = origin == seek_origin::current ? SEEK_CUR
                             : origin == seek_origin::end   ? SEEK_END
                                                            : SEEK_CUR;

    return std::fseek(to_file(), offset_good, origin_good);
}

void file::rewind() noexcept
{
    debug::ensure(file_handle);

    std::rewind(to_file());
}

bool file::read(bool& value) noexcept
{
    u8 integer_value{};

    if (read_from_file(*this, integer_value)) {
        value = integer_value != 0u;
        return true;
    }

    return false;
}

bool file::read(u8& value) noexcept { return read_from_file(*this, value); }

bool file::read(u16& value) noexcept { return read_from_file(*this, value); }

bool file::read(u32& value) noexcept { return read_from_file(*this, value); }

bool file::read(u64& value) noexcept { return read_from_file(*this, value); }

bool file::read(i8& value) noexcept { return read_from_file(*this, value); }

bool file::read(i16& value) noexcept { return read_from_file(*this, value); }

bool file::read(i32& value) noexcept { return read_from_file(*this, value); }

bool file::read(i64& value) noexcept { return read_from_file(*this, value); }

bool file::read(float& value) noexcept { return read_from_file(*this, value); }

bool file::read(double& value) noexcept { return read_from_file(*this, value); }

bool file::write(const bool value) noexcept
{
    const u8 new_value = value ? 0xff : 0x0;
    return write_to_file(*this, new_value);
}

bool file::write(const u8 value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const u16 value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const u32 value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const u64 value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const i8 value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const i16 value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const i32 value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const i64 value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const float value) noexcept
{
    return write_to_file(*this, value);
}

bool file::write(const double value) noexcept
{
    return write_to_file(*this, value);
}

bool file::read(void* buffer, i64 length) noexcept
{
    debug::ensure(file_handle);
    debug::ensure(buffer);
    debug::ensure(length > 0);

    if (not file_handle or not buffer or length <= 0)
        return false;

    const auto len  = static_cast<size_t>(length);
    const auto read = std::fread(buffer, len, 1, to_file());

    if (read != 1)
        return false;

    return true;
}

bool file::write(const void* buffer, i64 length) noexcept
{
    debug::ensure(file_handle);
    debug::ensure(buffer);
    debug::ensure(length > 0);

    if (not file_handle or not buffer or length <= 0)
        return false;

    const auto len     = static_cast<size_t>(length);
    const auto written = std::fwrite(buffer, len, 1, to_file());

    if (written != 1)
        return false;

    return true;
}

void* file::get_handle() const noexcept { return file_handle; }

std::FILE* file::to_file() const noexcept
{
    return reinterpret_cast<std::FILE*>(file_handle);
}

file_mode file::get_mode() const noexcept { return mode; }

memory::memory(const i64 length) noexcept
  : data(static_cast<i32>(length), static_cast<i32>(length))
  , pos(0)
{}

memory::memory(memory&& other) noexcept
  : data(std::move(other.data))
  , pos(other.pos)
{
    other.pos = 0;
}

memory& memory::operator=(memory&& other) noexcept
{
    data      = std::move(other.data);
    pos       = other.pos;
    other.pos = 0;

    return *this;
}

bool memory::is_open() const noexcept { return data.capacity() == 0; }
bool memory::is_eof() const noexcept { return pos == data.capacity(); }

i64 memory::length() const noexcept { return data.capacity(); }

i64 memory::tell() const noexcept { return pos; }

void memory::flush() const noexcept {}

i64 memory::seek(i64 offset, seek_origin origin) noexcept
{
    switch (origin) {
    case seek_origin::current:
        pos += offset;
        break;
    case seek_origin::end:
        pos = data.capacity() - offset;
        break;
    case seek_origin::set:
        pos = offset;
        break;
    }

    return pos;
}

void memory::rewind() noexcept { pos = 0; }

bool memory::read(bool& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(u8& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(u16& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(u32& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(u64& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(i8& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(i16& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(i32& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(i64& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(float& value) noexcept { return read(&value, sizeof(value)); }

bool memory::read(double& value) noexcept
{
    return read(&value, sizeof(value));
}

bool memory::write(const bool value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const u8 value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const u16 value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const u32 value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const u64 value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const i8 value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const i16 value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const i32 value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const i64 value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const float value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::write(const double value) noexcept
{
    return write(&value, sizeof(value));
}

bool memory::read(void* buffer, i64 length) noexcept
{
    debug::ensure(data.ssize() == data.capacity());
    debug::ensure(buffer);
    debug::ensure(length > 0);

    if (data.ssize() != data.capacity() or not buffer or length <= 0)
        return false;

    if (std::cmp_less_equal(pos + length, data.capacity())) {
        std::copy_n(data.data() + pos,
                    static_cast<size_t>(length),
                    reinterpret_cast<u8*>(buffer));

        pos += length;
        return true;
    }

    return false;
}

bool memory::write(const void* buffer, i64 length) noexcept
{
    debug::ensure(data.ssize() == data.capacity());
    debug::ensure(buffer);
    debug::ensure(length > 0);

    if (data.ssize() != data.capacity() or not buffer or length <= 0)
        return false;

    if (std::cmp_less_equal(pos + length, data.capacity())) {
        std::copy_n(reinterpret_cast<const u8*>(buffer),
                    static_cast<size_t>(length),
                    data.data() + pos);

        pos += length;
        return true;
    }

    return false;
}

} // namespace irt
