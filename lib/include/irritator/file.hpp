// Copyright (c) 2021 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_FILE_HPP
#define ORG_VLEPROJECT_IRRITATOR_FILE_HPP

#include <irritator/core.hpp>
#include <irritator/ext.hpp>

#include <cstdio>

namespace irt {

/** Determine the file access of the buffered_buffer.
 *
 * - read, Open a file for reading, read from start.
 * - write, Create a file for writing, destroy contents.
 * - append, Append to a file, write to end.
 */
enum class buffered_file_mode {
    text_or_binary = 0,      //!< Binary or text file mode.
    read           = 1 << 0, //!< Open a file for reading, read from start.
    write          = 1 << 1, //!< Create a file for writing, destroy contents.
    append         = 1 << 2, //!< Append to a file, write to end.
    Count
};

namespace details {

/** A wrapper to reduce the size of the buffered_file pointer. */
struct buffered_file_deleter {
    void operator()(std::FILE* fd) noexcept { std::fclose(fd); }
};

} // namespace details

using buffered_file =
  std::unique_ptr<std::FILE, details::buffered_file_deleter>;

/** Return a @c std::FILE pointer wrapped into a @c std::unique_ptr. This
 * function neither returns a nullptr.
 *
 * This function uses the buffered standard API @c std::fopen/std::fclose to
 * read/write in a text/binary format.
 *
 * This function uses a specific Win32 code to convert path in Win32 code
 * page.
 */
result<buffered_file> open_buffered_file(
  const std::filesystem::path&       path,
  const bitflags<buffered_file_mode> mode) noexcept;

enum class seek_origin { current, end, set };

enum class open_mode { read, write, append };

class file
{
public:
    struct memory_error {
        sz value = 0;
    };

    struct eof_error {};

    struct arg_error {};

    struct open_error {
        int value = 0;
    };

    using error_code = std::
      variant<std::monostate, memory_error, eof_error, arg_error, open_error>;

    /**
      @brief Try to open a file.

      @example
      auto file =  file::open(filename, file::mode::read,
                              [&](file::error_code ec) noexcept{
        if (const auto e = std::get_if<file::open_error>(&ec)) {
            // gui
            app.notifications.try_insert("fail to open file");

            // cli
            fprintf(stderr, "Fail to open %s", filename);
        });

      if (file) {
        int x, y, z;
        return file.read(x) && file.read(y) && file.read(z);
      }
      @endexample

      @param  filename File name in utf-8.
      @return @c file if success @c error_code otherwise.
     */
    template<std::invocable<file::error_code> Fn>
    static std::optional<file> open(const char*     filename,
                                    const open_mode mode,
                                    Fn&&            fn) noexcept;

    static std::optional<file> open(const char*     filename,
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
    //! @param  buffer A pointer to buffer (must be not null) with at least @c
    //!     length bytes available.
    //! @param  length The length of the buffer to read (must be greater than
    //!     0).
    //! @return false if failure, true otherwise.
    bool write(const void* buffer, i64 length) noexcept;

    void*     get_handle() const noexcept;
    open_mode get_mode() const noexcept;

private:
    file(void* handle, const open_mode mode) noexcept;

    void*     file_handle = nullptr;
    open_mode mode        = open_mode::read;
};

class memory
{
public:
    struct memory_error {
        sz value = 0;
    };
    struct eof_error {};
    struct arg_error {};

    using error_code =
      std::variant<std::monostate, memory_error, eof_error, arg_error>;

    ~memory() noexcept = default;

    template<std::invocable<memory::error_code> Fn>
    static std::optional<memory> make(const i64       length,
                                      const open_mode mode,
                                      Fn&&            fn) noexcept;

    static std::optional<memory> make(const i64       length,
                                      const open_mode mode) noexcept;

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
    //! @param  buffer A pointer to buffer (must be not null) with at least @c
    //!     length bytes available.
    //! @param  length The length of the buffer to read (must be greater than
    //!     0).
    //! @return false if failure, true otherwise.
    bool write(const void* buffer, i64 length) noexcept;

    vector<u8> data;
    i64        pos = 0;

private:
    memory(const i64 length, const open_mode mode) noexcept;
};

inline void*     file::get_handle() const noexcept { return file_handle; }
inline open_mode file::get_mode() const noexcept { return mode; }

template<std::invocable<file::error_code> Fn>
inline std::optional<file> file::open(const char*     filename,
                                      const open_mode mode,
                                      Fn&&            fn) noexcept
{
    if (not filename) {
        fn(arg_error{});
        return std::nullopt;
    }

    auto* ptr = fopen(filename,
                      mode == open_mode::read    ? "rb"
                      : mode == open_mode::write ? "wb"
                                                 : "ab");

    if (not ptr) {
        fn(open_error{ .value = errno });
        return std::nullopt;
    }

    return file(ptr, mode);
}

inline std::optional<file> file::open(const char*     filename,
                                      const open_mode mode) noexcept
{
    if (not filename)
        return std::nullopt;

    auto* ptr = fopen(filename,
                      mode == open_mode::read    ? "rb"
                      : mode == open_mode::write ? "wb"
                                                 : "ab");

    if (not ptr)
        return std::nullopt;

    return file(ptr, mode);
}

template<std::invocable<memory::error_code> Fn>
inline std::optional<memory> memory::make(const i64       length,
                                          const open_mode mode,
                                          Fn&&            fn) noexcept
{
    if (not(1 <= length and length <= INT32_MAX)) {
        fn(arg_error{});
        return std::nullopt;
    }

    memory mem(length, mode);
    if (not std::cmp_equal(mem.data.size(), length)) {
        fn(memory_error{ .value = static_cast<sz>(length) });
        return std::nullopt;
    }

    return mem;
}

inline std::optional<memory> memory::make(const i64       length,
                                          const open_mode mode) noexcept
{
    if (not(1 <= length and length <= INT32_MAX))
        return std::nullopt;

    memory mem(length, mode);
    if (not std::cmp_equal(mem.data.size(), length))
        return std::nullopt;

    return mem;
}

} // irt

#endif
