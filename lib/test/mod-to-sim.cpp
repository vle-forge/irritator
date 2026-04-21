// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/dot-parser.hpp>
#include <irritator/error.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <filesystem>
#include <numeric>

#include <boost/ut.hpp>

#include <fmt/format.h>

using namespace std::literals;

irt::expected<irt::graph> irt_parse_dot_buffer(irt::modeling&   mod,
                                               std::string_view str)
{
    return mod.files.read(
      [&](const auto& files, auto) -> irt::expected<irt::graph> {
          return mod.ids.read(
            [&](const auto& ids, auto) -> irt::expected<irt::graph> {
                return irt::parse_dot_buffer(files, ids, str, mod.journal);
            });
      });
}

irt::expected<irt::vector<char>> irt_write_dot_buffer(const irt::modeling& mod,
                                                      const irt::graph& graph)
{
    return mod.files.read(
      [&](const auto& files, auto) -> irt::expected<irt::vector<char>> {
          return mod.ids.read(
            [&](const auto& ids, auto) -> irt::expected<irt::vector<char>> {
                return irt::write_dot_buffer(files, ids, graph);
            });
      });
}

static auto get_connection_number(const irt::simulation& sim) noexcept
{
    const auto block_node_number =
      std::accumulate(sim.nodes.begin(),
                      sim.nodes.end(),
                      std::size_t(0u),
                      [](std::size_t acc, const irt::block_node& elem) {
                          return acc + elem.nodes.size();
                      });

    const auto output_port_number =
      std::accumulate(sim.output_ports.begin(),
                      sim.output_ports.end(),
                      std::size_t(0u),
                      [](std::size_t acc, const irt::output_port& port) {
                          return acc + port.connections.size();
                      });

    return block_node_number + output_port_number;
}

static auto get_input_connection_number(const irt::simulation& sim,
                                        const irt::model_id    id,
                                        const int              port) noexcept
{
    std::size_t sum = 0;

    for (const auto& block : sim.nodes) {
        sum += std::ranges::count_if(block.nodes, [&](const irt::node& elem) {
            return id == elem.model and port == elem.port_index;
        });
    }

    for (const auto& y : sim.output_ports) {
        sum += std::ranges::count_if(y.connections, [&](const irt::node& elem) {
            return id == elem.model and port == elem.port_index;
        });
    }

    return sum;
}

// static auto display_connection(
//   const irt::data_array<irt::block_node, irt::block_node_id>& data) noexcept
// {
//     fmt::print("{0:-^{1}}\n", "", 70);
//     for (const auto& elem : data) {
//         fmt::print("block ({})\n", elem.nodes.size());
//         for (const auto subelem : elem.nodes) {
//             fmt::print("  mdl: {} to port {}\n",
//                        irt::get_index(subelem.model),
//                        subelem.port_index);
//         }
//     }
//     fmt::print("{0:-^{1}}\n", "", 70);
// }

// template<typename T>
// static auto write_dot_graph(const irt::simulation& sim, T suffix) noexcept
// {
//     if (std::ofstream ofs(fmt::format("file-{}", suffix)); ofs.is_open()) {
//         irt::write_dot_graph_simulation(ofs, sim);
//         ofs.close();
//     }
// }

template<std::size_t length>
static bool get_temp_registred_path(irt::small_string<length>& str) noexcept
{
    std::error_code ec;

    try {
        auto p = std::filesystem::temp_directory_path(ec) / "reg-temp";
        str    = p.string();
        return true;
    } catch (...) {
        return false;
    }
}

