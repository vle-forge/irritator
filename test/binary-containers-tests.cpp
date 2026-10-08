// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/ut.hpp>

#include <irritator/binary-containers.hpp>

#include <algorithm>
#include <cstdint>
#include <map>
#include <memory_resource>
#include <set>
#include <span>
#include <type_traits>
#include <vector>

namespace ctest {

using alloc_t = irt::allocator<irt::new_delete_memory_resource>;

enum class aid : irt::u32 {}; // key:u16 | index:u16
enum class bid : irt::u64 {}; // key:u32 | index:u32

/// No padding, no float: @c binary_raw without opt-in.
struct plain {
    irt::u32 a;
    irt::u32 b;
};

/// A non trivial element: a vector, and a counter of the living instances to
/// detect a leak or a double destruction.
struct item {
    static inline int live = 0;

    irt::vector<irt::i32> values;
    irt::u32              tag = 0;

    item() noexcept { ++live; }

    item(const item& o) noexcept
      : values(o.values)
      , tag(o.tag)
    {
        ++live;
    }

    item(item&& o) noexcept
      : values(std::move(o.values))
      , tag(o.tag)
    {
        ++live;
    }

    item& operator=(const item&) noexcept = default;
    item& operator=(item&&) noexcept      = default;

    ~item() noexcept { --live; }
};

template<typename Ar>
void binary_serialize(Ar& ar, item& x) noexcept
{
    ar(x.values, x.tag);

    if constexpr (Ar::is_reader) {
        if (x.tag == 0xFFFFFFFFu) // a value that is refused
            ar.fail();
    }
}

static_assert(irt::binary_raw<plain>);
static_assert(not irt::binary_raw<item>);

inline plain make_plain(const irt::u64 v) noexcept
{
    return plain{ static_cast<irt::u32>(v),
                  static_cast<irt::u32>(v >> 32) ^ 0xA5A5A5A5u };
}

inline void fill(plain& p, const irt::u64 v) noexcept { p = make_plain(v); }
inline void fill(float& f, const irt::u64 v) noexcept
{
    f = static_cast<float>(v & 0xFFFFu);
}
inline void fill(item& x, const irt::u64 v) noexcept
{
    x.tag = static_cast<irt::u32>(v) & 0x7FFFFFFFu;
    x.values.resize(static_cast<std::size_t>(v % 5));
    for (std::size_t i = 0; i != x.values.size(); ++i)
        x.values[i] = static_cast<irt::i32>(v + i);
}

inline bool matches(const plain& p, const irt::u64 v) noexcept
{
    return p.a == make_plain(v).a and p.b == make_plain(v).b;
}
inline bool matches(const float& f, const irt::u64 v) noexcept
{
    return f == static_cast<float>(v & 0xFFFFu);
}
inline bool matches(const item& x, const irt::u64 v) noexcept
{
    if (x.tag != (static_cast<irt::u32>(v) & 0x7FFFFFFFu) or
        x.values.size() != static_cast<std::size_t>(v % 5))
        return false;

    for (std::size_t i = 0; i != x.values.size(); ++i)
        if (x.values[i] != static_cast<irt::i32>(v + i))
            return false;

    return true;
}

using da_plain    = irt::data_array<plain, aid>;
using da_item     = irt::data_array<item, bid>;
using ia32        = irt::id_array<aid>;
using ia64        = irt::id_array<bid>;
using ida_void    = irt::id_data_array<void, aid, alloc_t, float, plain, item>;
using ida_plain   = irt::id_data_array<plain, bid, alloc_t, float, item>;
using ida_item    = irt::id_data_array<item, aid, alloc_t, plain>;

template<typename C>
inline constexpr bool is_data_array = false;
template<typename T, typename I, typename A>
inline constexpr bool is_data_array<irt::data_array<T, I, A>> = true;

template<typename C>
inline constexpr bool is_id_array = false;
template<typename I, typename A>
inline constexpr bool is_id_array<irt::id_array<I, A>> = true;

template<typename C>
inline constexpr bool is_ida = false;
template<typename T, typename I, typename A, typename... Ts>
inline constexpr bool is_ida<irt::id_data_array<T, I, A, Ts...>> = true;

template<typename C, typename X>
inline constexpr bool has_col = false;
template<typename T, typename I, typename A, typename... Ts, typename X>
inline constexpr bool has_col<irt::id_data_array<T, I, A, Ts...>, X> =
  (std::is_same_v<Ts, X> or ...);

/// The same operations whatever the container.
template<typename C>
struct ops {
    using id_type = typename C::identifier_type;

