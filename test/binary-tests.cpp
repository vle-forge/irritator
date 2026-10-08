// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/ut.hpp>

#include <irritator/binary.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>
#include <vector>

namespace binary_test {

enum class node_id : irt::u64 {};
enum class mode : irt::u8 { idle, run, stop };

/// No padding: @c binary_raw without opt-in.
struct pair_i32 {
    irt::i32 x;
    irt::i32 y;
};

/// A float: @c binary_raw with the opt-in below.
struct cell {
    irt::u32 a;
    float    b;
};

/// Padding bytes: can not be dumped as a block.
struct padded {
    char     c;
    irt::i32 i;
};

} // namespace binary_test

namespace irt {

template<>
inline constexpr bool binary_raw_layout<binary_test::cell> = true;

} // namespace irt

namespace binary_test {

static_assert(std::has_unique_object_representations_v<pair_i32>);
static_assert(irt::binary_raw<pair_i32>);
static_assert(irt::binary_raw<cell>);
static_assert(irt::binary_raw<node_id>);
static_assert(irt::binary_raw<irt::u64>);
static_assert(not irt::binary_raw<padded>);
static_assert(not irt::binary_raw<bool>);

static_assert(irt::binary_scalar<irt::u8>);
static_assert(irt::binary_scalar<irt::i64>);
static_assert(irt::binary_scalar<float>);
static_assert(not irt::binary_scalar<bool>);
static_assert(not irt::binary_scalar<long double>);
static_assert(not irt::binary_scalar<wchar_t>);
static_assert(not irt::binary_scalar<int*>);
static_assert(irt::binary_enum<node_id>);
static_assert(not irt::binary_enum<irt::u32>);

/// A type with nested containers.
struct inner {
    irt::vector<irt::i32> data;
    irt::u8               flag = 0;
};

static_assert(not irt::binary_raw<inner>);

template<typename Ar>
void binary_serialize(Ar& ar, inner& x) noexcept
{
    ar(x.data, x.flag);
}

/// The type described once for the writing and the reading.
struct world {
    irt::vector<float>      values;
    irt::vector<pair_i32>   pairs;
    irt::vector<cell>       cells;
    irt::vector<inner>      inners;
    std::array<irt::u16, 3> counters{};
    irt::u64                step = 0;
    double                  time = 0;
    node_id                 head{};
    mode                    state   = mode::idle;
    bool                    running = false;
};

template<typename Ar>
void binary_serialize(Ar& ar, world& w) noexcept
{
    ar(w.values, w.pairs, w.cells, w.inners, w.counters, w.step, w.time,
       w.head, w.state, w.running);

    // validation of a value read in the stream

    if constexpr (Ar::is_reader) {
        if (static_cast<irt::u8>(w.state) > 2)
            ar.fail();
    }
}

template<typename C>
irt::i32 isize(const C& c) noexcept
{
    return static_cast<irt::i32>(c.size());
}

world make_world(const irt::u32 seed)
{
    world w;

    w.values.resize(5 + seed % 7);
    for (irt::i32 i = 0; i != isize(w.values); ++i)
        w.values.data()[i] = static_cast<float>(i) * 0.25f +
                             static_cast<float>(seed);

    w.pairs.resize(3 + seed % 5);
    for (irt::i32 i = 0; i != isize(w.pairs); ++i)
        w.pairs.data()[i] = { i, -i * static_cast<irt::i32>(seed) };

    w.cells.resize(2 + seed % 3);
    for (irt::i32 i = 0; i != isize(w.cells); ++i)
        w.cells.data()[i] = { static_cast<irt::u32>(i) + seed,
                              static_cast<float>(i) / 3.0f };

    w.inners.resize(2 + seed % 4);
    for (irt::i32 i = 0; i != isize(w.inners); ++i) {
        w.inners.data()[i].data.resize(i + 1);
        for (irt::i32 j = 0; j != i + 1; ++j)
            w.inners.data()[i].data.data()[j] = i * 100 + j;
        w.inners.data()[i].flag = static_cast<irt::u8>(i);
    }

    w.counters = { static_cast<irt::u16>(seed), 2, 3 };
    w.step     = seed * 1000003ull;
    w.time     = seed * 0.1;
    w.head     = node_id{ seed };
    w.state    = mode::run;
    w.running  = true;

    return w;
}

template<typename T>
bool same_array(const irt::vector<T>& a, const irt::vector<T>& b)
{
    if (a.size() != b.size())
        return false;

    for (irt::i32 i = 0; i != isize(a); ++i)
        if (std::memcmp(a.data() + i, b.data() + i, sizeof(T)) != 0)
            return false;

    return true;
}

bool same(const world& a, const world& b)
{
    if (not same_array(a.values, b.values) or not same_array(a.pairs, b.pairs) or
        not same_array(a.cells, b.cells) or a.inners.size() != b.inners.size())
        return false;

    for (irt::i32 i = 0; i != isize(a.inners); ++i)
        if (a.inners.data()[i].flag != b.inners.data()[i].flag or
            not same_array(a.inners.data()[i].data, b.inners.data()[i].data))
            return false;

    return a.counters == b.counters and a.step == b.step and
           a.time == b.time and a.head == b.head and a.state == b.state and
           a.running == b.running;
}

/// A copy of the bytes in a block of exactly the right size: reading after
/// the end is reported by AddressSanitizer.
std::vector<irt::u8> exact_copy(std::span<const irt::u8> bytes)
{
    return std::vector<irt::u8>(bytes.begin(), bytes.end());
}

/// A buffer with a fixed capacity: the allocation failure of a writer.
template<std::size_t N>
class fixed_buffer
{
public:
    irt::i32       size() const noexcept { return size_; }
    irt::u8*       data() noexcept { return bytes_.data(); }
    const irt::u8* data() const noexcept { return bytes_.data(); }

