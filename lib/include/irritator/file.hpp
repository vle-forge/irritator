// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_FILE_HPP
#define ORG_VLEPROJECT_IRRITATOR_FILE_HPP

#include <irritator/core.hpp>

namespace irt {

enum class seek_origin { current, end, set };

enum class open_mode { read, write, append };

class  file
{
public:
    enum class error_code {
        open_error,  // Fail to open file (@c e_file_name @c e_errno).
        read_error,  // Error during read file (@c e_file_name @c e_errno).
        write_error, // Error during write file (@c e_file_name @c e_errno).
        eof_error,   // End of file reach errro (@c e_file_name @c e_errno).
    };

    /**
     * @brief Try to open a file.
     * @example
     * attempt_all([]()
     * {
     *     auto file = file::open(file_path->sv(), file::mode::read);
     *     if (not file)
     *         return file.error();
     *
     *     ...
     *     return success();
     * },
     * [](file::error_code ec, const e_file_name *f, const e_errno *e) {
     *     std::cerr << ordinal(ec);
     *     if (f)
     *         std::cerr << f->value;
     *     if (e)
     *         std::cerr << e->value;
     * });
     * @param  filename File name in utf-8.
     * @return @c file if success @c error_code otherwise.
     */
    static result<file> open(const char*     filename,
                             const open_mode mode) noexcept;

    static bool exists(const char* filename) noexcept;

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

    status read(bool& value) noexcept;
    status read(u8& value) noexcept;
    status read(u16& value) noexcept;
    status read(u32& value) noexcept;
    status read(u64& value) noexcept;
    status read(i8& value) noexcept;
    status read(i16& value) noexcept;
    status read(i32& value) noexcept;
    status read(i64& value) noexcept;

    status read(float& value) noexcept;
    status read(double& value) noexcept;

    template<typename EnumType>
        requires(std::is_enum_v<EnumType>)
    status read(EnumType& value) noexcept
    {
        auto integer = ordinal(value);

        irt_check(read(integer));
        value = enum_cast<EnumType>(integer);

        return success();
    }

    status write(const bool value) noexcept;
    status write(const u8 value) noexcept;
    status write(const u16 value) noexcept;
    status write(const u32 value) noexcept;
    status write(const u64 value) noexcept;
    status write(const i8 value) noexcept;
    status write(const i16 value) noexcept;
    status write(const i32 value) noexcept;
    status write(const i64 value) noexcept;

    status write(const float value) noexcept;
    status write(const double value) noexcept;

    template<typename EnumType>
        requires(std::is_enum_v<EnumType>)
    status write(const EnumType value) noexcept
    {
        return write(ordinal(value));
    }

    //! Low level read function.
    //! @param buffer A pointer to buffer (must be not null)
    //! @param length The length of the buffer to read (must be greater than
    //!     0).
    //! @return false if failure, true otherwise.
    status read(void* buffer, i64 length) noexcept;

    //! Low level write function.
    //! @param  buffer A pointer to buffer (must be not null) with at least @c
    //!     length bytes available.
    //! @param  length The length of the buffer to read (must be greater than
    //!     0).
    //! @return false if failure, true otherwise.
    status write(const void* buffer, i64 length) noexcept;

    void*     get_handle() const noexcept;
    open_mode get_mode() const noexcept;

private:
    file(void* handle, const open_mode mode) noexcept;

    void*     file_handle = nullptr;
    open_mode mode        = open_mode::read;
};

class  memory
{
public:
    enum class error_code {
        open_error,  // Fail to open file (@c e_file_name @c e_errno).
        read_error,  // Error during read file (@c e_file_name @c e_errno).
        write_error, // Error during write file (@c e_file_name @c e_errno).
        eof_error,   // End of file reach errro (@c e_file_name @c e_errno).
    };

    memory(const i64 length, const open_mode mode) noexcept;
    ~memory() noexcept = default;

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

    status read(bool& value) noexcept;
    status read(u8& value) noexcept;
    status read(u16& value) noexcept;
    status read(u32& value) noexcept;
    status read(u64& value) noexcept;
    status read(i8& value) noexcept;
    status read(i16& value) noexcept;
    status read(i32& value) noexcept;
    status read(i64& value) noexcept;

    status read(float& value) noexcept;
    status read(double& value) noexcept;

    template<typename EnumType>
        requires(std::is_enum_v<EnumType>)
    status read(EnumType& value) noexcept
    {
        auto integer = ordinal(value);
        irt_check(read(integer));
        value = enum_cast<EnumType>(integer);
        return success();
    }

    status write(const bool value) noexcept;
    status write(const u8 value) noexcept;
    status write(const u16 value) noexcept;
    status write(const u32 value) noexcept;
    status write(const u64 value) noexcept;
    status write(const i8 value) noexcept;
    status write(const i16 value) noexcept;
    status write(const i32 value) noexcept;
    status write(const i64 value) noexcept;

    status write(const float value) noexcept;
    status write(const double value) noexcept;

    template<typename EnumType>
        requires(std::is_enum_v<EnumType>)
    status write(const EnumType value) noexcept
    {
        return write(ordinal(value));
    }

    //! Low level read function.
    //! @param buffer A pointer to buffer (must be not null)
    //! @param length The length of the buffer to read (must be greater than
    //!     0).
    //! @return false if failure, true otherwise.
    status read(void* buffer, i64 length) noexcept;

    //! Low level write function.
    //! @param  buffer A pointer to buffer (must be not null) with at least @c
    //!     length bytes available.
    //! @param  length The length of the buffer to read (must be greater than
    //!     0).
    //! @return false if failure, true otherwise.
    status write(const void* buffer, i64 length) noexcept;

    vector<u8> data;
    i64        pos = 0;
};

inline void*     file::get_handle() const noexcept { return file_handle; }
inline open_mode file::get_mode() const noexcept { return mode; }

} // irt

#endif