    static constexpr bool stale_free_allowed = not is_id_array<C>;

    static const char* name() noexcept
    {
        if constexpr (is_data_array<C>)
            return sizeof(id_type) == 4 ? "data_array<plain, u32 id>"
                                        : "data_array<item, u64 id>";
        else if constexpr (is_id_array<C>)
            return sizeof(id_type) == 4 ? "id_array<u32 id>"
                                        : "id_array<u64 id>";
        else if constexpr (std::is_void_v<typename C::value_type>)
            return "id_data_array<void, columns>";
        else if constexpr (std::is_same_v<typename C::value_type, plain>)
            return "id_data_array<plain, columns>";
        else
            return "id_data_array<item, columns>";
    }

    static irt::u64 raw(const id_type id) noexcept
    {
        return static_cast<irt::u64>(id);
    }

    static id_type from_raw(const irt::u64 v) noexcept
    {
        return static_cast<id_type>(
          static_cast<std::underlying_type_t<id_type>>(v));
    }

    static void make_room(C& c) noexcept
    {
        if constexpr (is_ida<C>) {
            irt::debug::ensure(c.can_alloc(1)); // grows by itself
        } else {
            if (not c.can_alloc(1))
                irt::debug::ensure(c.template grow<2, 1>(1));
        }
    }

    static id_type alloc(C& c, const irt::u64 v) noexcept
    {
        make_room(c);

        id_type id{};

        if constexpr (is_data_array<C>) {
            id = c.alloc_id();
            fill(c.get(id), v);
        } else if constexpr (is_id_array<C>) {
            id = c.alloc();
        } else {
            id = c.alloc_id();

            if constexpr (not std::is_void_v<typename C::value_type>)
                fill(*c.try_to_get(id), v);

            if constexpr (has_col<C, float>)
                fill(c.template get<float>(id), v);
            if constexpr (has_col<C, plain>)
                fill(c.template get<plain>(id), v);
            if constexpr (has_col<C, item>)
                fill(c.template get<item>(id), v);
        }

        return id;
    }

    static void free(C& c, const id_type id) noexcept { c.free(id); }

    static bool exists(const C& c, const id_type id) noexcept
    {
        return c.exists(id);
    }

    static bool check(const C& c, const id_type id, const irt::u64 v) noexcept
    {
        if constexpr (is_data_array<C>) {
            return matches(c.get(id), v);
        } else if constexpr (is_id_array<C>) {
            return true;
        } else {
            bool ok = true;

            if constexpr (not std::is_void_v<typename C::value_type>)
                ok = ok and matches(*c.try_to_get(id), v);

            if constexpr (has_col<C, float>)
                ok = ok and matches(c.template get<float>(id), v);
            if constexpr (has_col<C, plain>)
                ok = ok and matches(c.template get<plain>(id), v);
            if constexpr (has_col<C, item>)
                ok = ok and matches(c.template get<item>(id), v);

            return ok;
        }
    }

    static std::vector<irt::u64> ids(const C& c) noexcept
    {
        std::vector<irt::u64> out;

        if constexpr (is_data_array<C>) {
            for (const auto& e : c)
                out.push_back(raw(c.get_id(e)));
        } else if constexpr (is_id_array<C>) {
            for (const auto id : c)
                out.push_back(raw(id));
        } else {
            // for_each_id() iterates the identifiers only when T is void
            const auto& all = c.get_ids();

            if constexpr (std::is_void_v<typename C::value_type>) {
                for (const auto id : all)
                    out.push_back(raw(id));
            } else {
                for (const auto& e : all)
                    out.push_back(raw(all.get_id(e)));
            }
        }

        return out;
    }
};

/// A memory resource that can be told to fail (it returns nullptr).
struct failing_resource {
    class data
    {
    public:
        static inline bool fail = false;

