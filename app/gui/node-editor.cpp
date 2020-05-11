// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "gui.hpp"
#include "imnodes.hpp"

#include <filesystem>
#include <fstream>
#include <future>
#include <mutex>
#include <string>
#include <thread>

#include <fmt/format.h>
#include <irritator/core.hpp>
#include <irritator/io.hpp>

namespace irt {

window_logger log_w;

template<typename Identifier>
constexpr Identifier
undefined() noexcept
{
    static_assert(
      std::is_enum<Identifier>::value,
      "Identifier must be a enumeration: enum class id : unsigned {};");

    return static_cast<Identifier>(0);
}

struct observation_output
{
    enum class type
    {
        none,
        plot,
        file,
        both
    };

    observation_output() = default;

    observation_output(const char* name_)
      : name(name_)
    {}

    std::ofstream ofs;
    const char* name = nullptr;
    array<float> data;
    double tl = 0.0;
    float min = -1.f;
    float max = +1.f;
    int id = 0;
    type observation_type = type::none;
};

static void
observation_output_initialize(const irt::observer& obs,
                              const irt::time t) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);
    if (output->observation_type == observation_output::type::plot ||
        output->observation_type == observation_output::type::both) {
        std::fill_n(output->data.data(), output->data.size(), 0.0f);
        output->tl = t;
        output->min = -1.f;
        output->max = +1.f;
        output->id = 0;
    }

    if (output->observation_type == observation_output::type::file ||
        output->observation_type == observation_output::type::both) {

        if (!output->ofs.is_open()) {
            if (output->observation_type == observation_output::type::both)
                output->observation_type = observation_output::type::plot;
            else
                output->observation_type = observation_output::type::none;
        } else
            output->ofs << "t," << output->name << '\n';
    }
}

static void
observation_output_observe(const irt::observer& obs,
                           const irt::time t,
                           const irt::message& msg) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);
    const auto value = static_cast<float>(msg.cast_to_real_64(0));

    if (output->observation_type == observation_output::type::plot ||
        output->observation_type == observation_output::type::both) {
        output->min = std::min(output->min, value);
        output->max = std::max(output->max, value);

        for (double to_fill = output->tl; to_fill < t; to_fill += obs.time_step)
            if (static_cast<size_t>(output->id) < output->data.size())
                output->data[output->id++] = value;
    }

    if (output->observation_type == observation_output::type::file ||
        output->observation_type == observation_output::type::both) {
        output->ofs << t << ',' << value << '\n';
    }

    output->tl = t;
}

static void
observation_output_free(const irt::observer& obs, const irt::time t) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<observation_output*>(obs.user_data);

    if (output->observation_type == observation_output::type::file ||
        output->observation_type == observation_output::type::both) {
        output->ofs.close();
    }
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
    if (auto ret = sim.initialize(current); irt::is_bad(ret)) {
        log_w.log(3,
                  "Simulation initialization failure (%s)\n",
                  irt::status_string[static_cast<int>(ret)]);
        st = simulation_status::internal_error;
        return;
    }

    do {
        if (auto ret = sim.run(current); irt::is_bad(ret)) {
            log_w.log(3,
                      "Simulation failure (%s)\n",
                      irt::status_string[static_cast<int>(ret)]);

            st = simulation_status::internal_error;
            return;
        }
    } while (current < end && !stop);

    sim.clean();

    st = simulation_status::success;
}

struct editor
{
    small_string<16> name;
    std::filesystem::path path;
    imnodes::EditorContext* context = nullptr;
    bool initialized = false;
    bool show = true;

    simulation sim;
    double simulation_begin = 0.0;
    double simulation_end = 10.0;
    double simulation_current = 10.0;
    std::future<std::tuple<std::string, status>> future_content;
    std::thread simulation_thread;
    simulation_status st = simulation_status::uninitialized;
    bool stop = false;

    vector<observation_output> observation_outputs;
    array<observation_output::type> observation_types;
    std::filesystem::path observation_directory;

    void clear()
    {
        observation_outputs.clear();
        observation_types.clear();
        observation_directory.clear();

        sim.clear();
    }

    int get_model(model& mdl) const noexcept
    {
        return static_cast<int>(get_index(sim.models.get_id(mdl)));
    }

