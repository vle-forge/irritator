// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_RANDOM_HPP
#define ORG_VLEPROJECT_IRRITATOR_RANDOM_HPP

#include <irritator/macros.hpp>

#include <array>
#include <limits>
#include <span>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace irt {

namespace details {

/// A 128-bit multiplication (high and low parts)
inline void mulhilo(u64 a, u64 b, u64& lo, u64& hi) noexcept
{
#if defined(__SIZEOF_INT128__) && !defined(__wasm__)
    __uint128_t product =
      static_cast<__uint128_t>(a) * static_cast<__uint128_t>(b);
    lo = static_cast<u64>(product);
    hi = static_cast<u64>(product >> 64);
#elif defined(_MSC_VER) && defined(_WIN64)
    lo = _umul128(a, b, &hi);
#else
    /* First calculate all of the cross products. */
    u64 lo_lo = (lhs & 0xffffffff) * (rhs & 0xffffffff);
    u64 hi_lo = (lhs >> 32) * (rhs & 0xffffffff);
    u64 lo_hi = (lhs & 0xffffffff) * (rhs >> 32);
    u64 hi_hi = (lhs >> 32) * (rhs >> 32);

    /* Now add the products together. These will never overflow. */
    u64 cross = (lo_lo >> 32) + (hi_lo & 0xffffffff) + lo_hi;
    u64 upper = (hi_lo >> 32) + (cross >> 32) + hi_hi;

    hi = upper;
    lo = (cross << 32) | (lo_lo & 0xffffffff);
#endif
}

} // namespace details

/**
 * A Pseudo Random Generator based on Salmon et al. (Random123).
 *
 * @c philox_64 uses a @c irt::u64 buffer size of lenght 2. The interface of @c
 * philox_64 in fully compatible with the standard random header.
 */
class philox_64
{
public:
    using result_type = u64;

    static constexpr u64 PHILOX_M0 = 0xD2B74407B1CADAC9ULL;
    static constexpr u64 PHILOX_W0 = 0x9E3779B97F4A7C15ULL;
    static constexpr int ROUNDS    = 10;

    /**
     * @brief Ctor
     * @param seed Global simulation @c seed.
     * @param index Unique identifiant for model
     * @param step First step
     */
    constexpr philox_64(const u64 seed,
                        const u64 index,
                        const u64 step = 0) noexcept
      : m_key{ seed }
      , m_counter({ index, step })
      , m_buffer_pos(2) // To force buffer computation
    {}

    /** Build the next random number. Defined by the STL requirements for random
     * number generators. */
    constexpr result_type operator()() noexcept
    {
        if (m_buffer_pos >= 2)
            refill_buffer();

        return m_buffer[m_buffer_pos++];
    }

    /** Discard @c z random numbers. Useful to jump ahead in the random
     * sequence. */
    constexpr void discard(const u64 z) noexcept
    {
        m_counter[1] += z;
        m_buffer_pos = 2; // Invalid the buffer.
    }

    /** A state setter to give full random access for example to replay
     * simulation for example @c index is the @c model_id and @c step is the
     * number of the random.  */
    constexpr void set_state(const u64 index, const u64 step) noexcept
    {
        m_counter[0] = index;
        m_counter[1] = step;
        m_buffer_pos = 2; // Invalid the buffer.
    }

    /** Give the minimum integer of the random generator. Defined by the STL
     * requirements for random number generators. */
    static constexpr result_type min() noexcept
    {
        return std::numeric_limits<result_type>::min();
    }

    /** Give the maximun integer of the random generator. Defined by the STL
     * requirements for random number generators. */
    static constexpr result_type max() noexcept
    {
        return std::numeric_limits<result_type>::max();
    }

private:
    u64 m_key; /**< Global seed. */
    std::array<u64, 2>
      m_counter; /**< m_counter[0] the model_id and m_counter[1] the step. */
    std::array<u64, 2> m_buffer; /**< Buffer to store result and next result. */
    int                m_buffer_pos;

    void refill_buffer() noexcept
    {
        u64 ctr0 = m_counter[0];
        u64 ctr1 = m_counter[1];
        u64 key0 = m_key;

        for (int i = 0; i < ROUNDS; ++i) {
            u64 hi, lo;
            details::mulhilo(PHILOX_M0, ctr0, lo, hi);

            ctr0 = hi ^ key0 ^ ctr1;
            ctr1 = lo;

            key0 += PHILOX_W0;
        }

        m_buffer[0] = ctr0;
        m_buffer[1] = ctr1;

        // Finaly increase the counter and reset the buffer position.
        m_counter[1]++;
        m_buffer_pos = 0;
    }
};

/**
 * A Pseudo Random Generator based on Salmon et al. (Random123).
 *
 * @c philox_64_view uses a @c irt::u64 buffer size of lenght 2. The interface
 * of @c philox_64 in fully compatible with the standard random header.
 *
 * @c philox_64_view does not store any state. State must be provide in ctor.
 */
class philox_64_view
{
public:
    using result_type = u64;

    static constexpr u64 PHILOX_M0 = 0xD2B74407B1CADAC9ULL;
    static constexpr u64 PHILOX_W0 = 0x9E3779B97F4A7C15ULL;
    static constexpr int ROUNDS    = 10;

    /**
     * @brief Ctor
     * @param seed Global simulation @c seed.
     * @param index Unique identifiant for model
     * @param step First step
     */
    constexpr philox_64_view(std::span<u64, 6> state) noexcept
      : m_state(state)
    {
        m_state[pos] = 2; // Invalid the buffer.
    }

    /** Build the next random number. Defined by the STL requirements for random
     * number generators. */
    constexpr result_type operator()() noexcept
    {
        if (m_state[pos] >= 2u)
            refill_buffer();

        return m_state[buf_0 + m_state[pos]++];
    }

    /** Discard @c z random numbers. Useful to jump ahead in the random
     * sequence. */
    constexpr void discard(const u64 z) noexcept
    {
        m_state[ctr_1] += z;
        m_state[pos] = 2; // Invalid the buffer.
    }

    /** A state setter to give full random access for example to replay
     * simulation for example @c index is the @c model_id and @c step is the
     * number of the random.  */
    constexpr void set_state(const u64 index, const u64 step) noexcept
    {
        m_state[ctr_0] = index;
        m_state[ctr_1] = step;
        m_state[pos]   = 2; // Invalid the buffer.
    }

    /** Give the minimum integer of the random generator. Defined by the STL
     * requirements for random number generators. */
    static constexpr result_type min() noexcept
    {
        return std::numeric_limits<result_type>::min();
    }

    /** Give the maximun integer of the random generator. Defined by the STL
     * requirements for random number generators. */
    static constexpr result_type max() noexcept
    {
        return std::numeric_limits<result_type>::max();
    }

private:
    enum state_access : u8 { key = 0, ctr_0, ctr_1, pos, buf_0, buf_1 };

    std::span<u64, 6> m_state;

    void refill_buffer() noexcept
    {
        u64 ctr0 = m_state[ctr_0];
        u64 ctr1 = m_state[ctr_1];
        u64 key0 = m_state[key];

        for (int i = 0; i < ROUNDS; ++i) {
            u64 hi, lo;
            details::mulhilo(PHILOX_M0, ctr0, lo, hi);

            ctr0 = hi ^ key0 ^ ctr1;
            ctr1 = lo;

            key0 += PHILOX_W0;
        }

        m_state[buf_0] = ctr0;
        m_state[buf_1] = ctr1;

        // Finaly increase the counter and reset the buffer position.
        m_state[ctr_1]++;
        m_state[pos] = 0;
    }
};

} // namespace irt

#endif
