// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/error.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/macros.hpp>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string_view>
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

std::ifstream path::open_std_ifstream() const noexcept
{
    debug::ensure(not empty());

    const auto std_path = to_std_path();

    return std::ifstream(std_path);
}

std::ofstream path::open_std_ofstream() const noexcept
{
    debug::ensure(not empty());

    const auto std_path = to_std_path();

    return std::ofstream(std_path);
}

std_file path::open_std_file(const char* mode) const noexcept
{
    debug::ensure(not empty());

#if defined(_WIN32)
    try {
        const auto std_path = to_std_path();
        wchar_t    wmode[8]{};
        std::mbstowcs(wmode, mode, std::size(wmode) - 1);

        return std_file(_wfopen(std_path.c_str(), wmode));
    } catch (...) {
        return std_file();
    }
#else
    return std_file(std::fopen(c_str(), mode));
#endif
}

/* * * * * * * * * * *
 *
 * irt::file impl
 *
 * * * * * * * * * * */


static constexpr auto get_mode(const file_mode c) noexcept -> small_string<8>
{
    constexpr char char_file_open_options[] = "rwa+bx";

    auto vec = small_string<8>{};

    const int option_pos = c[file_open_options::read]    ? 0
                           : c[file_open_options::write] ? 1
                                                         : 2;

    vec.push_back(char_file_open_options[option_pos]);

    if (c[file_open_options::extended])
        vec.push_back(char_file_open_options[3]);
    if (not c[file_open_options::text])
        vec.push_back(char_file_open_options[4]);
    if (c[file_open_options::fail_if_exist] and c[file_open_options::write])
        vec.push_back(char_file_open_options[5]);

    return vec;
}

expected<file> file::open_tmp() noexcept
{
    const auto m = file_mode(file_open_options::write,
                             file_open_options::extended);

#if defined(_WIN32)
    std::FILE* tmpf = nullptr;
    if (auto err = tmpfile_s(&tmpf); err == 0 and tmpf != nullptr) {
        return file{ std_file(tmpf), m };
    }

    return make_error(static_cast<i16>(::GetLastError()), category::generic);
#else
    if (auto tmpf = std::tmpfile())
        return file{ std_file(tmpf), m };

    return make_error(errno, category::generic);
#endif
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
    return f.read(&value, 2);
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
                    if (buffer.resize(size)) {
                        std::fill_n(buffer.data(), buffer.size(), '\0');

                        const auto read_size = std::fread(
                          buffer.data(), 1, static_cast<size_t>(size),
                          to_file());

                        buffer.resize(read_size);
                    }
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
                    const auto real_size = std::cmp_less(size, buffer.size())
                                             ? static_cast<std::size_t>(size)
                                           : std::cmp_greater(size,
                                                              buffer.size())
                                             ? buffer.size()
                                             : static_cast<std::size_t>(size);

                    std::fill_n(buffer.data(), real_size, '\0');

                    const auto read_size = std::fread(buffer.data(), 1,
                                                      real_size, to_file());

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
    return f.write(&value, 2);
}

template<typename File>
bool read_from_file(File& f, i32& value) noexcept
{
    return f.read(&value, 4);
}

template<typename File>
bool write_to_file(File& f, const i32 value) noexcept
{
    return f.write(&value, 4);
}

template<typename File>
bool read_from_file(File& f, i64& value) noexcept
{
    return f.read(&value, 8);
}

template<typename File>
bool write_to_file(File& f, const i64 value) noexcept
{
    return f.write(&value, 8);
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
    return f.read(&value, 2);
}

template<typename File>
bool write_to_file(File& f, const u16 value) noexcept
{
    return f.write(&value, 2);
}

template<typename File>
bool read_from_file(File& f, u32& value) noexcept
{
    return f.read(&value, 4);
}

template<typename File>
bool write_to_file(File& f, const u32 value) noexcept
{
    return f.write(&value, 4);
}

template<typename File>
bool read_from_file(File& f, u64& value) noexcept
{
    return f.read(&value, 8);
}

template<typename File>
bool write_to_file(File& f, const u64 value) noexcept
{

    return f.write(&value, 8);
}

template<typename File>
bool read_from_file(File& f, float& value) noexcept
{
    return f.read(reinterpret_cast<void*>(&value), 4);
}

