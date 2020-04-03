// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <future>
#include <mutex>
#include <thread>

#include "imgui.h"
#include "imnodes.hpp"

#include "gui.hpp"

#include <fmt/format.h>

#include <irritator/core.hpp>

namespace irt {

enum class simulation_status
{
    success,
    running,
    uninitialized,
    internal_error,
};

static void
run_simulation(simulation& sim,
               double begin,
               double end,
               double obs_freq,
               double& current,
               simulation_status& st,
               std::vector<float>& obs_a,
               std::vector<float>& obs_b,
               double* a,
               double* b,
               const bool& stop) noexcept
{
    double last{ begin };
    size_t obs_id{ 0 };

    current = begin;
    if (irt::is_bad(sim.initialize(current))) {
        st = simulation_status::internal_error;
        return;
    }

    do {
        if (!is_success(sim.run(current))) {
            st = simulation_status::internal_error;
            return;
        }

        while (current - last > obs_freq &&
            obs_a.size() > (size_t)obs_id &&
            !stop) {
            obs_a[obs_id] = static_cast<float>(*a);
            obs_b[obs_id] = static_cast<float>(*b);
            last += obs_freq;
            obs_id++;
        }
    } while (current < end && !stop);

    st = simulation_status::success;
}

struct g_model
{
    g_model() = default;

    ImVec2 position{ 0.f, 0.f };
    int index{ -1 };
};

struct editor
{
    imnodes::EditorContext* context = nullptr;

    array<int> g_input_ports;
    array<int> g_output_ports;
    array<g_model> g_models;
    int current_model_id = 0;
    int current_port_id = 0;
    bool initialized = false;

    int get_model(const model_id id) const noexcept
    {
        return g_models[get_index(id)].index;
    }

    int get_in(const input_port_id id) const noexcept
    {
        return g_input_ports[get_index(id)];
    }

    int get_out(const output_port_id id) const noexcept
    {
        return g_output_ports[get_index(id)];
    }

    simulation sim;
    double simulation_begin = 0.0;
    double simulation_end = 10.0;
    double simulation_current = 10.0;
    int simulation_observation_number = 100;
    std::future<std::tuple<std::string, status>> future_content;
    std::thread simulation_thread;
    simulation_status st = simulation_status::uninitialized;
    bool show_simulation_box = true;
    double *value_a = nullptr, *value_b = nullptr;
    bool stop = false;

    std::vector<float> obs_a;
    std::vector<float> obs_b;

    bool initialize() noexcept
    {
        if (!is_success(sim.init(1024u, 32768u)))
            return false;

        if (!is_success(g_models.init(sim.models.capacity())))
            return false;

        if (!is_success(g_input_ports.init(sim.input_ports.capacity())))
            return false;

        if (!is_success(g_output_ports.init(sim.output_ports.capacity())))
            return false;

        initialized = true;

        return true;
    }

    template<typename Dynamics>
    status alloc(Dynamics& dynamics, dynamics_id dyn_id,
               const char* name = nullptr) noexcept
    {
        irt_return_if_bad(sim.alloc(dynamics, dyn_id, name));

        g_models[get_index(dynamics.id)].index = current_model_id++;

        if constexpr (is_detected_v<has_input_port_t, Dynamics>)
            for (size_t i = 0, e = std::size(dynamics.x); i != e; ++i)
                g_input_ports[get_index(dynamics.x[i])] = current_port_id++;

        if constexpr (is_detected_v<has_output_port_t, Dynamics>)
            for (size_t i = 0, e = std::size(dynamics.y); i != e; ++i)
                g_output_ports[get_index(dynamics.y[i])] = current_port_id++;

        printf("mdl %d port %d\n", current_model_id, current_port_id);

        return status::success;
    }