    int get_model(model_id id) const noexcept
    {
        return static_cast<int>(get_index(id));
    }

    model_id get_model(int index) const noexcept
    {
        auto* mdl = sim.models.try_to_get(static_cast<u32>(index));

        return mdl ? sim.models.get_id(mdl) : undefined<model_id>();
    }

    int get_in(input_port_id id) const noexcept
    {
        return static_cast<int>(get_index(id));
    }

    input_port_id get_in(int index) const noexcept
    {
        auto* port = sim.input_ports.try_to_get(static_cast<u32>(index));

        return port ? sim.input_ports.get_id(port)
                    : static_cast<input_port_id>(0);
    }

    int get_out(output_port_id id) const noexcept
    {
        constexpr u32 is_output = 1 << 31;
        u32 index = get_index(id);
        index |= is_output;

        return static_cast<int>(index);
    }

    output_port_id get_out(int index) const noexcept
    {
        constexpr u32 mask = ~(1 << 31); /* remove the first bit */
        index &= mask;

        auto* port = sim.output_ports.try_to_get(static_cast<u32>(index));

        return port ? sim.output_ports.get_id(port)
                    : static_cast<output_port_id>(0);
    }

    status initialize(u32 id) noexcept
    {
        if (is_bad(sim.init(1024u, 32768u)) ||
            is_bad(observation_outputs.init(sim.models.capacity())) ||
            is_bad(observation_types.init(sim.models.capacity())))
            return status::gui_not_enough_memory;

        auto sz =
          fmt::format_to_n(name.begin(), name.capacity(), "Editor {}", id);
        name.size(sz.size);

        initialized = true;

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
          sim.alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a"));
        irt_return_if_bad(
          sim.alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b"));
        irt_return_if_bad(
          sim.alloc(product, sim.mult_2_models.get_id(product), "prod"));
        irt_return_if_bad(sim.alloc(
          integrator_a, sim.integrator_models.get_id(integrator_a), "int_a"));
        irt_return_if_bad(sim.alloc(
          integrator_b, sim.integrator_models.get_id(integrator_b), "int_b"));
        irt_return_if_bad(sim.alloc(
          quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a"));
        irt_return_if_bad(sim.alloc(
          quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b"));

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

    status initialize_izhikevitch()
    {
        if (!sim.constant_models.can_alloc(3) ||
            !sim.adder_2_models.can_alloc(3) ||
            !sim.adder_4_models.can_alloc(1) ||
            !sim.mult_2_models.can_alloc(1) ||
            !sim.integrator_models.can_alloc(2) ||
            !sim.quantifier_models.can_alloc(2) ||
            !sim.cross_models.can_alloc(2) || !sim.models.can_alloc(14))
            return status::simulation_not_enough_model;

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

        irt_return_if_bad(
          sim.alloc(constant3, sim.constant_models.get_id(constant3), "tfun"));
        irt_return_if_bad(
          sim.alloc(constant, sim.constant_models.get_id(constant), "1.0"));
        irt_return_if_bad(
          sim.alloc(constant2, sim.constant_models.get_id(constant2), "-56.0"));

        irt_return_if_bad(
          sim.alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a"));
        irt_return_if_bad(
          sim.alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b"));
        irt_return_if_bad(
          sim.alloc(sum_c, sim.adder_4_models.get_id(sum_c), "sum_c"));
        irt_return_if_bad(
          sim.alloc(sum_d, sim.adder_2_models.get_id(sum_d), "sum_d"));

        irt_return_if_bad(
          sim.alloc(product, sim.mult_2_models.get_id(product), "prod"));
        irt_return_if_bad(sim.alloc(
          integrator_a, sim.integrator_models.get_id(integrator_a), "int_a"));
        irt_return_if_bad(sim.alloc(
          integrator_b, sim.integrator_models.get_id(integrator_b), "int_b"));
        irt_return_if_bad(sim.alloc(
          quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a"));
        irt_return_if_bad(sim.alloc(
          quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b"));
        irt_return_if_bad(
          sim.alloc(cross, sim.cross_models.get_id(cross), "cross"));
        irt_return_if_bad(
          sim.alloc(cross2, sim.cross_models.get_id(cross2), "cross2"));

        irt_return_if_bad(sim.connect(integrator_a.y[0], cross.x[0]));
        irt_return_if_bad(sim.connect(constant2.y[0], cross.x[1]));
        irt_return_if_bad(sim.connect(integrator_a.y[0], cross.x[2]));

        irt_return_if_bad(sim.connect(cross.y[0], quantifier_a.x[0]));
        irt_return_if_bad(sim.connect(cross.y[0], product.x[0]));
        irt_return_if_bad(sim.connect(cross.y[0], product.x[1]));
        irt_return_if_bad(sim.connect(product.y[0], sum_c.x[0]));
        irt_return_if_bad(sim.connect(cross.y[0], sum_c.x[1]));
        irt_return_if_bad(sim.connect(cross.y[0], sum_b.x[1]));

        irt_return_if_bad(sim.connect(constant.y[0], sum_c.x[2]));
        irt_return_if_bad(sim.connect(constant3.y[0], sum_c.x[3]));

        irt_return_if_bad(sim.connect(sum_c.y[0], sum_a.x[0]));
        irt_return_if_bad(sim.connect(integrator_b.y[0], sum_a.x[1]));
        irt_return_if_bad(sim.connect(cross2.y[0], sum_a.x[1]));
        irt_return_if_bad(sim.connect(sum_a.y[0], integrator_a.x[1]));
        irt_return_if_bad(sim.connect(cross.y[0], integrator_a.x[2]));
        irt_return_if_bad(sim.connect(quantifier_a.y[0], integrator_a.x[0]));

        irt_return_if_bad(sim.connect(cross2.y[0], quantifier_b.x[0]));
        irt_return_if_bad(sim.connect(cross2.y[0], sum_b.x[0]));
        irt_return_if_bad(sim.connect(quantifier_b.y[0], integrator_b.x[0]));
        irt_return_if_bad(sim.connect(sum_b.y[0], integrator_b.x[1]));

        irt_return_if_bad(sim.connect(cross2.y[0], integrator_b.x[2]));
        irt_return_if_bad(sim.connect(integrator_a.y[0], cross2.x[0]));
        irt_return_if_bad(sim.connect(integrator_b.y[0], cross2.x[2]));
        irt_return_if_bad(sim.connect(sum_d.y[0], cross2.x[1]));
        irt_return_if_bad(sim.connect(integrator_b.y[0], sum_d.x[0]));
        irt_return_if_bad(sim.connect(constant.y[0], sum_d.x[1]));

        return status::success;
    }

    void show_connections() noexcept
    {
        int id = 0;
        output_port* o_port = nullptr;
        while (sim.output_ports.next(o_port)) {
            for (const auto dst : o_port->connections) {
                if (auto* i_port = sim.input_ports.try_to_get(dst); i_port) {
                    imnodes::Link(id++,
                                  get_out(sim.output_ports.get_id(o_port)),
                                  get_in(dst));
                }
            }
        }
    }

    void show_model_dynamics(model& mdl)
    {
        ImGui::PushItemWidth(100.0f);

        {
            const char* items[] = { "none", "plot", "file", "both" };
            int current_item = 0; /* Default show none */
            auto* obs = sim.observers.try_to_get(mdl.obs_id);

            if (obs)
                current_item =
                  static_cast<int>(observation_types[get_index(mdl.obs_id)]);

            if (ImGui::Combo(
                  "observation", &current_item, items, IM_ARRAYSIZE(items))) {
                if (current_item == 0) {
                    if (obs) {
                        observation_types[get_index(mdl.obs_id)] =
                          observation_output::type::none;
                        sim.observers.free(*obs);
                        mdl.obs_id = static_cast<observer_id>(0);
                    }
                } else {
                    if (!obs) {
                        auto& o =
                          sim.observers.alloc(0.01, mdl.name.c_str(), nullptr);
                        sim.observe(mdl, o);
                    }

                    observation_types[get_index(mdl.obs_id)] =
                      current_item == 1
                        ? observation_output::type::plot
                        : current_item == 2 ? observation_output::type::file
                                            : observation_output::type::both;
                }

                if (auto* o = sim.observers.try_to_get(mdl.obs_id); o) {
                    float v = static_cast<float>(o->time_step);
                    if (ImGui::InputFloat("freq.", &v, 0.001f, 0.1f, "%.3f", 0))
                        o->time_step = static_cast<double>(v);
                }
            }
        }

        ImGui::PopItemWidth();

        switch (mdl.type) {
        case dynamics_type::none: /* none does not have input port. */
            break;
        case dynamics_type::integrator: {
            auto& dyn = sim.integrator_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("quanta");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("x_dot");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("reset");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("value", &dyn.default_current_value);
            ImGui::InputDouble("reset", &dyn.default_reset_value);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("x").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("quanta").x - text_width);
            ImGui::TextUnformatted("x");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::quantifier: {
            auto& dyn = sim.quantifier_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("x_dot");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("quantum", &dyn.default_step_size);
            ImGui::SliderInt(
              "archive length", &dyn.default_past_length, 3, 100);
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
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
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
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
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
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[3]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("sum").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
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
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
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
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
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
            ImGui::InputDouble("coeff-0", &dyn.default_input_coeffs[0]);
            ImGui::InputDouble("coeff-1", &dyn.default_input_coeffs[1]);
            ImGui::InputDouble("coeff-2", &dyn.default_input_coeffs[2]);
            ImGui::InputDouble("coeff-3", &dyn.default_input_coeffs[3]);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("prod").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("coeff-0").x -
                          text_width);
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

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("value", &dyn.default_value);
            ImGui::InputDouble("period", &dyn.default_period);
            ImGui::InputDouble("offset", &dyn.default_offset);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            ImGui::TextUnformatted("prod");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::constant: {
            auto& dyn = sim.constant_models.get(mdl.id);

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("value", &dyn.default_value);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::cross: {
            auto& dyn = sim.cross_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("value");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("if_value");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("else");
            imnodes::EndAttribute();

            ImGui::PushItemWidth(120.0f);
            ImGui::InputDouble("threshold", &dyn.default_threshold);
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::accumulator_2: {
            auto& dyn = sim.accumulator_2_models.get(mdl.id);
            imnodes::BeginInputAttribute(get_in(dyn.x[0]));
            ImGui::TextUnformatted("number-1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[1]));
            ImGui::TextUnformatted("number-2");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[2]));
            ImGui::TextUnformatted("acc-1");
            imnodes::EndAttribute();
            imnodes::BeginInputAttribute(get_in(dyn.x[3]));
            ImGui::TextUnformatted("acc-2");
            imnodes::EndAttribute();
        } break;
        case dynamics_type::time_func: {
            auto& dyn = sim.time_func_models.get(mdl.id);
            const char* items[] = { "time", "square" };
            ImGui::PushItemWidth(120.0f);
            int item_current = dyn.default_f == &time_function ? 0 : 1;
            if (ImGui::Combo(
                  "function", &item_current, items, IM_ARRAYSIZE(items))) {
                dyn.default_f =
                  item_current == 0 ? &time_function : square_time_function;
            }
            ImGui::PopItemWidth();

            imnodes::BeginOutputAttribute(get_out(dyn.y[0]));
            const float text_width = ImGui::CalcTextSize("out").x;
            ImGui::Indent(120.f + ImGui::CalcTextSize("value").x - text_width);
            ImGui::TextUnformatted("out");
            imnodes::EndAttribute();
        } break;
        }
    }

    bool show_editor()
    {
        imnodes::EditorContextSet(context);
        static bool show_load_file_dialog = false;
        static bool show_save_file_dialog = false;

        ImGuiWindowFlags windows_flags = 0;
        windows_flags |= ImGuiWindowFlags_MenuBar;

        ImGui::SetNextWindowSize(ImVec2(500, 500), ImGuiCond_Once);
        if (!ImGui::Begin(name.c_str(), &show, windows_flags)) {
            ImGui::End();
            return true;
        }

        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open"))
                    show_load_file_dialog = true;

                if (!path.empty() && ImGui::MenuItem("Save")) {
                    log_w.log(3,
                              "Write into file %s\n",
                              (const char*)path.u8string().c_str());
                    if (auto os = std::ofstream(path); os.is_open()) {
                        writer w(os);
                        auto ret = w(sim);
                        if (is_success(ret))
                            log_w.log(5, "success\n");
                        else
                            log_w.log(4, "error writing\n");
                    }
                }

                if (ImGui::MenuItem("Save as..."))
                    show_save_file_dialog = true;

                if (ImGui::MenuItem("Close")) {
                    ImGui::EndMenu();
                    ImGui::EndMenuBar();
                    ImGui::End();
                    return false;
                }

                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edition")) {
                if (ImGui::MenuItem("Clear"))
                    clear();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Examples")) {
                if (ImGui::MenuItem("Insert Lotka Volterra model")) {
                    if (auto ret = initialize_lotka_volterra(); is_bad(ret))
                        log_w.log(
                          3,
                          "Fail to initialize a Lotka Volterra model (%s)\n",
                          status_string[static_cast<int>(ret)]);
                }

                if (ImGui::MenuItem("Insert Izhikevitch model")) {
                    if (auto ret = initialize_izhikevitch(); is_bad(ret))
                        log_w.log(
                          3,
                          "Fail to initialize an Izhikevitch model (%s)\n",
                          status_string[static_cast<int>(ret)]);
                }

                ImGui::EndMenu();
            }

            ImGui::EndMenuBar();
        }

        if (show_load_file_dialog) {
            auto size = ImGui::GetContentRegionMax();
            ImGui::SetNextWindowSize(ImVec2(size.x * 0.7f, size.y * 0.7f));
            ImGui::OpenPopup("Select file path to load");

            if (load_file_dialog(path)) {
                show_load_file_dialog = false;
                log_w.log(5,
                          "Load file from %s\n",
                          (const char*)path.u8string().c_str());
                if (auto is = std::ifstream(path); is.is_open()) {
                    reader r(is);
                    auto ret = r(sim);
                    if (is_success(ret))
                        log_w.log(5, "success\n");
                    else
                        log_w.log(4, "error writing\n");
                }
            }
        }

        if (show_save_file_dialog) {
            if (sim.models.size()) {
                ImGui::OpenPopup("Select file path to save");

                auto size = ImGui::GetContentRegionMax();
                ImGui::SetNextWindowSize(ImVec2(size.x * 0.7f, size.y * 0.7f));
                if (save_file_dialog(path)) {
                    show_save_file_dialog = false;
                    log_w.log(5,
                              "Save file to %s\n",
                              (const char*)path.u8string().c_str());

                    log_w.log(3,
                              "Write into file %s\n",
                              (const char*)path.u8string().c_str());
                    if (auto os = std::ofstream(path); os.is_open()) {
                        writer w(os);
                        auto ret = w(sim);
                        if (is_success(ret))
                            log_w.log(5, "success\n");
                        else
                            log_w.log(4, "error writing\n");
                    }
                }
            }
        }

        ImGui::Text("X -- delete selected nodes and/or connections /"
                    " D -- duplicate selected nodes");

        imnodes::BeginNodeEditor();

        {
            model* mdl = nullptr;
            while (sim.models.next(mdl)) {
                imnodes::BeginNode(get_model(*mdl));

                imnodes::BeginNodeTitleBar();
                ImGui::TextUnformatted(mdl->name.c_str());
                imnodes::EndNodeTitleBar();

                show_model_dynamics(*mdl);

                imnodes::EndNode();
            }
        }

        show_connections();
        imnodes::EndNodeEditor();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.f, 8.f));

        if (!ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(1))
            ImGui::OpenPopup("Context menu");

        if (ImGui::BeginPopup("Context menu")) {
            model_id new_model = undefined<model_id>();
            ImVec2 click_pos = ImGui::GetMousePosOnOpeningCurrentPopup();

            if (ImGui::MenuItem("none")) {
                if (sim.none_models.can_alloc(1u) && sim.models.can_alloc(1u)) {
                    auto& mdl = sim.none_models.alloc();
                    sim.alloc(mdl, sim.none_models.get_id(mdl), "none");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("integrator")) {
                if (sim.integrator_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.integrator_models.alloc();
                    sim.alloc(
                      mdl, sim.integrator_models.get_id(mdl), "integrator");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("quantifier")) {
                if (sim.quantifier_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.quantifier_models.alloc();
                    sim.alloc(
                      mdl, sim.quantifier_models.get_id(mdl), "quantifier");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("adder_2")) {
                if (sim.adder_2_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.adder_2_models.alloc();
                    sim.alloc(mdl, sim.adder_2_models.get_id(mdl), "adder");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("adder_3")) {
                if (sim.adder_3_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.adder_3_models.alloc();
                    sim.alloc(mdl, sim.adder_3_models.get_id(mdl), "adder");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("adder_4")) {
                if (sim.adder_4_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.adder_4_models.alloc();
                    sim.alloc(mdl, sim.adder_4_models.get_id(mdl), "adder");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("mult_2")) {
                if (sim.mult_2_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.mult_2_models.alloc();
                    sim.alloc(mdl, sim.mult_2_models.get_id(mdl), "mult");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("mult_3")) {
                if (sim.mult_3_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.mult_3_models.alloc();
                    sim.alloc(mdl, sim.mult_3_models.get_id(mdl), "mult");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("mult_4")) {
                if (sim.mult_4_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.mult_4_models.alloc();
                    sim.alloc(mdl, sim.mult_4_models.get_id(mdl), "mult");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("counter")) {
                if (sim.counter_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.counter_models.alloc();
                    sim.alloc(mdl, sim.counter_models.get_id(mdl), "counter");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("generator")) {
                if (sim.generator_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.generator_models.alloc();
                    sim.alloc(
                      mdl, sim.generator_models.get_id(mdl), "generator");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("constant")) {
                if (sim.constant_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.constant_models.alloc();
                    sim.alloc(mdl, sim.constant_models.get_id(mdl), "constant");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("cross")) {
                if (sim.cross_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.cross_models.alloc();
                    sim.alloc(mdl, sim.cross_models.get_id(mdl), "cross");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("accumulator-2")) {
                if (sim.accumulator_2_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.accumulator_2_models.alloc();
                    sim.alloc(
                      mdl, sim.accumulator_2_models.get_id(mdl), "acc-2");
                    new_model = mdl.id;
                }
            }

            if (ImGui::MenuItem("time_func")) {
                if (sim.time_func_models.can_alloc(1u) &&
                    sim.models.can_alloc(1u)) {
                    auto& mdl = sim.time_func_models.alloc();
                    sim.alloc(
                      mdl, sim.time_func_models.get_id(mdl), "time-func");
                    new_model = mdl.id;
                }
            }

            ImGui::EndPopup();
            if (new_model != undefined<model_id>())
                imnodes::SetNodeScreenSpacePos(get_model(new_model), click_pos);
        }

        ImGui::PopStyleVar();

        {
            int start = 0, end = 0;
            if (imnodes::IsLinkCreated(&start, &end)) {
                output_port_id out = get_out(start);
                input_port_id in = get_in(end);

                auto* o_port = sim.output_ports.try_to_get(out);
                auto* i_port = sim.input_ports.try_to_get(in);

                if (i_port && o_port)
                    sim.connect(out, in);
            }
        }

        const int num_selected_links = imnodes::NumSelectedLinks();
        const int num_selected_nodes = imnodes::NumSelectedNodes();
        static vector<int> selected_nodes;
        static vector<int> selected_links;
        static vector<model_id> sources;
        static vector<model_id> destinations;

        if (num_selected_nodes > 0) {
            selected_nodes.resize(num_selected_nodes);

            if (ImGui::IsKeyReleased('X')) {
                imnodes::GetSelectedNodes(selected_nodes.data());

                log_w.log(7, "%d model(s) to delete\n", num_selected_nodes);

                for (const int node_id : selected_nodes) {
                    auto id = get_model(node_id);
                    if (auto* mdl = sim.models.try_to_get(id); mdl) {
                        log_w.log(7, "delete %s\n", mdl->name.c_str());
                        sim.deallocate(sim.models.get_id(mdl));
                    }
                }
            } else if (ImGui::IsKeyReleased('D')) {
                imnodes::GetSelectedNodes(selected_nodes.data());

                if (is_success(sources.resize(num_selected_nodes)) &&
                    is_success(destinations.resize(num_selected_nodes))) {

                    for (size_t i = 0; i != selected_nodes.size(); ++i)
                        sources[i] = get_model(selected_nodes[i]);
                }

                sim.copy(std::begin(sources),
                         std::end(sources),
                         std::begin(destinations));
            }

            selected_nodes.clear();
        } else if (num_selected_links > 0) {
            selected_links.resize(static_cast<size_t>(num_selected_links));

            if (ImGui::IsKeyReleased('X')) {
                std::fill_n(selected_links.data(), selected_links.size(), -1);
                imnodes::GetSelectedLinks(selected_links.data());
                std::sort(selected_links.begin(),
                          selected_links.end(),
                          std::less<int>());

                for (size_t i = 0; i != selected_links.size(); ++i)
                    assert(selected_links[i] != -1);

                size_t i = 0;
                int link_id_to_delete = selected_links[0];
                int current_link_id = 0;

                log_w.log(
                  7, "%d connection(s) to delete\n", num_selected_links);

                output_port* o_port = nullptr;
                while (sim.output_ports.next(o_port) &&
                       link_id_to_delete != -1) {
                    for (const auto dst : o_port->connections) {
                        if (auto* i_port = sim.input_ports.try_to_get(dst);
                            i_port) {
                            if (current_link_id == link_id_to_delete) {
                                sim.disconnect(sim.output_ports.get_id(o_port),
                                               dst);

                                ++i;

                                if (i != selected_links.size())
                                    link_id_to_delete = selected_links[i];
                                else
                                    link_id_to_delete = -1;
                            }

                            ++current_link_id;
                        }
                    }
                }
            }

            selected_links.clear();
        }

        ImGui::End();

        return true;
    }
};

enum class editor_id : std::uint64_t;
static data_array<editor, editor_id> editors;

editor*
editors_new()
{
    if (!editors.can_alloc(1u)) {
        log_w.log(2, "Too many open editor\n");
        return nullptr;
    }

    auto& ed = editors.alloc();
    if (auto ret = ed.initialize(get_index(editors.get_id(ed))); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize irritator: %s\n",
                  status_string[static_cast<int>(ret)]);
        editors.free(ed);
        return nullptr;
    }

    log_w.log(5, "Open editor %s\n", ed.name.c_str());
    return &ed;
}

void
editors_free(editor& ed)
{
    log_w.log(5, "Close editor %s\n", ed.name.c_str());
    editors.free(ed);
}

void
show_simulation_box(bool* show_simulation)
{
    static editor_id current_editor_id = static_cast<editor_id>(0);

    ImGui::SetNextWindowSize(ImVec2(250, 400), ImGuiCond_Always);
    if (!ImGui::Begin("Simulation", show_simulation)) {
        ImGui::End();
        return;
    }

    const char* combo_name = "";
    if (auto* ed = editors.try_to_get(current_editor_id); !ed) {
        ed = nullptr;
        if (editors.next(ed)) {
            current_editor_id = editors.get_id(ed);
            combo_name = ed->name.c_str();
        }
    } else {
        combo_name = ed->name.c_str();
    }

    if (ImGui::BeginCombo("Name", combo_name)) {
        editor* ed = nullptr;
        while (editors.next(ed)) {
            bool is_selected = current_editor_id == editors.get_id(ed);
            if (ImGui::Selectable(ed->name.c_str(), is_selected))
                current_editor_id = editors.get_id(ed);

            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (auto* ed = editors.try_to_get(current_editor_id); ed) {
        ImGui::InputDouble("Begin", &ed->simulation_begin);
        ImGui::InputDouble("End", &ed->simulation_end);

        if (ed->st != simulation_status::running) {
            if (ed->simulation_thread.joinable()) {
                ed->simulation_thread.join();
                ed->st = simulation_status::success;
            }

            if (ImGui::Button("Start")) {
                ed->observation_outputs.clear();
                observer* obs = nullptr;
                while (ed->sim.observers.next(obs)) {
                    auto* output =
                      ed->observation_outputs.emplace_back(obs->name.c_str());

                    const auto type = ed->observation_types[get_index(
                      ed->sim.observers.get_id(*obs))];
                    const auto diff = ed->simulation_end - ed->simulation_begin;
                    const auto freq = diff / obs->time_step;
                    const auto length = static_cast<size_t>(freq);
                    output->observation_type = type;

                    if (type == observation_output::type::plot ||
                        type == observation_output::type::both) {
                        output->data.init(length);
                    }

                    if (!obs->name.empty()) {
                        const std::filesystem::path obs_file_path =
                          ed->observation_directory / obs->name.c_str();

                        if (type == observation_output::type::file ||
                            type == observation_output::type::both) {
                            if (output->ofs.open(obs_file_path);
                                !output->ofs.is_open())
                                log_w.log(
                                  4,
                                  "Fail to open observation file: %s in %s\n",
                                  obs->name.c_str(),
                                  ed->observation_directory.u8string().c_str());
                        }
                    }

                    obs->initialize = &observation_output_initialize;
                    obs->observe = &observation_output_observe;
                    obs->free = &observation_output_free;
                    obs->user_data = static_cast<void*>(output);
                }

                ed->st = simulation_status::running;
                ed->stop = false;

                ed->simulation_thread =
                  std::thread(&run_simulation,
                              std::ref(ed->sim),
                              ed->simulation_begin,
                              ed->simulation_end,
                              std::ref(ed->simulation_current),
                              std::ref(ed->st),
                              std::cref(ed->stop));
            }
        }

        if (ed->st == simulation_status::success ||
            ed->st == simulation_status::running) {
            ImGui::Text("Current: %g", ed->simulation_current);

            if (ed->st == simulation_status::running && ImGui::Button("Stop"))
                ed->stop = true;

            const double duration = ed->simulation_end - ed->simulation_begin;
            const double elapsed =
              ed->simulation_current - ed->simulation_begin;
            const double fraction = elapsed / duration;
            ImGui::ProgressBar(static_cast<float>(fraction));

            for (const auto& obs : ed->observation_outputs) {
                if (obs.observation_type == observation_output::type::plot ||
                    obs.observation_type == observation_output::type::both)
                    ImGui::PlotLines(obs.name,
                                     obs.data.data(),
                                     static_cast<int>(obs.data.size()),
                                     0,
                                     nullptr,
                                     obs.min,
                                     obs.max,
                                     ImVec2(0.f, 50.f));

                if (obs.observation_type == observation_output::type::file ||
                    obs.observation_type == observation_output::type::both)
                    ImGui::Text("%s: output file", obs.name);
            }
        }
    }

    ImGui::End();
}

void
node_editor_initialize()
{
    if (auto ret = editors.init(50u); is_bad(ret)) {
        log_w.log(2,
                  "Fail to initialize irritator: %s\n",
                  irt::status_string[static_cast<int>(ret)]);
    } else {
        if (auto* ed = editors_new(); ed)
            ed->context = imnodes::EditorContextCreate();
    }
}

bool
node_editor_show()
{
    static bool show_log = true;
    static bool show_simulation = true;
    static bool show_demo_window = false;
    bool ret = true;

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New")) {
                if (auto* ed = editors_new(); ed)
                    ed->context = imnodes::EditorContextCreate();
            }

            ImGui::Separator();
            if (ImGui::MenuItem("Quit"))
                ret = false;

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
            editor* ed = nullptr;
            while (editors.next(ed))
                ImGui::MenuItem(ed->name.c_str(), nullptr, &ed->show);

            ImGui::MenuItem("Simulation", nullptr, &show_simulation);
            ImGui::MenuItem("Log", nullptr, &show_log);

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            ImGui::MenuItem("Demo window", nullptr, &show_demo_window);

            ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
    }

    editor* ed = nullptr;
    while (editors.next(ed)) {
        if (ed->show) {
            if (!ed->show_editor()) {
                editor* next = ed;
                editors.next(next);
                editors_free(*ed);
            }
        }
    }

    if (show_log)
        log_w.show(&show_log);

    if (show_simulation)
        show_simulation_box(&show_simulation);

    if (show_demo_window)
        ImGui::ShowDemoWindow();

    return ret;
}

void
node_editor_shutdown()
{
    editor* ed = nullptr;
    while (editors.next(ed))
        imnodes::EditorContextFree(ed->context);
}

} // namespace irt