template<typename File>
bool write_to_file(File& f, const float value) noexcept
{
    return f.write(&value, 4);
}

template<typename File>
bool read_from_file(File& f, double& value) noexcept
{
    return f.read(reinterpret_cast<void*>(&value), 8);
}

template<typename File>
bool write_to_file(File& f, const double value) noexcept
{
    return f.write(&value, 8);
}

expected<file> file::open(const path& filename, const file_mode mode) noexcept
{
    debug::ensure(filename != nullptr);

    if (filename.empty())
        return make_error(
          static_cast<std::int16_t>(std::errc::invalid_argument),
          category::generic);

    const auto m = ::irt::get_mode(mode);
    auto       f = filename.open_std_file(m.c_str());

    if (not f.get())
        return make_error(modeling_errc::file_error);

    return file{ std::move(f), mode };
}

expected<memory> memory::make(const i64 length) noexcept
{
    debug::ensure(1 <= length and length <= INT32_MAX);

    if (not(1 <= length and length <= INT32_MAX))
        return make_error(
          static_cast<std::int16_t>(std::errc::invalid_argument),
          category::generic);

    memory mem(length);
    if (not std::cmp_equal(mem.data.size(), length))
        return make_error(
          static_cast<std::int16_t>(std::errc::invalid_argument),
          category::generic);

    return mem;
}

file::file(file&& other) noexcept
  : file_handle(std::exchange(other.file_handle, nullptr))
  , mode(other.mode)
{}

file& file::operator=(file&& other) noexcept
{
    if (this != &other) {
        if (file_handle)
            file_handle.reset();

        file_handle = std::move(other.file_handle);
        other.file_handle.reset();
        mode = other.mode;
    }

    return *this;
}

void file::close() noexcept
{
    if (file_handle) {
        file_handle.reset();
    }
}

bool file::is_open() const noexcept { return file_handle.get() != nullptr; }
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

    if (read(integer_value)) {
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

    if (not file_handle or not buffer or length <= 0) {
        using namespace std::string_view_literals;

        debug::log(log_level::critical, "file read error: bad arguments"sv);
        return false;
    }

    const auto len  = static_cast<size_t>(length);
    const auto read = std::fread(buffer, len, 1, to_file());

    if (read != 1) {
        debug::log(log_level::critical, [&](auto& m) {
            using namespace std::string_view_literals;

            format(m, "file read error: length {} bytes", len);
        });

        return false;
    }

    return true;
}

bool file::write(const void* buffer, i64 length) noexcept
{
    debug::ensure(file_handle);
    debug::ensure(buffer);
    debug::ensure(length > 0);

    if (not file_handle or not buffer or length <= 0) {
        using namespace std::string_view_literals;

        debug::log(log_level::critical, "file write error: bad arguments"sv);
        return false;
    }

    const auto len     = static_cast<size_t>(length);
    const auto written = std::fwrite(buffer, len, 1, to_file());

    if (written != 1) {
        debug::log(log_level::critical, [&](auto& m) {
            using namespace std::string_view_literals;

            format(m, "file write error: length {} bytes", len);
        });

        return false;
    }

    return true;
}

std::FILE* file::to_file() const noexcept { return file_handle.get(); }

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
bool memory::is_eof() const noexcept
{
    return std::cmp_equal(pos, data.capacity());
}

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
    debug::ensure(data.size() == data.capacity());
    debug::ensure(buffer);
    debug::ensure(length > 0);

    if (data.size() != data.capacity() or not buffer or length <= 0)
        return false;

    if (std::cmp_less_equal(pos + length, data.capacity())) {
        std::copy_n(data.data() + pos, static_cast<size_t>(length),
                    reinterpret_cast<u8*>(buffer));

        pos += length;
        return true;
    }

    return false;
}

bool memory::write(const void* buffer, i64 length) noexcept
{
    debug::ensure(data.size() == data.capacity());
    debug::ensure(buffer);
    debug::ensure(length > 0);

    if (data.size() != data.capacity() or not buffer or length <= 0)
        return false;

    if (std::cmp_less_equal(pos + length, data.capacity())) {
        std::copy_n(reinterpret_cast<const u8*>(buffer),
                    static_cast<size_t>(length), data.data() + pos);

        pos += length;
        return true;
    }

    return false;
}

} // namespace irt
