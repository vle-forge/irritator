// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/ut.hpp>

#include "world.hpp"

#include <irritator/helpers.hpp>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <random>

namespace ut = boost::ut;

using namespace wtest;

namespace {

// A valid dump of the test world needs 33 KB: a corrupted input must not
// get more than a few times that (and the tests stay fast).
constexpr std::size_t fuzz_limit = 256u * 1024u;

bool load_dump(simulation& sim, const bytes& b, std::size_t limit = 0) noexcept
{
    binary_reader r(b);
    if (limit != 0)
        r.set_allocation_limit(limit);

    return read_simulation(r, sim).has_value();
}

bool load_snapshot(simulation&  sim,
                   const bytes& b,
                   std::size_t  limit = 0) noexcept
{
    binary_reader r(b);
    if (limit != 0)
        r.set_allocation_limit(limit);

    return read_snapshot(r, sim).has_value();
}

bool is_empty(const simulation& sim) noexcept
{
    return sim.models.size() == 0 and sim.observers.size() == 0 and
           sim.nodes.size() == 0 and sim.output_ports.size() == 0 and
           sim.dated_messages.size() == 0 and sim.sched.empty();
}

// The compiler assumes that a pointer to an over-aligned type is aligned (it
// folds @c address % alignment to 0): look at the address through a function
// that only sees a @c void*.
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
bool is_aligned(const void* p, std::size_t alignment) noexcept
{
    return reinterpret_cast<std::uintptr_t>(p) % alignment == 0;
}

struct alignas(64) over_aligned_column {
    double value = 0.0;
};

enum class over_aligned_id : std::uint32_t;

error_code error_of(const status& s) noexcept { return s.error(); }

/// A simulation wrapper and its embedded simulation. The original simulation
/// is a generator and two counters observed by the selections, the time advance
/// of the generator is the factor (0.5, 1 or 2): three copies. A generator of
/// the root simulation sends the time to run (1, 2, 3...) to the wrapper every
/// 5 time units (the wrapper sends its results before the next message).
struct wrapper_world {
    model_id               wrapper  = undefined<model_id>();
    model_id               best     = undefined<model_id>();
    embedded_simulation_id embedded = undefined<embedded_simulation_id>();
};

void build_wrapper_world(simulation&                        sim,
                         const simulation_wrapper::run_type run,
                         wrapper_world&                     w) noexcept
{
    debug::ensure(sim.sims.reserve(1));
    auto& e    = sim.sims.alloc();
    w.embedded = sim.sims.get_id(e);

    auto& orig = e.sim;
    orig.limits.set_bound(0, 10);
    auto& o_gen  = orig.alloc<generator>();
    auto& o_cnt1 = orig.alloc<counter>();
    auto& o_cnt2 = orig.alloc<counter>();
    orig.parameters[get_index(orig.get_id(o_gen))].clear().set_generator_ta(
      1.0);
    debug::ensure(orig.connect_dynamics(o_gen, 0, o_cnt1, 0));
    debug::ensure(orig.connect_dynamics(o_gen, 0, o_cnt2, 0));

    debug::ensure(e.factors.reserve(1) and e.selections.reserve(2));

    const auto f                  = e.factors.alloc_id();
    e.factors.get<model_id>(f)    = orig.get_id(o_gen);
    e.factors.get<name_str>(f)    = "ta";
    e.factors.get<factor_type>(f) = factor_type::fixed;
    auto& values                  = e.factors.get<fixed_factor>(f).values;
    debug::ensure(values.resize(3));
    values[0] = 0.5;
    values[1] = 1.0;
    values[2] = 2.0;

    for (const auto& [cnt, c] :
         { std::pair{ orig.get_id(o_cnt1), criteria_type::max },
           std::pair{ orig.get_id(o_cnt2), criteria_type::min_last } }) {
        const auto s                       = e.selections.alloc_id();
        e.selections.get<model_id>(s)      = cnt;
        e.selections.get<name_str>(s)      = "events";
        e.selections.get<criteria_type>(s) = c;
    }

    e.objective.method                = optimization_method::simple;
    e.objective.type                  = optimization_type::maximize;
    e.objective.simple_params.primary = *e.selections.begin();

    // the root simulation

    sim.limits.set_bound(0, 20);

    const real times[] = { 1, 2, 3, 4, 5, 6 };
    debug::ensure(sim.srcs.constant_sources.reserve(1));
    auto& cst = sim.srcs.constant_sources.alloc(times);

    auto& gen = sim.alloc<generator>();
    sim.parameters[get_index(sim.get_id(gen))]
      .clear()
      .set_generator_ta(5.0)
      .set_generator_value(source_type::constant,
                           sim.srcs.constant_sources.get_id(cst));

    auto& wrp = sim.alloc<simulation_wrapper>();
    w.wrapper = sim.get_id(wrp);
    debug::ensure(wrp.x.resize(3) and wrp.y.resize(3));

    auto& p = sim.parameters[get_index(w.wrapper)];
    p.integers[simulation_wrapper_tag::run] = ordinal(run);
    p.integers[simulation_wrapper_tag::id]  = ordinal(w.embedded);

    // with the complete mode, each message initializes and runs the copies
    if (run == simulation_wrapper::run_type::complete)
        debug::ensure(sim.connect_dynamics(gen, 0, wrp, 0));

    debug::ensure(sim.connect_dynamics(gen, 0, wrp, 1));

    for (int i = 0; i != 3; ++i) {
        auto& c = sim.alloc<counter>();
        if (i == 0)
            w.best = sim.get_id(c);

        debug::ensure(sim.connect_dynamics(wrp, i, c, 0));
    }
}

/// A chain of embedded simulations: the root simulation contains an embedded
/// simulation of depth 1 that contains an embedded simulation of depth 2...
void build_embedded_chain(simulation& sim, const int depth) noexcept
{
    simulation* current = &sim;

    for (int i = 0; i != depth; ++i) {
        debug::ensure(current->sims.reserve(1));
        current = &current->sims.alloc().sim;
    }
}

} // namespace

