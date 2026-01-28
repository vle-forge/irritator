// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_FILE_HPP
#define ORG_VLEPROJECT_IRRITATOR_FILE_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <filesystem>

#include <cstdio>

namespace irt {

enum class file_options : u8 {
    read_only,  ///< Opens a file for reading only.
    write_only, ///< Opens a file for writing only.
    read_write, ///< Opens a file for both reading and writing
    append,     ///< Moves the file pointer to the end of the file before every
                ///< write operation.
    create,     ///< Creates a file and opens it for writing
    trunc,      ///< Opens a file and truncates it to zero length
    binary      ///< Opens the file in binary mode.
};

constexpr struct r_tag_t {
    r_tag
};
;

using file_flags = bitflags<file_options>;

namespace details {

/// A wrapper to reduce the size of the buffered_file pointer.
struct buffered_file_deleter {
    void operator()(std::FILE* fd) const noexcept { std::fclose(fd); }
};

} // namespace details

/// @brief A typedef to manage automatically an @a std::FILE pointer.
using buffered_file =
  std::unique_ptr<std::FILE, details::buffered_file_deleter>;

/// Return a @a std::FILE pointer wrapped into a @a std::unique_ptr. This
/// function neither returns a nullptr. If an error occured, the unexpected
/// value stores an @a error_code.
///
/// This function uses the buffered standard API @c std::fopen/std::fclose to
/// read/write in a text/binary format.
///
/// This function uses a specific Win32 code to convert path in Win32 code
/// page @a _wfopen_s.
expected<buffered_file> open_buffered_file(
  const std::filesystem::path& path,
  const bitflags<file_options> mode) noexcept;

/// Return a @a std::FILE pointer wrapped into a @a std::unique_ptr. This
/// function neither returns a nullptr. If an error occured, the unexpected
/// value stores an @a error_code.
///
/// This function returns a temporary file automatically removes when file is
/// close.
expected<buffered_file> open_tmp_file() noexcept;

/// Return the entire content of the file pointed by @a f.
irt::vector<char> read_entire_file(std::FILE* f) noexcept;

enum class seek_origin : u8 { current, end, set };

class file
{
public:
    /// @brief Try to open a file.
    ///
    /// @example
    /// const auto char8_t* filename = u8"data.bin";
    /// auto file =  file::open(filename, file::mode::read);
    /// if (file) {
    ///   int x, y, z;
    ///   return file->read(x) && file->read(y) && file->read(z);
    /// }
    /// @endexample
    ///
    /// @param  filename File name in utf-8.
    /// @return @c file if success @c error_code otherwise.
    static expected<file> open(const char8_t*               filename,
                               const bitflags<file_options> mode) noexcept;

    /// @brief Try to open a file.
    ///
    /// @example
    /// auto path = std::filesystem::u8path(filename);
    /// auto file =  file::open(path, file::mode::read);
    /// if (file) {
    ///   int x, y, z;
    ///   return file->read(x) && file->read(y) && file->read(z);
    /// }
    /// @endexample
    ///
    /// @param  filename File name in utf-8.
    /// @return @c file if success @c error_code otherwise.
    static expected<file> open(const std::filesystem::path& path,
                               const bitflags<file_options> mode) noexcept;

    static bool exists(const char8_t* filename) noexcept;

    file() noexcept = default;
    ~file() noexcept;

    file(const file& other) noexcept            = delete;
    file& operator=(const file& other) noexcept = delete;
    file(file&& other) noexcept;
    file& operator=(file&& other) noexcept;

    void close() noexcept;
    bool is_open() const noexcept;
    bool is_eof() const noexcept;

    i64  length() const noexcept;
    i64  tell() const noexcept;
    void flush() const noexcept;
    i64  seek(i64 offset, seek_origin origin) noexcept;
    void rewind() noexcept;

    bool read(bool& value) noexcept;
    bool read(u8& value) noexcept;
    bool read(u16& value) noexcept;
    bool read(u32& value) noexcept;
    bool read(u64& value) noexcept;
    bool read(i8& value) noexcept;
    bool read(i16& value) noexcept;
    bool read(i32& value) noexcept;
    bool read(i64& value) noexcept;

    bool read(float& value) noexcept;
    bool read(double& value) noexcept;

    template<typename EnumType>
        requires(std::is_enum_v<EnumType>)
    bool read(EnumType& value) noexcept
    {
        auto integer = ordinal(value);

        irt_check(read(integer));
        value = enum_cast<EnumType>(integer);

        return true;
    }

