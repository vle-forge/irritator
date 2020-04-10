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

struct observation_output
{
    observation_output() = default;

    observation_output(const char* name_)
      : name(name_)
    {}

    const char* name = nullptr;
    array<float> data;
    double tl = 0.0;
    float min = -1.f;
    float max = +1.f;
    int id = 0;
};

static void
observation_output_initialize(const irt::observer& obs,
                              const irt::time t) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);
    std::fill_n(output->data.data(), output->data.size(), 0.0f);
    output->tl = t;
    output->min = -1.f;
    output->max = +1.f;
    output->id = 0;
}

static void
observation_output_observe(const irt::observer& obs,
                           const irt::time t,
                           const irt::message& msg) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);

    auto value = static_cast<float>(msg.to_real_64(0));
    output->min = std::min(output->min, value);
    output->max = std::max(output->max, value);

    for (double to_fill = output->tl; to_fill < t; to_fill += obs.time_step)
        if (static_cast<size_t>(output->id) < output->data.size())
            output->data[output->id++] = value;

    output->tl = t;
}

static void
run_simulation(simulation& sim,
               double begin,
               double end,
               double& current,
               simulation_status& st,
               const bool& stop) noexcept
{
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
    } while (current < end && !stop);

    sim.clean();

    st = simulation_status::success;
}

struct g_model
{
    g_model() = default;

    g_model(const model_id id_, const int current_model_id) noexcept
      : id(id_)
      , index(current_model_id)
    {}

    model_id id;
    int index;
};

struct g_connection
{
    g_connection() = default;

    g_connection(output_port_id src_,
                 input_port_id dst_,
                 int current_connection_id)
      : src(src_)
      , dst(dst_)
      , index(current_connection_id)
    {}

    output_port_id src;
    input_port_id dst;
    int index;
};

struct editor
{
    imnodes::EditorContext* context = nullptr;

    array<int> g_input_ports;
    array<int> g_output_ports;
    vector<g_model> g_models;
    vector<g_connection> g_connections;

    int current_model_id = 0;
    int current_port_id = 0;
    int current_connection_id = 0;
    bool initialized = false;

    vector<observation_output> observation_outputs;

    void clear()
    {
        g_input_ports.clear();
        g_output_ports.clear();
        g_models.clear();
        g_connections.clear();
        observation_outputs.clear();

        sim.clear();
    }

    model_id get_model(int index) const noexcept
    {
        for (size_t i = 0, e = g_models.size(); i != e; ++i)
            if (g_models[i].index == index)
                return g_models[i].id;

        return static_cast<model_id>(0);
    }

    int get_in(const input_port_id id) const noexcept
    {
        return g_input_ports[get_index(id)];
    }

    std::pair<bool, input_port_id> get_in(int index) const noexcept
    {
        input_port* port = nullptr;
        while (sim.input_ports.next(port)) {
            auto id = sim.input_ports.get_id(*port);
            if (index == get_in(id)) {
                return std::make_pair(true, id);
            }
        }

        return { false, static_cast<input_port_id>(0) };
    }

    int get_out(const output_port_id id) const noexcept
    {
        return g_output_ports[get_index(id)];
    }

    std::pair<bool, output_port_id> get_out(int index) const noexcept
    {
        output_port* port = nullptr;
        while (sim.output_ports.next(port)) {
            auto id = sim.output_ports.get_id(*port);
            if (index == get_out(id)) {
                return std::make_pair(true, id);
            }
        }

        return { false, static_cast<output_port_id>(0) };
    }

    simulation sim;
    double simulation_begin = 0.0;
    double simulation_end = 10.0;
    double simulation_current = 10.0;
    std::future<std::tuple<std::string, status>> future_content;
    std::thread simulation_thread;
    simulation_status st = simulation_status::uninitialized;
    bool show_simulation_box = true;
    double *value_a = nullptr, *value_b = nullptr;
    bool stop = false;

    array<float> obs_a;
    array<float> obs_b;

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

        if (!is_success(g_connections.init(sim.models.capacity() * 10lu)))
            return false;