ut::suite<"irt::binary simulation"> simulation_suite = [] {
    using namespace ut;
    using namespace ut::literals;

    "snapshot: the run after a rollback is the run that was rolled back"_test =
      [] {
          simulation sim;
          world      w;
          build_world(sim, w);
          expect(start(sim));
          expect(run_steps(sim, 100));

          const auto at_100 = snapshot(sim);
          expect(not at_100.empty());

          expect(run_steps(sim, 200));
          const auto original = dump(sim);
          expect(not original.empty());

          // the same simulation, back in time

          expect(load_snapshot(sim, at_100));
          expect(same(snapshot(sim), at_100, "fixed point"))
            << "the snapshot is a fixed point";

          expect(run_steps(sim, 200));
          expect(same(dump(sim), original, "rollback"))
            << "same models, same observers";

          // and again, from the same snapshot

          expect(load_snapshot(sim, at_100));
          expect(run_steps(sim, 200));
          expect(same(dump(sim), original, "rollback 2"));
      };

    "snapshot: a simulation built the same way replays the same run"_test = [] {
        simulation a;
        world      wa;
        build_world(a, wa);
        expect(start(a));
        expect(run_steps(a, 100));
        const auto at_100 = snapshot(a);
        expect(run_steps(a, 200));

        simulation b;
        world      wb;
        build_world(b, wb);
        expect(start(b));
        expect(run_steps(b, 7)); // another time, other models state

        expect(load_snapshot(b, at_100));
        expect(run_steps(b, 200));
        expect(same(dump(a), dump(b), "built the same way"));
    };

    "dump: a new simulation replays the same run"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        expect(start(sim));
        expect(run_steps(sim, 100));

        const auto at_100 = dump(sim);
        expect(not at_100.empty());
        expect(run_steps(sim, 200));

        simulation fresh;
        expect(load_dump(fresh, at_100));
        expect(same(dump(fresh), at_100, "dump fixed point"))
          << "the dump is a fixed point";
        expect(fresh.current_time() == sim.limits.begin() or
               fresh.current_time() > 0.0);

        expect(run_steps(fresh, 200));
        expect(same(dump(fresh), dump(sim), "fresh"));
    };

    "dump: the models keep their identifiers and the next identifiers"_test =
      [] {
          simulation sim;
          world      w;
          build_world(sim, w);
          expect(start(sim));
          expect(run_steps(sim, 50));

          // a hole in the models

          sim.deallocate(w.cnt_hsm);
          const auto bytes_with_hole = dump(sim);
          expect(not bytes_with_hole.empty());

          simulation fresh;
          expect(load_dump(fresh, bytes_with_hole));

          expect(fresh.models.exists(w.integ));
          expect(fresh.models.exists(w.hsm_model));
          expect(not fresh.models.exists(w.cnt_hsm));
          expect(fresh.models.size() == sim.models.size());

          auto& a = sim.alloc<counter>();
          auto& b = fresh.alloc<counter>();
          expect(sim.get_id(a) == fresh.get_id(b))
            << "the next identifier is the one of the original";
      };

    "dump: two identical runs give identical bytes"_test = [] {
        simulation a, b;
        world      wa, wb;
        build_world(a, wa);
        build_world(b, wb);
        expect(start(a) and start(b));
        expect(run_steps(a, 120) and run_steps(b, 120));

        expect(same(dump(a), dump(b), "identical runs"));
        expect(same(snapshot(a), snapshot(b), "identical runs, snapshot"));
        expect(same(dump(a), dump(a), "twice"))
          << "writing does not change the simulation";
    };

    "dump: the buffers of the sources are bound again"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        expect(start(sim));
        expect(run_steps(sim, 60));

        simulation fresh;
        expect(load_dump(fresh, dump(sim)));

        auto* gr = fresh.models.try_to_get(w.gen_rnd);
        auto* gc = fresh.models.try_to_get(w.gen_cst);
        expect(gr != nullptr and gc != nullptr);

        auto& rnd = get_dyn<generator>(*gr);
        auto& cst = get_dyn<generator>(*gc);

        // each source of the generator owns its state
        expect(rnd.source_ta.buffer.data() ==
               rnd.src_data_ta.chunk_real.data());
        expect(rnd.source_ta.buffer.size() == 2u);
        expect(rnd.source_value.buffer.data() ==
               rnd.src_data_value.chunk_real.data());

        const auto* c = fresh.srcs.constant_sources.try_to_get(w.cst);
        expect(c != nullptr);
        expect(cst.source_ta.buffer.data() == c->buffer.data());
        expect(cst.source_ta.buffer.size() == 3u);

        // and the same as the original

        auto& orig = get_dyn<generator>(*sim.models.try_to_get(w.gen_cst));
        expect(cst.source_ta.index == orig.source_ta.index);
        auto& orig_rnd = get_dyn<generator>(*sim.models.try_to_get(w.gen_rnd));
        expect(rnd.src_data_ta.chunk_id == orig_rnd.src_data_ta.chunk_id);
        expect(rnd.src_data_value.chunk_id == orig_rnd.src_data_value.chunk_id);
    };

    "dump: file"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        expect(start(sim));
        expect(run_steps(sim, 80));

        const auto tmp    = std::filesystem::temp_directory_path();
        const auto name_p = (tmp / "irt-binary-simulation-test.bin").string();
        const path p(name_p);
        expect(save_simulation(p, sim).has_value());

        simulation fresh;
        expect(load_simulation(p, fresh).has_value());
        expect(same(dump(fresh), dump(sim), "file"));

        simulation limited;
        const auto ret = load_simulation(p, limited, 64);
        expect(not ret.has_value()) << "allocation limit";
        expect(is_empty(limited));

        const auto name_m = (tmp / "irt-binary-simulation-test-missing.bin")
                              .string();
        const path missing(name_m);
        simulation nothing;
        expect(not load_simulation(missing, nothing).has_value());
    };

    "refuse to write an inconsistent simulation"_test = [] {
        {
            simulation sim;
            world      w;
            build_world(sim, w);
            expect(start(sim));

            auto* mdl = sim.models.try_to_get(w.gain);
            expect(mdl != nullptr);

            // the handle of a model must designate its own node

            const auto handle = mdl->handle;
            mdl->handle       = 4000u;
            binary_writer wr;
            const auto    ret = write_snapshot(wr, sim);
            expect(not ret.has_value());
            expect(error_of(ret) == make_error(simulation_errc::range_error));
            mdl->handle = handle;

            expect(not dump(sim).empty());

            // an observer must point to its own model

            auto* a = sim.models.try_to_get(w.cnt_cst);
            auto* b = sim.models.try_to_get(w.cnt_queue);
            std::swap(a->obs_id, b->obs_id);
            expect(dump(sim).empty());
            std::swap(a->obs_id, b->obs_id);
            expect(not dump(sim).empty());

            // a model that references something that does not exist

            auto& g = get_dyn<generator>(*sim.models.try_to_get(w.gen_cst));
            const auto old = g.y[0];
            g.y[0]         = static_cast<output_port_id>(0x700000009ull);
            expect(dump(sim).empty());
            g.y[0] = old;
            expect(not dump(sim).empty());
        }

        {
            // a value the writer can not write

            simulation sim;
            world      w;
            build_world(sim, w);
            expect(start(sim));

            sim.srcs.constant_sources.get(w.cst).length = 513;
            expect(dump(sim).empty()) << "more values than the buffer holds";
            sim.srcs.constant_sources.get(w.cst).length = 3;
            expect(not dump(sim).empty());

            auto*      mdl  = sim.models.try_to_get(w.integ);
            const auto type = mdl->type;
            mdl->type       = static_cast<dynamics_type>(250);
            expect(dump(sim).empty());
            expect(snapshot(sim).empty());
            mdl->type = type;
            expect(not dump(sim).empty());
        }

        {
            // a list of blocks that loops

            simulation sim;
            expect(sim.nodes.can_alloc(1));
            auto& block = sim.nodes.alloc();
            block.next  = sim.nodes.get_id(block);
            auto& port  = sim.output_ports.alloc();
            port.next   = sim.nodes.get_id(block);
            expect(dump(sim).empty());

            block.next = undefined<block_node_id>();
            expect(not dump(sim).empty());
        }
    };

    "the reader refuses what the writer lets through"_test = [] {
        auto refused = [](auto&& mutate) {
            simulation sim;
            world      w;
            build_world(sim, w);
            expect(start(sim));
            expect(run_steps(sim, 20));
            mutate(sim, w);

            const auto b = dump(sim);
            expect(not b.empty());

            simulation fresh;
            expect(not load_dump(fresh, b));
            expect(is_empty(fresh));
            expect(fresh.srcs.random_sources.size() == 0u);
        };

        refused([](simulation& sim, world& w) {
            sim.hsms.get(w.machine).states[1].if_transition = 254;
        });

        refused([](simulation& sim, world& w) {
            get_dyn<hsm_wrapper>(*sim.models.try_to_get(w.hsm_model))
              .exec.messages = 9;
        });

        refused([](simulation& sim, world& w) {
            get_dyn<hsm_wrapper>(*sim.models.try_to_get(w.hsm_model))
              .exec.current_state = 254;
        });

        refused([](simulation& sim, world& w) {
            sim.srcs.random_sources.get(w.rnd)
              .distribution = static_cast<distribution_type>(200);
        });

        refused([](simulation& sim, world& w) {
            get_dyn<generator>(*sim.models.try_to_get(w.gen_cst))
              .source_ta.type = static_cast<source_type>(9);
        });

        refused([](simulation& sim, world& w) {
            // a source that views a buffer that does not exist
            get_dyn<generator>(*sim.models.try_to_get(w.gen_cst))
              .source_ta.id = constant_source_id{ 0x30007u };
        });

        refused([](simulation& sim, world& w) {
            // the position is outside of the buffer
            get_dyn<generator>(*sim.models.try_to_get(w.gen_cst))
              .source_ta.index = 44;
        });
    };

    "truncated input: every strict prefix is refused and leaves nothing"_test =
      [] {
          simulation sim;
          world      w;
          build_world(sim, w);
          expect(start(sim));
          expect(run_steps(sim, 60));

          const auto d = dump(sim);
          const auto s = snapshot(sim);
          expect(not d.empty() and not s.empty());

          simulation target;
          world      wt;
          build_world(target, wt);
          expect(start(target));

          int accepted = 0;
          for (std::size_t n = 0; n != d.size(); ++n) {
              const bytes prefix(d.begin(), d.begin() + static_cast<long>(n));
              if (load_dump(target, prefix) or not is_empty(target))
                  ++accepted;
          }
          expect(accepted == 0_i) << "prefixes of the dump";

          accepted = 0;
          for (std::size_t n = 0; n != s.size(); ++n) {
              const bytes prefix(s.begin(), s.begin() + static_cast<long>(n));
              if (load_snapshot(target, prefix) or not is_empty(target))
                  ++accepted;
          }
          expect(accepted == 0_i) << "prefixes of the snapshot";

          // the reader stops after the record

          bytes more = d;
          more.push_back(0xAA);
          more.push_back(0xBB);
          binary_reader r(more);
          expect(read_simulation(r, target).has_value());
          expect(r.remaining() == 2_u);
      };

    "corrupted input: refused, or accepted as a consistent state"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        expect(start(sim));
        expect(run_steps(sim, 60));

        const auto d = dump(sim);
        const auto s = snapshot(sim);

        simulation target, other;

        const bool full = std::getenv("IRT_FUZZ_FULL") != nullptr;

        auto probe = [&](const bytes& original, auto&& load, auto&& write,
                         std::size_t step, u8 mask) {
            int bad = 0, accepted = 0;

            for (std::size_t i = 0; i < original.size(); i += step) {
                bytes b = original;
                b[i] ^= mask;

                if (load(target, b)) {
                    ++accepted;

                    // the fixed point costs three more passes: one accepted
                    // state out of four by default, all with IRT_FUZZ_FULL.
                    if (not full and accepted % 4 != 1)
                        continue;

                    // a state that is accepted is a state that can be written
                    // and read again, and that is a fixed point.

                    const auto again = write(target);
                    if (again.empty() or not load(other, again) or
                        write(other) != again)
                        ++bad;
                } else if (not is_empty(target)) {
                    ++bad;
                }
            }

            return std::pair{ bad, accepted };
        };

        auto load_d = [](simulation& sim_, const bytes& b) {
            return load_dump(sim_, b, fuzz_limit);
        };
        auto load_s = [](simulation& sim_, const bytes& b) {
            return load_snapshot(sim_, b, fuzz_limit);
        };
        auto write_d = [](const simulation& sim_) { return dump(sim_); };
        auto write_s = [](const simulation& sim_) { return snapshot(sim_); };

        // the snapshot is read in a simulation of the same shape

        build_world(other, *std::make_unique<world>());
        expect(start(other));
        build_world(target, *std::make_unique<world>());
        expect(start(target));

        int bad = 0, accepted = 0;
        // Two masks by default (the lowest bit, then every bit). Set the
        // environment variable IRT_FUZZ_FULL for an exhaustive run.
        const std::vector<u8> masks = full ? std::vector<u8>{ 0x01, 0x80, 0xff }
                                           : std::vector<u8>{ 0x01, 0xff };

        for (const u8 mask : masks) {
            auto [b1, a1] = probe(d, load_d, write_d, 1, mask);
            auto [b2, a2] = probe(s, load_s, write_s, 1, mask);
            bad += b1 + b2;
            accepted += a1 + a2;
        }

        expect(bad == 0_i);
        std::printf("corrupted inputs: %d accepted as a consistent state\n",
                    accepted);
    };

    "allocation limit"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        expect(start(sim));
        expect(run_steps(sim, 30));
        const auto d = dump(sim);

        simulation target;
        expect(not load_dump(target, d, 100));
        expect(is_empty(target));
        expect(load_dump(target, d, 100u * 1024u * 1024u));
        expect(same(dump(target), d, "limit"));
    };

    "rollback to several points, in any order"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        expect(start(sim));

        std::vector<bytes> snaps, finals;
        for (int i = 0; i != 6; ++i) {
            expect(run_steps(sim, 40));
            snaps.push_back(snapshot(sim));
        }

        // what each snapshot leads to, 50 steps after

        for (const auto& sn : snaps) {
            expect(load_snapshot(sim, sn));
            expect(run_steps(sim, 50));
            finals.push_back(dump(sim));
        }

        std::mt19937 rng(7);
        for (int k = 0; k != 30; ++k) {
            const auto i = rng() % snaps.size();
            expect(load_snapshot(sim, snaps[i]));
            expect(run_steps(sim, 50));
            expect(same(dump(sim), finals[i], "any order"));
        }
    };

    "heap: any legal sequence of scheduler operations gives a state the "
    "writer accepts and the reader restores"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);

        std::vector<model_id> ids;
        for (int i = 0; i != 150; ++i)
            ids.push_back(sim.get_id(sim.alloc<irt::constant>()));

        expect(start(sim));

        std::mt19937                           rng(42);
        std::uniform_real_distribution<double> dist(0.0, 50.0);
        irt::vector<model_id>                  popped;
        std::vector<model_id>                  detached;
        int                                    refused = 0, bad = 0;

        simulation other;
        build_world(other, *std::make_unique<world>());

        for (int step = 0; step != 4000; ++step) {
            const auto k = rng() % 6;

            if (k == 0 or k == 1) { // update a model in the tree
                auto id = ids[rng() % ids.size()];
                if (auto* m = sim.models.try_to_get(id);
                    m and sim.sched.is_in_tree(m->handle)) {
                    // the preconditions of the scheduler compare with
                    // model::tn, so keep it above any new date.
                    const auto tn = dist(rng);
                    m->tn         = 1e9;
                    if (tn < sim.sched.tn(m->handle))
                        sim.sched.decrease(*m, tn);
                    else
                        sim.sched.increase(*m, tn);
                    m->tn = tn;
                }
            } else if (k == 2) { // remove
                auto id = ids[rng() % ids.size()];
                if (auto* m = sim.models.try_to_get(id);
                    m and sim.sched.is_in_tree(m->handle)) {
                    sim.sched.remove(*m);
                    detached.push_back(id);
                }
            } else if (k == 3 or k == 4) { // reintegrate
                if (not detached.empty()) {
                    const auto i = rng() % detached.size();
                    auto*      m = sim.models.try_to_get(detached[i]);
                    if (m) {
                        m->tn = dist(rng);
                        sim.sched.reintegrate(*m, m->tn);
                    }
                    detached[i] = detached.back();
                    detached.pop_back();
                }
            } else { // pop all the earliest
                popped.clear();
                sim.sched.pop(popped);
                for (auto id : popped)
                    detached.push_back(id);
            }

            if (step % 7 == 0) {
                const auto d = dump(sim);
                if (d.empty()) {
                    ++refused;
                    continue;
                }

                if (not load_dump(other, d) or dump(other) != d)
                    ++bad;
            }
        }

        expect(refused == 0_i) << "the writer refuses a legal heap";
        expect(bad == 0_i) << "the reader rejects or changes a legal heap";
    };

    "dynamics: every byte of every dynamics is written"_test = [] {
        // The reader of a dump refuses nothing here: it is the writer that
        // must not omit a byte. A model whose bytes are all set to a
        // pattern must read back with the same bytes, padding excepted.

        simulation sim;
        world      w;
        build_world(sim, w);
        expect(start(sim));
        expect(run_steps(sim, 20));

        const auto d = dump(sim);
        expect(not d.empty());

        simulation fresh;
        expect(load_dump(fresh, d));

        // every model, byte by byte, with the padding cleared

        int different = 0, total = 0;
        for (const model* pa = nullptr; sim.models.next(pa);) {
            const model& a  = *pa;
            const auto   id = sim.models.get_id(a);
            const auto*  b  = fresh.models.try_to_get(id);
            ++total;
            if (not b or a.type != b->type) {
                ++different;
                continue;
            }

            dispatch(a, [&]<typename Dyn>(const Dyn& da) {
                const auto& db = get_dyn<Dyn>(*b);
                // Padding, and what the constructor leaves uninitialized,
                // differ between a construction in a storage filled with
                // 0x00 and one filled with 0xFF: those bytes are not
                // compared. Raw bytes: the copy constructors of the dynamics
                // skip fields.
                alignas(Dyn) unsigned char z0[sizeof(Dyn)], z1[sizeof(Dyn)];
                std::memset(z0, 0x00, sizeof(Dyn));
                std::memset(z1, 0xff, sizeof(Dyn));
                new (z0) Dyn();
                new (z1) Dyn();

                const auto* pa = reinterpret_cast<const unsigned char*>(&da);
                const auto* pb = reinterpret_cast<const unsigned char*>(&db);
                // the span of a source points into the buffers of its own
                // simulation: the addresses differ.
                bool skip[sizeof(Dyn)] = {};
                auto mark              = [&](const auto& src) {
                    const auto off = static_cast<std::size_t>(
                      reinterpret_cast<const unsigned char*>(&src) -
                      reinterpret_cast<const unsigned char*>(&da));
                    for (std::size_t k = 0; k != sizeof(src.buffer); ++k)
                        skip[off + k] = true;
                };
                if constexpr (requires { da.source_ta.buffer; })
                    mark(da.source_ta);
                if constexpr (requires { da.source_value.buffer; })
                    mark(da.source_value);

                for (std::size_t o = 0; o != sizeof(Dyn); ++o) {
                    if (not skip[o] and z0[o] == z1[o] and pa[o] != pb[o]) {
                        ++different;
                        break;
                    }
                }
            });
        }

        expect(total > 0_i);
        expect(different == 0_i);
    };

    "memory: the observers respect their alignment (shared_buffer is "
    "aligned on 64 bytes)"_test = [] {
        // Regression test: data_array allocated its items with the default
        // alignment (16) and clang -O2 used aligned stores to build the
        // observers.

        for (const int capacity : { 8, 100, 1000, 5000 }) {
            simulation sim;
            expect(sim.observers.reserve(capacity));

            int misaligned = 0;
            for (int i = 0; i != 4; ++i) {
                auto& mdl = sim.alloc<irt::constant>();
                expect(sim.observe(sim.models.get(sim.get_id(mdl)), 1.0)
                         .has_value());
            }

            for (const auto& obs : sim.observers)
                if (not is_aligned(&obs, alignof(observer)))
                    ++misaligned;

            expect(misaligned == 0_i) << "capacity" << capacity;
        }
    };

    "memory: the extra columns of an id_data_array respect their alignment"_test =
      [] {
          using array_type = id_data_array<
            int, over_aligned_id, allocator<new_delete_memory_resource>,
            over_aligned_column>;

          for (const int capacity : { 8, 100, 5000 }) {
              array_type  array(capacity);
              const void* base = array.get<over_aligned_column>().get();
              expect(is_aligned(base, alignof(over_aligned_column)))
                << "reserve" << capacity;

              // growing reallocates the columns
              expect(array.reserve(capacity * 2 + 1));
              base = array.get<over_aligned_column>().get();
              expect(is_aligned(base, alignof(over_aligned_column)))
                << "grow" << capacity;
          }
      };
};

