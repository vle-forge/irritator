// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/binary-simulation.hpp>
#include <irritator/core.hpp>
#include <irritator/file.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>
#include <irritator/random.hpp>

#include "utils.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include <boost/ut.hpp>

using namespace irt;
using namespace std::literals;

/// A file in the temporary directory, removed at the end of the scope.
struct temp_file {
    std::filesystem::path std_path;

    explicit temp_file(const std::string_view name)
      : std_path(std::filesystem::temp_directory_path() /
                 (std::string("irritator-test-") + std::string(name)))
    {}

    ~temp_file()
    {
        std::error_code ec;
        std::filesystem::remove(std_path, ec);
    }

    irt::path get() const noexcept
    {
        return irt::path(std::string_view(std_path.string()));
    }
};

/// Writes the reals @c first, @c first + 1, ..., @c first + count - 1 in a
/// text file.
static void write_text_file(const temp_file& f, const int count) noexcept
{
    std::ofstream ofs(f.std_path);

    for (int i = 0; i < count; ++i)
        ofs << i << (i % 10 == 9 ? '\n' : ' ');
}

/// Writes the reals 0, 1, ..., count - 1 in a binary file.
static void write_binary_file(const temp_file& f, const int count) noexcept
{
    std::ofstream ofs(f.std_path, std::ios::binary);

    for (int i = 0; i < count; ++i) {
        const double v = i;
        ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }
}

/// Reads the next real of the source, updating the buffer when it is empty.
static double next(external_source& srcs, source& src, source_data& data)
{
    if (src.is_empty())
        if (auto ret = srcs.update_source(src, data); not ret)
            return -1.0;

    return src.next();
}

static irt::vector<char> read_entire_file(std::FILE* f) noexcept
{
    using namespace boost::ut;

    expect(f != nullptr) << boost::ut::fatal;

    expect(0 == std::fseek(f, 0, SEEK_END)) << boost::ut::fatal;

    const auto size = std::ftell(f);
    expect(size >= 0) << boost::ut::fatal;
    expect(0 == std::fseek(f, 0, SEEK_SET)) << boost::ut::fatal;

    irt::vector<char> buffer(size);

    const auto read_size = std::fread(buffer.data(), 1,
                                      static_cast<size_t>(size), f);
    expect(static_cast<std::size_t>(read_size) == buffer.size())
      << boost::ut::fatal;

    return buffer;
}

template<typename Dynamics>
static parameter& get_p(simulation& sim, const Dynamics& d) noexcept
{
    return sim.parameters[sim.get_id(d)];
}

