// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_FILE_HPP
#define ORG_VLEPROJECT_IRRITATOR_FILE_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <optional>
#include <span>
#include <utility>

#include <cstdio>

namespace irt {

enum class file_open_options : u8 {
    read,     ///< Opens a file for reading only.
    write,    ///< Creates or opens a file for writing only.
    append,   ///< Moves the file pointer to the end of the file before every
              ///< write operation.
    extended, ///< @c read @c write or @c append are for both reading and
              ///< writing
    text,     ///< Opens the file in text mode (binary default).
    fail_if_exist ///< Fails the @c write or @c operation if the file already
                  ///< exists.
};

using file_mode = bitflags<file_open_options>;

/// Owning handle on a @c std::FILE* (RAII).
///
/// The class opens files from an utf-8 @c path (on Windows too), applies the
/// @c file_open_options and closes the file. The input and output
/// operations are done by the caller on the handle returned by @c to_file()
/// (rapidjson @c FileReadStream / @c FileWriteStream, @c fprintf, @c fputs,
/// @c fwrite...). The class is move-only.
///
/// @code
/// auto f = file::open(path("data.json"), file_mode(file_open_options::read));
/// if (f) {
///     char buffer[4096];
///     rapidjson::FileReadStream is(f->to_file(), buffer, sizeof(buffer));
///     ...
/// }
/// @endcode
class file
{
public:
    /// Try to open a file.
    ///
    /// @param filename File name in utf-8.
    /// @return @c file if success @c error_code otherwise.
    static expected<file> open(const path&     filename,
                               const file_mode mode) noexcept;

    /// Try to open a file.
    ///
    /// @param fillename The file name in utf-8.
    /// @return @c file if success or std::nullopt_t otherwise.
    static std::optional<file> try_open(const path&     filename,
                                        const file_mode mode) noexcept;

    /// Try to create a temporary @a file opened in binary mode for reading and
    /// writing (@c "w+b"). The file is removed when it is closed. This function
    /// neither returns a nullptr. If an error occured, the unexpected value
    /// stores an @a error_code.
    ///
    /// On Win32, the file is build in the root temporary directory.
    ///
    /// @return @c file if success @c error_code otherwise.
    static expected<file> open_tmp() noexcept;

    file() noexcept  = default;
    ~file() noexcept = default;

    file(const file&)            = delete;
    file& operator=(const file&) = delete;

    file(file&& other) noexcept;
    file& operator=(file&& other) noexcept;

    /// Close the file. Idempotent, @c to_file() returns @c nullptr after the
    /// call even on error.
    ///
    /// The destructor closes the file but can not report an error: call this
    /// function and check the result when the data must be written (the
    /// buffered data are written by @c fclose and a full disk is only
    /// reported there).
    ///
    /// @return false if the @c fclose failed.
    bool close() noexcept;

    bool is_open() const noexcept;

    /// Flush the buffered output data.
    ///
    /// @return false if the file is closed or if the flush failed.
    bool flush() const noexcept;

    /// Read the entire file from the beginning (whatever the current
    /// position) and returns a buffer with the read data. The position is
    /// unspecified after the call.
    ///
    /// @return If the function fail or if the file is empty, the @c
    /// vector<char> is empty. */
    irt::vector<char> read_entire_file() noexcept;

    /// Try to read the entire file from the beggining and fill the @c buffer.
    /// The result is always zero terminated, so at most @c buffer.size() - 1
    /// characters are read.
    ///
    /// @return Returns a @c span of the really read buffer (without the
    /// terminator). The returned buffer can be:
    /// - lower than @c buffer.size() - 1 if the file length is lower.
    /// - equal to @c buffer.size() - 1 if the file length is greater or equal.
    /// - empty if @c buffer is empty or on error.
    std::span<char> read_entire_file(std::span<char> buffer) noexcept;

    /// Get access to the underlying std::FILE handler (can be nullptr).
    std::FILE* to_file() const noexcept;

    /// Get the mode
    file_mode get_mode() const noexcept;

private:
    file(std_file&& f, file_mode m) noexcept
      : file_handle(std::move(f))
      , mode(m)
    {}

    std_file  file_handle;
    file_mode mode{};
};

enum class seek_origin : u8 { current, end, set };

class memory
{
public:
    ~memory() noexcept = default;

    static expected<memory> make(const i64 length) noexcept;

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
    memory(const i64 length) noexcept;
};

} // irt

#endif
