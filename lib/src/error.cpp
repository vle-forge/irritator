// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/error.hpp>

namespace irt {

void on_error_breakpoint(void) noexcept
{
#if !defined(NDEBUG) && defined(IRRITATOR_ENABLE_DEBUG)
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) &&         \
  __GNUC__ >= 2
    do {
        __asm__ __volatile__("int $03");
    } while (0);
#elif (defined(_MSC_VER) || defined(__DMC__)) && defined(_M_IX86)
    do {
        __asm int 3h
    } while (0);
#elif defined(_MSC_VER)
    do {
        __debugbreak();
    } while (0);
#elif defined(__alpha__) && !defined(__osf__) && defined(__GNUC__) &&          \
  __GNUC__ >= 2
    do {
        __asm__ __volatile__("bpt");
    } while (0);
#elif defined(__APPLE__)
    do {
        __builtin_trap();
    } while (0);
#else  /* !__i386__ && !__alpha__ */
    do {
        raise(SIGTRAP);
    } while (0);
#endif /* __i386__ */
#else
    do {
    } while (0);
#endif
}

} // namespace irt

