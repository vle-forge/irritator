// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_MACROS_2024
#define ORG_VLEPROJECT_IRRITATOR_MACROS_2024

#include <cassert>
#include <cstdlib>

#ifndef irt_assert
#define irt_assert(_expr) assert(_expr)
#endif

#if defined(_MSC_VER) && !defined(__clang__)
#define irt_force_inline_attribute [[msvc::forceinline]]
#else
#define irt_force_inline_attribute [[gnu::always_inline]]
#endif

namespace irt {

#ifndef IRRITATOR_REAL_TYPE_F32
using real = double;
#else
using real = float;
#endif

namespace debug {

#ifdef IRRITATOR_ENABLE_DEBUG
static constexpr bool enable_ensure = true;
#else
static constexpr bool enable_ensure = false;
#endif

//! @brief A c++ function to replace assert macro controlled via constexpr
//! boolean variable.
//!
//! Call @c quick_exit if the assertion fail. This function is disabled if
//! the boolean @c variables::enable_ensure_simulation is false.
//!
//! @tparam T The type of the assertion to test.
//! @param assertion The instance of the assertion to test.
template<typename T>
    requires(::irt::debug::enable_ensure == true)
inline constexpr void ensure(T&& assertion) noexcept
{
    if (!static_cast<bool>(assertion))
        std::abort();
}

//! @brief A c++ function to replace assert macro controlled via constexpr
//! boolean variable.
//!
//! This function is disabled if the boolean @c
//! variables::enable_ensure_simulation is false.
//!
//! @tparam T The type of the assertion to test.
//! @param assertion The instance of the assertion to test.
template<typename T>
    requires(::irt::debug::enable_ensure == false)
irt_force_inline_attribute constexpr void ensure(
  [[maybe_unused]] T&& assertion) noexcept
{}

//! Add a breakpoint (@c __debugbreak(), @c __builtin_trap(), into the code if
//! and only if @c NDEBUG is not defined and @c IRRITATOR_ENABLE_DEBUG is
//! defined. This function can be use with the @c on_error_callback to stop the
//! application when a @c new_error function is called.
void breakpoint() noexcept;

} // namespace debug

[[noreturn]] inline void unreachable() noexcept
{
    // Uses compiler specific extensions if possible.
    // Even if no extension is used, undefined behavior is still raised by
    // an empty function body and the noreturn attribute.
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
    __assume(false);
#else // GCC, Clang
    __builtin_unreachable();
#endif
}

} // namespace irt

#endif