ut::suite<"irt::binary embedded simulations"> embedded_suite = [] {
    using namespace ut;
    using namespace ut::literals;

    // A dump during the simulation: the loaded simulation runs as the
    // original simulation.
    for (const auto run : { simulation_wrapper::run_type::complete,
                            simulation_wrapper::run_type::until }) {
        const auto name = std::string(
                            "dump: the embedded simulations continue (") +
                          std::string(
                            simulation_wrapper::run_type_names[ordinal(run)]) +
                          ")";

        test(name) = [run] {
            simulation    sim;
            wrapper_world w;
            build_wrapper_world(sim, run, w);
            expect(start(sim) >> ut::fatal);
            expect(run_steps(sim, 7) >> ut::fatal);

            const auto middle = dump(sim);
            expect(not middle.empty() >> ut::fatal);

            simulation fresh;
            expect(load_dump(fresh, middle) >> ut::fatal);
            expect(same(dump(fresh), middle, "embedded fixed point"));

            const auto& e = *fresh.sims.try_to_get(w.embedded);
            expect(eq(e.embedded_sims.size(), 3u));
            expect(eq(e.input_parameters.size(), 1u));

            // with the until mode, the copies are dumped during their run
            if (run == simulation_wrapper::run_type::until) {
                const auto& sims    = e.embedded_sims.get<simulation>();
                auto        running = 0;

                for (const auto id : e.embedded_sims) {
                    const auto t = sims[get_index(id)].current_time();
                    running += (0.0 < t and t < 10.0) ? 1 : 0;
                }

                expect(running > 0);
            }

            expect(run_steps(sim, 10));
            expect(run_steps(fresh, 10));
            expect(same(dump(fresh), dump(sim), "embedded continuation"));

            const auto& a = get_dyn<counter>(sim.models.get(w.best));
            const auto& b = get_dyn<counter>(fresh.models.get(w.best));
            expect(a.event_number > 0);
            expect(eq(a.event_number, b.event_number));
            expect(eq(a.sum_values, b.sum_values));
        };
    }

    "snapshot: rollback with embedded simulations"_test = [] {
        simulation    sim;
        wrapper_world w;
        build_wrapper_world(sim, simulation_wrapper::run_type::until, w);
        expect(start(sim) >> ut::fatal);
        expect(run_steps(sim, 5) >> ut::fatal);

        const auto at_5 = snapshot(sim);
        expect(not at_5.empty() >> ut::fatal);

        expect(run_steps(sim, 10));
        const auto original = dump(sim);

        expect(load_snapshot(sim, at_5) >> ut::fatal);
        expect(same(snapshot(sim), at_5, "embedded snapshot fixed point"));
        expect(run_steps(sim, 10));
        expect(same(dump(sim), original, "embedded rollback"));
    };

    "dump: the depth of the embedded simulations is limited"_test = [] {
        simulation deepest;
        build_embedded_chain(deepest, embedded_simulation_max_depth);

        const auto b = dump(deepest);
        expect(not b.empty());

        simulation fresh;
        expect(load_dump(fresh, b));

        simulation too_deep;
        build_embedded_chain(too_deep, embedded_simulation_max_depth + 1);
        expect(dump(too_deep).empty());
        expect(snapshot(too_deep).empty());
    };

    "dump: refuse an inconsistent simulation wrapper"_test = [] {
        {
            // an embedded simulation that does not exist

            simulation sim;
            auto&      wrp = sim.alloc<simulation_wrapper>();
            expect(not dump(sim).empty());

            wrp.sim_id = embedded_simulation_id{ 0x10001u };
            expect(dump(sim).empty());
            expect(snapshot(sim).empty());
        }

        {
            // the ports do not match the embedded simulation

            simulation    sim;
            wrapper_world w;
            build_wrapper_world(sim, simulation_wrapper::run_type::until, w);
            expect(start(sim) >> ut::fatal);
            expect(not dump(sim).empty());

            auto& wrp = get_dyn<simulation_wrapper>(sim.models.get(w.wrapper));
            expect(wrp.x.resize(4));
            expect(dump(sim).empty());
            expect(wrp.x.resize(3));

            auto& e = *sim.sims.try_to_get(w.embedded);
            e.selections.get<criteria_type>(
              *e.selections.begin()) = static_cast<criteria_type>(42);
            expect(dump(sim).empty());
        }
    };

    "dump: an embedded simulation in an untrusted input"_test = [] {
        simulation    sim;
        wrapper_world w;
        build_wrapper_world(sim, simulation_wrapper::run_type::until, w);
        expect(start(sim) >> ut::fatal);
        expect(run_steps(sim, 7) >> ut::fatal);

        const auto b = dump(sim);
        expect(not b.empty() >> ut::fatal);

        // every truncated input is refused and leaves an empty simulation
        for (std::size_t len = 0; len < b.size(); len += 97) {
            const bytes cut(b.begin(),
                            b.begin() + static_cast<std::ptrdiff_t>(len));

            simulation fresh;
            expect(not load_dump(fresh, cut)) << "length" << len;
            expect(is_empty(fresh) and fresh.sims.size() == 0u);
        }

        // flipped bytes never crash
        std::mt19937 rng(42);
        for (int i = 0; i != 2000; ++i) {
            auto corrupted = b;
            corrupted[rng() % corrupted.size()] ^= static_cast<u8>(1 +
                                                                   rng() % 255);

            simulation fresh;
            (void)load_dump(fresh, corrupted, 64u * 1024u * 1024u);
        }
    };
};

int main() {}