    bool resize(std::size_t n) noexcept
    {
        if (n > N)
            return false;

        size_ = static_cast<irt::i32>(n);
        return true;
    }

private:
    std::array<irt::u8, N> bytes_{};
    irt::i32               size_ = 0;
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

} // namespace binary_test

namespace ut = boost::ut;

ut::suite<"irt::binary_writer / irt::binary_reader"> binary_suite = [] {
    using namespace ut;
    using namespace ut::literals;
    using namespace binary_test;

    "scalars, enumerations and booleans round trip"_test = [] {
        irt::binary_writer w;

        w(irt::u8{ 250 }, irt::i8{ -2 }, irt::u16{ 65000 },
          irt::i16{ -32000 }, irt::u32{ 4000000000u }, irt::i32{ -2000000000 },
          irt::u64{ 18000000000000000000ull },
          irt::i64{ -9000000000000000000ll }, 1.5f, -2.25, true, false,
          node_id{ 42 }, mode::stop);

        expect(w.ok());
        expect(w.size() == 1 + 1 + 2 + 2 + 4 + 4 + 8 + 8 + 4 + 8 + 1 + 1 + 8 + 1);

        irt::u8  u8v{};
        irt::i8  i8v{};
        irt::u16 u16v{};
        irt::i16 i16v{};
        irt::u32 u32v{};
        irt::i32 i32v{};
        irt::u64 u64v{};
        irt::i64 i64v{};
        float    fv{};
        double   dv{};
        bool     b1 = false, b2 = true;
        node_id  id{};
        mode     m{};

        irt::binary_reader r(w.bytes());
        r(u8v, i8v, u16v, i16v, u32v, i32v, u64v, i64v, fv, dv, b1, b2, id, m);

        expect(r.ok());
        expect(r.remaining() == 0u);
        expect(u8v == 250);
        expect(i8v == -2);
        expect(u16v == 65000);
        expect(i16v == -32000);
        expect(u32v == 4000000000u);
        expect(i32v == -2000000000);
        expect(u64v == 18000000000000000000ull);
        expect(i64v == -9000000000000000000ll);
        expect(fv == 1.5f);
        expect(dv == -2.25);
        expect(b1);
        expect(not b2);
        expect(id == node_id{ 42 });
        expect(m == mode::stop);
    };

    "the format is little endian"_test = [] {
        irt::binary_writer w;
        w(irt::u32{ 0x01020304u }, irt::i16{ -2 }, true);

        const auto b = w.bytes();
        expect(b.size() == 7u);
        expect(b[0] == 0x04 and b[1] == 0x03 and b[2] == 0x02 and
               b[3] == 0x01);
        expect(b[4] == 0xFE and b[5] == 0xFF) << "-2 as i16";
        expect(b[6] == 1) << "true is 1";
    };

    "a boolean is 0 or 1, any other byte is an error"_test = [] {
        const std::array<irt::u8, 3> bytes{ 0, 1, 2 };
        irt::binary_reader           r(bytes);

        bool a = true, b = false, c = false;

        r(a);
        expect(r.ok());
        expect(not a);

        r(b);
        expect(r.ok());
        expect(b);

        r(c);
        expect(not r.ok());
    };

    "a failed reader ignores the following reads and zeroes the outputs"_test =
      [] {
          const std::array<irt::u8, 3> bytes{ 1, 2, 3 };
          irt::binary_reader           r(bytes);

          irt::u32 a = 0xDEADBEEF;
          irt::u8  b = 0xFF;

          r(a);
          expect(not r.ok());
          expect(a == 0u);

          r(b);
          expect(not r.ok());
          expect(b == 0) << "no read after an error";
          expect(r.position() == 0u) << "nothing is consumed by an error";
      };

    "arrays: count, element size and block"_test = [] {
        irt::vector<float> in;
        in.resize(4);
        for (irt::i32 i = 0; i != 4; ++i)
            in.data()[i] = static_cast<float>(i) + 0.5f;

        irt::binary_writer w;
        w(in);
        expect(w.size() == 8u + 4u + 16u);

        // the count (u64), then the size of an element (u32)

        irt::u64 count{};
        irt::u32 size{};
        {
            irt::binary_reader r(w.bytes());
            r(count, size);
        }
        expect(count == 4u);
        expect(size == 4u);

        irt::vector<float> out;
        irt::binary_reader r(w.bytes());
        r(out);

        expect(r.ok());
        expect(r.remaining() == 0u);
        expect(same_array(in, out));

        // empty array

        irt::vector<float> empty;
        irt::binary_writer w2;
        w2(empty);

        irt::vector<float> out2;
        out2.resize(3);
        irt::binary_reader r2(w2.bytes());
        r2(out2);
        expect(r2.ok());
        expect(out2.size() == 0) << "the container is resized, not appended";
    };

    "arrays: a wrong element size is an error"_test = [] {
        irt::vector<float> in;
        in.resize(2);

        irt::binary_writer w;
        w(in);

        irt::vector<double> out;
        irt::binary_reader  r(w.bytes());
        r(out);

        expect(not r.ok());
        expect(out.size() == 0);

        // The count alone is plausible here (2 elements, 16 bytes of data
        // for 4 byte elements): only the element size can reject it.
        irt::vector<double> wide;
        wide.resize(2);

        irt::binary_writer w2;
        w2(wide);

        irt::vector<float> narrow;
        irt::binary_reader r2(w2.bytes());
        r2(narrow);

        expect(not r2.ok());
        expect(narrow.size() == 0);
    };

    "arrays: a hostile count is rejected before any allocation"_test = [] {
        const auto crafted = [](irt::u64 count, irt::u32 size,
                                std::size_t payload) {
            irt::binary_writer w;
            w(count, size);
            for (std::size_t i = 0; i != payload; ++i)
                w(irt::u8{ 7 });
            return exact_copy(w.bytes());
        };

        // each count must be rejected and the container must stay empty

        for (const irt::u64 count :
             { irt::u64{ 1 } << 60, irt::u64{ 0xFFFFFFFFFFFFFFFFull },
               irt::u64{ 0x40000000u }, irt::u64{ 5 } }) {
            const auto         bytes = crafted(count, 4, 16); // room for 4
            irt::vector<float> out;
            irt::binary_reader r(bytes);
            r(out);

            expect(not r.ok()) << "count " << count;
            expect(out.size() == 0) << "count " << count;
        }

        // exactly the remaining bytes is valid

        const auto         bytes = crafted(4, 4, 16);
        irt::vector<float> out;
        irt::binary_reader r(bytes);
        r(out);
        expect(r.ok());
        expect(out.size() == 4);

        // the same for the containers of non trivial elements (a count of
        // elements that is greater than the remaining bytes)

        const auto bytes2 = crafted(1000, 4, 10);
        irt::vector<inner> out2;
        irt::binary_reader r2(bytes2);

        // the first 8 bytes of a vector<inner> are its count (u64): 1000 > 4
        // remaining bytes after the u32 consumed by the helper's header

        r2(out2);
        expect(not r2.ok());
        expect(out2.size() == 0);
    };

    "a type described once: nested containers, enumerations, arrays"_test =
      [] {
          const auto in = make_world(3);

          irt::binary_writer w;
          w(in);
          expect(w.ok());

          world out;
          irt::binary_reader r(w.bytes());
          r(out);

          expect(r.ok());
          expect(r.remaining() == 0u);
          expect(same(in, out));
      };

    "a value rejected by the validation of the type is an error"_test = [] {
        auto in  = make_world(2);
        in.state = static_cast<mode>(7);

        irt::binary_writer w;
        w(in);

        world out;
        irt::binary_reader r(w.bytes());
        r(out);

        expect(not r.ok());
    };

    "every truncation of a valid dump is an error and reads nothing outside"_test =
      [] {
          const auto in = make_world(5);

          irt::binary_writer w;
          w(in);
          const auto full = exact_copy(w.bytes());

          {
              irt::binary_reader r(full);
              world              out;
              r(out);
              expect(r.ok());
              expect(r.remaining() == 0u);
              expect(same(in, out));
          }

          for (std::size_t len = 0; len != full.size(); ++len) {
              // a block of exactly `len` bytes: a read after the end is
              // reported by the sanitizers

              const std::vector<irt::u8> prefix(full.begin(),
                                                full.begin() +
                                                  static_cast<std::ptrdiff_t>(len));

              irt::binary_reader r(prefix);
              world              out;
              r(out);

              if (r.ok()) {
                  expect(false) << "a dump truncated to " << len << " bytes of "
                                << full.size() << " is accepted";
                  return;
              }
          }
      };

    "random corruption never crashes and never allocates more than the input"_test =
      [] {
          const auto in = make_world(4);

          irt::binary_writer w;
          w(in);
          const auto full = exact_copy(w.bytes());

          binary_test::xorshift rng{ 0x2545F4914F6CDD1Dull };

          for (int iteration = 0; iteration != 4000; ++iteration) {
              auto bytes = full;

              const auto changes = 1 + rng.next() % 4;
              for (std::uint64_t i = 0; i != changes; ++i) {
                  const auto at = rng.next() % bytes.size();
                  bytes[at]     = (rng.next() % 3 == 0)
                                    ? irt::u8{ 0xFF }
                                    : static_cast<irt::u8>(rng.next());
              }

              if (rng.next() % 4 == 0)
                  bytes.resize(rng.next() % bytes.size());

              const std::vector<irt::u8> input(bytes.begin(), bytes.end());

              irt::binary_reader r(input);
              world              out;
              r(out);

              // whatever the content, no container is bigger than the input

              const auto limit = static_cast<irt::i32>(input.size());
              if (isize(out.values) > limit or isize(out.pairs) > limit or
                  isize(out.cells) > limit or isize(out.inners) > limit) {
                  expect(false) << "iteration " << iteration
                                << ": a container is bigger than the input";
                  return;
              }
          }
      };

    "header: magic number, version and byte order"_test = [] {
        constexpr auto magic = irt::binary_magic('I', 'R', 'T', 'D');

        irt::binary_writer w;
        irt::write_header(w, magic, 3);
        w(irt::u32{ 99 });
        expect(w.size() == 12u + 4u);

        {
            irt::binary_reader r(w.bytes());
            expect(irt::read_header(r, magic) == 3u);
            irt::u32 v{};
            r(v);
            expect(r.ok() and v == 99u);
        }

        {
            irt::binary_reader r(w.bytes());
            expect(irt::read_header(r, irt::binary_magic('X', 'X', 'X', 'X')) ==
                   0u);
            expect(not r.ok()) << "wrong magic number";
        }

        {
            auto bytes = exact_copy(w.bytes());
            // the byte order mark of a big endian file: 01 02 03 04
            bytes[8]  = 0x01;
            bytes[9]  = 0x02;
            bytes[10] = 0x03;
            bytes[11] = 0x04;
            irt::binary_reader r(bytes);
            expect(irt::read_header(r, magic) == 0u);
            expect(not r.ok());
        }

        {
            const std::array<irt::u8, 5> too_short{ 1, 2, 3, 4, 5 };
            irt::binary_reader           r(too_short);
            expect(irt::read_header(r, magic) == 0u);
            expect(not r.ok());
        }
    };

    "writer: growth, clear keeps the capacity, reserve"_test = [] {
        irt::binary_writer w;

        for (irt::u32 i = 0; i != 100000; ++i)
            w(i);

        expect(w.ok());
        expect(w.size() == 400000u);

        irt::binary_reader r(w.bytes());
        bool               all_good = true;
        for (irt::u32 i = 0; i != 100000 and all_good; ++i) {
            irt::u32 v{};
            r(v);
            all_good = r.ok() and v == i;
        }
        expect(all_good) << "the content after the growths";
        expect(r.remaining() == 0u);

        const auto capacity = w.capacity();
        expect(capacity >= 400000u);

        w.clear();
        expect(w.size() == 0u);
        expect(w.capacity() == capacity) << "clear() must not release memory";

        w(irt::u8{ 1 }, irt::u8{ 2 });
        expect(w.size() == 2u);
        expect(w.capacity() == capacity) << "no allocation to write again";
        expect(w.bytes()[0] == 1 and w.bytes()[1] == 2);

        irt::binary_writer w2;
        expect(w2.reserve(1000));
        expect(w2.capacity() >= 1000u);
        expect(w2.size() == 0u);

        // A buffer that can not grow: reserve() reports it, the writer stays
        // usable (irt::vector never reports an allocation failure: it
        // terminates).
        irt::basic_binary_writer<fixed_buffer<8>> w3;
        expect(w3.reserve(8));
        expect(not w3.reserve(9));
        expect(w3.ok()) << "a failed reserve is not a failure of the writer";
        w3(irt::u32{ 1 });
        expect(w3.ok());
    };

    "writer: a buffer that can not grow is a sticky failure, not an overflow"_test =
      [] {
          irt::basic_binary_writer<fixed_buffer<8>> w;

          w(irt::u64{ 0x0102030405060708ull });
          expect(w.ok());
          expect(w.size() == 8u);

          w(irt::u8{ 9 });
          expect(not w.ok());
          expect(w.size() == 8u) << "the failed write writes nothing";
          expect(w.bytes()[0] == 0x08);

          w(irt::u8{ 1 }); // ignored
          expect(w.size() == 8u);

          w.clear();
          expect(w.ok());
          w(irt::u32{ 5 });
          expect(w.ok());
          expect(w.size() == 4u);
      };

    "snapshots: save and restore in place, reuse the memory"_test = [] {
        auto live = make_world(1);

        irt::binary_writer first;
        irt::binary_writer second;

        first(live); // snapshot at step 1

        live      = make_world(9);
        live.step = 77;
        second(live); // snapshot at step 2

        // the live state diverges (bigger and smaller containers)

        live = make_world(11);
        expect(not same(live, make_world(1)));

        // rollback to the second snapshot, then to the first one

        irt::binary_reader r2(second.bytes());
        r2(live);
        expect(r2.ok());
        auto expected2 = make_world(9);
        expected2.step = 77;
        expect(same(live, expected2));

        irt::binary_reader r1(first.bytes());
        r1(live);
        expect(r1.ok());
        expect(same(live, make_world(1)));

        // the writer of a snapshot is reused: same size, no new allocation

        const auto capacity = first.capacity();
        first.clear();
        first(live);
        expect(first.ok());
        expect(first.capacity() == capacity);

        irt::binary_reader again(first.bytes());
        world              copy;
        again(copy);
        expect(again.ok());
        expect(same(copy, live));
    };

    "a dump in a file"_test = [] {
        constexpr auto magic = irt::binary_magic('I', 'R', 'T', 'D');
        const auto     in    = make_world(6);

        irt::binary_writer w;
        irt::write_header(w, magic, 1);
        w(in);
        expect(w.ok());

        auto o = irt::file::open_tmp();
        if (not o) {
            expect(false) << "open_tmp failed";
            return;
        }

        expect(irt::write_all(*o, w.bytes()));
        expect(o->flush());

        const auto content = o->read_entire_file();
        expect(static_cast<std::size_t>(content.size()) == w.size());

        irt::binary_reader r(irt::bytes_of(content));
        expect(irt::read_header(r, magic) == 1u);

        world out;
        r(out);
        expect(r.ok());
        expect(r.remaining() == 0u);
        expect(same(in, out));
    };

    "a dump in an empty or closed file is an error"_test = [] {
        auto o = irt::file::open_tmp();
        if (not o) {
            expect(false) << "open_tmp failed";
            return;
        }

        const auto content = o->read_entire_file();
        expect(content.size() == 0);

        irt::binary_reader r(irt::bytes_of(content));
        expect(irt::read_header(r, irt::binary_magic('I', 'R', 'T', 'D')) ==
               0u);
        expect(not r.ok());

        irt::file          closed;
        irt::binary_writer w;
        w(irt::u8{ 1 });
        expect(not irt::write_all(closed, w.bytes()));
        expect(irt::write_all(closed, std::span<const irt::u8>{}) == false);
    };
};