int main()
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    using namespace boost::ut;

    "internal-component"_test = [] {
        {
            irt::journal_handler                     jnl;
            irt::modeling                            mod(jnl);
            irt::project                             pj;
            irt::small_vector<irt::component_id, 17> components;

            mod.ids.write([&](auto& ids) {
                for (auto i = 0; i < 17; ++i) {
                    auto c_id = ids.alloc_generic_component();

                    expect(
                      ids.copy(irt::enum_cast<irt::internal_component>(i), c_id)
                        .has_value());

                    components.push_back(c_id);
                }

                for (auto i = 0; i < 17; ++i) {
                    pj.clear();

                    expect(pj.set(mod, components[i]).has_value());
                    pj.sim.limits.set_bound(0, 20);

                    expect(pj.sim.initialize().has_value());

                    do {
                        expect(pj.sim.run().has_value());
                    } while (not pj.sim.current_time_expired());
                }
            });
        }
    };

    "external-source-write"_test = [] {
        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            const auto compo_id = mod.ids.write([&](auto& ids) {
                expect(fatal(ids.can_alloc_component(1) or
                             ids.components.reserve(1)));
                auto  component_id = ids.alloc_generic_component();
                auto& component    = ids.components[component_id];

                auto& generic =
                  ids.generic_components.get(component.id.generic_id);
                expect(fatal(generic.children.can_alloc(5) or
                             generic.children.reserve(5)));

                auto& generator = generic.alloc(irt::dynamics_type::generator);
                auto& priority_queue =
                  generic.alloc(irt::dynamics_type::priority_queue);
                auto& dynamic_queue =
                  generic.alloc(irt::dynamics_type::dynamic_queue);

                const auto gen_id = generic.children.get_id(generator);
                const auto pri_id = generic.children.get_id(priority_queue);
                const auto dyn_id = generic.children.get_id(dynamic_queue);

                expect(fatal(component.srcs.data.can_alloc(4) or
                             component.srcs.data.reserve(4)));
                const auto gen_src_1_id = component.srcs.data.alloc_id();
                const auto gen_src_2_id = component.srcs.data.alloc_id();
                const auto prio_src_id  = component.srcs.data.alloc_id();
                const auto dyn_src_id   = component.srcs.data.alloc_id();

                component.srcs.emplace(
                  gen_src_1_id, irt::source_type::constant, "generator-1");

                component.srcs.emplace(
                  gen_src_2_id, irt::source_type::constant, "generator-2");

                component.srcs.emplace(
                  prio_src_id, irt::source_type::constant, "priority-queue");

                component.srcs.emplace(
                  dyn_src_id, irt::source_type::constant, "dynamic-queue");

                auto& gen_src_1 =
                  component.srcs.data
                    .template get<
                      irt::external_source_definition::source_element>(
                      gen_src_1_id)
                    .cst;
                auto& gen_src_2 =
                  component.srcs.data
                    .template get<
                      irt::external_source_definition::source_element>(
                      gen_src_2_id)
                    .cst;
                auto& prio_src =
                  component.srcs.data
                    .template get<
                      irt::external_source_definition::source_element>(
                      prio_src_id)
                    .cst;
                auto& dyn_src =
                  component.srcs.data
                    .template get<
                      irt::external_source_definition::source_element>(
                      dyn_src_id)
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

                irt::registred_path_str temp_path;
                expect(fatal(get_temp_registred_path(temp_path)));

                mod.files.write([&](auto& fs) {
                    auto reg_id = fs.alloc_registred("temp", 0);
                    fs.registred_paths.get(reg_id).path = temp_path;

                    auto dir_id                     = fs.alloc_dir(reg_id);
                    fs.dir_paths.get(dir_id).parent = reg_id;
                    fs.dir_paths.get(dir_id).path   = "test";

                    fs.create_directories(reg_id);
                    fs.create_directories(dir_id);

                    expect(fs.file_paths.can_alloc(1));

                    auto file_id =
                      fs.alloc_file(dir_id,
                                    "external-source.irt",
                                    irt::file_path::file_type::component_file);

                    ids.component_file_paths[component_id].file = file_id;
                });

                return component_id;
            });

            mod.ids.read([&](const auto& ids, auto) {
                mod.files.read([&](const auto& fs, auto) {
                    expect(mod.save(ids, fs, compo_id).has_value());

                    expect(eq(ids.size(), 1u));
                    expect(eq(ids.generic_components.size(), 1u));

                    expect(fatal(mod.save(ids, fs, compo_id).has_value()));
                });
            });
        }

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            irt::registred_path_str temp_path;
            expect(fatal(get_temp_registred_path(temp_path)));

            mod.files.write([&](auto& fs) {
                auto  reg_id = fs.alloc_registred("temp", 0);
                auto& reg    = fs.registred_paths.get(reg_id);
                reg.path     = temp_path;
                fs.browse_registreds(jn);
            });

            mod.ids.read([&](const auto& ids, auto) {
                expect(eq(ids.size(), 0u));
                expect(eq(ids.generic_components.size(), 0u));
            });

            expect(fatal(mod.fill_components().has_value()));

            mod.ids.read([&](auto& ids, auto) {
                expect(ge(ids.size(), 1u));
                expect(ge(ids.generic_components.size(), 1u));

                const auto p_id =
                  mod.files.read([&](const irt::file_access& fs,
                                     const auto /*vers*/) noexcept {
                      return fs.find_directory("test"sv);
                  });

                expect(fatal(irt::is_defined(p_id)));

                const auto f_id =
                  mod.files.read([&](const irt::file_access& fs,
                                     const auto /*vers*/) noexcept {
                      return fs.find_file_in_directory(p_id,
                                                       "external-source.irt");
                  });

                expect(fatal(irt::is_defined(f_id)));

                const auto compo_id = [&]() {
                    for (const auto id : ids)
                        if (f_id == ids.component_file_paths[id].file)
                            return id;
                    return irt::undefined<irt::component_id>();
                }();

                expect(fatal(is_defined(compo_id)));
                auto& component = ids.components[compo_id];

                expect(fatal(component.type == irt::component_type::generic));

                auto& generic =
                  ids.generic_components.get(component.id.generic_id);
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

    "easy"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    c3_id{ 0 };

        mod.ids.write([&](auto& ids) {
            auto  c1_id = ids.alloc_generic_component();
            auto& c1    = ids.components[c1_id];
            auto& s1    = ids.generic_components.get(c1.id.generic_id);
            auto& ch1   = s1.alloc(irt::dynamics_type::counter);
            auto  p1_id = c1.get_or_add_x("in");
            expect(!!s1.connect_input(
              p1_id, ch1, irt::connection::port{ .model = 0 }));

            auto  c2_id = ids.alloc_generic_component();
            auto& c2    = ids.components[c2_id];
            auto& s2    = ids.generic_components.get(c2.id.generic_id);
            auto& ch2   = s2.alloc(irt::dynamics_type::time_func);
            auto  p2_id = c2.get_or_add_y("out");
            expect(!!s2.connect_output(
              p2_id, ch2, irt::connection::port{ .model = 0 }));

            c3_id      = ids.alloc_generic_component();
            auto& c3   = ids.components[c3_id];
            auto& s3   = ids.generic_components.get(c3.id.generic_id);
            auto& ch31 = s3.alloc(c2_id);
            auto& ch32 = s3.alloc(c1_id);
            expect(!!s3.connect(mod,
                                ch31,
                                irt::connection::port{ .compo = p2_id },
                                ch32,
                                irt::connection::port{ .compo = p1_id }));

            expect(eq(s1.children.ssize(), 1));
            expect(eq(s2.children.ssize(), 1));
            expect(eq(s3.children.ssize(), 2));
            expect(eq(s1.connections.ssize(), 0));
            expect(eq(s2.connections.ssize(), 0));
            expect(eq(s3.connections.ssize(), 1));
        });

        expect(!!pj.set(mod, c3_id));
        expect(eq(pj.tree_nodes_size().first, 3));

        expect(eq(pj.sim.models.ssize(), 2));
        auto* m1 = pj.sim.models.try_to_get_from_pos(0);
        expect(neq(m1, nullptr));
        auto* m2 = pj.sim.models.try_to_get_from_pos(1);
        expect(neq(m2, nullptr));
        expect(pj.sim.can_connect(1));

        if (m1->type == irt::dynamics_type::counter) {
            expect(!pj.sim.can_connect(*m2, 0, *m1, 0));
        } else {
            expect(!pj.sim.can_connect(*m1, 0, *m2, 0));
        }
    };

    "no-connection"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    c3_id{ 0 };

        mod.ids.write([&](auto& ids) {
            auto  c1_id = ids.alloc_generic_component();
            auto& c1    = ids.components[c1_id];
            auto& s1    = ids.generic_components.get(c1.id.generic_id);
            s1.alloc(irt::dynamics_type::counter);

            auto  c2_id = ids.alloc_generic_component();
            auto& c2    = ids.components[c2_id];
            auto& s2    = ids.generic_components.get(c2.id.generic_id);
            s2.alloc(irt::dynamics_type::time_func);

            c3_id    = ids.alloc_generic_component();
            auto& c3 = ids.components[c3_id];
            auto& s3 = ids.generic_components.get(c3.id.generic_id);
            s3.alloc(c2_id);
            s3.alloc(c1_id);
        });

        expect(!!pj.set(mod, c3_id));
        expect(eq(pj.tree_nodes_size().first, 3));

        expect(eq(pj.sim.models.ssize(), 2));

        auto* m1 = pj.sim.models.try_to_get_from_pos(0);
        expect(neq(m1, nullptr));
        auto* m2 = pj.sim.models.try_to_get_from_pos(1);
        expect(neq(m2, nullptr));
    };

    "empty-component"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    c3_id{ 0 };

        mod.ids.write([&](auto& ids) {
            auto  c1_id = ids.alloc_generic_component();
            auto& c1    = ids.components[c1_id];
            auto& s1    = ids.generic_components.get(c1.id.generic_id);
            s1.alloc(irt::dynamics_type::counter);
            auto p1_id = c1.get_or_add_x("in");

            auto  c11_id = ids.alloc_generic_component();
            auto& c11    = ids.components[c11_id];
            auto& s11    = ids.generic_components.get(c11.id.generic_id);
            auto& ch11   = s11.alloc(c1_id);
            auto  p11_id = c11.get_or_add_x("in");

            expect(!!s11.connect_input(
              p11_id, ch11, irt::connection::port{ .compo = p1_id }));

            auto  c2_id = ids.alloc_generic_component();
            auto& c2    = ids.components[c2_id];
            auto& s2    = ids.generic_components.get(c2.id.generic_id);
            s2.alloc(irt::dynamics_type::time_func);
            auto p2_id = c2.get_or_add_y("out");

            auto  c22_id = ids.alloc_generic_component();
            auto& c22    = ids.components[c22_id];
            auto& s22    = ids.generic_components.get(c22.id.generic_id);
            auto& ch22   = s22.alloc(c2_id);
            auto  p22_id = c22.get_or_add_y("out");

            expect(!!s22.connect_output(
              p22_id, ch22, irt::connection::port{ .compo = p2_id }));

            c3_id      = ids.alloc_generic_component();
            auto& c3   = ids.components[c3_id];
            auto& s3   = ids.generic_components.get(c3.id.generic_id);
            auto& ch31 = s3.alloc(c22_id);
            auto& ch32 = s3.alloc(c11_id);

            expect(!!s3.connect(mod,
                                ch31,
                                irt::connection::port{ .compo = p22_id },
                                ch32,
                                irt::connection::port{ .compo = p11_id }));
        });

        expect(!!pj.set(mod, c3_id));
        expect(eq(pj.tree_nodes_size().first, 5));

        expect(eq(pj.sim.models.ssize(), 2));

        auto* m1 = pj.sim.models.try_to_get_from_pos(0);
        expect(neq(m1, nullptr));
        auto* m2 = pj.sim.models.try_to_get_from_pos(1);
        expect(neq(m2, nullptr));

        if (m1->type == irt::dynamics_type::counter) {
            expect(pj.sim.can_connect(*m2, 0, *m1, 0));
        } else {
            expect(pj.sim.can_connect(*m1, 0, *m2, 0));
        }
    };

    "graph-small-world"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    cg_id{ 0 };

        int node_size = 0;

        mod.ids.write([&](auto& ids) {
            auto  c_id = ids.alloc_generic_component();
            auto& c    = ids.components[c_id];
            auto& s    = ids.generic_components.get(c.id.generic_id);
            s.alloc(irt::dynamics_type::counter);

            cg_id         = ids.alloc_graph_component();
            auto& cg      = ids.components[cg_id];
            auto& g       = ids.graph_components.get(cg.id.graph_id);
            g.g_type      = irt::graph_component::graph_type::small_world;
            g.type        = irt::graph_component::connection_type::in_out;
            g.small       = irt::graph_component::small_world_param{};
            g.small.nodes = 25;
            g.small.id    = c_id;

            node_size = g.g.nodes.ssize();
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, node_size + 1));
        expect(eq(pj.sim.models.ssize(), node_size));
    };

    "graph-scale-free"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    cg_id{ 0 };

        int node_size = 0;

        mod.ids.write([&](auto& ids) {
            auto  c_id = ids.alloc_generic_component();
            auto& c    = ids.components[c_id];
            auto& s    = ids.generic_components.get(c.id.generic_id);
            s.alloc(irt::dynamics_type::counter);

            cg_id         = ids.alloc_graph_component();
            auto& cg      = ids.components[cg_id];
            auto& g       = ids.graph_components.get(cg.id.graph_id);
            g.g_type      = irt::graph_component::graph_type::scale_free;
            g.type        = irt::graph_component::connection_type::in_out;
            g.scale       = irt::graph_component::scale_free_param{};
            g.scale.nodes = 25;
            g.scale.id    = c_id;

            node_size = g.g.nodes.ssize();
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, node_size + 1));
        expect(eq(pj.sim.models.ssize(), node_size));
    };

    "graph-scale-free-sum-in-out"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    cg_id{ 0 };

        int      node_size = 0;
        unsigned edge_size = 0;

        mod.ids.write([&](auto& ids) {
            auto  c_id  = ids.alloc_generic_component();
            auto& c     = ids.components[c_id];
            auto& s     = ids.generic_components.get(c.id.generic_id);
            auto& child = s.alloc(irt::dynamics_type::qss1_sum_2);

            const auto port_in  = c.get_or_add_x("in");
            const auto port_out = c.get_or_add_y("out");

            expect(!!s.connect_input(
              port_in, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_output(
              port_out, child, irt::connection::port{ .model = 0 }));

            cg_id         = ids.alloc_graph_component();
            auto& cg      = ids.components[cg_id];
            auto& g       = ids.graph_components.get(cg.id.graph_id);
            g.g_type      = irt::graph_component::graph_type::scale_free;
            g.type        = irt::graph_component::connection_type::in_out;
            g.scale       = irt::graph_component::scale_free_param{};
            g.scale.alpha = 2.5;
            g.scale.beta  = 1.e3;
            g.scale.id    = c_id;
            g.scale.nodes = 64;
            node_size     = g.g.nodes.ssize();
            edge_size     = g.g.edges.ssize();
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, node_size + 1));
        expect(eq(pj.sim.models.ssize(), node_size));
        expect(eq(get_connection_number(pj.sim), edge_size));
    };

    "graph-scale-free-sum-m-n"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    cg_id{ 0 };

        int      node_size = 0;
        unsigned edge_size = 0;

        mod.ids.write([&](auto& ids) {
            auto  c_id  = ids.alloc_generic_component();
            auto& c     = ids.components[c_id];
            auto& s     = ids.generic_components.get(c.id.generic_id);
            auto& child = s.alloc(irt::dynamics_type::qss1_sum_2);

            const auto port_in_m  = c.get_or_add_x("m");
            const auto port_in_n  = c.get_or_add_x("n");
            const auto port_out_m = c.get_or_add_y("m");
            const auto port_out_n = c.get_or_add_y("n");

            expect(!!s.connect_input(
              port_in_m, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_input(
              port_in_n, child, irt::connection::port{ .model = 1 }));
            expect(!!s.connect_output(
              port_out_m, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_output(
              port_out_n, child, irt::connection::port{ .model = 1 }));

            cg_id         = ids.alloc_graph_component();
            auto& cg      = ids.components[cg_id];
            auto& g       = ids.graph_components.get(cg.id.graph_id);
            g.g_type      = irt::graph_component::graph_type::scale_free;
            g.type        = irt::graph_component::connection_type::name;
            g.scale       = irt::graph_component::scale_free_param{};
            g.scale.alpha = 2.5;
            g.scale.beta  = 1.e3;
            g.scale.id    = c_id;
            g.scale.nodes = 64;

            node_size = g.g.nodes.ssize();
            edge_size = g.g.edges.ssize();
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, node_size + 1));
        expect(eq(pj.sim.models.ssize(), node_size));
        expect(eq(get_connection_number(pj.sim), 2u * edge_size));
    };

    "graph-scale-free-sum-m_3-n_3"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    cg_id{ 0 };

        int      node_size = 0;
        unsigned edge_size = 0;

        mod.ids.write([&](auto& ids) {
            auto  c_id  = ids.alloc_generic_component();
            auto& c     = ids.components[c_id];
            auto& s     = ids.generic_components.get(c.id.generic_id);
            auto& child = s.alloc(irt::dynamics_type::qss1_sum_2);

            const auto port_in_m  = c.get_or_add_x("m");
            const auto port_in_m0 = c.get_or_add_x("m_0");
            const auto port_in_m1 = c.get_or_add_x("m_1");
            const auto port_in_m2 = c.get_or_add_x("m_2");
            const auto port_in_m3 = c.get_or_add_x("m_3");
            const auto port_in_n  = c.get_or_add_x("n");
            const auto port_in_n0 = c.get_or_add_x("n_0");
            const auto port_in_n1 = c.get_or_add_x("n_1");
            const auto port_in_n2 = c.get_or_add_x("n_2");
            const auto port_in_n3 = c.get_or_add_x("n_3");
            const auto port_out_m = c.get_or_add_y("m");
            const auto port_out_n = c.get_or_add_y("n");

            expect(!!s.connect_input(
              port_in_m, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_input(
              port_in_n, child, irt::connection::port{ .model = 1 }));
            expect(!!s.connect_input(
              port_in_m0, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_input(
              port_in_n0, child, irt::connection::port{ .model = 1 }));
            expect(!!s.connect_input(
              port_in_m1, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_input(
              port_in_n1, child, irt::connection::port{ .model = 1 }));
            expect(!!s.connect_input(
              port_in_m2, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_input(
              port_in_n2, child, irt::connection::port{ .model = 1 }));
            expect(!!s.connect_input(
              port_in_m3, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_input(
              port_in_n3, child, irt::connection::port{ .model = 1 }));
            expect(!!s.connect_output(
              port_out_m, child, irt::connection::port{ .model = 0 }));
            expect(!!s.connect_output(
              port_out_n, child, irt::connection::port{ .model = 1 }));

            cg_id         = ids.alloc_graph_component();
            auto& cg      = ids.components[cg_id];
            auto& g       = ids.graph_components.get(cg.id.graph_id);
            g.g_type      = irt::graph_component::graph_type::scale_free;
            g.type        = irt::graph_component::connection_type::name_suffix;
            g.scale       = irt::graph_component::scale_free_param{};
            g.scale.alpha = 3;
            g.scale.beta  = 1.e3;
            g.scale.id    = c_id;
            g.scale.nodes = 16;

            node_size = g.g.nodes.ssize();
            edge_size = g.g.edges.ssize();
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, node_size + 1));
        expect(eq(pj.sim.models.ssize(), node_size));
        expect(eq(get_connection_number(pj.sim), 2 * edge_size));
    };

    "grid-3x3-empty-con"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    cg_id{ 0 };

        int cell_number = 0;

        mod.ids.write([&](auto& ids) {
            auto  c_id = ids.alloc_generic_component();
            auto& c    = ids.components[c_id];
            auto& s    = ids.generic_components.get(c.id.generic_id);
            s.alloc(irt::dynamics_type::counter);

            cg_id    = ids.alloc_grid_component();
            auto& cg = ids.components[cg_id];
            auto& g  = ids.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, c_id);

            cell_number = g.cells_number();
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, cell_number + 1));
        expect(eq(pj.sim.models.ssize(), cell_number));
    };

    "grid-3x3-empty-con-middle"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    cg_id{ 0 };

        int row = 0;
        int col = 0;

        mod.ids.write([&](auto& ids) {
            auto  c_id = ids.alloc_generic_component();
            auto& c    = ids.components[c_id];
            auto& s    = ids.generic_components.get(c.id.generic_id);
            s.alloc(irt::dynamics_type::counter);

            cg_id    = ids.alloc_grid_component();
            auto& cg = ids.components[cg_id];
            auto& g  = ids.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, irt::undefined<irt::component_id>());

            for (int i = 1; i < 4; ++i)
                for (int j = 1; j < 4; ++j)
                    g.children()[g.pos(i, j)] = c_id;

            row = g.row();
            col = g.column();
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, (row - 2) * (col - 2) + 1));
        expect(eq(pj.sim.models.ssize(), (row - 2) * (col - 2)));
    };

    "grid-3x3"_test = [] {
        irt::vector<char>       buffer;
        irt::registred_path_str temp_path;

        expect(get_temp_registred_path(temp_path) == true);

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        irt::component_id c1_id{ 0 };
        irt::component_id c2_id{ 0 };
        irt::component_id c3_id{ 0 };
        irt::component_id cg_id{ 0 };

        int cell_number = 0;

        mod.ids.write([&](auto& ids) {
            c1_id       = ids.alloc_generic_component();
            auto& c1    = ids.components[c1_id];
            auto& s1    = ids.generic_components.get(c1.id.generic_id);
            auto& ch1   = s1.alloc(irt::dynamics_type::counter);
            auto  p1_id = c1.get_or_add_x("in");
            expect(!!s1.connect_input(
              p1_id, ch1, irt::connection::port{ .model = 0 }));

            c2_id       = ids.alloc_generic_component();
            auto& c2    = ids.components[c2_id];
            auto& s2    = ids.generic_components.get(c2.id.generic_id);
            auto& ch2   = s2.alloc(irt::dynamics_type::time_func);
            auto  p2_id = c2.get_or_add_y("out");
            expect(!!s2.connect_output(
              p2_id, ch2, irt::connection::port{ .model = 0 }));

            c3_id      = ids.alloc_generic_component();
            auto& c3   = ids.components[c3_id];
            auto& s3   = ids.generic_components.get(c3.id.generic_id);
            auto& ch31 = s3.alloc(c2_id);
            auto& ch32 = s3.alloc(c1_id);
            expect(!!s3.connect(mod,
                                ch31,
                                irt::connection::port{ .compo = p2_id },
                                ch32,
                                irt::connection::port{ .compo = p1_id }));

            cg_id    = ids.alloc_grid_component();
            auto& cg = ids.components[cg_id];
            auto& g  = ids.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, c3_id);

            mod.files.write([&](auto& fs) {
                const auto reg_id = fs.alloc_registred("temp", 0);
                auto&      reg    = fs.registred_paths.get(reg_id);
                reg.path          = temp_path;

                const auto dir_id = fs.alloc_dir(reg_id);
                auto&      dir    = fs.dir_paths.get(dir_id);
                dir.path          = "test";
                dir.parent        = fs.registred_paths.get_id(reg);

                fs.create_directories(reg_id);
                fs.create_directories(dir_id);

                const auto c1_id = fs.alloc_file(
                  dir_id, "c1.irt", irt::file_path::file_type::component_file);
                const auto c2_id = fs.alloc_file(
                  dir_id, "c2.irt", irt::file_path::file_type::component_file);
                const auto c3_id = fs.alloc_file(
                  dir_id, "c3.irt", irt::file_path::file_type::component_file);
                const auto cg_id = fs.alloc_file(
                  dir_id, "cg.irt", irt::file_path::file_type::component_file);

                ids.component_file_paths[c1_id].file = c1_id;
                ids.component_file_paths[c2_id].file = c2_id;
                ids.component_file_paths[c3_id].file = c3_id;
                ids.component_file_paths[cg_id].file = cg_id;
            });

            cell_number = g.cells_number();
        });

        mod.ids.read([&](const auto& ids, auto) {
            mod.files.read([&](const auto& files, auto) {
                expect(!!mod.save(ids, files, c1_id));
                expect(!!mod.save(ids, files, c2_id));
                expect(!!mod.save(ids, files, c3_id));
                expect(!!mod.save(ids, files, cg_id));
            });
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, cell_number * 3 + 1));

        expect(eq(pj.sim.models.ssize(), cell_number * 2));

        mod.files.read([&](const auto& files, auto) noexcept {
            mod.ids.read([&](const auto& ids, auto) noexcept {
                irt::json_archiver j;
                expect(
                  j(pj,
                    mod,
                    files,
                    ids,
                    buffer,
                    irt::json_archiver::print_option::indent_2_one_line_array)
                    .has_value());
            });
        });

        expect(buffer.size() > 0u);

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            mod.files.write([&](auto& fs) {
                const auto reg_id = fs.alloc_registred("temp", 0);
                auto&      reg    = fs.registred_paths.get(reg_id);
                reg.path          = temp_path;

                fs.create_directories(reg_id);
            });

            auto old_cb = std::exchange(irt::on_error_callback, nullptr);
            expect(!!mod.fill_components());
            std::exchange(irt::on_error_callback, old_cb);

            mod.files.read([&](const auto& files, auto) noexcept {
                mod.ids.read([&](const auto& ids, auto) noexcept {
                    irt::json_dearchiver j;
                    expect(j(pj,
                             mod,
                             pj.sim,
                             files,
                             ids,
                             std::span(buffer.data(), buffer.size()))
                             .has_value());
                });
            });

            mod.files.write([&](auto& fs) noexcept {
                while (not fs.dir_paths.empty())
                    fs.remove_directory(
                      fs.dir_paths.get_id(*fs.dir_paths.begin()));
            });
        }
    };

    "hsm"_test = [] {
        irt::project         pj;
        irt::journal_handler jn;
        irt::modeling        mod{ jn };

        auto compo_id = mod.ids.write([&](auto& ids) {
            expect(ids.hsm_components.can_alloc(1)) << fatal;
            expect(ids.can_alloc_component(1)) << fatal;

            auto  compo_id = ids.alloc_hsm_component();
            auto& compo    = ids.components[compo_id];
            auto& hsm      = ids.hsm_components.get(compo.id.hsm_id);

            expect(!!hsm.machine.set_state(
              0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

            expect(!!hsm.machine.set_state(1u, 0u));
            hsm.machine.states[1u].condition.set(0b0011u, 0b0011u);
            hsm.machine.states[1u].if_transition = 2u;

            expect(!!hsm.machine.set_state(2u, 0u));
            hsm.machine.states[2u].enter_action.set_output(
              irt::hierarchical_state_machine::variable::port_0, 1.0f);

            return compo_id;
        });

        expect(!!pj.set(mod, compo_id));

        pj.sim.limits.set_bound(0, 10);
        expect(!!pj.sim.srcs.prepare());
        expect(!!pj.sim.initialize());

        irt::status st;

        do {
            st = pj.sim.run();
            expect(!!st);
        } while (not pj.sim.current_time_expired());
    };

    "internal_component_io"_test = [] {
        {
            irt::journal_handler jn{};
            irt::modeling        mod{ jn };

            std::array<irt::component_id, irt::internal_component_count> i_ids;

            mod.ids.write([&](auto& ids) {
                mod.files.write([&](auto& fs) {
                    fs.registred_paths.reserve(8);
                    fs.dir_paths.reserve(32);
                    fs.file_paths.reserve(256);

                    expect(fs.registred_paths.can_alloc(8));
                    expect(fs.dir_paths.can_alloc(32));
                    expect(fs.file_paths.can_alloc(256));

                    expect(
                      ids.can_alloc_component(irt::internal_component_count));

                    auto  reg_id = fs.alloc_registred("temp", 0);
                    auto& reg    = fs.registred_paths.get(reg_id);

                    get_temp_registred_path(reg.path);
                    fs.create_directories(reg_id);

                    auto dir_id = fs.alloc_dir(reg_id, "dir-temp");
                    fs.create_directories(dir_id);

                    for (int i = 0, e = irt::internal_component_count; i != e;
                         ++i) {
                        auto c_id = ids.alloc_generic_component();

                        const auto filename = irt::format_n<64>(
                          "{}.irt", irt::internal_component_names[i]);
                        auto f_id = fs.alloc_file(
                          dir_id,
                          filename.sv(),
                          irt::file_path::file_type::component_file);

                        ids.component_file_paths[c_id].file = f_id;

                        expect(
                          ids
                            .copy(irt::enum_cast<irt::internal_component>(i),
                                  c_id)
                            .has_value());
                        i_ids[i] = c_id;
                    }
                });
            });

            mod.ids.read([&](const auto& ids, auto) noexcept {
                mod.files.read([&](const auto& files, auto) noexcept {
                    for (int i = 0, e = irt::internal_component_count; i != e;
                         ++i) {
                        expect(fatal(ids.exists(i_ids[i])));
                        expect(mod.save(ids, files, i_ids[i]).has_value());
                    }
                });

                expect(eq(ids.ssize(), irt::internal_component_count));
            });
        }

        {
            irt::journal_handler jn{};
            irt::modeling        mod{ jn };

            mod.files.write([&](auto& fs) {
                fs.registred_paths.reserve(8);
                fs.dir_paths.reserve(32);
                fs.file_paths.reserve(256);

                expect(fs.registred_paths.can_alloc(8));
                expect(fs.dir_paths.can_alloc(32));
                expect(fs.file_paths.can_alloc(256));

                const auto reg_id = fs.alloc_registred("temp", 0);
                auto&      reg    = fs.registred_paths.get(reg_id);
                get_temp_registred_path(reg.path);
                fs.create_directories(reg_id);
                fs.browse_registreds(jn);
            });

            auto old_cb = std::exchange(irt::on_error_callback, nullptr);
            expect(!!mod.fill_components());
            std::exchange(irt::on_error_callback, old_cb);

            mod.ids.read([&](const auto& ids, auto) noexcept {
                expect(ids.ssize() >= irt::internal_component_count);
            });

            mod.files.write([&](auto& fs) noexcept {
                while (not fs.dir_paths.empty())
                    fs.remove_directory(
                      fs.dir_paths.get_id(*fs.dir_paths.begin()));
            });
        }
    };

    "grid-3x3-constant-model-init-port-all"_test = [] {
        irt::vector<char> buffer;

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        irt::i32 cell_number = 0;

        const auto cg_id = mod.ids.write([&](auto& ids) noexcept {
            auto  c1_id = ids.alloc_generic_component();
            auto& c1    = ids.components[c1_id];
            auto& s1    = ids.generic_components.get(c1.id.generic_id);
            auto& ch1   = s1.alloc(irt::dynamics_type::counter);

            auto p1_id = c1.get_or_add_x("in");
            expect(!!s1.connect_input(
              p1_id, ch1, irt::connection::port{ .model = 0 }));

            auto  c2_id = ids.alloc_generic_component();
            auto& c2    = ids.components[c2_id];
            auto& s2    = ids.generic_components.get(c2.id.generic_id);
            auto& ch2   = s2.alloc(irt::dynamics_type::time_func);
            auto  p2_id = c2.get_or_add_y("out");
            expect(!!s2.connect_output(
              p2_id, ch2, irt::connection::port{ .model = 0 }));

            auto  c3_id  = ids.alloc_generic_component();
            auto& c3     = ids.components[c3_id];
            auto& s3     = ids.generic_components.get(c3.id.generic_id);
            auto& ch3    = s3.alloc(c2_id);
            auto& ch4    = s3.alloc(c1_id);
            auto& ch5    = s3.alloc(irt::dynamics_type::constant);
            auto  p31_id = c3.get_or_add_x("in");
            auto  p32_id = c3.get_or_add_y("out");

            const auto ch5_id  = s3.children.get_id(ch5);
            const auto ch5_idx = get_index(ch5_id);
            auto&      p       = s3.children_parameters[ch5_idx];
            p.reals[0]         = 0.0;
            p.reals[1]         = 0.0;
            p.integers[0] =
              ordinal(irt::constant::init_type::incoming_component_all);
            p.integers[1] = 0;

            expect(!!s3.connect(mod,
                                ch3,
                                irt::connection::port{ .compo = p2_id },
                                ch4,
                                irt::connection::port{ .compo = p1_id }));
            expect(!!s3.connect_input(
              p31_id, ch4, irt::connection::port{ .compo = p1_id }));
            expect(!!s3.connect_output(
              p32_id, ch3, irt::connection::port{ .compo = p2_id }));

            auto  cg_id = ids.alloc_grid_component();
            auto& cg    = ids.components[cg_id];
            auto& g     = ids.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, c3_id);
            g.opts                = irt::grid_component::options::none;
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;
            g.neighbors           = irt::grid_component::neighborhood::four;

            cell_number = g.cells_number();

            return cg_id;
        });

        expect(!!pj.set(mod, cg_id));
        expect(eq(pj.tree_nodes_size().first, cell_number * 3 + 1));
        expect(eq(pj.sim.models.ssize(), cell_number * 3));

        int         nb_constant_model = 0;
        irt::model* cst_mdl           = nullptr;
        while (pj.sim.models.next(cst_mdl)) {
            if (cst_mdl->type == irt::dynamics_type::constant) {
                ++nb_constant_model;
                auto& dyn = irt::get_dyn<irt::constant>(*cst_mdl);
                expect(eq(
                  ordinal(dyn.type),
                  ordinal(irt::constant::init_type::incoming_component_all)));
                expect(eq(dyn.port, 0u));
                expect(neq(dyn.value, 0.0));
            }
        }

        expect(eq(nb_constant_model, cell_number));
    };

    "grid-3x3-constant-model-init-port-n"_test = [] {
        irt::vector<char> buffer;

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;
            irt::i32             cell_number = 0;

            const auto cg_id = mod.ids.write([&](auto& ids) {
                auto  compo_counter_id = ids.alloc_generic_component();
                auto& compo_counter    = ids.components[compo_counter_id];
                auto& gen_counter =
                  ids.generic_components.get(compo_counter.id.generic_id);
                auto& child_counter =
                  gen_counter.alloc(irt::dynamics_type::counter);
                auto compo_counter_in = compo_counter.get_or_add_x("in");
                expect(!!gen_counter.connect_input(
                  compo_counter_in,
                  child_counter,
                  irt::connection::port{ .model = 0 }));

                auto  compo_timef_id = ids.alloc_generic_component();
                auto& compo_timef    = ids.components[compo_timef_id];
                auto& gen_timef =
                  ids.generic_components.get(compo_timef.id.generic_id);
                auto& child_timef =
                  gen_timef.alloc(irt::dynamics_type::time_func);
                auto compo_timef_out = compo_timef.get_or_add_y("out");
                expect(!!gen_timef.connect_output(
                  compo_timef_out,
                  child_timef,
                  irt::connection::port{ .model = 0 }));

                auto  c3_id  = ids.alloc_generic_component();
                auto& c3     = ids.components[c3_id];
                auto& s3     = ids.generic_components.get(c3.id.generic_id);
                auto& ch3    = s3.alloc(compo_timef_id);
                auto& ch4    = s3.alloc(compo_counter_id);
                auto& ch5    = s3.alloc(irt::dynamics_type::constant);
                auto  p31_id = c3.get_or_add_x("in");
                auto  p32_id = c3.get_or_add_y("out");

                // auto& mdl          = mod.models.get(ch5.id.mdl_id);
                // auto& dyn          = irt::get_dyn<irt::constant>(mdl);
                // dyn.default_offset = 0;
                // dyn.type           =
                // irt::constant::init_type::incoming_component_n; dyn.port =
                // ordinal(p31_id);

                const auto ch5_id = s3.children.get_id(ch5);
                auto&      p_ch5  = s3.children_parameters[ch5_id];
                p_ch5.reals[0]    = 0.0;
                p_ch5.integers[0] =
                  ordinal(irt::constant::init_type::incoming_component_n);
                p_ch5.integers[1] = ordinal(p31_id);

                expect(!!s3.connect(
                  mod,
                  ch3,
                  irt::connection::port{ .compo = compo_timef_out },
                  ch4,
                  irt::connection::port{ .compo = compo_counter_in }));

                expect(!!s3.connect_input(
                  p31_id,
                  ch4,
                  irt::connection::port{ .compo = compo_counter_in }));
                expect(!!s3.connect_output(
                  p32_id,
                  ch3,
                  irt::connection::port{ .compo = compo_timef_out }));

                auto  cg_id = ids.alloc_grid_component();
                auto& cg    = ids.components[cg_id];
                auto& g     = ids.grid_components.get(cg.id.grid_id);
                g.resize(5, 5, c3_id);
                g.in_connection_type  = irt::grid_component::type::in_out;
                g.out_connection_type = irt::grid_component::type::in_out;

                cell_number = g.cells_number();

                return cg_id;
            });

            expect(!!pj.set(mod, cg_id));
            expect(eq(pj.tree_nodes_size().first, cell_number * 3 + 1));
            expect(eq(pj.sim.models.ssize(), cell_number * 3));

            int         nb_constant_model = 0;
            irt::model* cst_mdl           = nullptr;
            while (pj.sim.models.next(cst_mdl)) {
                if (cst_mdl->type == irt::dynamics_type::constant) {
                    ++nb_constant_model;
                    auto& dyn = irt::get_dyn<irt::constant>(*cst_mdl);
                    expect(neq(dyn.value, 0.0));
                    expect(eq(
                      ordinal(dyn.type),
                      ordinal(irt::constant::init_type::incoming_component_n)));
                }
            }

            expect(eq(nb_constant_model, cell_number));
        }
    };

    "grid-3x3-constant-model-init-port-empty"_test = [] {
        auto old_error_callback =
          std::exchange(irt::on_error_callback, nullptr);

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        const auto cg_id = mod.ids.write([&](auto& ids) {
            auto  c1_id = ids.alloc_generic_component();
            auto& c1    = ids.components[c1_id];
            auto& s1    = ids.generic_components.get(c1.id.generic_id);
            auto& ch1   = s1.alloc(irt::dynamics_type::counter);
            auto  p1_id = c1.get_or_add_x("in");
            expect(!!s1.connect_input(
              p1_id, ch1, irt::connection::port{ .model = 0 }));

            auto  c2_id = ids.alloc_generic_component();
            auto& c2    = ids.components[c2_id];
            auto& s2    = ids.generic_components.get(c2.id.generic_id);
            auto& ch2   = s2.alloc(irt::dynamics_type::time_func);
            auto  p2_id = c2.get_or_add_y("out");
            expect(!!s2.connect_output(
              p2_id, ch2, irt::connection::port{ .model = 0 }));

            auto  c3_id = ids.alloc_generic_component();
            auto& c3    = ids.components[c3_id];
            auto& s3    = ids.generic_components.get(c3.id.generic_id);
            auto& ch3   = s3.alloc(c2_id);
            auto& ch4   = s3.alloc(c1_id);
            auto& ch5   = s3.alloc(irt::dynamics_type::constant);

            const auto ch5_id = s3.children.get_id(ch5);
            auto&      p_ch5  = s3.children_parameters[ch5_id];

            p_ch5.reals[0] = 0.0;
            p_ch5.integers[0] =
              ordinal(irt::constant::init_type::incoming_component_n);
            p_ch5.integers[1] = 17; // Impossible port

            expect(!!s3.connect(mod,
                                ch3,
                                irt::connection::port{ .compo = p2_id },
                                ch4,
                                irt::connection::port{ .compo = p1_id }));

            auto  cg_id = ids.alloc_grid_component();
            auto& cg    = ids.components[cg_id];
            auto& g     = ids.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, c3_id);

            return cg_id;
        });

        expect(pj.set(mod, cg_id).has_error()); /* Fail to build the project
                                     since the constant models can not be
                                     initialized with dyn.port equals to 17. */

        irt::on_error_callback = old_error_callback;
    };

    "grid-5x5-4-neighbors-input-port-type"_test = [] {
        irt::vector<char> buffer;

        /* The component in a 5x5 grid:
         +-----------------------------+
         |component                    |
         |                             |
         |   +---------+ +--------+    |
         +-->| counter | |constant| -->|
         |   +---------+ +--------+    |
         |in                        out|
         |                             |
         +-----------------------------+
          */

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::i32             cell_number = 0;

        const auto cg_id = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            auto& ch_ct  = gen.alloc(irt::dynamics_type::counter);
            auto& ch_cst = gen.alloc(irt::dynamics_type::constant);

            auto p_in  = compo.get_or_add_x("in");
            auto p_out = compo.get_or_add_y("out");

            // Switches the counter component input port from @a classic to
            // @a sums. This change will add @a dynamics_type::qss3_sum4 and
            // connections.

            compo.x.template get<irt::port_option>(p_in) =
              irt::port_option::sum;

            expect(compo.x.template get<irt::port_option>(p_in) ==
                   irt::port_option::sum);

            expect(
              gen
                .connect_input(p_in, ch_ct, irt::connection::port{ .model = 0 })
                .has_value());

            expect(gen
                     .connect_output(
                       p_out, ch_cst, irt::connection::port{ .model = 0 })
                     .has_value());

            auto  cg_id = ids.alloc_grid_component();
            auto& cg    = ids.components[cg_id];
            auto& g     = ids.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, compo_id);
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;
            g.neighbors           = irt::grid_component::neighborhood::four;
            cell_number           = g.cells_number();

            return cg_id;
        });

        expect(pj.set(mod, cg_id).has_value());

        int nb_sum_model      = 0;
        int nb_counter_model  = 0;
        int nb_constant_model = 0;
        int nb_unknown_model  = 0;

        for (const auto& mdl : pj.sim.models) {
            if (mdl.type == irt::dynamics_type::constant)
                ++nb_constant_model;
            else if (mdl.type == irt::dynamics_type::counter)
                ++nb_counter_model;
            else if (mdl.type == irt::dynamics_type::qss3_sum_4)
                ++nb_sum_model;
            else
                ++nb_unknown_model;
        }

        // The 3x3 in the 5x5 need two sum models.

        expect(eq(nb_sum_model, cell_number + 9));
        expect(eq(nb_counter_model, cell_number));
        expect(eq(nb_constant_model, cell_number));
        expect(eq(nb_unknown_model, 0));
    };

    "grid-5x5-8-neighbors-input-port-type"_test = [] {
        irt::vector<char> buffer;

        /* The component in a 5x5 grid:
         +-----------------------------+
         |component                    |
         |                             |
         |   +---------+ +--------+    |
         +-->| counter | |constant| -->|
         |   +---------+ +--------+    |
         |in                        out|
         |                             |
         +-----------------------------+
          */

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::i32             cell_number = 0;

        const auto cg_id = mod.ids.write([&](auto& ids) {
            auto  compo_id = ids.alloc_generic_component();
            auto& compo    = ids.components[compo_id];
            auto& gen      = ids.generic_components.get(compo.id.generic_id);

            auto& ch_ct  = gen.alloc(irt::dynamics_type::counter);
            auto& ch_cst = gen.alloc(irt::dynamics_type::constant);

            auto p_in  = compo.get_or_add_x("in");
            auto p_out = compo.get_or_add_y("out");

            // Switches the counter component input port from @a classic to
            // @a sums. This change will add @a dynamics_type::qss3_sum4 and
            // connections.

            compo.x.template get<irt::port_option>(p_in) =
              irt::port_option::sum;

            expect(compo.x.template get<irt::port_option>(p_in) ==
                   irt::port_option::sum);

            expect(
              gen
                .connect_input(p_in, ch_ct, irt::connection::port{ .model = 0 })
                .has_value());

            expect(gen
                     .connect_output(
                       p_out, ch_cst, irt::connection::port{ .model = 0 })
                     .has_value());

            auto  cg_id = ids.alloc_grid_component();
            auto& cg    = ids.components[cg_id];
            auto& g     = ids.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, compo_id);
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;
            g.neighbors           = irt::grid_component::neighborhood::eight;
            cell_number           = g.cells_number();

            return cg_id;
        });

        expect(pj.set(mod, cg_id).has_value());

        int nb_sum_model      = 0;
        int nb_counter_model  = 0;
        int nb_constant_model = 0;
        int nb_unknown_model  = 0;

        for (const auto& mdl : pj.sim.models) {
            if (mdl.type == irt::dynamics_type::constant)
                ++nb_constant_model;
            else if (mdl.type == irt::dynamics_type::counter)
                ++nb_counter_model;
            else if (mdl.type == irt::dynamics_type::qss3_sum_4)
                ++nb_sum_model;
            else
                ++nb_unknown_model;
        }

        // The 3x3 in the 5x5 need two sum models.

        expect(eq(nb_sum_model, 1 * 4 + 2 * 12 + 2 * 9 + 9));
        expect(eq(nb_counter_model, cell_number));
        expect(eq(nb_constant_model, cell_number));
        expect(eq(nb_unknown_model, 0));
    };

    "grid-5x5-8-neighbors-input-port-type-connetion-pack"_test = [] {
        irt::vector<char> buffer;

        /* The component in a 5x5 grid:
         +-----------------------------+
         |component                    |
         |                             |
         |   +---------+ +--------+    |
         +-->| counter | |constant| -->|
         |   +---------+ +--------+    |
         |in                        out|
         |                             |
         +-----------------------------+
          */

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;
            irt::component_id    cg_id{ 0 };
            irt::port_id         cg_output_port_id{ 0 };
            irt::port_id         p_out{ 0 };
            irt::component_id    compo_id{ 0 };
            irt::child_id        counter_child_id{ 0 };

            const auto root_id = mod.ids.write([&](auto& ids) {
                compo_id    = ids.alloc_generic_component();
                auto& compo = ids.components[compo_id];
                auto& gen   = ids.generic_components.get(compo.id.generic_id);

                auto& ch_ct  = gen.alloc(irt::dynamics_type::counter);
                auto& ch_cst = gen.alloc(irt::dynamics_type::constant);

                auto p_in = compo.get_or_add_x("in");
                p_out     = compo.get_or_add_y("out");

                expect(gen
                         .connect_input(
                           p_in, ch_ct, irt::connection::port{ .model = 0 })
                         .has_value());

                expect(gen
                         .connect_output(
                           p_out, ch_cst, irt::connection::port{ .model = 0 })
                         .has_value());

                cg_id    = ids.alloc_grid_component();
                auto& cg = ids.components[cg_id];
                auto& g  = ids.grid_components.get(cg.id.grid_id);
                g.resize(5, 5, compo_id);
                g.in_connection_type  = irt::grid_component::type::in_out;
                g.out_connection_type = irt::grid_component::type::in_out;
                g.neighbors           = irt::grid_component::neighborhood::four;
                cg_output_port_id     = cg.get_or_add_y("out");

                expect(
                  g.connect_output(cg_output_port_id, 3, 3, p_out).has_value());

                auto  root_id  = ids.alloc_generic_component();
                auto& root     = ids.components[root_id];
                auto& root_gen = ids.generic_components.get(root.id.generic_id);

                auto& grid_child = root_gen.alloc(cg_id);
                auto& counter_child =
                  root_gen.alloc(irt::dynamics_type::counter);
                counter_child_id = root_gen.children.get_id(counter_child);

                expect(root_gen
                         .connect(
                           mod,
                           grid_child,
                           irt::connection::port{ .compo = cg_output_port_id },
                           counter_child,
                           irt::connection::port{ .model = 0 })
                         .has_value());

                return root_id;
            });

            expect(pj.set(mod, root_id).has_value());

            int nb_sum_model      = 0;
            int nb_counter_model  = 0;
            int nb_constant_model = 0;
            int nb_unknown_model  = 0;

            for (const auto& mdl : pj.sim.models) {
                if (mdl.type == irt::dynamics_type::constant)
                    ++nb_constant_model;
                else if (mdl.type == irt::dynamics_type::counter)
                    ++nb_counter_model;
                else if (mdl.type == irt::dynamics_type::qss3_sum_4)
                    ++nb_sum_model;
                else
                    ++nb_unknown_model;
            }

            expect(eq(nb_sum_model, 0));
            expect(eq(nb_counter_model, 5 * 5 + 1));
            expect(eq(nb_constant_model, 5 * 5));
            expect(eq(nb_unknown_model, 0));
            expect(eq(get_connection_number(pj.sim),
                      9u * 4u      // The 3x3 center models with 4 connections
                        + 4u * 2u  // The 4 corner models with 2 connections
                        + 12u * 3u // The 12 border models with 3 connections
                        + 1u // The connection from component (3, 3) in grid
                             // to root counter model.
                      ));

            // We replace the output-connection in grid with a
            // connection-pack.

            mod.ids.write([&](auto& ids) {
                auto& cg = ids.components[cg_id];
                auto& g  = ids.grid_components.get(cg.id.grid_id);

                g.output_connections.clear();

                cg.output_connection_pack.push_back(
                  irt::connection_pack{ .parent_port     = cg_output_port_id,
                                        .child_port      = p_out,
                                        .child_component = compo_id });
            });

            expect(pj.set(mod, root_id).has_value());

            nb_sum_model      = 0;
            nb_counter_model  = 0;
            nb_constant_model = 0;
            nb_unknown_model  = 0;

            for (const auto& mdl : pj.sim.models) {
                if (mdl.type == irt::dynamics_type::constant)
                    ++nb_constant_model;
                else if (mdl.type == irt::dynamics_type::counter)
                    ++nb_counter_model;
                else if (mdl.type == irt::dynamics_type::qss3_sum_4)
                    ++nb_sum_model;
                else
                    ++nb_unknown_model;
            }

            expect(eq(nb_sum_model, 0));
            expect(eq(nb_counter_model, 5 * 5 + 1));
            expect(eq(nb_constant_model, 5 * 5));
            expect(eq(nb_unknown_model, 0));
            expect(eq(get_connection_number(pj.sim),
                      9u * 4u      // The 3x3 center models with 4 connections
                        + 4u * 2u  // The 4 corner models with 2 connections
                        + 12u * 3u // The 12 border models with 3 connections
                        + 25u      // The connection-pack.
                      ));

            mod.ids.read([&](const auto& ids, auto) noexcept {
                expect(
                  eq(get_input_connection_number(
                       pj.sim, pj.tn_head()->children[counter_child_id].mdl, 0),
                     25u));

                auto& cg = ids.components[cg_id];

                // We replace the @c port_option::classic component output port
                // to a @c port_option::sum.
                expect(cg.y.template get<irt::port_option>(cg_output_port_id) ==
                       irt::port_option::classic);
            });

            mod.ids.write([&](auto& ids) noexcept {
                auto& cg = ids.components[cg_id];

                expect(cg.y.template get<irt::port_option>(cg_output_port_id) ==
                       irt::port_option::classic);

                cg.y.template get<irt::port_option>(cg_output_port_id) =
                  irt::port_option::sum;

                expect(cg.y.template get<irt::port_option>(cg_output_port_id) ==
                       irt::port_option::sum);
            });

            expect(pj.set(mod, root_id).has_value());

            nb_sum_model      = 0;
            nb_counter_model  = 0;
            nb_constant_model = 0;
            nb_unknown_model  = 0;

            for (const auto& mdl : pj.sim.models) {
                if (mdl.type == irt::dynamics_type::constant)
                    ++nb_constant_model;
                else if (mdl.type == irt::dynamics_type::counter)
                    ++nb_counter_model;
                else if (mdl.type == irt::dynamics_type::qss3_sum_4)
                    ++nb_sum_model;
                else
                    ++nb_unknown_model;
            }

            expect(eq(nb_sum_model, 25 / 3 + 1));
            expect(eq(nb_counter_model, 5 * 5 + 1));
            expect(eq(nb_constant_model, 5 * 5));
            expect(eq(nb_unknown_model, 0));
            expect(eq(get_connection_number(pj.sim),
                      9u * 4u      // The 3x3 center models with 4 connections
                        + 4u * 2u  // The 4 corner models with 2 connections
                        + 12u * 3u // The 12 border models with 3 connections
                        + 25u      // The connection-pack.
                        + 25u / 3u + 1u // The 9 sum models
                      ));

            expect(
              eq(get_input_connection_number(
                   pj.sim, pj.tn_head()->children[counter_child_id].mdl, 0),
                 1u));
        }
    };

    "graph-dot-m-n-ports"_test = [] {
        //         component
        //      +----------------+
        //      | +----+  +----+ |
        //    m | |cnt |  | cst| |m
        //    --+>|    |  |    +-+>
        //      | +----+  +----+ |
        //      |                |
        //    n | +----+  +----+ |n
        //    --+>|cnt |  | cst+-+>
        //      | |    |  |    | |
        //      | +----+  +----+ |
        //      +----------------+

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    cg_id{ 0 };
        irt::component_id    compo_id{ 0 };

        mod.ids.write([&](auto& ids) {
            compo_id    = ids.alloc_generic_component();
            auto& compo = ids.components[compo_id];
            auto& gen   = ids.generic_components.get(compo.id.generic_id);

            auto& ch_m_ct  = gen.alloc(irt::dynamics_type::counter);
            auto& ch_m_cst = gen.alloc(irt::dynamics_type::constant);
            auto& ch_n_ct  = gen.alloc(irt::dynamics_type::counter);
            auto& ch_n_cst = gen.alloc(irt::dynamics_type::constant);

            auto p_m_in  = compo.get_or_add_x("m");
            auto p_m_out = compo.get_or_add_y("m");
            auto p_n_in  = compo.get_or_add_x("n");
            auto p_n_out = compo.get_or_add_y("n");

            expect(gen
                     .connect_input(
                       p_m_in, ch_m_ct, irt::connection::port{ .model = 0 })
                     .has_value());
            expect(gen
                     .connect_input(
                       p_n_in, ch_n_ct, irt::connection::port{ .model = 0 })
                     .has_value());

            expect(gen
                     .connect_output(
                       p_m_out, ch_m_cst, irt::connection::port{ .model = 0 })
                     .has_value());
            expect(gen
                     .connect_output(
                       p_n_out, ch_n_cst, irt::connection::port{ .model = 0 })
                     .has_value());

            cg_id = ids.alloc_graph_component();
        });

        const std::string_view buf = R"(digraph D {
            A
            B
            C
            A -- B
            B -- C
            C -- A
        })";

        auto ret = irt_parse_dot_buffer(mod, buf);
        expect(ret.has_value() >> fatal);

        expect(eq(ret->nodes.size(), 3u));

        const auto table = ret->make_toc();
        expect(eq(table.ssize(), 3));

        expect(table.get("A"sv) >> fatal);
        expect(table.get("B"sv) >> fatal);
        expect(table.get("C"sv) >> fatal);

        irt::u32 edge_size{ 0 };

        mod.ids.write([&](auto& ids) {
            auto& cg = ids.components[cg_id];
            auto& g  = ids.graph_components.get(cg.id.graph_id);
            g.g      = *ret;

            for (const auto id : g.g.nodes)
                g.g.node_components[id] = compo_id;

            g.type = irt::graph_component::connection_type::name;
            g.g.flags.reset(irt::graph::option_flags::directed);
            edge_size = g.g.edges.size() * 2u * 2u;
        });

        expect(pj.set(mod, cg_id).has_value());
        expect(eq(pj.sim.models.ssize(), 3 * 4));
        expect(eq(get_connection_number(pj.sim), edge_size));
    };

    "graph-dot-m-n-ports-sum-port"_test = [] {
        //         component
        //      +----------------+
        //      | +----+  +----+ |
        //    m | |cnt |  | cst| |m
        //    --+>|    |  |    +-+>
        //      | +----+  +----+ |
        //      |                |
        //    n | +----+  +----+ |n
        //    --+>|cnt |  | cst+-+>
        //      | |    |  |    | |
        //      | +----+  +----+ |
        //      +----------------+

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    compo_id{ 0 };
        irt::component_id    cg_id{ 0 };

        mod.ids.write([&](auto& ids) noexcept {
            compo_id    = ids.alloc_generic_component();
            auto& compo = ids.components[compo_id];
            auto& gen   = ids.generic_components.get(compo.id.generic_id);

            auto& ch_m_ct  = gen.alloc(irt::dynamics_type::counter);
            auto& ch_m_cst = gen.alloc(irt::dynamics_type::constant);
            auto& ch_n_ct  = gen.alloc(irt::dynamics_type::counter);
            auto& ch_n_cst = gen.alloc(irt::dynamics_type::constant);

            auto p_m_in  = compo.get_or_add_x("m");
            auto p_m_out = compo.get_or_add_y("m");
            auto p_n_in  = compo.get_or_add_x("n");
            auto p_n_out = compo.get_or_add_y("n");

            compo.x.template get<irt::port_option>(p_m_in) =
              irt::port_option::sum;
            compo.x.template get<irt::port_option>(p_n_in) =
              irt::port_option::sum;

            expect(gen
                     .connect_input(
                       p_m_in, ch_m_ct, irt::connection::port{ .model = 0 })
                     .has_value());
            expect(gen
                     .connect_input(
                       p_n_in, ch_n_ct, irt::connection::port{ .model = 0 })
                     .has_value());

            expect(gen
                     .connect_output(
                       p_m_out, ch_m_cst, irt::connection::port{ .model = 0 })
                     .has_value());
            expect(gen
                     .connect_output(
                       p_n_out, ch_n_cst, irt::connection::port{ .model = 0 })
                     .has_value());

            cg_id = ids.alloc_graph_component();
        });

        const std::string_view buf = R"(digraph D {
            A
            B
            C
            D
            E
            F
            A -- F
            B -- F
            C -- F
            D -- F
            E -- F
        })";

        auto ret = irt_parse_dot_buffer(mod, buf);
        expect(ret.has_value() >> fatal);

        expect(eq(ret->nodes.size(), 6u));

        const auto table = ret->make_toc();
        expect(eq(table.ssize(), 6));

        expect(table.get("A"sv) >> fatal);
        expect(table.get("B"sv) >> fatal);
        expect(table.get("C"sv) >> fatal);

        irt::u32 edge_size = 0;
        mod.ids.write([&](auto& ids) {
            auto& cg = ids.components[cg_id];
            auto& g  = ids.graph_components.get(cg.id.graph_id);

            g.g = *ret;

            for (const auto id : g.g.nodes)
                g.g.node_components[id] = compo_id;

            g.type    = irt::graph_component::connection_type::name;
            edge_size = g.g.edges.size();
        });

        expect(pj.set(mod, cg_id).has_value());

        // Six components plus 2 automatic 4 sum models (5 input models
        // A,
        // .., E for port m and 2 for port n.

        expect(eq(pj.sim.models.ssize(), 6 * 4 + 2 + 2));

        // 5 edges + 2 edge for sum models for port m and n.

        expect(eq(get_connection_number(pj.sim), (edge_size + 2u) * 2u));
    };

    "graph-dot-m-n-ports-sum-port-sum-output"_test = [] {
        //      compo component
        //   +----------------+
        //   | +----+  +----+ |
        // m | |cnt |  | cst| |m
        // --+>|    |  |    +-+>
        //   | +----+  +----+ |
        //   |                |
        // n | +----+  +----+ |n
        // --+>|cnt |  | cst+-+>
        //   | |    |  |    | |
        //   | +----+  +----+ |
        //   +----------------+
        //
        //
        //  +------------------+
        //  | +-+              |
        //  | |A++-------+     |
        //  | +-+|       |     |
        //  |    |       |     |
        //  | +-+|       |     |
        //  | |B+++------+     |
        //  | +-+||      |     |
        //  |    ||      |     |
        //  | +-+||      v+-+  |
        //  | |C++++----->|F|  |
        //  | +-+|||     ^+++  |
        //  |    |||     | |   |
        //  | +-+|||     | |   |
        //  | |D+++++----+ |   |
        //  | +-+||||    | |   |
        //  |    ||||    | |   |
        //  | +-+||||    | |   |
        //  | |E+++++----++|   |
        //  | +-+||||     ||   |
        //  +----++++-----++---+
        //       ||||     ||
        //       vvvv     ||
        //       +---+    ||
        //       |sum|    ||
        //       +--++    ||
        //          +---> vv
        //              +---+    +-------+
        //              |sum+---->counter|
        //              +---+    +-------+

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;
        irt::component_id    compo_id{ 0 };
        irt::component_id    cg_id{ 0 };
        irt::component_id    head_id{ 0 };
        irt::port_id         p_cg_m_out{ 0 };

        mod.ids.write([&](auto& ids) noexcept {
            compo_id    = ids.alloc_generic_component();
            auto& compo = ids.components[compo_id];
            auto& gen   = ids.generic_components.get(compo.id.generic_id);

            auto& ch_m_ct  = gen.alloc(irt::dynamics_type::counter);
            auto& ch_m_cst = gen.alloc(irt::dynamics_type::constant);
            auto& ch_n_ct  = gen.alloc(irt::dynamics_type::counter);
            auto& ch_n_cst = gen.alloc(irt::dynamics_type::constant);

            auto p_m_in  = compo.get_or_add_x("m");
            auto p_m_out = compo.get_or_add_y("m");
            auto p_n_in  = compo.get_or_add_x("n");
            auto p_n_out = compo.get_or_add_y("n");

            compo.x.template get<irt::port_option>(p_m_in) =
              irt::port_option::sum;
            compo.x.template get<irt::port_option>(p_n_in) =
              irt::port_option::sum;

            expect(gen
                     .connect_input(
                       p_m_in, ch_m_ct, irt::connection::port{ .model = 0 })
                     .has_value());
            expect(gen
                     .connect_input(
                       p_n_in, ch_n_ct, irt::connection::port{ .model = 0 })
                     .has_value());

            expect(gen
                     .connect_output(
                       p_m_out, ch_m_cst, irt::connection::port{ .model = 0 })
                     .has_value());
            expect(gen
                     .connect_output(
                       p_n_out, ch_n_cst, irt::connection::port{ .model = 0 })
                     .has_value());

            cg_id    = ids.alloc_graph_component();
            auto& cg = ids.components[cg_id];

            p_cg_m_out = cg.get_or_add_y("m");

            cg.y.template get<irt::port_option>(p_m_in) = irt::port_option::sum;

            cg.output_connection_pack.push_back(
              irt::connection_pack{ .parent_port     = p_cg_m_out,
                                    .child_port      = p_m_out,
                                    .child_component = compo_id });
        });

        const std::string_view buf = R"(digraph D {
            A
            B
            C
            D
            E
            F
            A -- F
            B -- F
            C -- F
            D -- F
            E -- F
        })";

        auto ret = irt_parse_dot_buffer(mod, buf);
        expect(ret.has_value() >> fatal);

        expect(eq(ret->nodes.size(), 6u));

        const auto table = ret->make_toc();
        expect(eq(table.ssize(), 6));

        expect(table.get("A"sv) >> fatal);
        expect(table.get("B"sv) >> fatal);
        expect(table.get("C"sv) >> fatal);

        irt::u32 edge_size{ 0 };

        mod.ids.write([&](auto& ids) noexcept {
            auto& cg = ids.components[cg_id];
            auto& g  = ids.graph_components.get(cg.id.graph_id);

            g.g = *ret;

            for (const auto id : g.g.nodes)
                g.g.node_components[id] = compo_id;

            g.type = irt::graph_component::connection_type::name;

            // Finally we build

            head_id        = ids.alloc_generic_component();
            auto& head     = ids.components[head_id];
            auto& gen_head = ids.generic_components.get(head.id.generic_id);
            auto& ch_head_graph = gen_head.alloc(cg_id);
            auto& ch_head_cnt   = gen_head.alloc(irt::dynamics_type::counter);

            expect(gen_head
                     .connect(mod,
                              ch_head_graph,
                              irt::connection::port{ .compo = p_cg_m_out },
                              ch_head_cnt,
                              irt::connection::port{ .model = 0 })
                     .has_value());

            edge_size = g.g.edges.size();
        });

        expect(pj.set(mod, head_id).has_value());

        // Six components plus 2 automatic 4 sum models (5 input models
        // A,
        // .., E for port m and 2 for port n + 2 output sum models and 1
        // counter.
        expect(eq(pj.sim.models.ssize(), 6 * 4 + 2 + 2 + 2 + 1));

        // 5 edges + 2 edge for sum models for port m and n + 6 edges to
        // sum models and 1 edge between sum models and finally on 1edge
        // from sum model to counter.
        expect(eq(get_connection_number(pj.sim), (edge_size + 2u) * 2u) + 8u);
    };
}