        if (!is_success(observation_outputs.init(sim.models.capacity())))
            return false;

        initialized = true;

        return true;
    }

    template<typename Dynamics>
    status alloc(Dynamics& dynamics,
                 dynamics_id dyn_id,
                 const char* name = nullptr,
                 int* id = nullptr) noexcept
    {
        irt_return_if_bad(sim.alloc(dynamics, dyn_id, name));

        auto* gmdl = g_models.emplace_back(dynamics.id, current_model_id++);
        if (id != nullptr)
            *id = gmdl->index;

        if constexpr (is_detected_v<has_input_port_t, Dynamics>)
            for (size_t i = 0, e = std::size(dynamics.x); i != e; ++i)
                g_input_ports[get_index(dynamics.x[i])] = current_port_id++;

        if constexpr (is_detected_v<has_output_port_t, Dynamics>)
            for (size_t i = 0, e = std::size(dynamics.y); i != e; ++i)
                g_output_ports[get_index(dynamics.y[i])] = current_port_id++;

        return status::success;
    }

    template<typename Dynamics>
    void do_free([[maybe_unused]] Dynamics& dyn)
    {
        if constexpr (is_detected_v<has_output_port_t, Dynamics>) {
            for (size_t i = 0, e = std::size(dyn.y); i != e; ++i) {
                auto& src = sim.output_ports.get(dyn.y[i]);

                for (input_port_id dst : src.connections)
                    disconnect(dyn.y[i], dst);
            }
        }

        if constexpr (is_detected_v<has_input_port_t, Dynamics>) {
            for (size_t i = 0, e = std::size(dyn.x); i != e; ++i) {
                auto& dst = sim.input_ports.get(dyn.x[i]);

                for (output_port_id src : dst.connections)
                    disconnect(src, dyn.x[i]);
            }
        }
    }

    status free(const int node_id) noexcept
    {
        auto mdl_id = get_model(node_id);
        auto* mdl = sim.models.try_to_get(mdl_id);
        if (!mdl)
            return status::success;

        switch (mdl->type) {
        case dynamics_type::none:
            do_free(sim.none_models.get(mdl->id));
            break;
        case dynamics_type::integrator:
            do_free(sim.integrator_models.get(mdl->id));
            break;
        case dynamics_type::quantifier:
            do_free(sim.quantifier_models.get(mdl->id));
            break;
        case dynamics_type::adder_2:
            do_free(sim.adder_2_models.get(mdl->id));
            break;
        case dynamics_type::adder_3:
            do_free(sim.adder_3_models.get(mdl->id));
            break;
        case dynamics_type::adder_4:
            do_free(sim.adder_4_models.get(mdl->id));
            break;
        case dynamics_type::mult_2:
            do_free(sim.mult_2_models.get(mdl->id));
            break;
        case dynamics_type::mult_3:
            do_free(sim.mult_3_models.get(mdl->id));
            break;
        case dynamics_type::mult_4:
            do_free(sim.mult_4_models.get(mdl->id));
            break;
        case dynamics_type::counter:
            do_free(sim.counter_models.get(mdl->id));
            break;
        case dynamics_type::generator:
            do_free(sim.generator_models.get(mdl->id));
            break;
        case dynamics_type::constant:
            do_free(sim.constant_models.get(mdl->id));
            break;
        case dynamics_type::cross:
            do_free(sim.cross_models.get(mdl->id));
            break;
        case dynamics_type::time_func:
            do_free(sim.time_func_models.get(mdl->id));
            break;
        default:
            irt_bad_return(status::unknown_dynamics);
        }

        sim.deallocate(mdl_id);

        for (size_t i = 0, e = g_models.size(); i != e; ++i) {
            if (g_models[i].id == mdl_id) {
                g_models.erase_and_swap(g_models.data() + i);
                break;
            }
        }

        return status::success;
    }

    status connect(output_port_id src, input_port_id dst)
    {
        irt_return_if_bad(sim.connect(src, dst));

        auto [success, ptr] =
          g_connections.try_emplace_back(src, dst, current_connection_id++);

        irt_return_if_fail(success, status::gui_too_many_connection);

        return status::success;
    }

    void disconnect(const int link_id) noexcept
    {
        if (link_id >= 0) {
            for (size_t i = 0, e = g_connections.size(); i != e; ++i) {
                if (g_connections[i].index == link_id) {
                    sim.disconnect(g_connections[i].src, g_connections[i].dst);

                    g_connections.erase_and_swap(g_connections.begin() + i);
                    break;
                }
            }
        }
    }

    void disconnect(output_port_id src, input_port_id dst) noexcept
    {
        for (size_t i = 0, e = g_connections.size(); i != e; ++i) {
            if (g_connections[i].src == src && g_connections[i].dst == dst) {
                sim.disconnect(src, dst);

                g_connections.erase_and_swap(g_connections.begin() + i);
                break;
            }
        }
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

        quantifier_a.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_a.default_zero_init_offset = true;
        quantifier_a.default_step_size = 0.01;
        quantifier_a.default_past_length = 3;

        integrator_b.default_current_value = 7.0;

        quantifier_b.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_b.default_zero_init_offset = true;
        quantifier_b.default_step_size = 0.01;
        quantifier_b.default_past_length = 3;

        product.default_input_coeffs[0] = 1.0;
        product.default_input_coeffs[1] = 1.0;
        sum_a.default_input_coeffs[0] = 2.0;
        sum_a.default_input_coeffs[1] = -0.4;
        sum_b.default_input_coeffs[0] = -1.0;
        sum_b.default_input_coeffs[1] = 0.1;

        irt_return_if_bad(
          alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a"));
        irt_return_if_bad(
          alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b"));
        irt_return_if_bad(
          alloc(product, sim.mult_2_models.get_id(product), "prod"));
        irt_return_if_bad(alloc(
          integrator_a, sim.integrator_models.get_id(integrator_a), "int_a"));
        irt_return_if_bad(alloc(
          integrator_b, sim.integrator_models.get_id(integrator_b), "int_b"));
        irt_return_if_bad(alloc(
          quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a"));
        irt_return_if_bad(alloc(
          quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b"));

        irt_return_if_bad(connect(sum_a.y[0], integrator_a.x[1]));
        irt_return_if_bad(connect(sum_b.y[0], integrator_b.x[1]));

        irt_return_if_bad(connect(integrator_a.y[0], sum_a.x[0]));
        irt_return_if_bad(connect(integrator_b.y[0], sum_b.x[0]));

        irt_return_if_bad(connect(integrator_a.y[0], product.x[0]));
        irt_return_if_bad(connect(integrator_b.y[0], product.x[1]));

        irt_return_if_bad(connect(product.y[0], sum_a.x[1]));
        irt_return_if_bad(connect(product.y[0], sum_b.x[1]));

        irt_return_if_bad(connect(quantifier_a.y[0], integrator_a.x[0]));
        irt_return_if_bad(connect(quantifier_b.y[0], integrator_b.x[0]));
        irt_return_if_bad(connect(integrator_a.y[0], quantifier_a.x[0]));
        irt_return_if_bad(connect(integrator_b.y[0], quantifier_b.x[0]));

        value_a = &integrator_a.expected_value;
        value_b = &integrator_b.expected_value;

        return status::success;
    }

    status initialize_izhikevitch()
    {
        auto& constant = sim.constant_models.alloc();
        auto& constant2 = sim.constant_models.alloc();
        auto& constant3 = sim.constant_models.alloc();
        auto& sum_a = sim.adder_2_models.alloc();
        auto& sum_b = sim.adder_2_models.alloc();
        auto& sum_c = sim.adder_4_models.alloc();
        auto& sum_d = sim.adder_2_models.alloc();
        auto& product = sim.mult_2_models.alloc();
        auto& integrator_a = sim.integrator_models.alloc();
        auto& integrator_b = sim.integrator_models.alloc();
        auto& quantifier_a = sim.quantifier_models.alloc();
        auto& quantifier_b = sim.quantifier_models.alloc();
        auto& cross = sim.cross_models.alloc();
        auto& cross2 = sim.cross_models.alloc();

        double a = 0.2;
        double b = 2.0;
        double c = -56.0;
        double d = -16.0;
        double I = -99.0;
        double vt = 30.0;

        constant.default_value = 1.0;
        constant2.default_value = c;
        constant3.default_value = I;

        cross.default_threshold = vt;
        cross2.default_threshold = vt;

        integrator_a.default_current_value = 0.0;

        quantifier_a.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_a.default_zero_init_offset = true;
        quantifier_a.default_step_size = 0.01;
        quantifier_a.default_past_length = 3;

        integrator_b.default_current_value = 0.0;

        quantifier_b.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_b.default_zero_init_offset = true;
        quantifier_b.default_step_size = 0.01;
        quantifier_b.default_past_length = 3;

        product.default_input_coeffs[0] = 1.0;
        product.default_input_coeffs[1] = 1.0;

        sum_a.default_input_coeffs[0] = 1.0;
        sum_a.default_input_coeffs[1] = -1.0;
        sum_b.default_input_coeffs[0] = -a;
        sum_b.default_input_coeffs[1] = a * b;
        sum_c.default_input_coeffs[0] = 0.04;
        sum_c.default_input_coeffs[1] = 5.0;
        sum_c.default_input_coeffs[2] = 140.0;
        sum_c.default_input_coeffs[3] = 1.0;
        sum_d.default_input_coeffs[0] = 1.0;
        sum_d.default_input_coeffs[1] = d;

        irt_return_if_fail(sim.models.can_alloc(14),
                           status::gui_too_many_model);

        irt_return_if_bad(
          alloc(constant3, sim.constant_models.get_id(constant3), "tfun"));
        irt_return_if_bad(
          alloc(constant, sim.constant_models.get_id(constant), "1.0"));
        irt_return_if_bad(
          alloc(constant2, sim.constant_models.get_id(constant2), "-56.0"));

        irt_return_if_bad(
          alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a"));
        irt_return_if_bad(
          alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b"));
        irt_return_if_bad(
          alloc(sum_c, sim.adder_4_models.get_id(sum_c), "sum_c"));
        irt_return_if_bad(
          alloc(sum_d, sim.adder_2_models.get_id(sum_d), "sum_d"));

        irt_return_if_bad(
          alloc(product, sim.mult_2_models.get_id(product), "prod"));
        irt_return_if_bad(alloc(
          integrator_a, sim.integrator_models.get_id(integrator_a), "int_a"));
        irt_return_if_bad(alloc(
          integrator_b, sim.integrator_models.get_id(integrator_b), "int_b"));
        irt_return_if_bad(alloc(
          quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a"));
        irt_return_if_bad(alloc(
          quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b"));
        irt_return_if_bad(
          alloc(cross, sim.cross_models.get_id(cross), "cross"));
        irt_return_if_bad(
          alloc(cross2, sim.cross_models.get_id(cross2), "cross2"));

        irt_return_if_bad(connect(integrator_a.y[0], cross.x[0]));
        irt_return_if_bad(connect(constant2.y[0], cross.x[1]));
        irt_return_if_bad(connect(integrator_a.y[0], cross.x[2]));

        irt_return_if_bad(connect(cross.y[0], quantifier_a.x[0]));
        irt_return_if_bad(connect(cross.y[0], product.x[0]));
        irt_return_if_bad(connect(cross.y[0], product.x[1]));
        irt_return_if_bad(connect(product.y[0], sum_c.x[0]));
        irt_return_if_bad(connect(cross.y[0], sum_c.x[1]));
        irt_return_if_bad(connect(cross.y[0], sum_b.x[1]));

        irt_return_if_bad(connect(constant.y[0], sum_c.x[2]));
        irt_return_if_bad(connect(constant3.y[0], sum_c.x[3]));

        irt_return_if_bad(connect(sum_c.y[0], sum_a.x[0]));
        irt_return_if_bad(connect(integrator_b.y[0], sum_a.x[1]));
        irt_return_if_bad(connect(cross2.y[0], sum_a.x[1]));
        irt_return_if_bad(connect(sum_a.y[0], integrator_a.x[1]));
        irt_return_if_bad(connect(cross.y[0], integrator_a.x[2]));
        irt_return_if_bad(connect(quantifier_a.y[0], integrator_a.x[0]));

        irt_return_if_bad(connect(cross2.y[0], quantifier_b.x[0]));
        irt_return_if_bad(connect(cross2.y[0], sum_b.x[0]));
        irt_return_if_bad(connect(quantifier_b.y[0], integrator_b.x[0]));
        irt_return_if_bad(connect(sum_b.y[0], integrator_b.x[1]));

        irt_return_if_bad(connect(cross2.y[0], integrator_b.x[2]));
        irt_return_if_bad(connect(integrator_a.y[0], cross2.x[0]));
        irt_return_if_bad(connect(integrator_b.y[0], cross2.x[2]));
        irt_return_if_bad(connect(sum_d.y[0], cross2.x[1]));
        irt_return_if_bad(connect(integrator_b.y[0], sum_d.x[0]));
        irt_return_if_bad(connect(constant.y[0], sum_d.x[1]));

        return status::success;
    }
};

static void
show_connections(editor& ed) noexcept
{
    for (size_t i = 0, e = ed.g_connections.size(); i != e; ++i)
        imnodes::Link(ed.g_connections[i].index,
                      ed.get_out(ed.g_connections[i].src),
                      ed.get_in(ed.g_connections[i].dst));
}

static void
show_model_dynamics(editor& ed, model& mdl)
{
    auto* obs = ed.sim.observers.try_to_get(mdl.obs_id);
    bool open = obs != nullptr;

    ImGui::PushItemWidth(120.0f);
    if (ImGui::Checkbox("observation", &open)) {
        if (open) {
            if (auto* old = ed.sim.observers.try_to_get(mdl.obs_id); old)
                ed.sim.observers.free(*old);

            auto& o = ed.sim.observers.alloc(0.01, mdl.name.c_str(), nullptr);
            mdl.obs_id = ed.sim.observers.get_id(o);
        } else {
            if (auto* old = ed.sim.observers.try_to_get(mdl.obs_id); old)
                ed.sim.observers.free(*old);

            mdl.obs_id = static_cast<observer_id>(0);
        }
    }

    if (auto* o = ed.sim.observers.try_to_get(mdl.obs_id); o) {
        float v = static_cast<float>(o->time_step);
        if (ImGui::InputFloat("freq.", &v, 0.001f, 0.1f, "%.3f", 0))
            o->time_step = static_cast<double>(v);
    }
    ImGui::PopItemWidth();

    switch (mdl.type) {
    case dynamics_type::none: /* none does not have input port. */
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
    case dynamics_type::cross: {
        auto& dyn = ed.sim.cross_models.get(mdl.id);
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[0]));
        ImGui::TextUnformatted("value");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[1]));
        ImGui::TextUnformatted("if_value");
        imnodes::EndAttribute();
        imnodes::BeginInputAttribute(ed.get_in(dyn.x[2]));
        ImGui::TextUnformatted("else");
        imnodes::EndAttribute();

        ImGui::PushItemWidth(120.0f);
        ImGui::InputDouble("threshold", &dyn.default_threshold);
        ImGui::PopItemWidth();

        imnodes::BeginOutputAttribute(ed.get_out(dyn.y[0]));
        const float text_width = ImGui::CalcTextSize("out").x;
        ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
        ImGui::TextUnformatted("out");
        imnodes::EndAttribute();
    } break;
    case dynamics_type::time_func: {
        auto& dyn = ed.sim.time_func_models.get(mdl.id);
        // ImGui::PushItemWidth(120.0f);
        // ImGui::InputDouble("threshold", &dyn.default_threshold);
        // ImGui::PopItemWidth();

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

    ImGuiWindowFlags windows_flags = 0;
    windows_flags |= ImGuiWindowFlags_MenuBar;

    ImGui::Begin(editor_name, nullptr, windows_flags);

    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu("Edition")) {
            if (ImGui::MenuItem("Clear"))
                ed.clear();

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Examples")) {
            if (ImGui::MenuItem("Insert Lotka Volterra model")) {
                if (is_bad(ed.initialize_lotka_volterra()))
                    fmt::print("Error: fail to initialize a lotka volterra");
            }

            if (ImGui::MenuItem("Insert Izzhikevitch model")) {
                if (is_bad(ed.initialize_izhikevitch()))
                    fmt::print(stderr,
                               "fail to initialize an Izzhikevitch model\n");
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGui::Text("D -- delete selected nodes and/or connections");

    imnodes::BeginNodeEditor();

    for (size_t i = 0, e = ed.g_models.size(); i != e; ++i) {
        irt::model* mdl = ed.sim.models.try_to_get(ed.g_models[i].id);
        if (mdl) {
            imnodes::BeginNode(ed.g_models[i].index);

            imnodes::BeginNodeTitleBar();
            ImGui::TextUnformatted(mdl->name.c_str());
            imnodes::EndNodeTitleBar();

            show_model_dynamics(ed, *mdl);

            imnodes::EndNode();
        }
    }

    show_connections(ed);
    imnodes::EndNodeEditor();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));

    if (!ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(1))
        ImGui::OpenPopup("context menu");

    if (ImGui::BeginPopup("context menu")) {
        ImVec2 click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();
        int new_node = -1;

        if (ImGui::MenuItem("none")) {
            if (ed.sim.none_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.none_models.alloc();
                ed.alloc(
                  mdl, ed.sim.none_models.get_id(mdl), "none", &new_node);
            }
        }

        if (ImGui::MenuItem("integrator")) {
            if (ed.sim.integrator_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.integrator_models.alloc();
                ed.alloc(mdl,
                         ed.sim.integrator_models.get_id(mdl),
                         "integrator",
                         &new_node);
            }
        }

        if (ImGui::MenuItem("quantifier")) {
            if (ed.sim.quantifier_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.quantifier_models.alloc();
                ed.alloc(mdl,
                         ed.sim.quantifier_models.get_id(mdl),
                         "quantifier",
                         &new_node);
            }
        }

        if (ImGui::MenuItem("adder_2")) {
            if (ed.sim.adder_2_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.adder_2_models.alloc();
                ed.alloc(
                  mdl, ed.sim.adder_2_models.get_id(mdl), "adder", &new_node);
            }
        }

        if (ImGui::MenuItem("adder_3")) {
            if (ed.sim.adder_3_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.adder_3_models.alloc();
                ed.alloc(
                  mdl, ed.sim.adder_3_models.get_id(mdl), "adder", &new_node);
            }
        }

        if (ImGui::MenuItem("adder_4")) {
            if (ed.sim.adder_4_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.adder_4_models.alloc();
                ed.alloc(
                  mdl, ed.sim.adder_4_models.get_id(mdl), "adder", &new_node);
            }
        }

        if (ImGui::MenuItem("mult_2")) {
            if (ed.sim.mult_2_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.mult_2_models.alloc();
                ed.alloc(
                  mdl, ed.sim.mult_2_models.get_id(mdl), "mult", &new_node);
            }
        }

        if (ImGui::MenuItem("mult_3")) {
            if (ed.sim.mult_3_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.mult_3_models.alloc();
                ed.alloc(
                  mdl, ed.sim.mult_3_models.get_id(mdl), "mult", &new_node);
            }
        }

        if (ImGui::MenuItem("mult_4")) {
            if (ed.sim.mult_4_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.mult_4_models.alloc();
                ed.alloc(
                  mdl, ed.sim.mult_4_models.get_id(mdl), "mult", &new_node);
            }
        }

        if (ImGui::MenuItem("counter")) {
            if (ed.sim.counter_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.counter_models.alloc();
                ed.alloc(
                  mdl, ed.sim.counter_models.get_id(mdl), "counter", &new_node);
            }
        }

        if (ImGui::MenuItem("generator")) {
            if (ed.sim.generator_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.generator_models.alloc();
                ed.alloc(mdl,
                         ed.sim.generator_models.get_id(mdl),
                         "generator",
                         &new_node);
            }
        }

        if (ImGui::MenuItem("constant")) {
            if (ed.sim.constant_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.constant_models.alloc();
                ed.alloc(mdl,
                         ed.sim.constant_models.get_id(mdl),
                         "constant",
                         &new_node);
            }
        }

        if (ImGui::MenuItem("cross")) {
            if (ed.sim.cross_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.cross_models.alloc();
                ed.alloc(
                  mdl, ed.sim.cross_models.get_id(mdl), "cross", &new_node);
            }
        }

        if (ImGui::MenuItem("time_func")) {
            if (ed.sim.time_func_models.can_alloc(1u) &&
                ed.sim.models.can_alloc(1u)) {
                auto& mdl = ed.sim.time_func_models.alloc();
                ed.alloc(mdl,
                         ed.sim.time_func_models.get_id(mdl),
                         "time-func",
                         &new_node);
            }
        }

        ImGui::EndPopup();

        if (new_node != -1) {
            imnodes::SetNodeScreenSpacePos(new_node, click_pos);
        }
    }

    ImGui::PopStyleVar();

    {
        int start = 0, end = 0;
        if (imnodes::IsLinkCreated(&start, &end)) {
            auto [found_1, out] = ed.get_out(start);
            auto [found_2, in] = ed.get_in(end);

            if (found_1 && found_2)
                ed.connect(out, in);
        }
    }

    {
        const int num_selected = imnodes::NumSelectedLinks();
        if (num_selected > 0 && ImGui::IsKeyReleased('D')) {
            static array<int> selected_links;
            if (selected_links.capacity() < static_cast<size_t>(num_selected))
                selected_links.init(num_selected);

            std::fill_n(selected_links.data(), selected_links.size(), -1);
            imnodes::GetSelectedLinks(selected_links.data());
            std::sort(selected_links.begin(),
                      selected_links.end(),
                      std::greater<int>());

            for (const int link_id : selected_links)
                ed.disconnect(link_id);

            selected_links.clear();
        }
    }

    {
        const int num_selected = imnodes::NumSelectedNodes();
        if (num_selected > 0 && ImGui::IsKeyReleased('D')) {
            static array<int> selected_nodes;
            if (selected_nodes.capacity() < static_cast<size_t>(num_selected))
                selected_nodes.init(num_selected);

            std::fill_n(selected_nodes.data(), selected_nodes.size(), -1);
            imnodes::GetSelectedNodes(selected_nodes.data());

            for (const int node_id : selected_nodes)
                ed.free(node_id);

            selected_nodes.clear();
        }
    }

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

        if (ed.st != simulation_status::running) {
            if (ed.simulation_thread.joinable()) {
                ed.simulation_thread.join();
                ed.st = simulation_status::success;
            }

            if (ImGui::Button("Start")) {
                ed.observation_outputs.clear();
                observer* obs = nullptr;
                while (ed.sim.observers.next(obs)) {
                    ed.observation_outputs.emplace_back(obs->name.c_str());

                    const auto diff = ed.simulation_end - ed.simulation_begin;
                    const auto freq = diff / obs->time_step;
                    const size_t lenght = static_cast<size_t>(freq);

                    ed.observation_outputs.back().data.init(lenght);
                    obs->initialize = &observation_output_initialize;
                    obs->observe = &observation_output_observe;
                    obs->user_data =
                      static_cast<void*>(&ed.observation_outputs.back());
                }

                ed.st = simulation_status::running;
                ed.stop = false;

                ed.simulation_thread =
                  std::thread(&run_simulation,
                              std::ref(ed.sim),
                              ed.simulation_begin,
                              ed.simulation_end,
                              std::ref(ed.simulation_current),
                              std::ref(ed.st),
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

            for (const auto& obs : ed.observation_outputs)
                ImGui::PlotLines(obs.name,
                                 obs.data.data(),
                                 static_cast<int>(obs.data.size()),
                                 0,
                                 nullptr,
                                 obs.min,
                                 obs.max,
                                 ImVec2(0.f, 50.f));
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
