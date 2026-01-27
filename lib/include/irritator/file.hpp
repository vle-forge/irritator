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

enum class seek_origin : u8 { current, end, set };

class file
{
public:
    /**
       @brief Try to open a file.

       @example
       const auto char8_t* filename = u8"data.bin";
       auto file =  file::open(filename, file::mode::read);
       if (file) {
         int x, y, z;
         return file->read(x) && file->read(y) && file->read(z);
       }
       @endexample

       @param  filename File name in utf-8.
       @return @c file if success @c error_code otherwise.
     */
    static expected<file> open(const char8_t*  filename,
                               const file_mode mode) noexcept;

    /**
       @brief Try to open a file.

       @example
       auto path = std::filesystem::u8path(filename);
       auto file =  file::open(path, file::mode::read);
       if (file) {
         int x, y, z;
         return file->read(x) && file->read(y) && file->read(z);
       }
       @endexample

       @param  filename File name in utf-8.
       @return @c file if success @c error_code otherwise.
     */
    static expected<file> open(const std::filesystem::path& path,
                               const file_mode              mode) noexcept;

    /**
       Try to create a temporary @a file. This function neither returns a
       nullptr. If an error occured, the unexpected value stores an @a
       error_code.
       On Win32, the file is build in the root temporary directory.
     */
    static expected<file> open_tmp() noexcept;

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

    /** Read the entire file and returns a buffer with the read data.
     *  @return If the function fail, the @c vector<char> is empty. */
    irt::vector<char> read_entire_file() noexcept;

    /**
     *  Try to read the entire file from the beggining and fill the @c buffer.
     *
     *  @return Returns a @c span of the really read buffer. The returned buffer
     * can be:
     * - lower than @c buffer is the file length is lower the buffer.
     * - equal to @c buffer is the file length is greater or equal to the
     * buffer.
     */
    std::span<char> read_entire_file(std::span<char> buffer) noexcept;

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

    template<typename T>
        requires(std::is_arithmetic_v<T>)
    bool write(const std::span<T> buffer) noexcept
    {
        return write(buffer.data(), static_cast<i64>(buffer.size()));
    }

    bool write(const std::string_view buffer) noexcept
    {
        return write(buffer.data(), static_cast<i64>(buffer.size()));
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

    std::FILE* to_file() const noexcept;
    void*      get_handle() const noexcept;
    file_mode  get_mode() const noexcept;

private:
    file(void* handle, const file_mode mode) noexcept;

    void*     file_handle = nullptr;
    file_mode mode;
};

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
