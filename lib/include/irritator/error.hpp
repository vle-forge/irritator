// Copyright (c) 2024 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_2023_ERROR_HPP
#define ORG_VLEPROJECT_IRRITATOR_2023_ERROR_HPP

#define BOOST_LEAF_EMBEDDED
#include <boost/leaf.hpp>

namespace boost {

inline void throw_exception(std::exception const& e)
{
    std::puts(e.what());
    std::puts("\n");
}

} // namespace boost

#define irt_check BOOST_LEAF_CHECK

namespace irt {

template<typename T, T... value>
using match2 = boost::leaf::match<T, value...>;

template<class T>
using result = boost::leaf::result<T>;

using status2 = result<void>;

using error_handler = void(void);

inline error_handler* on_error_callback = nullptr;

/**
 * @brief a readability function for returning successful results;
 *
 * For functions that return `status`, rather than returning `{}` to default
 * initialize the status object as "success", use this function to make it more
 * clear to the reader.
 *
 * EXAMPLE:
 *
 *     irt::status some_function() {
 *        return irt::success();
 *     }
 *
 * @return status - that is always successful
 */
inline status2 success()
{
    // Default initialize the status object using the brace initialization,
    // which will set the status to the default "success" state.
    status2 successful_status{};
    return successful_status;
}

template<class TryBlock, class... H>
[[nodiscard]] constexpr auto attempt(TryBlock&& p_try_block, H&&... p_handlers)
{
    return boost::leaf::try_handle_some(p_try_block, p_handlers...);
}

template<class TryBlock, class... H>
[[nodiscard]] constexpr auto attempt_all(TryBlock&& p_try_block,
                                         H&&... p_handlers)
{
    return boost::leaf::try_handle_all(p_try_block, p_handlers...);
}

template<class... Item>
[[nodiscard]] inline auto new_error(Item&&... p_item)
{
    if (on_error_callback) {
        on_error_callback();
    }

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

    return boost::leaf::new_error(std::forward<Item>(p_item)...);
}

} // namespace irt

#endif