        void* allocate(
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            if (fail)
                return nullptr;

            return std::pmr::new_delete_resource()->allocate(bytes, alignment);
        }

        void deallocate(
          void*       p,
          std::size_t bytes,
          std::size_t alignment = alignof(std::max_align_t)) noexcept
        {
            std::pmr::new_delete_resource()->deallocate(p, bytes, alignment);
        }

        void release() noexcept {}

        std::pair<std::size_t, std::size_t> get_memory_usage() noexcept
        {
            return { 0, 0 };
        }
    };

    static data& instance() noexcept
    {
        static data d;
        return d;
    }
};

struct xorshift {
    irt::u64 s;

    irt::u64 next() noexcept
    {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
};

/// What the container must hold (id -> value), what it allocated and which
/// identifiers are dead.
struct model {
    std::map<irt::u64, irt::u64> alive;
    std::vector<irt::u64>        allocated; // in order
    std::vector<irt::u64>        dead;
};

template<typename C>
void churn(C& c, model& m, xorshift& rng, const int steps, const int max_alive) noexcept
{
    using O = ops<C>;

    for (int s = 0; s != steps; ++s) {
        const auto r = rng.next() % 10;

        if ((r < 6 and static_cast<int>(m.alive.size()) < max_alive) or
            m.alive.empty()) {
            const auto v  = rng.next();
            const auto id = O::raw(O::alloc(c, v));
            m.alive[id]   = v;
            m.allocated.push_back(id);
        } else if (r < 9 or not O::stale_free_allowed or m.dead.empty()) {
            auto it = m.alive.begin();
            std::advance(it, static_cast<long>(rng.next() % m.alive.size()));
            O::free(c, O::from_raw(it->first));
            m.dead.push_back(it->first);
            m.alive.erase(it);
        } else {
            // a stale identifier is ignored by free()
            O::free(c, O::from_raw(m.dead[rng.next() % m.dead.size()]));
        }
    }
}

template<typename C>
bool same_content(const C& c, const model& m) noexcept
{
    using O = ops<C>;

    if (c.size() != m.alive.size())
        return false;

    auto ids = O::ids(c);
    std::sort(ids.begin(), ids.end());

    std::vector<irt::u64> expected;
    for (const auto& [id, v] : m.alive) {
        expected.push_back(id);

        if (not O::exists(c, O::from_raw(id)) or
            not O::check(c, O::from_raw(id), v))
            return false;
    }

    if (ids != expected)
        return false;

    for (const auto id : m.dead)
        if (O::exists(c, O::from_raw(id)))
            return false;

    return true;
}

/// The structure is usable: what the program does with a container after a
/// restore. Allocates and frees, the new identifiers must be new.
template<typename C>
bool usable(C& c) noexcept
{
    using O = ops<C>;

    const auto before = O::ids(c);
    if (before.size() != c.size())
        return false;

    for (const auto id : before)
        if (not O::exists(c, O::from_raw(id)))
            return false;

    const std::set<irt::u64> known(before.begin(), before.end());
    std::vector<irt::u64>    added;

    for (irt::u64 i = 0; i != 20; ++i) {
        const auto id = O::raw(O::alloc(c, i));
        if (known.contains(id))
            return false;
        added.push_back(id);
    }

    if (c.size() != before.size() + added.size())
        return false;

    for (const auto id : added)
        O::free(c, O::from_raw(id));

    return c.size() == before.size();
}

inline std::vector<irt::u8> copy_of(const std::span<const irt::u8> bytes)
{
    return std::vector<irt::u8>(bytes.begin(), bytes.end());
}

template<typename C>
std::vector<irt::u8> dump(C& c)
{
    irt::binary_writer w;
    w(c);
    return w.ok() ? copy_of(w.bytes()) : std::vector<irt::u8>{};
}

/// The result of a restore: all the bytes must be used.
template<typename C>
bool load(C&                         c,
          const std::vector<irt::u8>& bytes,
          const std::size_t          limit = static_cast<std::size_t>(-1))
{
    irt::binary_reader r(bytes);
    r.set_allocation_limit(limit);
    r(c);
    return r.ok() and r.remaining() == 0;
}

template<typename F>
void for_each_kind(F&& f)
{
    f.template operator()<da_plain>();
    f.template operator()<da_item>();
    f.template operator()<ia32>();
    f.template operator()<ia64>();
    f.template operator()<ida_void>();
    f.template operator()<ida_plain>();
    f.template operator()<ida_item>();
}

/// Little endian bytes, to write the expected formats by hand.
struct bytes_builder {
    std::vector<irt::u8> v;

