// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/error.hpp>
#include <irritator/file.hpp>
#include <irritator/format.hpp>
#include <irritator/macros.hpp>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <span>
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

namespace {

// 64 bits positions: `long` (fseek/ftell) has only 32 bits on Windows.

int seek64(std::FILE* f, const i64 offset, const int whence) noexcept
{
#if defined(_WIN32)
    return ::_fseeki64(f, offset, whence);
#else
    if (not std::in_range<off_t>(offset))
        return -1;

    return ::fseeko(f, static_cast<off_t>(offset), whence);
#endif
}

i64 tell64(std::FILE* f) noexcept
{
#if defined(_WIN32)
    return ::_ftelli64(f);
#else
    return ::ftello(f);
#endif
}

} // namespace

expected<file> file::open_tmp() noexcept
{
    const auto m = file_mode(file_open_options::write,
                             file_open_options::extended);

#if defined(_WIN32)
    std::FILE* tmpf = nullptr;
    const auto err  = tmpfile_s(&tmpf);
    if (err == 0 and tmpf != nullptr) {
        return file{ std_file(tmpf), m };
    }

    return make_error(static_cast<i16>(err != 0 ? err : EINVAL),
                      category::generic);
#else
    if (auto tmpf = std::tmpfile())
        return file{ std_file(tmpf), m };

    return make_error(errno, category::generic);
#endif
}

