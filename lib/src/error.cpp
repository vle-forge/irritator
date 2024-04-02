// Copyright (c) 2023 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/error.hpp>

namespace irt {

namespace debug {

void breakpoint() noexcept
{
#if ((defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__) &&        \
     __GNUC__ >= 2)
    __asm__ __volatile__("int $03");
#elif ((defined(_MSC_VER) || defined(__DMC__)) && defined(_M_IX86))
    __asm int 3h
#elif defined(_MSC_VER)
    __debugbreak();
#elif (defined(__alpha__) && !defined(__osf__) && defined(__GNUC__) &&         \
       __GNUC__ >= 2)
    __asm__ __volatile__("bpt");
#elif defined(__APPLE__)
    __builtin_trap();
#else  /* !__i386__ && !__alpha__ */
    raise(SIGTRAP);
#endif /* __i386__ */
}

} // namespace debug
} // namespace irt