    bytes_builder& u16(const irt::u32 x)
    {
        v.push_back(static_cast<irt::u8>(x));
        v.push_back(static_cast<irt::u8>(x >> 8));
        return *this;
    }

    bytes_builder& u32(const irt::u32 x)
    {
        u16(x & 0xFFFFu);
        return u16(x >> 16);
    }

    bytes_builder& u64(const irt::u64 x)
    {
        u32(static_cast<irt::u32>(x));
        return u32(static_cast<irt::u32>(x >> 32));
    }
};

} // namespace ctest

namespace ut = boost::ut;

ut::suite<"irt::binary_serialize of the containers"> container_suite = [] {
    using namespace ut;
    using namespace ut::literals;
    using namespace ctest;

    "format: data_array with a hole"_test = [] {
        da_plain d;
        expect(d.reserve(4));

        const auto i0 = d.alloc_id(plain{ 10, 11 });
        const auto i1 = d.alloc_id(plain{ 20, 21 });
        const auto i2 = d.alloc_id(plain{ 30, 31 });
        expect(irt::get_index(i0) == 0 and irt::get_index(i2) == 2);

        d.free(i1);

        // capacity, max_size, max_used, next_key, free_head
        bytes_builder b;
        b.u16(4).u16(2).u16(3).u16(4).u16(1);
        // slot 0: id (key 1, index 0), element
        b.u32(0x00010000u).u32(10).u32(11);
        // slot 1: dead, the id is the end of the free list; no element
        b.u32(0x0000FFFFu);
        // slot 2: id (key 3, index 2), element
        b.u32(0x00030002u).u32(30).u32(31);

        expect(dump(d) == b.v) << "the format of a data_array";
    };

    "format: id_array with a hole"_test = [] {
        ia32 d;
        expect(d.reserve(4));

        const auto i0 = d.alloc();
        const auto i1 = d.alloc();
        const auto i2 = d.alloc();
        (void)i0;
        (void)i2;
        d.free(i1);

        bytes_builder b;
        b.u16(4).u16(2).u16(3).u16(4).u16(1);
        b.u32(0x00010000u).u32(0x0000FFFFu).u32(0x00030002u);

        expect(dump(d) == b.v) << "the format of an id_array";
    };

    "format: id_data_array, only the alive slots of the columns"_test = [] {
        ida_void d;
        expect(d.reserve(4));

        const auto i0 = d.alloc_id();
        const auto i1 = d.alloc_id();
        const auto i2 = d.alloc_id();

        d.get<float>(i0)  = 1.5f;
        d.get<float>(i1)  = 2.5f;
        d.get<float>(i2)  = 3.5f;
        d.get<plain>(i0)  = plain{ 1, 2 };
        d.get<plain>(i1)  = plain{ 3, 4 };
        d.get<plain>(i2)  = plain{ 5, 6 };
        d.get<item>(i0).tag = 7;
        d.get<item>(i1).tag = 8;
        d.get<item>(i2).tag = 9;

        d.free(i1);

        irt::binary_writer expected;
        expected(irt::u16{ 4 }, irt::u16{ 2 }, irt::u16{ 3 }, irt::u16{ 4 },
                 irt::u16{ 1 });
        expected(irt::u32{ 0x00010000u }, irt::u32{ 0x0000FFFFu },
                 irt::u32{ 0x00030002u });
        expected(1.5f, 3.5f);             // the float column
        expected(plain{ 1, 2 }, plain{ 5, 6 }); // the plain column
        expected(d.get<item>(i0));        // the item column
        expected(d.get<item>(i2));

        expect(dump(d) == copy_of(expected.bytes()));
    };

    "restore: same content, same identifiers, same next identifiers (rollback)"_test =
      [] {
          for_each_kind([]<typename C>() {
              using O = ops<C>;

              {
                  xorshift rng{ 0x9E3779B97F4A7C15ull };

                  C     original;
                  model m;
                  churn(original, m, rng, 3000, 300);
                  expect(same_content(original, m)) << O::name();

                  const auto snapshot = dump(original);
                  expect(not snapshot.empty()) << O::name();

                  const model at_snapshot = m;

                  // the future of the original run

                  xorshift future{ 0x1234567ull };
                  auto     m_a = m;
                  churn(original, m_a, future, 1500, 300);
                  expect(same_content(original, m_a)) << O::name();
                  const auto end_a = dump(original);

                  // the same future after a restore in a fresh container

                  C fresh;
                  expect(load(fresh, snapshot)) << O::name();
                  expect(same_content(fresh, at_snapshot)) << O::name();
                  expect(dump(fresh) == snapshot)
                    << O::name() << ": a restore then a dump is the identity";

                  xorshift future_b{ 0x1234567ull };
                  auto     m_b = at_snapshot;
                  churn(fresh, m_b, future_b, 1500, 300);

                  expect(m_b.allocated == m_a.allocated)
                    << O::name() << ": the same identifiers are produced";
                  expect(same_content(fresh, m_a)) << O::name();
                  expect(dump(fresh) == end_a)
                    << O::name() << ": the same state at the end";

                  // the same future after a restore in place, in a container
                  // which went elsewhere (more elements, more capacity)

                  expect(load(original, snapshot)) << O::name();
                  expect(same_content(original, at_snapshot)) << O::name();

                  xorshift future_c{ 0x1234567ull };
                  auto     m_c = at_snapshot;
                  churn(original, m_c, future_c, 1500, 300);

                  expect(m_c.allocated == m_a.allocated)
                    << O::name() << ": in place, the same identifiers";
                  expect(same_content(original, m_a)) << O::name();
              }

              expect(item::live == 0) << O::name() << ": no leak of elements";
          });
      };

    "restore: the identifiers kept outside the container"_test = [] {
        for_each_kind([]<typename C>() {
            using O = ops<C>;

            {
                C c;
                const auto kept  = O::alloc(c, 1);
                const auto freed = O::alloc(c, 2);
                const auto last  = O::alloc(c, 3);
                O::free(c, freed);

                const auto snapshot = dump(c);

                // after the snapshot: kept is freed, new elements appear

                O::free(c, kept);
                const auto later = O::alloc(c, 4);
                O::free(c, last);
                (void)later;

                expect(load(c, snapshot)) << O::name();

                expect(O::exists(c, kept)) << O::name();
                expect(O::exists(c, last)) << O::name();
                expect(not O::exists(c, freed)) << O::name();
                expect(not O::exists(c, later)) << O::name();
                expect(O::check(c, kept, 1)) << O::name();
                expect(O::check(c, last, 3)) << O::name();
            }

            expect(item::live == 0) << O::name();
        });
    };

    "restore: capacity (never reduced, restored when needed)"_test = [] {
        for_each_kind([]<typename C>() {
            using O = ops<C>;

            {
                C small;
                for (irt::u64 i = 0; i != 5; ++i)
                    O::alloc(small, i);
                const auto small_snapshot = dump(small);

                C big;
                expect(big.reserve(500));
                for (irt::u64 i = 0; i != 100; ++i)
                    O::alloc(big, i);
                const auto big_capacity = big.capacity();

                // a smaller snapshot in a bigger container

                expect(load(big, small_snapshot)) << O::name();
                expect(big.size() == 5u) << O::name();
                expect(big.capacity() == big_capacity) << O::name();
                expect(usable(big)) << O::name();

                // a bigger snapshot in an empty container

                const auto big_snapshot = dump(big);
                C          empty;
                expect(empty.capacity() == 0);
                expect(load(empty, big_snapshot)) << O::name();
                expect(empty.capacity() == big_capacity) << O::name();
                expect(usable(empty)) << O::name();

                // every slot of the capacity is reachable (columns included)

                while (static_cast<irt::u64>(empty.size()) <
                       static_cast<irt::u64>(big_capacity))
                    O::alloc(empty, empty.size());

                expect(empty.capacity() == big_capacity) << O::name();

                // an empty container as a snapshot

                C nothing;
                const auto nothing_snapshot = dump(nothing);
                expect(load(big, nothing_snapshot)) << O::name();
                expect(big.size() == 0u) << O::name();
                expect(usable(big)) << O::name();
            }

            expect(item::live == 0) << O::name();
        });
    };

    "restore: the stale values of the dead slots are not part of the dump"_test =
      [] {
          ida_void a;
          expect(a.reserve(8));

          for (irt::u64 i = 0; i != 6; ++i)
              ops<ida_void>::alloc(a, i);

          const auto id1 = a.get_from_index(1);
          const auto id4 = a.get_from_index(4);
          a.free(id1);
          a.free(id4);

          const auto clean = dump(a);

          // scribble over the dead slots

          a.get<float>()[1]       = 1234.f;
          a.get<float>()[4]       = -1.f;
          a.get<plain>()[1]       = plain{ 9, 9 };
          a.get<item>()[4].tag    = 77;
          a.get<float>()[7]       = 5.f; // after max_used

          expect(dump(a) == clean) << "equal states, equal dumps";
      };

    "restore: every truncation fails and leaves an empty usable container"_test =
      [] {
          for_each_kind([]<typename C>() {
              using O = ops<C>;

              {
                  xorshift rng{ 77 };
                  C        c;
                  model    m;
                  churn(c, m, rng, 40, 12);

                  const auto full = dump(c);
                  expect(not full.empty()) << O::name();

                  bool all_failed = true;
                  bool all_clean  = true;

                  for (std::size_t length = 0; length != full.size(); ++length) {
                      const std::vector<irt::u8> prefix(full.begin(),
                                                        full.begin() +
                                                          static_cast<long>(length));

                      // the destination has its own content, which is replaced

                      C target;
                      for (irt::u64 i = 0; i != 3; ++i)
                          O::alloc(target, i);

                      if (load(target, prefix))
                          all_failed = false;

                      if (target.size() != 0u or not usable(target))
                          all_clean = false;
                  }

                  expect(all_failed) << O::name();
                  expect(all_clean) << O::name();

                  // and the full dump is read

                  C target;
                  expect(load(target, full)) << O::name();
                  expect(same_content(target, m)) << O::name();

                  // bytes after the dump are not consumed

                  auto longer = full;
                  longer.push_back(0);
                  irt::binary_reader r(longer);
                  C                  again;
                  r(again);
                  expect(r.ok() and r.remaining() == 1u) << O::name();
              }

              expect(item::live == 0) << O::name() << ": no leak after failures";
          });
      };

    "restore: random corruptions never break the container"_test = [] {
        for_each_kind([]<typename C>() {
            using O = ops<C>;

            {
                xorshift rng{ 4242 };
                C        c;
                model    m;
                churn(c, m, rng, 60, 15);
                const auto full = dump(c);

                xorshift noise{ 0xDEADBEEFull };
                int      accepted = 0;
                int      refused  = 0;

                for (int iteration = 0; iteration != 3000; ++iteration) {
                    auto bytes = full;

                    const auto changes = 1 + noise.next() % 4;
                    for (std::uint64_t i = 0; i != changes; ++i) {
                        const auto at = noise.next() % bytes.size();
                        bytes[at]     = (noise.next() % 3 == 0)
                                          ? irt::u8{ 0xFF }
                                          : static_cast<irt::u8>(noise.next());
                    }

                    C target;
                    for (irt::u64 i = 0; i != 3; ++i)
                        O::alloc(target, i);

                    // a corrupted capacity must not allocate gigabytes

                    const bool ok = load(target, bytes, 1u << 20);

                    if (ok)
                        ++accepted;
                    else
                        ++refused;

                    if (not ok and target.size() != 0u) {
                        expect(false) << O::name() << ": cleared after a failure";
                        break;
                    }

                    if (not usable(target)) {
                        expect(false) << O::name() << ": usable after a restore";
                        break;
                    }
                }

                expect(refused > 0) << O::name() << ": some corruptions are seen";
                expect(accepted + refused == 3000);
            }

            expect(item::live == 0) << O::name();
        });
    };

    "restore: an element refused by its own function"_test = [] {
        da_item d;
        expect(d.reserve(8));
        for (irt::u64 i = 0; i != 5; ++i)
            ops<da_item>::alloc(d, i);

        // the third element has an invalid tag

        const auto ids = ops<da_item>::ids(d);
        d.get(ops<da_item>::from_raw(ids[2])).tag = 0xFFFFFFFFu;
        const auto bytes = dump(d);

        da_item target;
        for (irt::u64 i = 0; i != 4; ++i)
            ops<da_item>::alloc(target, i);

        expect(not load(target, bytes));
        expect(target.size() == 0u);
        expect(usable(target));

        d.destroy();
        target.destroy();
        expect(item::live == 0) << "the elements built before the refusal are destroyed";
    };

    "restore: fields that contradict each other are refused"_test = [] {
        // id_array<aid>: capacity, max_size, max_used, next_key, free_head,
        // identifiers
        const auto craft = [](irt::u32 capacity,
                              irt::u32 max_size,
                              irt::u32 max_used,
                              irt::u32 next_key,
                              irt::u32 free_head,
                              std::vector<irt::u32> ids) {
            bytes_builder b;
            b.u16(capacity).u16(max_size).u16(max_used).u16(next_key).u16(free_head);
            for (const auto id : ids)
                b.u32(id);
            return b.v;
        };

        const auto good = [&](const std::vector<irt::u8>& bytes) {
            ia32 target;
            const bool ok = load(target, bytes);
            return ok and usable(target);
        };

        // two elements alive (0 and 2), the slot 1 is dead
        expect(good(craft(4, 2, 3, 4, 1,
                          { 0x00010000u, 0x0000FFFFu, 0x00030002u })))
          << "the reference is valid";

        // two dead slots separated by an alive one
        expect(good(craft(4, 1, 3, 4, 0,
                          { 0x00000002u, 0x00020001u, 0x0000FFFFu })))
          << "a free list over two dead slots";

        const std::vector<std::pair<const char*, std::vector<irt::u8>>> bad = {
            { "capacity == none", craft(0xFFFF, 0, 0, 1, 0xFFFF, {}) },
            { "max_used > capacity",
              craft(2, 3, 3, 4, 0xFFFF, { 0x10000u, 0x20001u, 0x30002u }) },
            { "max_size > max_used", craft(4, 3, 2, 4, 0xFFFF, { 0x10000u, 0x20001u }) },
            { "next_key == 0",
              craft(4, 2, 2, 0, 0xFFFF, { 0x10000u, 0x20001u }) },
            { "free list with all the slots alive",
              craft(4, 2, 2, 4, 1, { 0x10000u, 0x20001u }) },
            { "dead slots without free list",
              craft(4, 1, 2, 4, 0xFFFF, { 0x10000u, 0x0000FFFFu }) },
            { "free_head out of max_used",
              craft(4, 1, 2, 4, 5, { 0x10000u, 0x0000FFFFu }) },
            { "free_head on an alive slot",
              craft(4, 1, 2, 4, 0, { 0x10000u, 0x0000FFFFu }) },
            { "alive slot with the index of another",
              craft(4, 2, 2, 4, 0xFFFF, { 0x10000u, 0x20000u }) },
            { "alive count wrong",
              craft(4, 1, 3, 4, 2, { 0x10000u, 0x20001u, 0x0000FFFFu }) },
            { "free list that skips a dead slot",
              craft(4, 1, 3, 4, 1, { 0x10000u, 0x0000FFFFu, 0x0000FFFFu }) },
            { "free list not sorted",
              craft(4, 1, 3, 4, 2, { 0x10000u, 0x00000002u, 0x00000001u }) },
            { "free list with a cycle",
              craft(4, 0, 2, 4, 0, { 0x00000001u, 0x00000000u }) },
            { "free list pointing out",
              craft(4, 0, 2, 4, 0, { 0x00000007u, 0x0000FFFFu }) },
            { "truncated identifiers", craft(4, 2, 2, 4, 0xFFFF, { 0x10000u }) },
        };

        for (const auto& [name, bytes] : bad)
            expect(not good(bytes)) << name;

        // the same checks with a 64 bits identifier and u32 indices
        bytes_builder b;
        b.u32(4).u32(1).u32(2).u32(9).u32(1);
        b.u64(0x0000000100000000ull).u64(0x00000000FFFFFFFFull);

        ia64 target64;
        expect(load(target64, b.v) and usable(target64)) << "64 bits ids";

        bytes_builder wrong;
        wrong.u32(4).u32(1).u32(2).u32(9).u32(1);
        wrong.u64(0x0000000100000001ull).u64(0x00000000FFFFFFFFull); // index 1

        ia64 refused64;
        expect(not load(refused64, wrong.v)) << "wrong index in a 64 bits id";
    };

    "restore: an allocation that fails is a clean failure"_test = [] {
        using fa = irt::allocator<failing_resource>;
        using da = irt::data_array<plain, aid, fa>;
        using ia = irt::id_array<aid, fa>;

        da source_da;
        ia source_ia;
        expect(source_da.reserve(16));
        expect(source_ia.reserve(16));
        for (irt::u64 i = 0; i != 5; ++i) {
            ops<da>::alloc(source_da, i);
            ops<ia>::alloc(source_ia, i);
        }

        const auto bytes_da = dump(source_da);
        const auto bytes_ia = dump(source_ia);

        da target_da;
        ia target_ia;

        failing_resource::data::fail = true;
        const bool da_loaded         = load(target_da, bytes_da);
        const bool ia_loaded         = load(target_ia, bytes_ia);
        failing_resource::data::fail = false;

        expect(not da_loaded);
        expect(not ia_loaded);
        expect(target_da.size() == 0u and target_da.capacity() == 0);
        expect(target_ia.size() == 0u and target_ia.capacity() == 0);

        expect(load(target_da, bytes_da) and usable(target_da));
        expect(load(target_ia, bytes_ia) and usable(target_ia));
    };

    "restore: the allocation limit"_test = [] {
        // a huge capacity in a tiny input: id_array<bid>, index_type is u32
        bytes_builder b;
        b.u32(0x7FFFFFFEu).u32(0).u32(0).u32(1).u32(0xFFFFFFFFu);

        ia64 target;
        expect(not load(target, b.v, 1u << 20));
        expect(target.capacity() == 0) << "nothing was allocated";

        // the same stream is accepted when the limit allows it: a small one
        bytes_builder small;
        small.u32(1000).u32(0).u32(0).u32(1).u32(0xFFFFFFFFu);

        ia64 other;
        expect(load(other, small.v, 1u << 20));
        expect(other.capacity() == 1000);

        // the columns count in the limit
        ida_void columns;
        columns.reserve(1000);
        const auto bytes = dump(columns);

        ida_void restricted;
        expect(not load(restricted, bytes, 100));
        expect(restricted.capacity() == 0);
        expect(load(restricted, bytes, 1u << 20));

        // the limit is on the bytes of the slots, and exact

        const std::size_t slots = 1000u * (sizeof(aid) + sizeof(float) +
                                           sizeof(plain) + sizeof(item));
        ida_void          exact;
        expect(not load(exact, bytes, slots - 1));
        expect(exact.capacity() == 0);
        expect(load(exact, bytes, slots));

        ia64 exact64;
        expect(not load(exact64, small.v, 1000u * sizeof(bid) - 1));
        expect(load(exact64, small.v, 1000u * sizeof(bid)));

        // a container that already has the capacity does not allocate

        ia64 prefilled;
        expect(prefilled.reserve(2000));
        expect(load(prefilled, small.v, 0));
        expect(prefilled.capacity() == 2000);
    };
};
