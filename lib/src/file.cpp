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
static constexpr auto get_mode_impl(
  const bitflags<file_open_options> c) noexcept -> std::array<CharType, 8>
{
    std::array<CharType, 8> vec;
    vec.fill(char_empty);

    int vec_pos    = 0;
    int option_pos = file_access[file::flags::read]   ? 0
                     : file_access[file_flags::write] ? 1
                                                      : 2;

    vec[vec_pos++] = char_file_open_options[option_pos];

    if (file_access[file_access::extended])
        vec[pos++] = char_file_open_options[3];
    if (file_access[file_access::binary])
        vec[pos++] = char_file_open_options[4];
    if (file_access[file_access::fail_if_exist] and
        file_access[file_flags::write])
        vec[pos++] = char_file_open_options[5];

    return vec;
}

static auto convert_path(const char8_t* filename) noexcept -> vector<wchar_t>
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

static constexpr auto get_mode(const bitflags<file_open_options> c) noexcept
{
#if defined(_WIN32)
    return get_mode_impl<wchar_t>(c);
#else
    return get_mode_impl<char>(c);
#endif
}

inline static std::FILE* to_handle(void* f) noexcept
{
    return reinterpret_cast<std::FILE*>(f);
}

expected<buffered_file> open_buffered_file(
  const std::filesystem::path&      path,
  const bitflags<file_open_options> mode) noexcept
{
    try {
        const auto  m = get_mode(mode);
        const auto* v = path.c_str();
        auto        f = open_file(v, m.data());

        if (f) {
            return buffered_file(f);
        } else {
            return new_error(file_errc::open_error);
        }
    } catch (...) {
        return new_error(file_errc::memory_error);
    }
}

expected<buffered_file> open_tmp_file() noexcept
{
#if defined(_WIN32)
    std::FILE* tmpf = nullptr;
    if (auto err = tmpfile_s(&tmpf); err == 0 and tmpf != nullptr)
        return buffered_file{ tmpf };
#else
    if (auto tmpf = std::tmpfile())
        return buffered_file{ tmpf };
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

irt::vector<char> read_entire_file(std::FILE* f) noexcept
{
    vector<char> buffer;

    if (f) {
        const auto end = std::fseek(f, 0, SEEK_END);
        if (end >= 0) {
            const auto size = std::ftell(f);
            if (size >= 0) {
                const auto beg = std::fseek(f, 0, SEEK_SET);
                if (beg == 0) {
                    irt::vector<char> buffer(size, '\0');
                    const auto        read_size = std::fread(
                      buffer.data(), 1, static_cast<size_t>(size), f);

                    if (static_cast<std::size_t>(read_size) == buffer.size()) {
                    }
                }
            }
        }
    }

    return buffer;
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

bool file::exists(const char8_t* filename) noexcept
{
#if defined(_WIN32)
    const auto vec = build_wchar_from_utf8(filename);

    return ::GetFileAttributesW(vec.data()) != INVALID_FILE_ATTRIBUTES;
#else
    struct ::stat buffer;
    return ::stat(filename, &buffer) == 0;
#endif
}

inline expected<file> file::open(
  const char8_t*                    filename,
  const bitflags<file_open_options> mode) noexcept
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

expected<file> file::open(const std::filesystem::path&      path,
                          const bitflags<file_open_options> mode) noexcept
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

inline expected<memory> memory::make(
  const i64                         length,
  const bitflags<file_open_options> mode) noexcept
{
    debug::ensure(1 <= length and length <= INT32_MAX);

    if (not(1 <= length and length <= INT32_MAX))
        return new_error(file_errc::memory_error);

    memory mem(length, mode);
    if (not std::cmp_equal(mem.data.size(), length))
        return new_error(file_errc::memory_error);

    return mem;
}

file::file(void* handle, const bitflags<file_open_options> m) noexcept
  : file_handle(handle)
  , mode(m)
{}

file::file(file&& other) noexcept
  : file_handle(std::exchange(other.file_handle, nullptr))
  , mode(std::exchange(other.mode, file_open_options::read))
{}

file& file::operator=(file&& other) noexcept
{
    if (this != &other) {
        if (file_handle)
            std::fclose(to_handle(file_handle));

        file_handle = std::exchange(other.file_handle, nullptr);
        mode        = std::exchange(other.mode, file_open_options::read);
    }

    return *this;
}

file::~file() noexcept
{
    if (file_handle)
        std::fclose(to_handle(file_handle));
}

void file::close() noexcept
{
    if (file_handle) {
        std::fclose(to_handle(file_handle));
        file_handle = nullptr;
    }
}

bool file::is_open() const noexcept { return file_handle != nullptr; }
bool file::is_eof() const noexcept { return std::feof(to_handle(file_handle)); }

i64 file::length() const noexcept
{
    debug::ensure(file_handle);

    const auto prev = std::ftell(to_handle(file_handle));
    std::fseek(to_handle(file_handle), 0, SEEK_END);

    const auto size = std::ftell(to_handle(file_handle));
    std::fseek(to_handle(file_handle), prev, SEEK_SET);

    return size;
}

i64 file::tell() const noexcept
{
    debug::ensure(file_handle);

    return std::ftell(to_handle(file_handle));
}

void file::flush() const noexcept
{
    debug::ensure(file_handle);

    std::fflush(to_handle(file_handle));
}

i64 file::seek(i64 offset, seek_origin origin) noexcept
{
    debug::ensure(file_handle);

    const auto offset_good = static_cast<long int>(offset);
    const auto origin_good = origin == seek_origin::current ? SEEK_CUR
                             : origin == seek_origin::end   ? SEEK_END
                                                            : SEEK_CUR;

    return std::fseek(to_handle(file_handle), offset_good, origin_good);
}

void file::rewind() noexcept
{
    debug::ensure(file_handle);

    std::rewind(to_handle(file_handle));
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
    const auto read = std::fread(buffer, len, 1, to_handle(file_handle));

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
    const auto written = std::fwrite(buffer, len, 1, to_handle(file_handle));

    if (written != 1)
        return false;

    return true;
}

memory::memory(const i64 length,
               const bitflags<file_open_options> /*m*/) noexcept
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
