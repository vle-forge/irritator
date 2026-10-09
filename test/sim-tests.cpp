// Copyright (c) 2025 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Tests of the binary serialization of a simulation (the public API of
// binary-simulation.hpp): rollback and reload replay the same run, hostile
// inputs, consistency of the scheduler.

#include <boost/ut.hpp>

#include "world.hpp"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <random>

namespace ut = boost::ut;

using namespace wtest;

namespace {

// corrupted inputs must not ask for gigabytes
constexpr std::size_t fuzz_limit = 64u * 1024u * 1024u;

bool load_dump(simulation& sim, const bytes& b, std::size_t limit = 0) noexcept
{
    binary_reader r(b);
    if (limit != 0)
        r.set_allocation_limit(limit);

    return read_simulation(r, sim).has_value();
}

bool load_snapshot(simulation& sim, const bytes& b, std::size_t limit = 0) noexcept
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

error_code error_of(const status& s) noexcept { return s.error(); }

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
          expect(same(snapshot(sim), at_100, "fixed point")) << "the snapshot is a fixed point";

          expect(run_steps(sim, 200));
          expect(same(dump(sim), original, "rollback")) << "same models, same observers";

          // and again, from the same snapshot

          expect(load_snapshot(sim, at_100));
          expect(run_steps(sim, 200));
          expect(same(dump(sim), original, "rollback 2"));
      };

    "snapshot: a simulation built the same way replays the same run"_test =
      [] {
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
        expect(same(dump(fresh), at_100, "dump fixed point")) << "the dump is a fixed point";
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
        expect(same(dump(a), dump(a), "twice")) << "writing does not change the simulation";
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

        expect(rnd.source_ta.buffer.data() == rnd.src_data.chunk_real.data());
        expect(rnd.source_ta.buffer.size() == 2u);
        expect(rnd.source_value.buffer.data() ==
               rnd.src_data.chunk_real.data());

        const auto* c = fresh.srcs.constant_sources.try_to_get(w.cst);
        expect(c != nullptr);
        expect(cst.source_ta.buffer.data() == c->buffer.data());
        expect(cst.source_ta.buffer.size() == 3u);

        // and the same as the original

        auto& orig = get_dyn<generator>(*sim.models.try_to_get(w.gen_cst));
        expect(cst.source_ta.index == orig.source_ta.index);
        expect(rnd.src_data.chunk_id ==
               get_dyn<generator>(*sim.models.try_to_get(w.gen_rnd))
                 .src_data.chunk_id);
    };

    "dump: file"_test = [] {
        simulation sim;
        world      w;
        build_world(sim, w);
        expect(start(sim));
        expect(run_steps(sim, 80));

        const auto tmp = std::filesystem::temp_directory_path();
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

        const auto name_m =
          (tmp / "irt-binary-simulation-test-missing.bin").string();
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

            auto& g     = get_dyn<generator>(*sim.models.try_to_get(w.gen_cst));
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

            auto* mdl = sim.models.try_to_get(w.integ);
            const auto type = mdl->type;
            mdl->type       = static_cast<dynamics_type>(250);
            expect(dump(sim).empty());
            expect(snapshot(sim).empty());
            mdl->type = type;
            expect(not dump(sim).empty());
        }

        {
            // the embedded simulations are not supported

            simulation sim;
            sim.alloc<simulation_wrapper>();
            expect(dump(sim).empty());
            expect(snapshot(sim).empty());
        }

        {
            // a list of blocks that loops

            simulation sim;
            expect(sim.nodes.can_alloc(1));
            auto& block   = sim.nodes.alloc();
            block.next    = sim.nodes.get_id(block);
            auto& port    = sim.output_ports.alloc();
            port.next     = sim.nodes.get_id(block);
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
            sim.srcs.random_sources.get(w.rnd).distribution =
              static_cast<distribution_type>(200);
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

        auto probe = [&](const bytes& original, auto&& load, auto&& write,
                         std::size_t step, u8 mask) {
            int bad = 0, accepted = 0;

            for (std::size_t i = 0; i < original.size(); i += step) {
                bytes b = original;
                b[i] ^= mask;

                if (load(target, b)) {
                    ++accepted;

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
        for (const u8 mask : { u8{ 0x01 }, u8{ 0x80 }, u8{ 0xff } }) {
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
                    const auto i  = rng() % detached.size();
                    auto*      m  = sim.models.try_to_get(detached[i]);
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
            const auto* b = fresh.models.try_to_get(id);
            ++total;
            if (not b or a.type != b->type) {
                ++different;
                continue;
            }

            dispatch(a, [&]<typename Dyn>(const Dyn& da) {
                const auto& db = get_dyn<Dyn>(*b);
                Dyn ca = da, cb = db;
#if defined(__has_builtin)
#if __has_builtin(__builtin_clear_padding)
                __builtin_clear_padding(&ca);
                __builtin_clear_padding(&cb);
                if constexpr (std::is_trivially_copyable_v<Dyn>)
                    if (std::memcmp(&ca, &cb, sizeof(Dyn)) != 0)
                        ++different;
#endif
#endif
                (void)ca;
                (void)cb;
            });
        }

        expect(total > 0_i);
        expect(different == 0_i);
    };
};

int main() {}