    status initialize_lotka_volterra() noexcept
    {
        if (!sim.adder_2_models.can_alloc(2) ||
            !sim.mult_2_models.can_alloc(2) ||
            !sim.integrator_models.can_alloc(2) ||
            !sim.quantifier_models.can_alloc(2) || !sim.models.can_alloc(10))
            return status::simulation_not_enough_model;

        auto& sum_a = sim.adder_2_models.alloc();
        auto& sum_b = sim.adder_2_models.alloc();
        auto& product = sim.mult_2_models.alloc();
        auto& integrator_a = sim.integrator_models.alloc();
        auto& integrator_b = sim.integrator_models.alloc();
        auto& quantifier_a = sim.quantifier_models.alloc();
        auto& quantifier_b = sim.quantifier_models.alloc();

        integrator_a.default_current_value = 18.0;

        quantifier_a.default_adapt_state = irt::quantifier::adapt_state::possible;
        quantifier_a.default_zero_init_offset = true;
        quantifier_a.default_step_size = 0.01;
        quantifier_a.default_past_length = 3;

        integrator_b.default_current_value = 7.0;

        quantifier_b.default_adapt_state = irt::quantifier::adapt_state::possible;
        quantifier_b.default_zero_init_offset = true;
        quantifier_b.default_step_size = 0.01;
        quantifier_b.default_past_length = 3;

        product.default_input_coeffs[0] = 1.0;
        product.default_input_coeffs[1] = 1.0;
        sum_a.default_input_coeffs[0] = 2.0;
        sum_a.default_input_coeffs[1] = -0.4;
        sum_b.default_input_coeffs[0] = -1.0;
        sum_b.default_input_coeffs[1] = 0.1;

        irt_return_if_bad(alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a"));
        irt_return_if_bad(alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b"));
        irt_return_if_bad(alloc(product, sim.mult_2_models.get_id(product), "prod"));
        irt_return_if_bad(alloc(integrator_a, sim.integrator_models.get_id(integrator_a), "int_a"));
        irt_return_if_bad(alloc(integrator_b, sim.integrator_models.get_id(integrator_b), "int_b"));
        irt_return_if_bad(alloc(quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a"));
        irt_return_if_bad(alloc(quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b"));

        irt_return_if_bad(sim.connect(sum_a.y[0], integrator_a.x[1]));
        irt_return_if_bad(sim.connect(sum_b.y[0], integrator_b.x[1]));

        irt_return_if_bad(sim.connect(integrator_a.y[0], sum_a.x[0]));
        irt_return_if_bad(sim.connect(integrator_b.y[0], sum_b.x[0]));

        irt_return_if_bad(sim.connect(integrator_a.y[0], product.x[0]));
        irt_return_if_bad(sim.connect(integrator_b.y[0], product.x[1]));

        irt_return_if_bad(sim.connect(product.y[0], sum_a.x[1]));
        irt_return_if_bad(sim.connect(product.y[0], sum_b.x[1]));

        irt_return_if_bad(sim.connect(quantifier_a.y[0], integrator_a.x[0]));
        irt_return_if_bad(sim.connect(quantifier_b.y[0], integrator_b.x[0]));
        irt_return_if_bad(sim.connect(integrator_a.y[0], quantifier_a.x[0]));
        irt_return_if_bad(sim.connect(integrator_b.y[0], quantifier_b.x[0]));

        value_a = &integrator_a.expected_value;
        value_b = &integrator_b.expected_value;

        return status::success;
    }
};

static void
show_connections(editor& ed) noexcept
{
    int number = 1;
    irt::output_port* output_port = nullptr;
    while (ed.sim.output_ports.next(output_port)) {
        output_port_id src = ed.sim.output_ports.get_id(output_port);

        for (const input_port_id id_dst : output_port->connections) {
            if (auto* input_port = ed.sim.input_ports.try_to_get(id_dst);
                input_port) {
                ++number;
                imnodes::Link(number, ed.get_out(src), ed.get_in(id_dst));
            }
        }
    }
}

static void
show_model_dynamics(editor& ed, model& mdl)
{
    switch (mdl.type) {
    case dynamics_type::none: /* none does not have input port. */
    case dynamics_type::cross:
    case dynamics_type::time_func:
        break;
    case dynamics_type::integrator: {
        auto& dyn = ed.sim.integrator_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("quanta");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[1]));
        ImGui::TextUnformatted("x_dot");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[2]));
        ImGui::TextUnformatted("reset");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("value", &dyn.default_current_value);
        ImGui::InputDouble("reset", &dyn.default_reset_value);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("x").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("quanta").x - text_width);
        ImGui::TextUnformatted("x");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::quantifier: {
        auto& dyn = ed.sim.quantifier_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("x_dot");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("quantum", &dyn.default_step_size);
        ImGui::SliderInt("archive length", &dyn.default_past_length, 3, 100);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        ImGui::TextUnformatted("quanta");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::adder_2: {
        auto& dyn = ed.sim.adder_2_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("sum").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("sum");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::adder_3: {
        auto& dyn = ed.sim.adder_3_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[2]));
        ImGui::TextUnformatted("x2");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
        ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("sum").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("sum");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::adder_4: {
        auto& dyn = ed.sim.adder_4_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[2]));
        ImGui::TextUnformatted("x2");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[3]));
        ImGui::TextUnformatted("x3");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
        ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
        ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[3]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("sum").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("sum");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::mult_2: {
        auto& dyn = ed.sim.mult_2_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("prod").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("prod");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::mult_3: {
        auto& dyn = ed.sim.mult_3_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[2]));
        ImGui::TextUnformatted("x2");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
        ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("prod").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("prod");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::mult_4: {
        auto& dyn = ed.sim.mult_4_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[2]));
        ImGui::TextUnformatted("x2");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[3]));
        ImGui::TextUnformatted("x3");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
        ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
        ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("prod").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("prod");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::counter: {
        auto& dyn = ed.sim.counter_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("in");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::generator: {
        auto& dyn = ed.sim.generator_models.get(mdl.id);
        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        ImGui::TextUnformatted("prod");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::constant: {
        auto& dyn = ed.sim.constant_models.get(mdl.id);

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("value", &dyn.default_value);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("out").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
        ImGui::TextUnformatted("out");
        imnodes::EndAttribute();
    } break;
    }
}