expected<file> file::open(const path& filename, const file_mode mode) noexcept
{
    debug::ensure(not filename.empty());

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

std::optional<file> file::try_open(const path&     filename,
                                   const file_mode mode) noexcept
{
    debug::ensure(not filename.empty());

    if (filename.empty())
        return std::nullopt;

    const auto m = ::irt::get_mode(mode);
    auto       f = filename.open_std_file(m.c_str());

    if (not f.get())
        return std::nullopt;

    return file{ std::move(f), mode };
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

bool file::close() noexcept
{
    if (not file_handle)
        return true;

    // `fclose` is called by hand to get its result: a delayed write error
    // (disk full) is only reported here.

    return std::fclose(file_handle.release()) == 0;
}

bool file::is_open() const noexcept { return file_handle.get() != nullptr; }

bool file::flush() const noexcept
{
    debug::ensure(file_handle);

    if (not file_handle)
        return false;

    return std::fflush(to_file()) == 0;
}

irt::vector<char> file::read_entire_file() noexcept
{
    debug::ensure(is_open());
    debug::ensure(mode[file_open_options::read] or
                  mode[file_open_options::extended]);

    vector<char> buffer;

    if (not is_open() or
        not(mode[file_open_options::read] or mode[file_open_options::extended]))
        return buffer;

    if (seek64(to_file(), 0, SEEK_END) != 0)
        return buffer;

    const auto size = tell64(to_file());
    if (size <= 0 or seek64(to_file(), 0, SEEK_SET) != 0)
        return buffer;

    if (buffer.resize(size)) {
        const auto read_size = std::fread(buffer.data(), 1,
                                          static_cast<size_t>(size), to_file());

        buffer.resize(read_size);
    }

    return buffer;
}

std::span<char> file::read_entire_file(std::span<char> buffer) noexcept
{
    debug::ensure(is_open());
    debug::ensure(mode[file_open_options::read] or
                  mode[file_open_options::extended]);

    if (buffer.empty() or not is_open() or
        not(mode[file_open_options::read] or mode[file_open_options::extended]))
        return std::span<char>();

    if (seek64(to_file(), 0, SEEK_END) != 0)
        return std::span<char>();

    const auto size = tell64(to_file());
    if (size < 0 or seek64(to_file(), 0, SEEK_SET) != 0)
        return std::span<char>();

    // One byte is reserved for the terminator.

    const auto real_size = std::min(static_cast<std::size_t>(size),
                                    buffer.size() - 1);

    const auto read_size = std::fread(buffer.data(), 1, real_size, to_file());

    buffer[read_size] = '\0';

    return buffer.subspan(0, read_size);
}

std::FILE* file::to_file() const noexcept { return file_handle.get(); }

file_mode file::get_mode() const noexcept { return mode; }

/* * * * * * * * * *
 *
 * is-portable-filenamme
 *
 * * * * * * * * * */

namespace details {

struct decoded_codepoint {
    char32_t    cp  = 0;
    std::size_t len = 0; // 0 means utf-8 sequence is invalid
};

// Decodes one utf-8 code point at position i. Rejects truncated
// sequences, overlong encodings, utf16 surrogates (U+D800..U+DFFF) and
// values beyond U+10FFFF.
static constexpr decoded_codepoint decode_utf8(std::string_view s,
                                               std::size_t      i) noexcept
{
    const auto b0 = static_cast<unsigned char>(s[i]);

    if (b0 < 0x80)
        return { b0, 1 };

    std::size_t len     = 0;
    char32_t    cp      = 0;
    char32_t    min_val = 0;

    if ((b0 & 0xE0) == 0xC0) {
        len     = 2;
        cp      = b0 & 0x1F;
        min_val = 0x80;
    } else if ((b0 & 0xF0) == 0xE0) {
        len     = 3;
        cp      = b0 & 0x0F;
        min_val = 0x800;
    } else if ((b0 & 0xF8) == 0xF0) {
        len     = 4;
        cp      = b0 & 0x07;
        min_val = 0x10000;
    } else {
        return {};
    }

    if (len > s.size() - i)
        return {};

    for (std::size_t k = 1; k < len; ++k) {
        const auto b = static_cast<unsigned char>(s[i + k]);
        if ((b & 0xC0) != 0x80)
            return {};
        cp = (cp << 6) | (b & 0x3F);
    }

    if (cp < min_val || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        return {};

    return { cp, len };
}

static constexpr char ascii_upper(char c) noexcept
{
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
}

static constexpr bool iequals_ascii(std::string_view a,
                                    std::string_view b) noexcept
{
    if (a.size() != b.size())
        return false;

    for (std::size_t i = 0, e = a.size(); i < e; ++i)
        if (ascii_upper(a[i]) != ascii_upper(b[i]))
            return false;

    return true;
}

// Device names reserved by Windows, case-insensitive, even when followed
// by an extension ("con.txt", "Nul.tar.gz", "COM1 .log").
static constexpr bool is_windows_reserved(std::string_view name) noexcept
{
    auto stem = name.substr(0, name.find('.'));
    while (!stem.empty() && stem.back() == ' ')
        stem.remove_suffix(1);

    constexpr std::string_view simple[] = { "CON", "PRN",    "AUX",
                                            "NUL", "CONIN$", "CONOUT$" };
    for (const auto r : simple)
        if (iequals_ascii(stem, r))
            return true;

    constexpr std::string_view numbered[] = { "COM", "LPT" };
    for (const auto p : numbered) {
        if (stem.size() < 4 || !iequals_ascii(stem.substr(0, 3), p))
            continue;

        const auto suffix = stem.substr(3);
        if (suffix.size() == 1 && suffix[0] >= '0' && suffix[0] <= '9')
            return true;

        // COM¹ COM² COM³ LPT¹ LPT² LPT³ (U+00B9, U+00B2, U+00B3)
        if (suffix == "\xC2\xB9" || suffix == "\xC2\xB2" ||
            suffix == "\xC2\xB3")
            return true;
    }

    return false;
}

// Returns true if @c name is a valid file or directory name on Windows,
// Linux and macOS simultaneously.
static constexpr bool is_portable_filename(std::string_view name) noexcept
{
    // 255 utf8 bytes: ext4/APFS limit. Since a code point never takes
    // more UTF-16 code units than utf8 bytes, this also guarantees the
    // NTFS/HFS+ limit of 255 UTF-16 code units.

    if (name.empty() || name.size() > 255)
        return false;

    // Windows silently strips a trailing dot or space.
    // Also covers "." and "..".

    if (name.back() == ' ' || name.back() == '.')
        return false;

    for (std::size_t i = 0; i < name.size();) {
        const auto [cp, len] = decode_utf8(name, i);
        if (len == 0)
            return false; // invalid utf8 (rejected by APFS)

        if (cp < 0x20 || cp == 0x7F)
            return false; // control characters (0x7F: out of caution)

        switch (cp) {
        case U'<':
        case U'>':
        case U':':
        case U'"':
        case U'/':
        case U'\\':
        case U'|':
        case U'?':
        case U'*':
            return false;
        default:
            break;
        }

        i += len;
    }

    return !is_windows_reserved(name);
}

static_assert(is_portable_filename("rapport.txt"));
static_assert(is_portable_filename("r\xC3\xA9sum\xC3\xA9.txt")); // résumé.txt
static_assert(is_portable_filename("\xF0\x9F\x98\x80"));         // emoji
static_assert(is_portable_filename("console.log"));
static_assert(is_portable_filename("COM10"));
static_assert(!is_portable_filename(""));
static_assert(!is_portable_filename("."));
static_assert(!is_portable_filename(".."));
static_assert(!is_portable_filename("fichier."));
static_assert(!is_portable_filename("fichier "));
static_assert(!is_portable_filename("a:b"));
static_assert(!is_portable_filename("a/b"));
static_assert(!is_portable_filename("a\\b"));
static_assert(!is_portable_filename("con"));
static_assert(!is_portable_filename("Nul.tar.gz"));
static_assert(!is_portable_filename("lpt9.txt"));
static_assert(!is_portable_filename("COM\xC2\xB9.txt")); // COM¹.txt
static_assert(!is_portable_filename("\xC0\xAF"));        // overlong encoding
static_assert(!is_portable_filename("\xED\xA0\x80"));    // surrogate
static_assert(!is_portable_filename("a\xC3"));           // truncated sequence

} // namespace details

bool is_portable_filename(std::string_view name) noexcept
{
    return details::is_portable_filename(name);
}

path& path::operator/=(std::string_view directory) noexcept
{
    if (!empty() && back() != '/')
        push_back('/');

    debug::ensure(can_append(directory));
    debug::ensure(is_portable_filename(directory));

    append(directory);

    return *this;
}

file_type path::has_extension() const noexcept
{
    const auto str = sv();

    auto best        = file_type::undefined_file;
    auto best_length = sz{ 0 };

    for (std::size_t i = 1; i < std::size(file_type_names); ++i) {
        const auto ext = file_type_names[i];
        if (ext.size() > best_length && str.ends_with(ext)) {
            best        = static_cast<file_type>(i);
            best_length = ext.size();
        }
    }

    return best;
}

void path::replace_extension(const file_type type) noexcept
{
    debug::ensure(type != file_type::undefined_file);

    if (const auto current = has_extension();
        current != file_type::undefined_file) {
        const auto old_ext = file_type_names[static_cast<std::size_t>(current)];
        resize(size() - old_ext.size());
    }

    const auto new_ext = file_type_names[static_cast<std::size_t>(type)];
    debug::ensure(std::cmp_less_equal(size() + new_ext.size(), capacity()));
    append(new_ext);
}

std::string_view path::filename() const noexcept
{
    const std::string_view str = sv();
    const auto             pos = str.find_last_of('/');

    return pos == std::string_view::npos ? str : str.substr(pos + 1);
}

std::string_view path::extension() const noexcept
{
    const auto fn  = filename();
    const auto pos = fn.find_last_of('.');
    return pos == std::string_view::npos ? std::string_view{} : fn.substr(pos);
}

path path::parent_directory() const noexcept
{
    const std::string_view str = sv();
    const auto             pos = str.find_last_of('/');

    path ret;
    if (pos != std::string_view::npos)
        ret.append(str.substr(0, pos));
    return ret;
}

std::filesystem::path path::to_std_path() const noexcept
{
    debug::ensure(not empty());

    return std::filesystem::path(
      reinterpret_cast<const char8_t*>(data()),
      reinterpret_cast<const char8_t*>(data() + size()));
}

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