    bool write(const bool value) noexcept;
    bool write(const u8 value) noexcept;
    bool write(const u16 value) noexcept;
    bool write(const u32 value) noexcept;
    bool write(const u64 value) noexcept;
    bool write(const i8 value) noexcept;
    bool write(const i16 value) noexcept;
    bool write(const i32 value) noexcept;
    bool write(const i64 value) noexcept;

    bool write(const float value) noexcept;
    bool write(const double value) noexcept;

    template<typename EnumType>
        requires(std::is_enum_v<EnumType>)
    bool write(const EnumType value) noexcept
    {
        return write(ordinal(value));
    }

    //! Low level read function.
    //! @param buffer A pointer to buffer (must be not null)
    //! @param length The length of the buffer to read (must be greater than
    //!     0).
    //! @return false if failure, true otherwise.
    bool read(void* buffer, i64 length) noexcept;

    //! Low level write function.
    //! @param  buffer A pointer to buffer (must be not null) with at least
    //! @c
    //!     length bytes available.
    //! @param  length The length of the buffer to read (must be greater
    //! than
    //!     0).
    //! @return false if failure, true otherwise.
    bool write(const void* buffer, i64 length) noexcept;

    void*                  get_handle() const noexcept;
    bitflags<file_options> get_mode() const noexcept;

private:
    file(void* handle, const bitflags<file_options> mode) noexcept;

    void*                  file_handle = nullptr;
    bitflags<file_options> mode;
};

class memory
{
public:
    ~memory() noexcept = default;

    static expected<memory> make(const i64                    length,
                                 const bitflags<file_options> mode) noexcept;

    memory(const memory& other) noexcept            = delete;
    memory& operator=(const memory& other) noexcept = delete;
    memory(memory&& other) noexcept;
    memory& operator=(memory&& other) noexcept;

    bool is_open() const noexcept;
    bool is_eof() const noexcept;

    i64  length() const noexcept;
    i64  tell() const noexcept;
    void flush() const noexcept;
    i64  seek(i64 offset, seek_origin origin) noexcept;
    void rewind() noexcept;

    bool read(bool& value) noexcept;
    bool read(u8& value) noexcept;
    bool read(u16& value) noexcept;
    bool read(u32& value) noexcept;
    bool read(u64& value) noexcept;
    bool read(i8& value) noexcept;
    bool read(i16& value) noexcept;
    bool read(i32& value) noexcept;
    bool read(i64& value) noexcept;

    bool read(float& value) noexcept;
    bool read(double& value) noexcept;

    template<typename EnumType>
        requires(std::is_enum_v<EnumType>)
    bool read(EnumType& value) noexcept
    {
        auto integer = ordinal(value);
        irt_check(read(integer));
        value = enum_cast<EnumType>(integer);
        return true;
    }

    bool write(const bool value) noexcept;
    bool write(const u8 value) noexcept;
    bool write(const u16 value) noexcept;
    bool write(const u32 value) noexcept;
    bool write(const u64 value) noexcept;
    bool write(const i8 value) noexcept;
    bool write(const i16 value) noexcept;
    bool write(const i32 value) noexcept;
    bool write(const i64 value) noexcept;

    bool write(const float value) noexcept;
    bool write(const double value) noexcept;

    template<typename EnumType>
        requires(std::is_enum_v<EnumType>)
    bool write(const EnumType value) noexcept
    {
        return write(ordinal(value));
    }

    //! Low level read function.
    //! @param buffer A pointer to buffer (must be not null)
    //! @param length The length of the buffer to read (must be greater than
    //!     0).
    //! @return false if failure, true otherwise.
    bool read(void* buffer, i64 length) noexcept;

    //! Low level write function.
    //! @param  buffer A pointer to buffer (must be not null) with at least
    //! @c
    //!     length bytes available.
    //! @param  length The length of the buffer to read (must be greater
    //! than
    //!     0).
    //! @return false if failure, true otherwise.
    bool write(const void* buffer, i64 length) noexcept;

    vector<u8> data;
    i64        pos = 0;

private:
    memory(const i64 length, const bitflags<file_options> mode) noexcept;
};

inline void* file::get_handle() const noexcept { return file_handle; }
inline bitflags<file_options> file::get_mode() const noexcept { return mode; }

} // irt

#endif
