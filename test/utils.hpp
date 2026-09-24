// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef IRRITATOR_TEST_UTILS_HPP
#define IRRITATOR_TEST_UTILS_HPP

#include <irritator/core.hpp>

#include <filesystem>
#include <random>
#include <string_view>

#include <fmt/format.h>

namespace irt {

/**
 * @class temp_path_with_unlink
 * @brief Manages a temporary path with automatic cleanup on destruction.
 *
 * This class creates a **unique temporary directory** in the system's temporary
 * directory (e.g., `/tmp` on Linux, `%TEMP%` on Windows) and **automatically
 * deletes** it when the object is destroyed. Useful for unit tests requiring an
 * isolated and clean environment.
 *
 * @note
 * - The directory name is **unique per instance** to prevent conflicts between
 * parallel tests.
 * - The directory is **automatically created** during construction.
 * - Deletion is **silent** (errors are logged to `stderr`).
 *
 * @warning
 * - If the system's temporary directory is inaccessible, the object will be in
 * an invalid state
 *   (`success() == false`).
 * - The `c_str()` method returns a pointer that is **only valid for the
 * lifetime of this object**.
 *
 * @see std::filesystem::temp_directory_path,
 * std::filesystem::create_directories, std::filesystem::remove_all
 */
class temp_path_with_unlink
{
public:
    /**
     * @brief Constructs an object and creates a unique temporary directory.
     *
     * @details
     * 1. Generates a unique directory name (e.g., `rd-1a2b3c4d`) in
     * `std::filesystem::temp_directory_path()`.
     * 2. Cleans up any existing directory with the same name.
     * 3. Creates the directory.
     *
     * @param prefix A prefix for the unique temporary directory name.
     *
     * @throws No exceptions are propagated (all are caught and handled
     * internally). On error, the object will be in an invalid state (`success()
     * == false`).
     */
    temp_path_with_unlink(const std::string_view prefix) noexcept
    {
        using namespace std::string_view_literals;

        static constexpr std::string_view def = "def-"sv;

        const auto valid       = is_valid(prefix);
        const auto real_prefix = valid ? prefix : def;

        std::error_code ec;
        try {
            p = std::filesystem::temp_directory_path(ec);
            irt::fatal::ensure(not ec);
            p /= generate_dir_name(real_prefix);
            try_do_remove(p);
            irt::fatal::ensure(std::filesystem::create_directories(p, ec));
            const auto u8 = p.u8string();
            b = { reinterpret_cast<const char*>(u8.c_str()), u8.size() };
        } catch (...) {
            p.clear();
            b.clear();
        }
    }

    /**
     * @brief Destructor: removes the temporary directory and its contents.
     *
     * @note
     * - Deletion is **recursive** (all files and subdirectories are removed).
     * - Deletion errors are **logged to `stderr`** but do not interrupt
     * execution.
     */
    ~temp_path_with_unlink() noexcept { do_remove(p); }

    /**
     * @brief Returns the temporary directory path as a C-style string.
     *
     * @return Pointer to a **null-terminated** UTF-8 C-style string.
     *         The pointer remains valid **as long as this object exists**.
     *
     * @warning
     * - **Do not store** this pointer beyond the lifetime of this object.
     * - The content may be **modified by other processes** (no protection).
     */
    const char* c_str() const noexcept { return b.c_str(); }

    /**
     * @brief Returns the temporary directory path as a string-view.
     *
     * @return The string-view remains valid **as long as this object exists**.
     *
     * @warning
     * - **Do not store** this object beyond the lifetime of this object.
     * - The content may be **modified by other processes** (no protection).
     */
    const std::string_view sv() const noexcept { return { b }; }

    /**
     * @brief Checks if the temporary directory was created successfully.
     *
     * @return `true` if the directory exists **and** is accessible, `false`
     * otherwise.
     *
     * @note
     * This method **actually checks** the existence of the directory.
     */
    bool success() const noexcept
    {
        if (p.empty())
            return false;

        std::error_code ec;
        return std::filesystem::exists(p, ec) &&
               std::filesystem::is_directory(p, ec);
    }

private:
    static std::string generate_dir_name(const std::string_view prefix) noexcept
    {
        static constexpr std::size_t           generated_chars = 8;
        static std::random_device              rd;
        static std::minstd_rand                gen(rd());
        static std::uniform_int_distribution<> dis(0, 15);
        std::string                            name;

        name.reserve(prefix.size() + generated_chars);
        name.assign(prefix);

        for (int i = 0; i < 8; ++i)
            name += "0123456789abcdef"[dis(gen)];

        return name;
    }

    static void try_do_remove(const std::filesystem::path& p) noexcept
    {
        try {
            std::error_code ec;
            (void)std::filesystem::remove_all(p, ec);
        } catch (const std::exception& e) {
            fmt::println(stderr, "Warning: Exception while removing {}: {}",
                         p.string(), e.what());
        } catch (...) {
            fmt::println(stderr, "Warning: Unknown error while removing {}",
                         p.string());
        }
    }

    static void do_remove(const std::filesystem::path& p) noexcept
    {
        try {
            std::error_code ec;
            std::uintmax_t  nb = std::filesystem::remove_all(p, ec);
            if (nb == 0 or nb == static_cast<std::uintmax_t>(-1)) {
                fmt::println(stderr, "Warning: Failed to remove {}: {}",
                             p.string(), ec.message());
            }
        } catch (const std::exception& e) {
            fmt::println(stderr, "Warning: Exception while removing {}: {}",
                         p.string(), e.what());
        } catch (...) {
            fmt::println(stderr, "Warning: Unknown error while removing {}",
                         p.string());
        }
    }

    static bool is_valid(const std::string_view str) noexcept
    {
        if (str.empty())
            return false;

        for (const auto& c : str)
            if (not std::isalnum(c))
                return false;

        return true;
    }

    std::filesystem::path p;
    std::string           b;
};

} // namespace irt

#endif