int main()
{
    using namespace boost::ut;

    "constant: an empty or too long buffer is an error"_test = [] {
        source      src;
        source_data data;

        constant_source cst;
        cst.length = 0;
        expect(cst.init(src, data).has_error());

        cst.length = external_source_chunk_size + 1;
        expect(cst.init(src, data).has_error());

        const real      values[] = { 1.0, 2.0, 3.0 };
        constant_source cst3(values);
        expect(cst3.init(src, data).has_value());
        expect(eq(src.buffer.size(), 3u));
    };

    "constant: the values loop"_test = [] {
        external_source srcs(
          external_source_reserve_definition{ .constant_nb = 1 });

        const real values[] = { 1.0, 2.0, 3.0 };
        auto&      cst      = srcs.constant_sources.alloc(values);

        source      src(srcs.constant_sources.get_id(cst));
        source_data data;
        expect(cst.init(src, data).has_value());

        for (int i = 0; i < 9; ++i)
            expect(eq(next(srcs, src, data), values[i % 3]));
    };

    "text: values, incomplete last chunk and end of file"_test = [] {
        temp_file f("text-values.txt");
        write_text_file(f, 1000); // 512 + 488 values

        external_source srcs(
          external_source_reserve_definition{ .text_file_nb = 1 });
        auto& txt = srcs.text_file_sources.alloc(f.get());
        expect(srcs.prepare().has_value() >> boost::ut::fatal);

        source      src(srcs.text_file_sources.get_id(txt));
        source_data data;
        expect(txt.init(src, data).has_value() >> boost::ut::fatal);

        bool ok = true;
        for (int i = 0; i < 1000; ++i)
            ok = ok and next(srcs, src, data) == static_cast<double>(i);
        expect(ok);

        expect(src.is_empty());
        expect(srcs.update_source(src, data).has_error());
    };

    "text: a text file can not be shared"_test = [] {
        temp_file f("text-shared.txt");
        write_text_file(f, 100);

        external_source srcs(
          external_source_reserve_definition{ .text_file_nb = 1 });
        auto& txt = srcs.text_file_sources.alloc(f.get());
        expect(srcs.prepare().has_value() >> boost::ut::fatal);

        source      src1(srcs.text_file_sources.get_id(txt));
        source      src2(srcs.text_file_sources.get_id(txt));
        source_data data1, data2;

        expect(txt.init(src1, data1).has_value());
        expect(txt.init(src2, data2).has_error());

        // After the finalize and a new prepare, the file is free again.
        expect(txt.finalize(src1, data1).has_value());
        expect(srcs.prepare().has_value());
        expect(txt.init(src2, data2).has_value());
        expect(eq(src2.next(), 0.0));
    };

    "text: restore the chunk of a previous state"_test = [] {
        temp_file f("text-restore.txt");
        write_text_file(f, 2000);

        external_source srcs(
          external_source_reserve_definition{ .text_file_nb = 1 });
        auto& txt = srcs.text_file_sources.alloc(f.get());
        expect(srcs.prepare().has_value() >> boost::ut::fatal);

        source      src(srcs.text_file_sources.get_id(txt));
        source_data data;
        expect(txt.init(src, data).has_value() >> boost::ut::fatal);

        // Read 600 values: the source is in the second chunk.
        for (int i = 0; i < 600; ++i)
            (void)next(srcs, src, data);

        const auto saved_index = src.index;
        const auto saved_data  = data;

        // Continue the simulation in the third chunk.
        for (int i = 0; i < 600; ++i)
            (void)next(srcs, src, data);
        expect(eq(next(srcs, src, data), 1200.0));

        // Back in the past: the second chunk is read again.
        src.index = saved_index;
        data      = saved_data;
        expect(srcs.restore_source(src, data).has_value() >> boost::ut::fatal);
        expect(eq(next(srcs, src, data), 600.0));
        expect(eq(next(srcs, src, data), 601.0));

        // And the following chunks are read from this position.
        for (int i = 602; i < 1100; ++i)
            (void)next(srcs, src, data);
        expect(eq(next(srcs, src, data), 1100.0));
    };

    "text: missing file"_test = [] {
        external_source srcs(
          external_source_reserve_definition{ .text_file_nb = 1 });
        srcs.text_file_sources.alloc(irt::path("/irritator/does/not/exist"));
        expect(srcs.prepare().has_error());
    };

    "binary: each client reads its own chunks"_test = [] {
        temp_file f("binary-clients.bin");
        const int total = 4 * external_source_chunk_size + 100;
        write_binary_file(f, total);

        external_source srcs(
          external_source_reserve_definition{ .binary_file_nb = 1 });
        auto& bin       = srcs.binary_file_sources.alloc(f.get());
        bin.max_clients = 2;
        expect(srcs.prepare().has_value() >> boost::ut::fatal);
        expect(eq(bin.max_reals, static_cast<u64>(total)));

        const auto  id = srcs.binary_file_sources.get_id(bin);
        source      src0(id), src1(id), src2(id);
        source_data data0, data1, data2;

        expect(bin.init(src0, data0).has_value() >> boost::ut::fatal);
        expect(bin.init(src1, data1).has_value() >> boost::ut::fatal);
        expect(bin.init(src2, data2).has_error()); // only two clients

        const auto chunk = external_source_chunk_size;

        // client 0: chunks 0 and 2, client 1: chunks 1 and 3.
        bool ok = true;
        for (int i = 0; i < 2 * chunk; ++i) {
            const int c = i / chunk; // chunk of the client
            const int p = i % chunk; // position in the chunk

            ok = ok and next(srcs, src0, data0) == (2 * c) * chunk + p;
            ok = ok and next(srcs, src1, data1) == (2 * c + 1) * chunk + p;
        }
        expect(ok);

        // The fifth chunk is incomplete: no more data.
        expect(src0.is_empty());
        expect(srcs.update_source(src0, data0).has_error());
    };

    "binary: the number of clients from the size of the file"_test = [] {
        temp_file f("binary-auto.bin");
        write_binary_file(f, 3 * external_source_chunk_size);

        external_source srcs(
          external_source_reserve_definition{ .binary_file_nb = 1 });
        auto& bin       = srcs.binary_file_sources.alloc(f.get());
        bin.max_clients = 0;
        expect(srcs.prepare().has_value() >> boost::ut::fatal);
        expect(eq(bin.max_clients, 3u));
        expect(eq(bin.buffers.ssize(), 3));
    };

    "binary: a client reads at least one chunk"_test = [] {
        temp_file f("binary-small.bin");
        write_binary_file(f, external_source_chunk_size - 1);

        external_source srcs(
          external_source_reserve_definition{ .binary_file_nb = 1 });
        auto& bin       = srcs.binary_file_sources.alloc(f.get());
        bin.max_clients = 1;
        expect(srcs.prepare().has_error());

        temp_file f2("binary-small-clients.bin");
        write_binary_file(f2, 2 * external_source_chunk_size);
        bin.file_path   = f2.get();
        bin.max_clients = 3;
        expect(srcs.prepare().has_error());

        bin.max_clients = 2;
        expect(srcs.prepare().has_value());
    };

    "binary: missing file"_test = [] {
        external_source srcs(
          external_source_reserve_definition{ .binary_file_nb = 1 });
        srcs.binary_file_sources.alloc(irt::path("/irritator/does/not/exist"));
        expect(srcs.prepare().has_error());
    };

    "binary: restore the chunk of a previous state"_test = [] {
        temp_file f("binary-restore.bin");
        write_binary_file(f, 8 * external_source_chunk_size);

        external_source srcs(
          external_source_reserve_definition{ .binary_file_nb = 1 });
        auto& bin       = srcs.binary_file_sources.alloc(f.get());
        bin.max_clients = 2;
        expect(srcs.prepare().has_value() >> boost::ut::fatal);

        const auto  id = srcs.binary_file_sources.get_id(bin);
        source      src0(id), src1(id);
        source_data data0, data1;
        expect(bin.init(src0, data0).has_value() >> boost::ut::fatal);
        expect(bin.init(src1, data1).has_value() >> boost::ut::fatal);

        const auto chunk = external_source_chunk_size;
        for (int i = 0; i < chunk + 10; ++i)
            (void)next(srcs, src1, data1);

        const auto saved_index = src1.index;
        const auto saved_data  = data1;
        expect(eq(saved_data.chunk_id[1], static_cast<u64>(3 * chunk)));

        for (int i = 0; i < 2 * chunk; ++i)
            (void)next(srcs, src1, data1);

        src1.index = saved_index;
        data1      = saved_data;
        expect(srcs.restore_source(src1, data1).has_value() >>
               boost::ut::fatal);
        expect(eq(next(srcs, src1, data1), 3.0 * chunk + 10));

        // the other client is not modified.
        expect(eq(next(srcs, src0, data0), 0.0));
    };

    "random: every distribution fills the two values"_test = [] {
        struct dist {
            distribution_type type;
            real              r0, r1;
            i32               i0, i1;
        };

        const dist dists[] = {
            { distribution_type::bernouilli, 0.5, 0, 0, 0 },
            { distribution_type::binomial, 0.5, 0, 10, 0 },
            { distribution_type::cauchy, 0, 1, 0, 0 },
            { distribution_type::chi_squared, 3, 0, 0, 0 },
            { distribution_type::exponential, 1, 0, 0, 0 },
            { distribution_type::exterme_value, 0, 1, 0, 0 },
            { distribution_type::fisher_f, 2, 3, 0, 0 },
            { distribution_type::gamma, 2, 1, 0, 0 },
            { distribution_type::geometric, 0.5, 0, 0, 0 },
            { distribution_type::lognormal, 0, 1, 0, 0 },
            { distribution_type::negative_binomial, 0.5, 0, 5, 0 },
            { distribution_type::normal, 0, 1, 0, 0 },
            { distribution_type::poisson, 4, 0, 0, 0 },
            { distribution_type::student_t, 3, 0, 0, 0 },
            { distribution_type::uniform_int, 0, 0, 0, 1000 },
            { distribution_type::uniform_real, 0, 1, 0, 0 },
            { distribution_type::weibull, 1, 1, 0, 0 },
        };

        for (const auto& d : dists) {
            const real    reals[2] = { d.r0, d.r1 };
            const i32     ints[2]  = { d.i0, d.i1 };
            random_source rnd(d.type, reals, ints);

            source      src;
            source_data data;
            expect(rnd.init(0x1234u, model_id{ 5 }, 0u, src, data).has_value());

            // The second value changes: it is not left uninitialized.
            int changes = 0;
            for (int i = 0; i < 50; ++i) {
                const auto old = data.chunk_real[1];
                expect(rnd.update(src, data).has_value());
                changes += old != data.chunk_real[1];
            }

            expect(changes > 0) << "distribution" << ordinal(d.type);
        }
    };

    "random: the first values are random values"_test = [] {
        const real    reals[2] = { 10.0, 20.0 };
        const i32     ints[2]  = { 0, 0 };
        random_source rnd(distribution_type::uniform_real, reals, ints);

        source      src;
        source_data data;
        expect(rnd.init(42u, model_id{ 1 }, 0u, src, data).has_value());
        expect(eq(src.buffer.size(), 2u));

        const auto a = src.next();
        const auto b = src.next();
        expect(a >= 10.0 and a < 20.0);
        expect(b >= 10.0 and b < 20.0);
    };

    "random: reproducible and independent streams"_test = [] {
        const real    reals[2] = { 0.0, 1.0 };
        const i32     ints[2]  = { 0, 0 };
        random_source rnd(distribution_type::uniform_real, reals, ints);

        auto sequence = [&](u64 seed, u64 mdl, u64 stream) {
            source      src;
            source_data data;
            (void)rnd.init(seed, model_id{ mdl }, stream, src, data);

            std::array<real, 16> ret{};
            for (auto& v : ret) {
                if (src.is_empty())
                    (void)rnd.update(src, data);
                v = src.next();
            }
            return ret;
        };

        expect(sequence(1, 2, 0) == sequence(1, 2, 0));
        expect(sequence(1, 2, 0) != sequence(1, 2, 1)); // stream
        expect(sequence(1, 2, 0) != sequence(1, 3, 0)); // model
        expect(sequence(1, 2, 0) != sequence(7, 2, 0)); // seed
    };

    "random: fill an odd buffer"_test = [] {
        const real    reals[2] = { 0.0, 1.0 };
        const i32     ints[2]  = { 0, 0 };
        random_source rnd(distribution_type::uniform_real, reals, ints);

        source      src;
        source_data data;
        expect(rnd.init(42u, model_id{ 1 }, 0u, src, data).has_value());

        std::array<real, 6> buffer{};
        buffer.back() = -1.0;
        rnd.fill(std::span<real>(buffer.data(), 5), src, data);
        expect(eq(buffer.back(), -1.0));
        expect(buffer[4] >= 0.0 and buffer[4] < 1.0);
    };

    "simulation: generator with random ta and value"_test = [] {
        simulation sim(simulation_reserve_definition(),
                       external_source_reserve_definition{ .random_nb = 1 });

        const real reals[2] = { 0.5, 1.5 };
        const i32  ints[2]  = { 0, 0 };
        auto&      rnd      = sim.srcs.random_sources.alloc(
          distribution_type::uniform_real, reals, ints);
        const auto rnd_id = sim.srcs.random_sources.get_id(rnd);

        auto& gen = sim.alloc<generator>();
        auto& cnt = sim.alloc<counter>();
        get_p(sim, gen)
          .clear()
          .set_generator_ta(source_type::random, rnd_id)
          .set_generator_value(source_type::random, rnd_id);
        expect(sim.connect_dynamics(gen, 0, cnt, 0).has_value() >>
               boost::ut::fatal);

        sim.limits.set_bound(0, 100);
        expect(sim.srcs.prepare().has_value() >> boost::ut::fatal);
        expect(sim.initialize().has_value() >> boost::ut::fatal);

        // the two sources use independent sequences.
        expect(gen.sigma >= 0.5 and gen.sigma < 1.5);
        expect(gen.value >= 0.5 and gen.value < 1.5);
        expect(neq(gen.sigma, gen.value));

        do {
            expect(sim.run().has_value() >> boost::ut::fatal);
        } while (not sim.current_time_expired());

        // 100 time units with a mean time advance of 1.
        expect(cnt.event_number > 70 and cnt.event_number < 130);
        expect(cnt.sum_values > 0.5 * static_cast<real>(cnt.event_number));
        expect(cnt.sum_values < 1.5 * static_cast<real>(cnt.event_number));
    };

    "simulation: generators share a binary file"_test = [] {
        temp_file f("binary-sim.bin");
        write_binary_file(f, 8 * external_source_chunk_size);

        simulation sim(
          simulation_reserve_definition(),
          external_source_reserve_definition{ .binary_file_nb = 1 });
        auto& bin       = sim.srcs.binary_file_sources.alloc(f.get());
        bin.max_clients = 2;
        const auto id   = sim.srcs.binary_file_sources.get_id(bin);

        auto& gen1 = sim.alloc<generator>();
        auto& gen2 = sim.alloc<generator>();
        auto& cnt1 = sim.alloc<counter>();
        auto& cnt2 = sim.alloc<counter>();
        get_p(sim, gen1).clear().set_generator_ta(1.0).set_generator_value(
          source_type::binary_file, id);
        get_p(sim, gen2).clear().set_generator_ta(1.0).set_generator_value(
          source_type::binary_file, id);
        expect(sim.connect_dynamics(gen1, 0, cnt1, 0).has_value() >>
               boost::ut::fatal);
        expect(sim.connect_dynamics(gen2, 0, cnt2, 0).has_value() >>
               boost::ut::fatal);

        sim.limits.set_bound(0, 700);
        expect(sim.srcs.prepare().has_value() >> boost::ut::fatal);
        expect(sim.initialize().has_value() >> boost::ut::fatal);

        do {
            expect(sim.run().has_value() >> boost::ut::fatal);
        } while (not sim.current_time_expired());

        // Each generator reads its own chunks: the values of the first client
        // are 0..511 then 1024..1535, the others 512..1023 then 1536..2047.
        const auto chunk = external_source_chunk_size;
        const auto n     = cnt1.event_number;
        expect(n > chunk);
        expect(eq(cnt2.event_number, n));

        const auto expected_last = [&](int client) {
            const auto i = static_cast<int>(n) - 1;
            return static_cast<real>((2 * (i / chunk) + client) * chunk +
                                     i % chunk);
        };

        // the clients are given in the initialization order of the models.
        const auto a = expected_last(0), b = expected_last(1);
        expect((cnt1.last_value == a and cnt2.last_value == b) or
               (cnt1.last_value == b and cnt2.last_value == a));
    };

    "simulation: generator with a text file value"_test = [] {
        temp_file f("text-sim.txt");
        write_text_file(f, 1000);

        simulation sim(simulation_reserve_definition(),
                       external_source_reserve_definition{ .text_file_nb = 1 });
        auto&      txt = sim.srcs.text_file_sources.alloc(f.get());
        const auto id  = sim.srcs.text_file_sources.get_id(txt);

        auto& gen = sim.alloc<generator>();
        auto& cnt = sim.alloc<counter>();
        get_p(sim, gen).clear().set_generator_ta(1.0).set_generator_value(
          source_type::text_file, id);
        expect(sim.connect_dynamics(gen, 0, cnt, 0).has_value() >>
               boost::ut::fatal);

        sim.limits.set_bound(0, 700);
        expect(sim.srcs.prepare().has_value() >> boost::ut::fatal);
        expect(sim.initialize().has_value() >> boost::ut::fatal);

        do {
            expect(sim.run().has_value() >> boost::ut::fatal);
        } while (not sim.current_time_expired());

        // The values are 0, 1, 2... read across the chunks of the file.
        const auto n = cnt.event_number;
        expect(n > external_source_chunk_size);
        expect(eq(cnt.last_value, static_cast<real>(n - 1)));
        expect(eq(cnt.sum_values, static_cast<real>(n * (n - 1) / 2)));
    };

    "simulation: dump and load with file sources"_test = [] {
        temp_file ftxt("text-dump.txt");
        write_text_file(ftxt, 2000);
        temp_file fbin("binary-dump.bin");
        write_binary_file(fbin, 8 * external_source_chunk_size);

        const auto build = [&](simulation& sim) {
            auto& txt       = sim.srcs.text_file_sources.alloc(ftxt.get());
            auto& bin       = sim.srcs.binary_file_sources.alloc(fbin.get());
            bin.max_clients = 1;

            auto& gen_txt = sim.alloc<generator>();
            auto& gen_bin = sim.alloc<generator>();
            auto& cnt_txt = sim.alloc<counter>();
            auto& cnt_bin = sim.alloc<counter>();
            get_p(sim, gen_txt)
              .clear()
              .set_generator_ta(1.0)
              .set_generator_value(source_type::text_file,
                                   sim.srcs.text_file_sources.get_id(txt));
            get_p(sim, gen_bin)
              .clear()
              .set_generator_ta(1.0)
              .set_generator_value(source_type::binary_file,
                                   sim.srcs.binary_file_sources.get_id(bin));
            (void)sim.connect_dynamics(gen_txt, 0, cnt_txt, 0);
            (void)sim.connect_dynamics(gen_bin, 0, cnt_bin, 0);

            return std::make_pair(sim.get_id(cnt_txt), sim.get_id(cnt_bin));
        };

        const auto run_until = [](simulation& sim, real t) {
            while (sim.current_time() < t)
                if (not sim.run().has_value())
                    return false;
            return true;
        };

        const auto reserve = external_source_reserve_definition{
            .text_file_nb = 1, .binary_file_nb = 1
        };

        simulation sim(simulation_reserve_definition(), reserve);
        const auto [id_txt, id_bin] = build(sim);
        sim.limits.set_bound(0, 1500);
        expect(sim.srcs.prepare().has_value() >> boost::ut::fatal);
        expect(sim.initialize().has_value() >> boost::ut::fatal);
        expect(run_until(sim, 600.5) >> boost::ut::fatal);

        binary_writer w;
        expect(write_simulation(w, sim).has_value() >> boost::ut::fatal);
        const auto bytes = std::vector<u8>(w.bytes().begin(), w.bytes().end());

        // The original simulation continues in the next chunks.
        expect(run_until(sim, 1200.5) >> boost::ut::fatal);

        simulation    fresh(simulation_reserve_definition(), reserve);
        binary_reader r(bytes);
        expect(read_simulation(r, fresh).has_value() >> boost::ut::fatal);
        expect(run_until(fresh, 1200.5) >> boost::ut::fatal);

        for (const auto id : { id_txt, id_bin }) {
            const auto& a = get_dyn<counter>(sim.models.get(id));
            const auto& b = get_dyn<counter>(fresh.models.get(id));

            expect(a.event_number > 1100);
            expect(eq(a.event_number, b.event_number));
            expect(eq(a.last_value, b.last_value));
            expect(eq(a.sum_values, b.sum_values));
        }
    };

    "simulation: a negative ta of a queue source is an error"_test = [] {
        // @return The error of the simulation or a default error_code.
        const auto run_queue = [](auto tag, real ta) -> error_code {
            using Queue = decltype(tag);

            simulation sim(
              simulation_reserve_definition(),
              external_source_reserve_definition{ .constant_nb = 1 });

            const real values[] = { ta };
            auto&      cst      = sim.srcs.constant_sources.alloc(values);
            const auto cst_id   = sim.srcs.constant_sources.get_id(cst);

            auto& gen   = sim.alloc<generator>();
            auto& queue = sim.alloc<Queue>();
            get_p(sim, gen).clear().set_generator_ta(1.0);

            if constexpr (std::is_same_v<Queue, dynamic_queue>)
                get_p(sim, queue)
                  .set_dynamic_queue_ta(source_type::constant, cst_id);
            else
                get_p(sim, queue)
                  .set_priority_queue_ta(source_type::constant, cst_id);

            (void)sim.connect_dynamics(gen, 0, queue, 0);

            sim.limits.set_bound(0, 10);
            if (auto ret = sim.srcs.prepare(); not ret)
                return ret.error();
            if (auto ret = sim.initialize(); not ret)
                return ret.error();

            do {
                if (auto ret = sim.run(); not ret)
                    return ret.error();
            } while (not sim.current_time_expired());

            return error_code{};
        };

        const auto abnormal = error_code(simulation_errc::ta_abnormal);

        expect(run_queue(dynamic_queue{}, 2.0) == error_code{});
        expect(run_queue(dynamic_queue{}, -1.0) == abnormal);
        expect(run_queue(priority_queue{}, 2.0) == error_code{});
        expect(run_queue(priority_queue{}, -1.0) == abnormal);
    };

    "simulation: dynamic_queue is FIFO, priority_queue is ordered by date"_test =
      [] {
          struct output {
              real t;
              i64  events;
              real sum;

              bool operator==(const output&) const = default;
          };

          // A generator sends 10, 20, 30, 40 at t = 1, 2, 3, 4 and the delays
          // of the queue are 4, 1, 1, 1: the dates are 5, 3, 4, 5.
          const auto run_queue = [](auto tag, std::span<const real> delays) {
              using Queue = decltype(tag);

              simulation sim(
                simulation_reserve_definition(),
                external_source_reserve_definition{ .constant_nb = 2 });

              const real values[]   = { 10, 20, 30, 40 };
              auto&      cst_values = sim.srcs.constant_sources.alloc(values);
              auto&      cst_delays = sim.srcs.constant_sources.alloc(delays);

              auto& gen   = sim.alloc<generator>();
              auto& queue = sim.alloc<Queue>();
              auto& cnt   = sim.alloc<counter>();

              get_p(sim, gen).clear().set_generator_ta(1.0).set_generator_value(
                source_type::constant,
                sim.srcs.constant_sources.get_id(cst_values));

              const auto delay_id = sim.srcs.constant_sources.get_id(
                cst_delays);
              if constexpr (std::is_same_v<Queue, dynamic_queue>)
                  get_p(sim, queue)
                    .set_dynamic_queue_ta(source_type::constant, delay_id);
              else
                  get_p(sim, queue)
                    .set_priority_queue_ta(source_type::constant, delay_id);

              (void)sim.connect_dynamics(gen, 0, queue, 0);
              (void)sim.connect_dynamics(queue, 0, cnt, 0);

              std::vector<output> outputs;
              sim.limits.set_bound(0, 5.5);
              if (not sim.srcs.prepare() or not sim.initialize())
                  return outputs;

              do {
                  const auto events = cnt.event_number;
                  const auto sum    = cnt.sum_values;

                  if (not sim.run())
                      return outputs;

                  if (cnt.event_number != events)
                      outputs.push_back({ sim.current_time(),
                                          cnt.event_number - events,
                                          cnt.sum_values - sum });
              } while (not sim.current_time_expired());

              return outputs;
          };

          const auto dynamic = run_queue(dynamic_queue{},
                                         std::array<real, 4>{ 4, 1, 1, 1 });
          expect(dynamic == std::vector<output>{
                              { 5.0, 1, 10.0 },
                              { 5.0, 1, 20.0 },
                              { 5.0, 1, 30.0 },
                              { 5.0, 1, 40.0 },
                            });

          // A delay of zero: the messages leave immediately.
          const auto expected_zero = std::vector<output>{
              { 1.0, 1, 10.0 }, { 2.0, 1, 20.0 }, { 3.0, 1, 30.0 },
              { 4.0, 1, 40.0 }, { 5.0, 1, 10.0 },
          };
          expect(run_queue(dynamic_queue{}, std::array<real, 1>{ 0 }) ==
                 expected_zero);
          expect(run_queue(priority_queue{}, std::array<real, 1>{ 0 }) ==
                 expected_zero);

          const auto priority = run_queue(priority_queue{},
                                          std::array<real, 4>{ 4, 1, 1, 1 });
          expect(priority == std::vector<output>{
                               { 3.0, 1, 20.0 },
                               { 4.0, 1, 30.0 },
                               { 5.0, 1, 10.0 },
                               { 5.0, 1, 40.0 },
                             });
      };

    "external_source"_test = [] {
        std::error_code ec;

        auto ofs_b = irt::file::open_tmp();
        auto ofs_t = irt::file::open_tmp();
        expect(ofs_b.has_value());
        expect(ofs_t.has_value());

        std::default_random_engine gen(1234);
        std::poisson_distribution  dist(4.0);

        irt::generate_random_file(ofs_b.value().to_file(), gen, dist, 1024,
                                  irt::random_file_type::binary);

        auto str_b = read_entire_file(ofs_b.value().to_file());
        expect(str_b.size() ==
               static_cast<size_t>(1024) *
                 sizeof(std::poisson_distribution<>::result_type));

        irt::generate_random_file(ofs_t.value().to_file(), gen, dist, 1024,
                                  irt::random_file_type::text);

        auto str_t = read_entire_file(ofs_t.value().to_file());
        expect(str_t.size() > static_cast<size_t>(1024) * 2);
    };

    "random-philox-64"_test = [] {
        constexpr irt::u64 seed   = 0x1234567890123456;
        constexpr irt::u64 mdl_id = 0xffffffff00000001;
        constexpr irt::u64 step   = 0;

        irt::philox_64                            rng{ seed, mdl_id, step };
        std::uniform_real_distribution<irt::real> dist{ 0.0, 1.0 };

        auto sum = 0.0;
        for (irt::u32 i = 0; i < 1000; ++i) {
            const auto v = dist(rng);
            expect(ge(v, 0.0));
            expect(lt(v, 1.0));

            sum += v;
        }

        expect(approx(sum / 1000.0, 0.5, 1.e-1));
    };

    "random-philox-64-view"_test = [] {
        std::array<irt::u64, 6> param{
            0x1234567890123456, // seed
            0xffffffff00000001, // ordinal(model_id)
            0u,                 // step
            0u,                 // index
            0u,                 // first random number
            0u,                 // second random number
        };

        irt::philox_64_view                       rng(param);
        std::uniform_real_distribution<irt::real> dist{ 0.0, 1.0 };

        auto sum = 0.0;
        for (irt::u32 i = 0; i < 1000; ++i) {
            const auto v = dist(rng);
            expect(ge(v, 0.0));
            expect(lt(v, 1.0));

            sum += v;
        }

        expect(approx(sum / 1000.0, 0.5, 1.e-1));
    };

    "random-philo-64-compare"_test = [] {
        constexpr irt::u64 seed   = 0x1234567890123456;
        constexpr irt::u64 mdl_id = 0xffffffff00000001;
        constexpr irt::u64 step   = 0;

        irt::real sum_1 = 0.0;
        irt::real sum_2 = 0.0;

        {
            irt::philox_64                            rng{ seed, mdl_id, step };
            std::uniform_real_distribution<irt::real> dist{ 0.0, 1.0 };

            for (irt::u32 i = 0; i < 1000; ++i)
                sum_1 += dist(rng);
        }

        {
            std::array<irt::u64, 6> param{
                seed, mdl_id, step, 0u, 0u, 0u,
            };

            irt::philox_64_view                       rng(param);
            std::uniform_real_distribution<irt::real> dist{ 0.0, 1.0 };

            for (irt::u32 i = 0; i < 1000; ++i)
                sum_2 += dist(rng);
        }

        expect(eq(sum_1, sum_2));
    };

    "external-source-write"_test = [] {
        const auto temp_path = irt::temp_path_with_unlink{
            "external_source"sv
        };

        {

            irt::modeling mod;
            irt::project  pj;

            const auto compo_id = mod.ids.write([&](auto& ids) {
                expect(boost::ut::fatal(ids.can_alloc_component(1) or
                                        ids.components.reserve(1)));
                auto  component_id = ids.alloc_generic_component();
                auto& component    = ids.components[component_id];

                auto& generic = ids.generic_components.get(
                  component.id.generic_id);
                expect(boost::ut::fatal(generic.children.can_alloc(5) or
                                        generic.children.reserve(5)));

                auto& generator = generic.alloc(irt::dynamics_type::generator);
                auto& priority_queue = generic.alloc(
                  irt::dynamics_type::priority_queue);
                auto& dynamic_queue = generic.alloc(
                  irt::dynamics_type::dynamic_queue);

                const auto gen_id = generic.children.get_id(generator);
                const auto pri_id = generic.children.get_id(priority_queue);
                const auto dyn_id = generic.children.get_id(dynamic_queue);

                expect(boost::ut::fatal(component.srcs.data.can_alloc(4) or
                                        component.srcs.data.reserve(4)));
                const auto gen_src_1_id = component.srcs.data.alloc_id();
                const auto gen_src_2_id = component.srcs.data.alloc_id();
                const auto prio_src_id  = component.srcs.data.alloc_id();
                const auto dyn_src_id   = component.srcs.data.alloc_id();

                component.srcs.emplace(gen_src_1_id, irt::source_type::constant,
                                       "generator-1");

                component.srcs.emplace(gen_src_2_id, irt::source_type::constant,
                                       "generator-2");

                component.srcs.emplace(prio_src_id, irt::source_type::constant,
                                       "priority-queue");

                component.srcs.emplace(dyn_src_id, irt::source_type::constant,
                                       "dynamic-queue");

                auto&
                  gen_src_1 = component.srcs.data
                                .template get<irt::external_source_definition::
                                                source_element>(gen_src_1_id)
                                .cst;
                auto&
                  gen_src_2 = component.srcs.data
                                .template get<irt::external_source_definition::
                                                source_element>(gen_src_2_id)
                                .cst;
                auto&
                  prio_src = component.srcs.data
                               .template get<irt::external_source_definition::
                                               source_element>(prio_src_id)
                               .cst;
                auto&
                  dyn_src = component.srcs.data
                              .template get<irt::external_source_definition::
                                              source_element>(dyn_src_id)
                              .cst;

                gen_src_1.data.resize(3, 1.0);
                gen_src_2.data.resize(3, 2.0);
                prio_src.data.resize(3, 3.0);
                dyn_src.data.resize(3, 4.0);

                generic.children_parameters[gen_id].set_generator_ta(
                  gen_src_1_id);
                generic.children_parameters[gen_id].set_generator_value(
                  gen_src_2_id);
                generic.children_parameters[pri_id].set_priority_queue_ta(
                  prio_src_id);
                generic.children_parameters[dyn_id].set_dynamic_queue_ta(
                  dyn_src_id);

                mod.files.write([&](auto& fs) {
                    auto reg_id = fs.alloc_registred("temp", 0);
                    fs.registred_paths.get(reg_id).path = temp_path.sv();

                    auto dir_id                     = fs.alloc_dir(reg_id);
                    fs.dir_paths.get(dir_id).parent = reg_id;
                    fs.dir_paths.get(dir_id).path   = "test";

                    fs.create_directories(reg_id);
                    fs.create_directories(dir_id);

                    expect(fs.file_paths.can_alloc(1));

                    auto file_id = fs.alloc_file(
                      dir_id, "external-source.irt",
                      irt::file_type::component_file);

                    ids.component_file_paths[component_id].file = file_id;
                });

                return component_id;
            });

            mod.ids.read([&](const auto& ids, auto) {
                mod.files.read([&](const auto& fs, auto) {
                    expect(mod.save(ids, fs, compo_id).has_value());

                    expect(eq(ids.size(), 1u));
                    expect(eq(ids.generic_components.size(), 1u));

                    expect(boost::ut::fatal(
                      mod.save(ids, fs, compo_id).has_value()));
                });
            });
        }

        {
            irt::modeling mod;
            irt::project  pj;

            mod.files.write([&](auto& fs) {
                auto  reg_id = fs.alloc_registred("temp", 0);
                auto& reg    = fs.registred_paths.get(reg_id);
                reg.path     = temp_path.sv();
                fs.browse_registreds();
            });

            mod.ids.read([&](const auto& ids, auto) {
                expect(eq(ids.size(), 0u));
                expect(eq(ids.generic_components.size(), 0u));
            });

            expect(boost::ut::fatal(mod.fill_components().has_value()));

            mod.ids.read([&](auto& ids, auto) {
                expect(ge(ids.size(), 1u));
                expect(ge(ids.generic_components.size(), 1u));

                const auto p_id = mod.files.read(
                  [&](const irt::file_access& fs,
                      const auto /*vers*/) noexcept {
                      return fs.find_directory("test"sv);
                  });

                expect(boost::ut::fatal(irt::is_defined(p_id)));

                const auto f_id = mod.files.read(
                  [&](const irt::file_access& fs,
                      const auto /*vers*/) noexcept {
                      return fs.find_file_in_directory(p_id,
                                                       "external-source.irt");
                  });

                expect(boost::ut::fatal(irt::is_defined(f_id)));

                const auto compo_id = [&]() {
                    for (const auto id : ids)
                        if (f_id == ids.component_file_paths[id].file)
                            return id;
                    return irt::undefined<irt::component_id>();
                }();

                expect(boost::ut::fatal(is_defined(compo_id)));
                auto& component = ids.components[compo_id];

                expect(boost::ut::fatal(component.type ==
                                        irt::component_type::generic));

                auto& generic = ids.generic_components.get(
                  component.id.generic_id);
                expect(eq(generic.children.size(), 3u));
                expect(eq(component.srcs.data.size(), 4u));

                irt::external_source_definition::id esd[4]{};
                for (const auto src : component.srcs.data) {
                    if (component.srcs.data.template get<irt::name_str>(src) ==
                        "generator-1"sv) {
                        esd[0] = src;
                    } else if (component.srcs.data.template get<irt::name_str>(
                                 src) == "generator-2"sv) {
                        esd[1] = src;
                    } else if (component.srcs.data.template get<irt::name_str>(
                                 src) == "priority-queue"sv) {
                        esd[2] = src;
                    } else if (component.srcs.data.template get<irt::name_str>(
                                 src) == "dynamic-queue"sv) {
                        esd[3] = src;
                    }
                }

                for (const auto& id : esd)
                    expect(irt::is_defined(id));

                irt::child_id cids[3]{};
                for (const auto& ch : generic.children) {
                    if (ch.type == irt::child_type::model) {
                        if (ch.id.mdl_type == irt::dynamics_type::generator) {
                            cids[0] = generic.children.get_id(ch);
                        } else if (ch.id.mdl_type ==
                                   irt::dynamics_type::priority_queue) {
                            cids[1] = generic.children.get_id(ch);
                        } else if (ch.id.mdl_type ==
                                   irt::dynamics_type::dynamic_queue) {
                            cids[2] = generic.children.get_id(ch);
                        }
                    }
                }

                for (const auto& id : cids)
                    expect(irt::is_defined(id));

                mod.files.write([&](auto& fs) noexcept {
                    while (not fs.dir_paths.empty())
                        fs.remove_directory(
                          fs.dir_paths.get_id(*fs.dir_paths.begin()));
                });
            });
        }
    };
}
