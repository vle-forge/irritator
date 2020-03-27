// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "imgui.h"
#include "imnodes.hpp"

#include "gui.hpp"

#include <irritator/core.hpp>

namespace irt {

struct node
{
    int id;

    model_id model;
};

struct link
{
    int id;
    int start; // start node
    int end;   // end node

    output_port_id out;
    input_port_id in;
};

struct editor
{
    imnodes::EditorContext* context = nullptr;

    simulation sim;
    array<node> nodes;
    array<link> links;

    bool initialize()
    {
        if (!is_success(sim.init(1024u, 32768u)))
            return false;

        if (!is_success(nodes.init(1024u)))
            return false;

        if (!is_success(links.init(4096u)))
            return false;

        return true;
    }

    status initialize_lotka_volterra()
    {
        if (!sim.adder_2_models.can_alloc(2) ||
            !sim.mult_2_models.can_alloc(2) ||
            !sim.integrator_models.can_alloc(2) ||
            !sim.quantifier_models.can_alloc(2) ||
            !sim.models.can_alloc(10))
            return status::simulation_not_enough_model;

        auto& sum_a = sim.adder_2_models.alloc();
        auto& sum_b = sim.adder_2_models.alloc();
        auto& product = sim.mult_2_models.alloc();
        auto& integrator_a = sim.integrator_models.alloc();
        auto& integrator_b = sim.integrator_models.alloc();
        auto& quantifier_a = sim.quantifier_models.alloc();
        auto& quantifier_b = sim.quantifier_models.alloc();

        integrator_a.current_value = 18.0;

        quantifier_a.m_adapt_state = irt::quantifier::adapt_state::possible;
        quantifier_a.m_zero_init_offset = true;
        quantifier_a.m_step_size = 0.01;
        quantifier_a.m_past_length = 3;

        integrator_b.current_value = 7.0;

        quantifier_b.m_adapt_state = irt::quantifier::adapt_state::possible;
        quantifier_b.m_zero_init_offset = true;
        quantifier_b.m_step_size = 0.01;
        quantifier_b.m_past_length = 3;

        product.input_coeffs[0] = 1.0;
        product.input_coeffs[1] = 1.0;
        sum_a.input_coeffs[0] = 2.0;
        sum_a.input_coeffs[1] = -0.4;
        sum_b.input_coeffs[0] = -1.0;
        sum_b.input_coeffs[1] = 0.1;

        irt_return_if_bad(sim.alloc(
            sum_a, sim.adder_2_models.get_id(sum_a), "sum_a"));
        irt_return_if_bad(sim.alloc(
            sum_b, sim.adder_2_models.get_id(sum_b), "sum_b"));
        irt_return_if_bad(sim.alloc(
            product, sim.mult_2_models.get_id(product), "prod"));
        irt_return_if_bad(sim.alloc(integrator_a, sim.integrator_models.get_id(integrator_a), "int_a"));
        irt_return_if_bad(sim.alloc(
            integrator_b, sim.integrator_models.get_id(integrator_b),
            "int_b"));
        irt_return_if_bad(sim.alloc(
            quantifier_a, sim.quantifier_models.get_id(quantifier_a),
            "qua_a"));
        irt_return_if_bad(sim.alloc(
            quantifier_b, sim.quantifier_models.get_id(quantifier_b),
            "qua_b"));

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

        return status::success;
    }
};

int
to_int(u32 value) noexcept
{
    assert((u64)value < (u64)std::numeric_limits<int>::max());
    return static_cast<int>(value);
}

int get_model(model_id id)
{
    return to_int(get_key(id));
}

int get_model(const data_array<model, model_id>& array, const model& mdl)
{
    return get_model(array.get_id(mdl));
}

int get_model(const data_array<model, model_id>& array, const model* mdl)
{
    assert(mdl);
    return get_model(array.get_id(*mdl));
}

int get_out(output_port_id id)
{
    return to_int(get_key(id)) << 16;
}

int get_out(const data_array<output_port, output_port_id>& array, const output_port& out)
{
    return get_out(array.get_id(out));
}

int get_out(const data_array<output_port, output_port_id>& array, const output_port* out)
{
    assert(out);
    return get_out(array.get_id(*out));
}

int get_in(input_port_id id)
{
    return to_int(get_key(id));
}

int get_in(const data_array<input_port, input_port_id>& array, const input_port& in)
{
    return get_in(array.get_id(in));
}

int get_in(const data_array<input_port, input_port_id>& array, const input_port* in)
{
    assert(in);
    return get_in(array.get_id(*in));
}

static void
show_connections(simulation& sim) noexcept
{
    int number = 1;
    irt::output_port* output_port = nullptr;
    while (sim.output_ports.next(output_port)) {
        int src = get_out(sim.output_ports, output_port);

        for (const input_port_id id_dst : output_port->connections) {
            if (auto* input_port = sim.input_ports.try_to_get(id_dst);
                input_port) {
                int dst = get_in(id_dst);
                ++number;
                imnodes::Link(number, src, dst);
            }
        }
    }
}

static void show_model_dynamics(simulation& sim, model& mdl)
{
    switch (mdl.type) {
    case dynamics_type::none: /* none does not have input port. */
        break;
    case dynamics_type::integrator: {
        auto& dyn = sim.integrator_models.get(mdl.id);
        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        ImGui::TextUnformatted("x_dot");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("quanta");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("value", &dyn.current_value);
        ImGui::PopItemWidth();

        imnodes::BeginInputAttribute(get_in(dyn.x[1]));
        const float text_width = ImGui::CalcTextSize("prod").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("x_dot");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::quantifier: {
        auto& dyn = sim.quantifier_models.get(mdl.id);
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("x_dot");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("quantum", &dyn.m_step_size);
        ImGui::SliderInt("archive length", &dyn.m_past_length, 3, 100);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        ImGui::TextUnformatted("quanta");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::adder_2: {
        auto& dyn = sim.adder_2_models.get(mdl.id);
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.input_coeffs[1]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("sum").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("sum");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::adder_3: {
        auto& dyn = sim.adder_3_models.get(mdl.id);
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[2]));
        ImGui::TextUnformatted("x2");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.input_coeffs[1]);
        ImGui::InputDouble("coeff-2", &dyn.input_coeffs[2]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("sum").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("sum");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::adder_4: {
        auto& dyn = sim.adder_4_models.get(mdl.id);
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[2]));
        ImGui::TextUnformatted("x2");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[3]));
        ImGui::TextUnformatted("x3");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.input_coeffs[1]);
        ImGui::InputDouble("coeff-2", &dyn.input_coeffs[2]);
        ImGui::InputDouble("coeff-2", &dyn.input_coeffs[3]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("sum").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("sum");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::mult_2: {
        auto& dyn = sim.mult_2_models.get(mdl.id);
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.input_coeffs[1]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("prod").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("prod");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::mult_3: {
        auto& dyn = sim.mult_3_models.get(mdl.id);
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[2]));
        ImGui::TextUnformatted("x2");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.input_coeffs[1]);
        ImGui::InputDouble("coeff-2", &dyn.input_coeffs[2]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("prod").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("prod");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::mult_4: {
        auto& dyn = sim.mult_4_models.get(mdl.id);
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("x0");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[1]));
        ImGui::TextUnformatted("x1");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[2]));
        ImGui::TextUnformatted("x2");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(get_in(dyn.x[3]));
        ImGui::TextUnformatted("x3");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("coeff-0", &dyn.input_coeffs[0]);
        ImGui::InputDouble("coeff-1", &dyn.input_coeffs[1]);
        ImGui::InputDouble("coeff-2", &dyn.input_coeffs[2]);
        ImGui::InputDouble("coeff-3", &dyn.input_coeffs[3]);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("prod").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x - text_width);
        ImGui::TextUnformatted("prod");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::counter: {
        auto& dyn = sim.counter_models.get(mdl.id);
        imnodes::BeginInputAttribute(get_in(dyn.x[0]));
        ImGui::TextUnformatted("in");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::generator: {
        auto& dyn = sim.generator_models.get(mdl.id);
        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
        ImGui::TextUnformatted("prod");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::constant: {
        auto& dyn = sim.constant_models.get(mdl.id);

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("value", &dyn.value);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
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
        imnodes::BeginNode(get_model(ed.sim.models, mdl));

        imnodes::BeginNodeTitleBar();
        ImGui::TextUnformatted(mdl->name.c_str());
        imnodes::EndNodeTitleBar();

        show_model_dynamics(ed.sim, *mdl);

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

    show_connections(ed.sim);

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
}

editor ed;

void
node_editor_initialize()
{
    ed.initialize();
    ed.initialize_lotka_volterra();
    ed.context = imnodes::EditorContextCreate();
}

void
node_editor_show()
{
    show_editor("editor1", ed);
}

void
node_editor_shutdown()
{
    imnodes::EditorContextFree(ed.context);
}

} // namespace irt
