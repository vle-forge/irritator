// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/file.hpp>

#include <bit>

#include <cerrno>
#include <cstdio>
#include <cstdlib>

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

static std::FILE* internal_fopen(const char* filename,
                                 const char* mode) noexcept
{
#ifdef _WIN32
    const auto filname_sz =
      ::MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
    const auto mode_sz =
      ::MultiByteToWideChar(CP_UTF8, 0, mode, -1, nullptr, 0);
    vector<wchar_t> buf;
    buf.resize(filname_sz + mode_sz);

    ::MultiByteToWideChar(
      CP_UTF8, 0, filename, -1, (wchar_t*)&buf[0], filname_sz);
    ::MultiByteToWideChar(
      CP_UTF8, 0, mode, -1, (wchar_t*)&buf[filname_sz], mode_sz);

    return ::_wfopen((const wchar_t*)&buf[0], (const wchar_t*)&buf[filname_sz]);
#else
    return std::fopen(filename, mode);
#endif //  _WIN32
}

inline static std::FILE* to_handle(void* f) noexcept
{
    return reinterpret_cast<std::FILE*>(f);
}

inline static void* to_void(std::FILE* f) noexcept
{
    return reinterpret_cast<void*>(f);
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
    if (!f.read(&value, 2))
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
    if (!f.read(&value, 4))
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
    if (!f.read(&value, 8))
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
    if (!f.read(&value, 2))
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
    if (!f.read(&value, 4))
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
    if (!f.read(&value, 8))
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
    if (!f.read(reinterpret_cast<void*>(&value), 4))
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
    if (!f.read(reinterpret_cast<void*>(&value), 8))
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

auto file::make_file(const char*     filename,
                     const open_mode mode_) noexcept -> result<file>
{
    errno = 0;

    auto       f      = internal_fopen(filename,
                            mode_ == open_mode::read    ? "rb"
                                       : mode_ == open_mode::write ? "wb"
                                                                   : "ab");
    const auto cerrno = errno;
    if (f == nullptr) {
        if (cerrno == EACCES)
            return new_error(file::permission_error{});
        if (cerrno == ENFILE)
            return new_error(file::permission_error{});

        return new_error(file::not_exist_error{});
    }

    return file{ to_void(f), mode_ };
}

bool file::exists(const char* filename) noexcept
{
#if defined(_WIN32)
    const auto filname_sz =
      ::MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
    vector<wchar_t> buf;
    buf.resize(filname_sz);
    ::MultiByteToWideChar(
      CP_UTF8, 0, filename, -1, (wchar_t*)&buf[0], filname_sz);

    return ::GetFileAttributesW((const wchar_t*)&buf[0]) !=
           INVALID_FILE_ATTRIBUTES;
#else
    struct ::stat buffer;
    return ::stat(filename, &buffer) == 0;
#endif
}

file::file(void* handle, const open_mode m) noexcept
  : file_handle(handle)
  , mode(m)
{}

file::file(file&& other) noexcept
  : file_handle(other.file_handle)
{
    other.file_handle = nullptr;
}

file& file::operator=(file&& other) noexcept
{
    if (this != &other) {
        if (file_handle)
            std::fclose(to_handle(file_handle));

        file_handle       = other.file_handle;
        other.file_handle = nullptr;
    }

    return *this;
}

file::~file() noexcept
{
    if (file_handle)
        std::fclose(to_handle(file_handle));
}

void file::open(const char* filename, const open_mode mode_) noexcept
{
    if (file_handle)
        std::fclose(to_handle(file_handle));

    mode = mode_;

    file_handle = to_void(internal_fopen(filename,
                                         mode == open_mode::read    ? "rb"
                                         : mode == open_mode::write ? "wb"
                                                                    : "ab"));
}

void file::close() noexcept
{
    if (file_handle) {
        std::fclose(to_handle(file_handle));
        file_handle = nullptr;
    }
}

bool file::is_open() const noexcept { return file_handle != nullptr; }

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

    if (auto ret = read_from_file(*this, integer_value); ret) {
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

    const auto len     = static_cast<size_t>(length);
    const auto written = std::fread(buffer, len, 1, to_handle(file_handle));
    return written == 1;
}

bool file::write(const void* buffer, i64 length) noexcept
{
    debug::ensure(file_handle);
    debug::ensure(buffer);
    debug::ensure(length > 0);

    const auto len     = static_cast<size_t>(length);
    const auto written = std::fwrite(buffer, len, 1, to_handle(file_handle));
    return written == 1;
}

memory::memory(const i64 length, const open_mode /*mode*/) noexcept
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

    if (pos + length < static_cast<i64>(data.capacity())) {
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

    if (pos + length < static_cast<i64>(data.capacity())) {
        std::copy_n(reinterpret_cast<const u8*>(buffer),
                    static_cast<size_t>(length),
                    data.data() + pos);

        pos += length;

        return true;
    }

    return false;
}

} // namespace irt