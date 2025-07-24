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
#include <fstream>
#include <numeric>

#include <boost/ut.hpp>

#include <fmt/format.h>

using namespace std::literals;

static auto get_connection_number(
  const irt::data_array<irt::block_node, irt::block_node_id>& data) noexcept
{
    return std::accumulate(data.begin(),
                           data.end(),
                           std::size_t(0u),
                           [](std::size_t acc, const irt::block_node& elem) {
                               return acc + elem.nodes.size();
                           });
}

static auto get_input_connection_number(
  const irt::data_array<irt::block_node, irt::block_node_id>& data,
  const irt::model_id                                         id,
  const int                                                   port) noexcept
{
    std::size_t sum = 0;

    for (const auto& block : data) {
        sum += std::ranges::count_if(block.nodes, [&](const irt::node& elem) {
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
        auto p = std::filesystem::current_path(ec) / "reg-temp";
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

    "easy"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        auto& c1    = mod.alloc_generic_component();
        auto& s1    = mod.generic_components.get(c1.id.generic_id);
        auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
        auto  p1_id = c1.get_or_add_x("in");
        expect(
          !!s1.connect_input(p1_id, ch1, irt::connection::port{ .model = 0 }));

        auto& c2    = mod.alloc_generic_component();
        auto& s2    = mod.generic_components.get(c2.id.generic_id);
        auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
        auto  p2_id = c2.get_or_add_y("out");
        expect(
          !!s2.connect_output(p2_id, ch2, irt::connection::port{ .model = 0 }));

        auto& c3   = mod.alloc_generic_component();
        auto& s3   = mod.generic_components.get(c3.id.generic_id);
        auto& ch31 = mod.alloc(s3, mod.components.get_id(c2));
        auto& ch32 = mod.alloc(s3, mod.components.get_id(c1));
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

        expect(!!pj.set(mod, c3));
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

        auto& c1 = mod.alloc_generic_component();
        auto& s1 = mod.generic_components.get(c1.id.generic_id);
        mod.alloc(s1, irt::dynamics_type::counter);

        auto& c2 = mod.alloc_generic_component();
        auto& s2 = mod.generic_components.get(c2.id.generic_id);
        mod.alloc(s2, irt::dynamics_type::time_func);

        auto& c3 = mod.alloc_generic_component();
        auto& s3 = mod.generic_components.get(c3.id.generic_id);
        mod.alloc(s3, mod.components.get_id(c2));
        mod.alloc(s3, mod.components.get_id(c1));

        expect(!!pj.set(mod, c3));
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

        auto& c1 = mod.alloc_generic_component();
        auto& s1 = mod.generic_components.get(c1.id.generic_id);
        mod.alloc(s1, irt::dynamics_type::counter);
        auto p1_id = c1.get_or_add_x("in");

        auto& c11    = mod.alloc_generic_component();
        auto& s11    = mod.generic_components.get(c11.id.generic_id);
        auto& ch11   = mod.alloc(s11, mod.components.get_id(c1));
        auto  p11_id = c11.get_or_add_x("in");

        expect(!!s11.connect_input(
          p11_id, ch11, irt::connection::port{ .compo = p1_id }));

        auto& c2 = mod.alloc_generic_component();
        auto& s2 = mod.generic_components.get(c2.id.generic_id);
        mod.alloc(s2, irt::dynamics_type::time_func);
        auto p2_id = c2.get_or_add_y("out");

        auto& c22    = mod.alloc_generic_component();
        auto& s22    = mod.generic_components.get(c22.id.generic_id);
        auto& ch22   = mod.alloc(s22, mod.components.get_id(c2));
        auto  p22_id = c22.get_or_add_y("out");

        expect(!!s22.connect_output(
          p22_id, ch22, irt::connection::port{ .compo = p2_id }));

        auto& c3   = mod.alloc_generic_component();
        auto& s3   = mod.generic_components.get(c3.id.generic_id);
        auto& ch31 = mod.alloc(s3, mod.components.get_id(c22));
        auto& ch32 = mod.alloc(s3, mod.components.get_id(c11));

        expect(!!s3.connect(mod,
                            ch31,
                            irt::connection::port{ .compo = p22_id },
                            ch32,
                            irt::connection::port{ .compo = p11_id }));

        expect(!!pj.set(mod, c3));
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

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        auto& cg            = mod.alloc_graph_component();
        auto& g             = mod.graph_components.get(cg.id.graph_id);
        g.g_type            = irt::graph_component::graph_type::small_world;
        g.type              = irt::graph_component::connection_type::in_out;
        g.param.small       = irt::graph_component::small_world_param{};
        g.param.small.nodes = 25;
        g.param.small.id    = mod.components.get_id(c);

        expect(!!pj.set(mod, cg));
        expect(eq(pj.tree_nodes_size().first, g.g.nodes.ssize() + 1));
        expect(eq(pj.sim.models.ssize(), g.g.nodes.ssize()));
    };

    "graph-scale-free"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        auto& cg            = mod.alloc_graph_component();
        auto& g             = mod.graph_components.get(cg.id.graph_id);
        g.g_type            = irt::graph_component::graph_type::scale_free;
        g.type              = irt::graph_component::connection_type::in_out;
        g.param.scale       = irt::graph_component::scale_free_param{};
        g.param.scale.nodes = 25;
        g.param.scale.id    = mod.components.get_id(c);

        expect(!!pj.set(mod, cg));
        expect(eq(pj.tree_nodes_size().first, g.g.nodes.ssize() + 1));
        expect(eq(pj.sim.models.ssize(), g.g.nodes.ssize()));
    };

    "graph-scale-free-sum-in-out"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        auto& c     = mod.alloc_generic_component();
        auto& s     = mod.generic_components.get(c.id.generic_id);
        auto& child = mod.alloc(s, irt::dynamics_type::qss1_sum_2);

        const auto port_in  = c.get_or_add_x("in");
        const auto port_out = c.get_or_add_y("out");

        expect(!!s.connect_input(
          port_in, child, irt::connection::port{ .model = 0 }));
        expect(!!s.connect_output(
          port_out, child, irt::connection::port{ .model = 0 }));

        auto& cg            = mod.alloc_graph_component();
        auto& g             = mod.graph_components.get(cg.id.graph_id);
        g.g_type            = irt::graph_component::graph_type::scale_free;
        g.type              = irt::graph_component::connection_type::in_out;
        g.param.scale       = irt::graph_component::scale_free_param{};
        g.param.scale.alpha = 2.5;
        g.param.scale.beta  = 1.e3;
        g.param.scale.id    = mod.components.get_id(c);
        g.param.scale.nodes = 64;

        expect(!!pj.set(mod, cg));
        expect(eq(pj.tree_nodes_size().first, g.g.nodes.ssize() + 1));
        expect(eq(pj.sim.models.ssize(), g.g.nodes.ssize()));
        expect(eq(get_connection_number(pj.sim.nodes), g.g.edges.size()));
    };

    "graph-scale-free-sum-m-n"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        auto& c     = mod.alloc_generic_component();
        auto& s     = mod.generic_components.get(c.id.generic_id);
        auto& child = mod.alloc(s, irt::dynamics_type::qss1_sum_2);

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

        auto& cg            = mod.alloc_graph_component();
        auto& g             = mod.graph_components.get(cg.id.graph_id);
        g.g_type            = irt::graph_component::graph_type::scale_free;
        g.type              = irt::graph_component::connection_type::name;
        g.param.scale       = irt::graph_component::scale_free_param{};
        g.param.scale.alpha = 2.5;
        g.param.scale.beta  = 1.e3;
        g.param.scale.id    = mod.components.get_id(c);
        g.param.scale.nodes = 64;

        expect(!!pj.set(mod, cg));
        expect(eq(pj.tree_nodes_size().first, g.g.nodes.ssize() + 1));
        expect(eq(pj.sim.models.ssize(), g.g.nodes.ssize()));
        expect(eq(get_connection_number(pj.sim.nodes), 2u * g.g.edges.size()));
    };

    "graph-scale-free-sum-m_3-n_3"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        auto& c     = mod.alloc_generic_component();
        auto& s     = mod.generic_components.get(c.id.generic_id);
        auto& child = mod.alloc(s, irt::dynamics_type::qss1_sum_2);

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

        auto& cg      = mod.alloc_graph_component();
        auto& g       = mod.graph_components.get(cg.id.graph_id);
        g.g_type      = irt::graph_component::graph_type::scale_free;
        g.type        = irt::graph_component::connection_type::name_suffix;
        g.param.scale = irt::graph_component::scale_free_param{};
        g.param.scale.alpha = 3;
        g.param.scale.beta  = 1.e3;
        g.param.scale.id    = mod.components.get_id(c);
        g.param.scale.nodes = 16;

        expect(!!pj.set(mod, cg));
        expect(eq(pj.tree_nodes_size().first, g.g.nodes.ssize() + 1));
        expect(eq(pj.sim.models.ssize(), g.g.nodes.ssize()));
        expect(eq(get_connection_number(pj.sim.nodes), 2 * g.g.edges.size()));
    };

    "grid-3x3-empty-con"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        auto& cg = mod.alloc_grid_component();
        auto& g  = mod.grid_components.get(cg.id.grid_id);
        g.resize(5, 5, mod.components.get_id(c));

        expect(!!pj.set(mod, cg));
        expect(eq(pj.tree_nodes_size().first, g.cells_number() + 1));
        expect(eq(pj.sim.models.ssize(), g.cells_number()));
    };

    "grid-3x3-empty-con-middle"_test = [] {
        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        auto& c = mod.alloc_generic_component();
        auto& s = mod.generic_components.get(c.id.generic_id);
        mod.alloc(s, irt::dynamics_type::counter);

        auto& cg = mod.alloc_grid_component();
        auto& g  = mod.grid_components.get(cg.id.grid_id);
        g.resize(5, 5, irt::undefined<irt::component_id>());

        for (int i = 1; i < 4; ++i)
            for (int j = 1; j < 4; ++j)
                g.children()[g.pos(i, j)] = mod.components.get_id(c);

        expect(!!pj.set(mod, cg));
        expect(
          eq(pj.tree_nodes_size().first, (g.row() - 2) * (g.column() - 2) + 1));
        expect(eq(pj.sim.models.ssize(), (g.row() - 2) * (g.column() - 2)));
    };

    "grid-3x3"_test = [] {
        irt::vector<char>       buffer;
        irt::registred_path_str temp_path;

        expect(get_temp_registred_path(temp_path) == true);

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& c1    = mod.alloc_generic_component();
            auto& s1    = mod.generic_components.get(c1.id.generic_id);
            auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
            auto  p1_id = c1.get_or_add_x("in");
            expect(!!s1.connect_input(
              p1_id, ch1, irt::connection::port{ .model = 0 }));

            auto& c2    = mod.alloc_generic_component();
            auto& s2    = mod.generic_components.get(c2.id.generic_id);
            auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
            auto  p2_id = c2.get_or_add_y("out");
            expect(!!s2.connect_output(
              p2_id, ch2, irt::connection::port{ .model = 0 }));

            auto& c3   = mod.alloc_generic_component();
            auto& s3   = mod.generic_components.get(c3.id.generic_id);
            auto& ch31 = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch32 = mod.alloc(s3, mod.components.get_id(c1));
            expect(!!s3.connect(mod,
                                ch31,
                                irt::connection::port{ .compo = p2_id },
                                ch32,
                                irt::connection::port{ .compo = p1_id }));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));

            auto& reg     = mod.alloc_registred("temp", 0);
            auto& dir     = mod.alloc_dir(reg);
            auto& file_c1 = mod.alloc_file(dir);
            auto& file_c2 = mod.alloc_file(dir);
            auto& file_c3 = mod.alloc_file(dir);
            auto& file_cg = mod.alloc_file(dir);
            reg.path      = temp_path;
            dir.path      = "test";

            mod.create_directories(reg);
            mod.create_directories(dir);

            file_c1.path = "c1.irt";
            file_c2.path = "c2.irt";
            file_c3.path = "c3.irt";
            file_cg.path = "cg.irt";

            const auto reg_id = mod.registred_paths.get_id(reg);
            const auto dir_id = mod.dir_paths.get_id(dir);

            expect(mod.registred_paths.try_to_get(reg_id) != nullptr);
            expect(mod.dir_paths.try_to_get(dir_id) != nullptr);

            c1.reg_path = reg_id;
            c1.dir      = dir_id;
            c1.file     = mod.file_paths.get_id(file_c1);

            c2.reg_path = reg_id;
            c2.dir      = dir_id;
            c2.file     = mod.file_paths.get_id(file_c2);

            c3.reg_path = reg_id;
            c3.dir      = dir_id;
            c3.file     = mod.file_paths.get_id(file_c3);

            cg.reg_path = reg_id;
            cg.dir      = dir_id;
            cg.file     = mod.file_paths.get_id(file_cg);

            expect(!!mod.save(c1));

            expect(!!mod.save(c2));

            expect(!!mod.save(c3));
            expect(!!mod.save(cg));

            expect(!!pj.set(mod, cg));
            expect(eq(pj.tree_nodes_size().first, g.cells_number() * 3 + 1));

            expect(eq(pj.sim.models.ssize(), g.cells_number() * 2));

            irt::json_archiver j;
            expect(j(pj,
                     mod,
                     pj.sim,
                     buffer,
                     irt::json_archiver::print_option::indent_2_one_line_array)
                     .has_value());
        }

        expect(buffer.size() > 0u);
        fmt::print("Buffer: {:{}}\n", buffer.data(), buffer.size());

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& reg = mod.alloc_registred("temp", 0);
            reg.path  = temp_path;

            mod.create_directories(reg);

            auto old_cb = std::exchange(irt::on_error_callback, nullptr);
            expect(!!mod.fill_components());
            std::exchange(irt::on_error_callback, old_cb);

            irt::json_dearchiver j;
            expect(j(pj, mod, pj.sim, std::span(buffer.data(), buffer.size()))
                     .has_value());
        }
    };

    "hsm"_test = [] {
        irt::project         pj;
        irt::journal_handler jn;
        irt::modeling        mod{ jn };

        expect(mod.hsm_components.can_alloc(1)) << fatal;
        expect(mod.components.can_alloc(1)) << fatal;

        auto& compo = mod.alloc_hsm_component();
        auto& hsm   = mod.hsm_components.get(compo.id.hsm_id);

        expect(!!hsm.machine.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsm.machine.set_state(1u, 0u));
        hsm.machine.states[1u].condition.set(0b0011u, 0b0011u);
        hsm.machine.states[1u].if_transition = 2u;

        expect(!!hsm.machine.set_state(2u, 0u));
        hsm.machine.states[2u].enter_action.set_output(
          irt::hierarchical_state_machine::variable::port_0, 1.0f);

        expect(!!pj.set(mod, compo));

        pj.sim.t = 0.0;
        expect(!!pj.sim.srcs.prepare());
        expect(!!pj.sim.initialize());

        irt::status st;

        do {
            st = pj.sim.run();
            expect(!!st);
        } while (pj.sim.t < 10);
    };

    "internal_component_io"_test = [] {
        {
            irt::journal_handler jn{};
            irt::modeling        mod{ jn };

            std::array<irt::component_id, irt::internal_component_count> ids;

            mod.registred_paths.reserve(8);
            mod.dir_paths.reserve(32);
            mod.file_paths.reserve(256);

            expect(mod.registred_paths.can_alloc(8));
            expect(mod.dir_paths.can_alloc(32));
            expect(mod.file_paths.can_alloc(256));

            expect(mod.components.can_alloc(irt::internal_component_count));

            auto& reg    = mod.alloc_registred("temp", 0);
            auto  reg_id = mod.registred_paths.get_id(reg);
            get_temp_registred_path(reg.path);
            mod.create_directories(reg);

            auto& dir    = mod.alloc_dir(reg);
            auto  dir_id = mod.dir_paths.get_id(dir);
            dir.path     = "dir-temp";
            mod.create_directories(dir);

            for (int i = 0, e = irt::internal_component_count; i != e; ++i) {
                auto& file     = mod.alloc_file(dir);
                auto  file_id  = mod.file_paths.get_id(file);
                file.component = irt::undefined<irt::component_id>();
                irt::format(
                  file.path, "{}.irt", irt::internal_component_names[i]);

                auto& c    = mod.alloc_generic_component();
                auto& g    = mod.generic_components.get(c.id.generic_id);
                auto  c_id = mod.components.get_id(c);
                c.reg_path = reg_id;
                c.dir      = dir_id;
                c.file     = file_id;

                expect(
                  mod.copy(irt::enum_cast<irt::internal_component>(i), c, g)
                    .has_value());
                ids[i] = c_id;
            }

            for (int i = 0, e = irt::internal_component_count; i != e; ++i) {
                auto* c = mod.components.try_to_get<irt::component>(ids[i]);
                expect(fatal(c != nullptr));
                expect(mod.save(*c).has_value());
            }

            expect(mod.components.ssize() == irt::internal_component_count);
        }

        {
            irt::journal_handler jn{};
            irt::modeling        mod{ jn };

            mod.registred_paths.reserve(8);
            mod.dir_paths.reserve(32);
            mod.file_paths.reserve(256);

            expect(mod.registred_paths.can_alloc(8));
            expect(mod.dir_paths.can_alloc(32));
            expect(mod.file_paths.can_alloc(256));

            expect(mod.components.can_alloc(irt::internal_component_count));

            auto& reg = mod.alloc_registred("temp", 0);
            get_temp_registred_path(reg.path);
            mod.create_directories(reg);

            auto old_cb = std::exchange(irt::on_error_callback, nullptr);
            expect(!!mod.fill_components());
            std::exchange(irt::on_error_callback, old_cb);
            expect(mod.components.ssize() >= irt::internal_component_count);
        }
    };

    "grid-3x3-constant-model-init-port-all"_test = [] {
        irt::vector<char> buffer;

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& c1  = mod.alloc_generic_component();
            auto& s1  = mod.generic_components.get(c1.id.generic_id);
            auto& ch1 = mod.alloc(s1, irt::dynamics_type::counter);

            auto p1_id = c1.get_or_add_x("in");
            expect(!!s1.connect_input(
              p1_id, ch1, irt::connection::port{ .model = 0 }));

            auto& c2    = mod.alloc_generic_component();
            auto& s2    = mod.generic_components.get(c2.id.generic_id);
            auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
            auto  p2_id = c2.get_or_add_y("out");
            expect(!!s2.connect_output(
              p2_id, ch2, irt::connection::port{ .model = 0 }));

            auto& c3     = mod.alloc_generic_component();
            auto& s3     = mod.generic_components.get(c3.id.generic_id);
            auto& ch3    = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch4    = mod.alloc(s3, mod.components.get_id(c1));
            auto& ch5    = mod.alloc(s3, irt::dynamics_type::constant);
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

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));
            g.opts                = irt::grid_component::options::none;
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;
            g.neighbors           = irt::grid_component::neighborhood::four;

            expect(!!pj.set(mod, cg));
            expect(gt(g.cache_connections.ssize(), 0));
            expect(eq(pj.tree_nodes_size().first, g.cells_number() * 3 + 1));

            expect(eq(pj.sim.models.ssize(), g.cells_number() * 3));

            int         nb_constant_model = 0;
            irt::model* cst_mdl           = nullptr;
            while (pj.sim.models.next(cst_mdl)) {
                if (cst_mdl->type == irt::dynamics_type::constant) {
                    ++nb_constant_model;
                    auto& dyn = irt::get_dyn<irt::constant>(*cst_mdl);
                    expect(
                      eq(ordinal(dyn.type),
                         ordinal(
                           irt::constant::init_type::incoming_component_all)));
                    expect(eq(dyn.port, 0u));
                    expect(neq(dyn.value, 0.0));
                }
            }

            expect(eq(nb_constant_model, g.cells_number()));
        }
    };

    "grid-3x3-constant-model-init-port-n"_test = [] {
        irt::vector<char> buffer;

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& compo_counter = mod.alloc_generic_component();
            auto& gen_counter =
              mod.generic_components.get(compo_counter.id.generic_id);
            auto& child_counter =
              mod.alloc(gen_counter, irt::dynamics_type::counter);
            auto compo_counter_in = compo_counter.get_or_add_x("in");
            expect(
              !!gen_counter.connect_input(compo_counter_in,
                                          child_counter,
                                          irt::connection::port{ .model = 0 }));

            auto& compo_timef = mod.alloc_generic_component();
            auto& gen_timef =
              mod.generic_components.get(compo_timef.id.generic_id);
            auto& child_timef =
              mod.alloc(gen_timef, irt::dynamics_type::time_func);
            auto compo_timef_out = compo_timef.get_or_add_y("out");
            expect(
              !!gen_timef.connect_output(compo_timef_out,
                                         child_timef,
                                         irt::connection::port{ .model = 0 }));

            auto& c3     = mod.alloc_generic_component();
            auto& s3     = mod.generic_components.get(c3.id.generic_id);
            auto& ch3    = mod.alloc(s3, mod.components.get_id(compo_timef));
            auto& ch4    = mod.alloc(s3, mod.components.get_id(compo_counter));
            auto& ch5    = mod.alloc(s3, irt::dynamics_type::constant);
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

            expect(
              !!s3.connect(mod,
                           ch3,
                           irt::connection::port{ .compo = compo_timef_out },
                           ch4,
                           irt::connection::port{ .compo = compo_counter_in }));

            expect(!!s3.connect_input(
              p31_id, ch4, irt::connection::port{ .compo = compo_counter_in }));
            expect(!!s3.connect_output(
              p32_id, ch3, irt::connection::port{ .compo = compo_timef_out }));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(c3));
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;

            expect(!!pj.set(mod, cg));
            expect(eq(pj.tree_nodes_size().first, g.cells_number() * 3 + 1));

            expect(eq(pj.sim.models.ssize(), g.cells_number() * 3));

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

            expect(eq(nb_constant_model, g.cells_number()));
        }
    };

    "grid-3x3-constant-model-init-port-empty"_test = [] {
        auto old_error_callback =
          std::exchange(irt::on_error_callback, nullptr);

        irt::journal_handler jn;
        irt::modeling        mod{ jn };
        irt::project         pj;

        auto& c1    = mod.alloc_generic_component();
        auto& s1    = mod.generic_components.get(c1.id.generic_id);
        auto& ch1   = mod.alloc(s1, irt::dynamics_type::counter);
        auto  p1_id = c1.get_or_add_x("in");
        expect(
          !!s1.connect_input(p1_id, ch1, irt::connection::port{ .model = 0 }));

        auto& c2    = mod.alloc_generic_component();
        auto& s2    = mod.generic_components.get(c2.id.generic_id);
        auto& ch2   = mod.alloc(s2, irt::dynamics_type::time_func);
        auto  p2_id = c2.get_or_add_y("out");
        expect(
          !!s2.connect_output(p2_id, ch2, irt::connection::port{ .model = 0 }));

        auto& c3  = mod.alloc_generic_component();
        auto& s3  = mod.generic_components.get(c3.id.generic_id);
        auto& ch3 = mod.alloc(s3, mod.components.get_id(c2));
        auto& ch4 = mod.alloc(s3, mod.components.get_id(c1));
        auto& ch5 = mod.alloc(s3, irt::dynamics_type::constant);

        const auto ch5_id = s3.children.get_id(ch5);
        auto&      p_ch5  = s3.children_parameters[ch5_id];
        p_ch5.reals[0]    = 0.0;
        p_ch5.integers[0] =
          ordinal(irt::constant::init_type::incoming_component_n);
        p_ch5.integers[1] = 17; // Impossible port

        expect(!!s3.connect(mod,
                            ch3,
                            irt::connection::port{ .compo = p2_id },
                            ch4,
                            irt::connection::port{ .compo = p1_id }));

        auto& cg = mod.alloc_grid_component();
        auto& g  = mod.grid_components.get(cg.id.grid_id);
        g.resize(5, 5, mod.components.get_id(c3));

        expect(!pj.set(
          mod,
          cg)); /* Fail to build the project since the constant
                   models can not be initialized with dyn.port equals to 17. */

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

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& compo = mod.alloc_generic_component();
            auto& gen   = mod.generic_components.get(compo.id.generic_id);

            auto& ch_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_cst = mod.alloc(gen, irt::dynamics_type::constant);

            auto p_in  = compo.get_or_add_x("in");
            auto p_out = compo.get_or_add_y("out");

            // Switches the counter component input port from @a classic to @a
            // sums. This change will add @a dynamics_type::qss3_sum4 and
            // connections.

            compo.x.get<irt::port_option>(p_in) = irt::port_option::sum;

            expect(compo.x.get<irt::port_option>(p_in) ==
                   irt::port_option::sum);

            expect(
              gen
                .connect_input(p_in, ch_ct, irt::connection::port{ .model = 0 })
                .has_value());

            expect(gen
                     .connect_output(
                       p_out, ch_cst, irt::connection::port{ .model = 0 })
                     .has_value());

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(compo));
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;
            g.neighbors           = irt::grid_component::neighborhood::four;

            expect(pj.set(mod, cg).has_value());

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

            expect(eq(nb_sum_model, g.cells_number() + 9));
            expect(eq(nb_counter_model, g.cells_number()));
            expect(eq(nb_constant_model, g.cells_number()));
            expect(eq(nb_unknown_model, 0));
        }
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

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& compo = mod.alloc_generic_component();
            auto& gen   = mod.generic_components.get(compo.id.generic_id);

            auto& ch_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_cst = mod.alloc(gen, irt::dynamics_type::constant);

            auto p_in  = compo.get_or_add_x("in");
            auto p_out = compo.get_or_add_y("out");

            // Switches the counter component input port from @a classic to @a
            // sums. This change will add @a dynamics_type::qss3_sum4 and
            // connections.

            compo.x.get<irt::port_option>(p_in) = irt::port_option::sum;

            expect(compo.x.get<irt::port_option>(p_in) ==
                   irt::port_option::sum);

            expect(
              gen
                .connect_input(p_in, ch_ct, irt::connection::port{ .model = 0 })
                .has_value());

            expect(gen
                     .connect_output(
                       p_out, ch_cst, irt::connection::port{ .model = 0 })
                     .has_value());

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(compo));
            g.in_connection_type  = irt::grid_component::type::in_out;
            g.out_connection_type = irt::grid_component::type::in_out;
            g.neighbors           = irt::grid_component::neighborhood::eight;

            expect(pj.set(mod, cg).has_value());

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
            expect(eq(nb_counter_model, g.cells_number()));
            expect(eq(nb_constant_model, g.cells_number()));
            expect(eq(nb_unknown_model, 0));
        }
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

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& compo = mod.alloc_generic_component();
            auto& gen   = mod.generic_components.get(compo.id.generic_id);

            auto& ch_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_cst = mod.alloc(gen, irt::dynamics_type::constant);

            auto p_in  = compo.get_or_add_x("in");
            auto p_out = compo.get_or_add_y("out");

            expect(
              gen
                .connect_input(p_in, ch_ct, irt::connection::port{ .model = 0 })
                .has_value());

            expect(gen
                     .connect_output(
                       p_out, ch_cst, irt::connection::port{ .model = 0 })
                     .has_value());

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.resize(5, 5, mod.components.get_id(compo));
            g.in_connection_type   = irt::grid_component::type::in_out;
            g.out_connection_type  = irt::grid_component::type::in_out;
            g.neighbors            = irt::grid_component::neighborhood::four;
            auto cg_output_port_id = cg.get_or_add_y("out");

            expect(
              g.connect_output(cg_output_port_id, 3, 3, p_out).has_value());

            auto& root     = mod.alloc_generic_component();
            auto& root_gen = mod.generic_components.get(root.id.generic_id);

            auto& grid_child = mod.alloc(root_gen, mod.components.get_id(cg));
            auto& counter_child =
              mod.alloc(root_gen, irt::dynamics_type::counter);

            expect(
              root_gen
                .connect(mod,
                         grid_child,
                         irt::connection::port{ .compo = cg_output_port_id },
                         counter_child,
                         irt::connection::port{ .model = 0 })
                .has_value());

            expect(pj.set(mod, root).has_value());

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
            expect(eq(get_connection_number(pj.sim.nodes),
                      9u * 4u      // The 3x3 center models with 4 connections
                        + 4u * 2u  // The 4 corner models with 2 connections
                        + 12u * 3u // The 12 border models with 3 connections
                        + 1u // The connection from component (3, 3) in grid to
                             // root counter model.
                      ));

            // We replace the output-connection in grid with a connection-pack.

            g.output_connections.clear();

            cg.output_connection_pack.push_back(irt::connection_pack{
              .parent_port     = cg_output_port_id,
              .child_port      = p_out,
              .child_component = mod.components.get_id(compo) });

            expect(pj.set(mod, root).has_value());

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
            expect(eq(get_connection_number(pj.sim.nodes),
                      9u * 4u      // The 3x3 center models with 4 connections
                        + 4u * 2u  // The 4 corner models with 2 connections
                        + 12u * 3u // The 12 border models with 3 connections
                        + 25u      // The connection-pack.
                      ));
            expect(eq(get_input_connection_number(
                        pj.sim.nodes,
                        pj.tn_head()
                          ->children[root_gen.children.get_id(counter_child)]
                          .mdl,
                        0),
                      25u));

            // We replace the @c port_option::classic component output port to a
            // @c port_option::sum.
            expect(cg.y.get<irt::port_option>(cg_output_port_id) ==
                   irt::port_option::classic);

            cg.y.get<irt::port_option>(cg_output_port_id) =
              irt::port_option::sum;

            expect(cg.y.get<irt::port_option>(cg_output_port_id) ==
                   irt::port_option::sum);

            expect(pj.set(mod, root).has_value());

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
            expect(eq(get_connection_number(pj.sim.nodes),
                      9u * 4u      // The 3x3 center models with 4 connections
                        + 4u * 2u  // The 4 corner models with 2 connections
                        + 12u * 3u // The 12 border models with 3 connections
                        + 25u      // The connection-pack.
                        + 25u / 3u + 1u // The 9 sum models
                      ));
            expect(eq(get_input_connection_number(
                        pj.sim.nodes,
                        pj.tn_head()
                          ->children[root_gen.children.get_id(counter_child)]
                          .mdl,
                        0),
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

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& compo = mod.alloc_generic_component();
            auto& gen   = mod.generic_components.get(compo.id.generic_id);

            auto& ch_m_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_m_cst = mod.alloc(gen, irt::dynamics_type::constant);
            auto& ch_n_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_n_cst = mod.alloc(gen, irt::dynamics_type::constant);

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

            auto& cg = mod.alloc_graph_component();
            auto& g  = mod.graph_components.get(cg.id.graph_id);

            const std::string_view buf = R"(digraph D {
            A
            B
            C
            A -- B
            B -- C
            C -- A
        })";

            auto ret = irt::parse_dot_buffer(mod, buf);
            expect(ret.has_value() >> fatal);

            expect(eq(ret->nodes.size(), 3u));

            const auto table = ret->make_toc();
            expect(eq(table.ssize(), 3));

            expect(table.get("A"sv) >> fatal);
            expect(table.get("B"sv) >> fatal);
            expect(table.get("C"sv) >> fatal);

            g.g = *ret;

            for (const auto id : g.g.nodes)
                g.g.node_components[id] = mod.components.get_id(compo);

            g.type = irt::graph_component::connection_type::name;
            g.g.flags.reset(irt::graph::option_flags::directed);

            expect(pj.set(mod, cg).has_value());
            expect(eq(pj.sim.models.ssize(), 3 * 4));
            expect(eq(get_connection_number(pj.sim.nodes),
                      g.g.edges.size() * 2u * 2u));
        }
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

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& compo = mod.alloc_generic_component();
            auto& gen   = mod.generic_components.get(compo.id.generic_id);

            auto& ch_m_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_m_cst = mod.alloc(gen, irt::dynamics_type::constant);
            auto& ch_n_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_n_cst = mod.alloc(gen, irt::dynamics_type::constant);

            auto p_m_in  = compo.get_or_add_x("m");
            auto p_m_out = compo.get_or_add_y("m");
            auto p_n_in  = compo.get_or_add_x("n");
            auto p_n_out = compo.get_or_add_y("n");

            compo.x.get<irt::port_option>(p_m_in) = irt::port_option::sum;
            compo.x.get<irt::port_option>(p_n_in) = irt::port_option::sum;

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

            auto& cg = mod.alloc_graph_component();
            auto& g  = mod.graph_components.get(cg.id.graph_id);

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

            auto ret = irt::parse_dot_buffer(mod, buf);
            expect(ret.has_value() >> fatal);

            expect(eq(ret->nodes.size(), 6u));

            const auto table = ret->make_toc();
            expect(eq(table.ssize(), 6));

            expect(table.get("A"sv) >> fatal);
            expect(table.get("B"sv) >> fatal);
            expect(table.get("C"sv) >> fatal);

            g.g = *ret;

            for (const auto id : g.g.nodes)
                g.g.node_components[id] = mod.components.get_id(compo);

            g.type = irt::graph_component::connection_type::name;

            expect(pj.set(mod, cg).has_value());

            // Six components plus 2 automatic 4 sum models (5 input models A,
            // .., E for port m and 2 for port n.

            expect(eq(pj.sim.models.ssize(), 6 * 4 + 2 + 2));

            // 5 edges + 2 edge for sum models for port m and n.

            expect(eq(get_connection_number(pj.sim.nodes),
                      (g.g.edges.size() + 2u) * 2u));
        }
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

        {
            irt::journal_handler jn;
            irt::modeling        mod{ jn };
            irt::project         pj;

            auto& compo = mod.alloc_generic_component();
            auto& gen   = mod.generic_components.get(compo.id.generic_id);

            auto& ch_m_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_m_cst = mod.alloc(gen, irt::dynamics_type::constant);
            auto& ch_n_ct  = mod.alloc(gen, irt::dynamics_type::counter);
            auto& ch_n_cst = mod.alloc(gen, irt::dynamics_type::constant);

            auto p_m_in  = compo.get_or_add_x("m");
            auto p_m_out = compo.get_or_add_y("m");
            auto p_n_in  = compo.get_or_add_x("n");
            auto p_n_out = compo.get_or_add_y("n");

            compo.x.get<irt::port_option>(p_m_in) = irt::port_option::sum;
            compo.x.get<irt::port_option>(p_n_in) = irt::port_option::sum;

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

            auto& cg = mod.alloc_graph_component();
            auto& g  = mod.graph_components.get(cg.id.graph_id);

            auto p_cg_m_out = cg.get_or_add_y("m");

            cg.y.get<irt::port_option>(p_m_in) = irt::port_option::sum;

            cg.output_connection_pack.push_back(irt::connection_pack{
              .parent_port     = p_cg_m_out,
              .child_port      = p_m_out,
              .child_component = mod.components.get_id(compo) });

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

            auto ret = irt::parse_dot_buffer(mod, buf);
            expect(ret.has_value() >> fatal);

            expect(eq(ret->nodes.size(), 6u));

            const auto table = ret->make_toc();
            expect(eq(table.ssize(), 6));

            expect(table.get("A"sv) >> fatal);
            expect(table.get("B"sv) >> fatal);
            expect(table.get("C"sv) >> fatal);

            g.g = *ret;

            for (const auto id : g.g.nodes)
                g.g.node_components[id] = mod.components.get_id(compo);

            g.type = irt::graph_component::connection_type::name;

            // Finally we build

            auto& head     = mod.alloc_generic_component();
            auto& gen_head = mod.generic_components.get(head.id.generic_id);
            auto& ch_head_graph =
              mod.alloc(gen_head, mod.components.get_id(cg));
            auto& ch_head_cnt =
              mod.alloc(gen_head, irt::dynamics_type::counter);

            expect(gen_head
                     .connect(mod,
                              ch_head_graph,
                              irt::connection::port{ .compo = p_cg_m_out },
                              ch_head_cnt,
                              irt::connection::port{ .model = 0 })
                     .has_value());

            expect(pj.set(mod, head).has_value());

            // Six components plus 2 automatic 4 sum models (5 input models A,
            // .., E for port m and 2 for port n + 2 output sum models and 1
            // counter.
            expect(eq(pj.sim.models.ssize(), 6 * 4 + 2 + 2 + 2 + 1));

            // 5 edges + 2 edge for sum models for port m and n + 6 edges to sum
            // models and 1 edge between sum models and finally on 1edge from
            // sum model to counter.
            expect(eq(get_connection_number(pj.sim.nodes),
                      (g.g.edges.size() + 2u) * 2u) +
                   8u);
        }
    };
}