static void
show_editor(const char* editor_name, editor& ed)
{
    imnodes::EditorContextSet(ed.context);

    ImGui::Begin(editor_name);
    ImGui::TextUnformatted("A -- add node");

    imnodes::BeginNodeEditor();

    irt::model* mdl = nullptr;
    while (ed.sim.models.next(mdl)) {
        imnodes::BeginNode(ed.get_model(ed.sim.models.get_id(mdl)));

        imnodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(mdl->name.c_str());
        imnodes::EndNodeTitleBar();

        show_model_dynamics(ed, *mdl);

        // ImGui::PushItemWidth(120.0f);
        // ImGui::DragFloat("value", &node.value, 0.01f);
        // ImGui::PopItemWidth();

        // imnodes::BeginOutputAttribute(node.id << 16);
        // const float text_width = ImGui::CalcTextSize("output").x;
        // ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
        // ImGui::TextUnformatted("output");
        // imnodes::EndAttribute();

        imnodes::EndNode();
    }

    show_connections(ed);

    imnodes::EndNodeEditor();

    /*
    {
        Link link;
        int start = 0, end = 0;
        if (imnodes::IsLinkCreated(&start, &end)) {

            link.id = ++editor.current_id;
            editor.links.push_back(link);
        }
    }
    */

    /*
    if (ImGui::IsKeyReleased(SDL_SCANCODE_A) &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
        const int node_id = ++editor.current_id;
        imnodes::SetNodeScreenSpacePos(node_id, ImGui::GetMousePos());
        editor.nodes.push_back(Node{ node_id, 0.f });
    }
    */

    ImGui::End();

    if (!ImGui::Begin("Simulation box", &ed.show_simulation_box)) {
        ImGui::End();
    } else {
        ImGui::InputDouble("Begin", &ed.simulation_begin);
        ImGui::InputDouble("End", &ed.simulation_end);
        ImGui::SliderInt(
          "Observations", &ed.simulation_observation_number, 1, 10000);

        if (ed.st != simulation_status::running) {
            if (ed.simulation_thread.joinable()) {
                ed.simulation_thread.join();
                ed.st = simulation_status::success;
            }

            if (ImGui::Button("Start")) {
                ed.st = simulation_status::running;
                ed.obs_a.resize(ed.simulation_observation_number, 0.0);
                ed.obs_b.resize(ed.simulation_observation_number, 0.0);
                ed.stop = false;

                ed.simulation_thread = std::thread(
                  &run_simulation,
                  std::ref(ed.sim),
                  ed.simulation_begin,
                  ed.simulation_end,
                  (ed.simulation_end - ed.simulation_begin) /
                    static_cast<double>(ed.simulation_observation_number),
                  std::ref(ed.simulation_current),
                  std::ref(ed.st),
                  std::ref(ed.obs_a),
                  std::ref(ed.obs_b),
                  ed.value_a,
                  ed.value_b,
                  std::cref(ed.stop));
            }
        }

        if (ed.st == simulation_status::success ||
            ed.st == simulation_status::running) {
            ImGui::Text("Current: %g", ed.simulation_current);

            if (ImGui::Button("Stop"))
                ed.stop = true;

            const double duration = ed.simulation_end - ed.simulation_begin;
            const double elapsed = ed.simulation_current - ed.simulation_begin;
            const double fraction = elapsed / duration;
            ImGui::ProgressBar(static_cast<float>(fraction));

            ImGui::PlotLines("A",
                             ed.obs_a.data(),
                             ed.simulation_observation_number,
                             0,
                             nullptr,
                             -5.0,
                             +30.0,
                             ImVec2(0, 50));

            ImGui::PlotLines("B",
                             ed.obs_b.data(),
                             ed.simulation_observation_number,
                             0,
                             nullptr,
                             -5.0,
                             +30.0,
                             ImVec2(0, 50));
        }

        ImGui::End();
    }
}

editor ed;

void
node_editor_initialize()
{
    if (!ed.initialize()) {
        fmt::print(stderr, "node_editor_initialize failed\n");
        return;
    }

    ed.initialize_lotka_volterra();
    ed.context = imnodes::EditorContextCreate();
}

void
node_editor_show()
{
    if (ed.initialized)
        show_editor("editor1", ed);
}

void
node_editor_shutdown()
{
    if (ed.initialized)
        imnodes::EditorContextFree(ed.context);
}

} // namespace irt
