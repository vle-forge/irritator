// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/file.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>
#include <irritator/random.hpp>

#include <fmt/format.h>

#include <numbers>
#include <random>

#include <cstdio>

#include <boost/ut.hpp>

struct file_output {
    using value_type = irt::observation;

    std::FILE*       os = nullptr;
    irt::simulation& sim;
    irt::observer_id id;

    file_output(irt::simulation&       sim_,
                const irt::observer_id id_,
                const char*            filename) noexcept
      : os{ std::fopen(filename, "w") }
      , sim{ sim_ }
      , id{ id_ }
    {
        using namespace boost::ut;

        expect(neq(os, nullptr));

        if (os)
            fmt::print(os, "t,v\n");
    }

    ~file_output() noexcept
    {
        if (os) {
            (void)sim.drain_observations(
              [&](const irt::observation_batch& b) noexcept -> irt::status {
                  if (b.id == id)
                      for (const auto& p : b.points)
                          fmt::print(os, "{},{}\n", p.t, p.value);
                  return irt::success();
              });

            std::fflush(os);
            std::fclose(os);
        }
    }
};

struct global_allocator_t {
    size_t allocation_size   = 0;
    int    allocation_number = 0;
} global_allocator;

struct global_deallocator_t {
    int free_number = 0;
} global_deallocator;

struct struct_with_static_member {
    static int i;
    static int j;

    struct_with_static_member() noexcept { ++i; }
    ~struct_with_static_member() noexcept { ++j; }
};

int struct_with_static_member::i = 0;
int struct_with_static_member::j = 0;

inline int make_input_node_id(const irt::model_id mdl, const int port) noexcept
{
    fmt::print("make_input_node_id({},{})\n", static_cast<irt::u64>(mdl), port);
    boost::ut::expect(port >= 0 && port < 8);

    auto index = irt::get_index(mdl);
    boost::ut::expect(index < 268435456u);

    fmt::print("{0:32b} <- index\n", index);
    fmt::print("{0:32b} <- port\n", port);

    irt::u32 port_index = static_cast<irt::u32>(port) << 28u;
    fmt::print("{0:32b} <- port_index\n", port_index);

    index |= port_index;
    fmt::print("{0:32b} <- index final\n", index);

    return static_cast<int>(index);
}

inline int make_output_node_id(const irt::model_id mdl, const int port) noexcept
{
    fmt::print("make_output_node_id({},{})\n", static_cast<irt::u64>(mdl),
               port);
    boost::ut::expect(port >= 0 && port < 8);

    auto index = irt::get_index(mdl);
    boost::ut::expect(index < 268435456u);

    fmt::print("{0:32b} <- index\n", index);
    fmt::print("{0:32b} <- port\n", port);
    fmt::print("{0:32b} <- port + 8u\n", 8u + static_cast<unsigned>(port));

    irt::u32 port_index = static_cast<irt::u32>(8u +
                                                static_cast<unsigned>(port))
                          << 28u;
    fmt::print("{0:32b} <- port_index\n", port_index);

    index |= port_index;
    fmt::print("{0:32b} <- index final\n", index);

    return static_cast<int>(index);
}

inline std::pair<irt::u32, irt::u32> get_model_input_port(
  const int node_id) noexcept
{
    fmt::print("get_model_input_port {}\n", node_id);

    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);
    fmt::print("{0:32b} <- real_node_id\n", real_node_id);

    irt::u32 port = real_node_id >> 28u;
    fmt::print("{0:32b} <- port\n", port);
    boost::ut::expect(port < 8u);

    constexpr irt::u32 mask = ~(15u << 28u);
    fmt::print("{0:32b} <- mask\n", mask);
    irt::u32 index = real_node_id & mask;
    fmt::print("{0:32b} <- real_node_id & mask\n", index);

    fmt::print("index: {} port: {}\n", index, port);
    return std::make_pair(index, port);
}

inline std::pair<irt::u32, irt::u32> get_model_output_port(
  const int node_id) noexcept
{
    fmt::print("get_model_output_port {}\n", node_id);

    const irt::u32 real_node_id = static_cast<irt::u32>(node_id);
    fmt::print("{0:32b} <- real_node_id\n", real_node_id);

    irt::u32 port = real_node_id >> 28u;
    fmt::print("{0:32b} <- port\n", port);

    boost::ut::expect(port >= 8u && port < 16u);
    port -= 8u;
    fmt::print("{0:32b} <- port - 8u\n", port);
    boost::ut::expect(port < 8u);

    constexpr irt::u32 mask = ~(15u << 28u);
    fmt::print("{0:32b} <- mask\n", mask);

    irt::u32 index = real_node_id & mask;
    fmt::print("{0:32b} <- real_node_id & mask\n", index);

    fmt::print("index: {} port: {}\n", index, port);
    return std::make_pair(index, port);
}

template<typename Dynamics>
inline auto get_p(irt::simulation& sim, const Dynamics& d) noexcept
  -> irt::parameter&
{
    return sim.parameters[sim.get_id(d)];
}

/// Runs @c sim from 0 to @c end.
static bool run_until(irt::simulation& sim, const irt::real end) noexcept
{
    sim.limits.set_bound(0, end);
    if (not sim.srcs.prepare() or not sim.initialize())
        return false;

    do {
        if (not sim.run())
            return false;
    } while (not sim.current_time_expired());

    return true;
}

/// A simulation of a @c hsm_wrapper whose outputs y[0] and y[1] go to the
/// counters @c ticks and @c stops. The machine is the root 0 > Marche 1 >
/// Tic 2, and Arret 3: Tic sends a tick every 2 and Marche leaves to Arret
/// with a message on y[1]. Each test sets the condition of Marche.
struct hsm_bench {
    using hsm_t = irt::hierarchical_state_machine;
    using var   = hsm_t::variable;

    irt::simulation sim{ irt::simulation_reserve_definition(),
                         irt::external_source_reserve_definition() };

    hsm_t&            machine = sim.hsms.alloc();
    irt::hsm_wrapper& hsmw    = sim.alloc<irt::hsm_wrapper>();
    irt::counter&     ticks   = sim.alloc<irt::counter>();
    irt::counter&     stops   = sim.alloc<irt::counter>();

    hsm_bench() noexcept
    {
        (void)machine.set_state(0u, hsm_t::invalid_state_id, 1u);
        (void)machine.set_state(1u, 0u, 2u);
        (void)machine.set_state(2u, 1u);
        (void)machine.set_state(3u, 0u);

        machine.states[1u].if_transition = 3u;
        machine.states[1u].if_action.set_output(var::port_1, 1.f);

        machine.states[2u].enter_action.set_affect(var::var_timer, 2.f);
        machine.states[2u].condition.set_timer();
        machine.states[2u].if_transition = 2u;
        machine.states[2u].if_action.set_output(var::port_0, 1.f);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(machine)));
        (void)sim.connect_dynamics(hsmw, 0, ticks, 0);
        (void)sim.connect_dynamics(hsmw, 1, stops, 0);
    }

    /// Sends @c value on the input port @c port of the machine at @c t.
    void send_at(const irt::real t, const int port) noexcept
    {
        auto& cst = sim.alloc<irt::constant>();
        get_p(sim, cst).set_constant(1.0, t);
        (void)sim.connect_dynamics(cst, 0, hsmw, port);
    }
};

int main()
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    using namespace boost::ut;

    "model-id-port-node-id"_test = [] {
        auto i  = make_input_node_id(irt::model_id{ 50 }, 7);
        auto j  = make_output_node_id(irt::model_id{ 50 }, 3);
        auto k1 = make_input_node_id(irt::model_id{ 268435455 }, 0);
        auto k2 = make_output_node_id(irt::model_id{ 268435455 }, 0);
        auto k3 = make_input_node_id(irt::model_id{ 268435455 }, 7);
        auto k4 = make_output_node_id(irt::model_id{ 268435455 }, 7);

        expect(i != j);

        auto ni  = get_model_input_port(i);
        auto nj  = get_model_output_port(j);
        auto nk1 = get_model_input_port(k1);
        auto nk2 = get_model_output_port(k2);
        auto nk3 = get_model_input_port(k3);
        auto nk4 = get_model_output_port(k4);

        expect(ni.first == 50u);
        expect(ni.second == 7u);
        expect(nj.first == 50u);
        expect(nj.second == 3u);
        expect(nk1.first == 268435455u);
        expect(nk1.second == 0u);
        expect(nk2.first == 268435455u);
        expect(nk2.second == 0u);
        expect(nk3.first == 268435455u);
        expect(nk3.second == 7u);
        expect(nk4.first == 268435455u);
        expect(nk4.second == 7u);
    };

    "better_sizeof_dynamics"_test = [] {
        irt::model mdl;

        for (auto i = 0, e = irt::dynamics_type_size(); i != e; ++i) {
            mdl.type        = irt::enum_cast<irt::dynamics_type>(i);
            const auto name = irt::dynamics_type_names[i];
            const auto size = irt::dispatch(
              mdl, []<typename Dynamics>(Dynamics& dyn) -> std::size_t {
                  return sizeof(dyn);
              });

            fmt::print("{:25}|{:5}\n", name, size);
        }

        fmt::print("------------------------------\n");
        fmt::print("dynamic number:          |{:5}\n",
                   irt::dynamics_type_size());
        fmt::print("max dynamic size:        |{:5}\n",
                   irt::max_size_in_bytes());
        fmt::print("------------------------------\n");
        fmt::print("model                    |{:5}\n", sizeof(irt::model));
        fmt::print("message                  |{:5}\n", sizeof(irt::message));
        fmt::print("observer                 |{:5}\n", sizeof(irt::observer));
        fmt::print("node                     |{:5}\n", sizeof(irt::node));
        fmt::print("parameter                |{:5}\n", sizeof(irt::parameter));
        fmt::print("model size:              |{:5}\n", sizeof(irt::model));
    };

    "model_constepxr"_test = [] {
        static_assert(irt::has_initialize_function<irt::constant>);
        static_assert(irt::has_lambda_function<irt::constant>);
        static_assert(irt::has_transition_function<irt::constant>);
        static_assert(not irt::has_input_port<irt::constant>);
        static_assert(irt::has_output_port<irt::constant>);
        static_assert(irt::has_observation_function<irt::constant>);

        static_assert(irt::has_initialize_function<irt::counter>);
        static_assert(not irt::has_lambda_function<irt::counter>);
        static_assert(irt::has_transition_function<irt::counter>);
        static_assert(irt::has_input_port<irt::counter>);
        static_assert(not irt::has_output_port<irt::counter>);
        static_assert(irt::has_observation_function<irt::counter>);

        static_assert(irt::has_initialize_function<irt::generator>);
        static_assert(irt::has_lambda_function<irt::generator>);
        static_assert(irt::has_transition_function<irt::generator>);
        static_assert(irt::has_input_port<irt::generator>);
        static_assert(irt::has_output_port<irt::generator>);
        static_assert(irt::has_observation_function<irt::generator>);

        static_assert(irt::has_initialize_function<irt::qss1_cross>);
        static_assert(irt::has_lambda_function<irt::qss1_cross>);
        static_assert(irt::has_transition_function<irt::qss1_cross>);
        static_assert(irt::has_input_port<irt::qss1_cross>);
        static_assert(irt::has_output_port<irt::qss1_cross>);
        static_assert(irt::has_observation_function<irt::qss1_cross>);

        static_assert(irt::has_initialize_function<irt::qss1_filter>);
        static_assert(irt::has_lambda_function<irt::qss1_filter>);
        static_assert(irt::has_transition_function<irt::qss1_filter>);
        static_assert(irt::has_input_port<irt::qss1_filter>);
        static_assert(irt::has_output_port<irt::qss1_filter>);
        static_assert(irt::has_observation_function<irt::qss1_filter>);

        static_assert(irt::has_initialize_function<irt::qss1_sample_hold>);
        static_assert(irt::has_lambda_function<irt::qss1_sample_hold>);
        static_assert(irt::has_transition_function<irt::qss1_sample_hold>);
        static_assert(irt::has_input_port<irt::qss1_sample_hold>);
        static_assert(irt::has_output_port<irt::qss1_sample_hold>);
        static_assert(irt::has_observation_function<irt::qss1_sample_hold>);

        static_assert(irt::has_initialize_function<irt::qss2_sample_hold>);
        static_assert(irt::has_lambda_function<irt::qss2_sample_hold>);
        static_assert(irt::has_transition_function<irt::qss2_sample_hold>);
        static_assert(irt::has_input_port<irt::qss2_sample_hold>);
        static_assert(irt::has_output_port<irt::qss2_sample_hold>);
        static_assert(irt::has_observation_function<irt::qss2_sample_hold>);

        static_assert(irt::has_initialize_function<irt::qss3_sample_hold>);
        static_assert(irt::has_lambda_function<irt::qss3_sample_hold>);
        static_assert(irt::has_transition_function<irt::qss3_sample_hold>);
        static_assert(irt::has_input_port<irt::qss3_sample_hold>);
        static_assert(irt::has_output_port<irt::qss3_sample_hold>);
        static_assert(irt::has_observation_function<irt::qss3_sample_hold>);

        static_assert(irt::has_initialize_function<irt::zero_order_hold>);
        static_assert(irt::has_lambda_function<irt::zero_order_hold>);
        static_assert(irt::has_transition_function<irt::zero_order_hold>);
        static_assert(irt::has_input_port<irt::zero_order_hold>);
        static_assert(irt::has_output_port<irt::zero_order_hold>);
        static_assert(irt::has_observation_function<irt::zero_order_hold>);

        static_assert(irt::has_initialize_function<irt::qss1_quantizer>);
        static_assert(irt::has_lambda_function<irt::qss1_quantizer>);
        static_assert(irt::has_transition_function<irt::qss1_quantizer>);
        static_assert(irt::has_input_port<irt::qss1_quantizer>);
        static_assert(irt::has_output_port<irt::qss1_quantizer>);
        static_assert(irt::has_observation_function<irt::qss1_quantizer>);

        static_assert(irt::has_initialize_function<irt::qss2_quantizer>);
        static_assert(irt::has_lambda_function<irt::qss2_quantizer>);
        static_assert(irt::has_transition_function<irt::qss2_quantizer>);
        static_assert(irt::has_input_port<irt::qss2_quantizer>);
        static_assert(irt::has_output_port<irt::qss2_quantizer>);
        static_assert(irt::has_observation_function<irt::qss2_quantizer>);

        static_assert(irt::has_initialize_function<irt::qss3_quantizer>);
        static_assert(irt::has_lambda_function<irt::qss3_quantizer>);
        static_assert(irt::has_transition_function<irt::qss3_quantizer>);
        static_assert(irt::has_input_port<irt::qss3_quantizer>);
        static_assert(irt::has_output_port<irt::qss3_quantizer>);
        static_assert(irt::has_observation_function<irt::qss3_quantizer>);

        static_assert(
          irt::has_initialize_function<irt::qss1_integrate_and_fire>);
        static_assert(irt::has_lambda_function<irt::qss1_integrate_and_fire>);
        static_assert(
          irt::has_transition_function<irt::qss1_integrate_and_fire>);
        static_assert(irt::has_input_port<irt::qss1_integrate_and_fire>);
        static_assert(irt::has_output_port<irt::qss1_integrate_and_fire>);
        static_assert(
          irt::has_observation_function<irt::qss1_integrate_and_fire>);

        static_assert(
          irt::has_initialize_function<irt::qss2_integrate_and_fire>);
        static_assert(irt::has_lambda_function<irt::qss2_integrate_and_fire>);
        static_assert(
          irt::has_transition_function<irt::qss2_integrate_and_fire>);
        static_assert(irt::has_input_port<irt::qss2_integrate_and_fire>);
        static_assert(irt::has_output_port<irt::qss2_integrate_and_fire>);
        static_assert(
          irt::has_observation_function<irt::qss2_integrate_and_fire>);

        static_assert(
          irt::has_initialize_function<irt::qss3_integrate_and_fire>);
        static_assert(irt::has_lambda_function<irt::qss3_integrate_and_fire>);
        static_assert(
          irt::has_transition_function<irt::qss3_integrate_and_fire>);
        static_assert(irt::has_input_port<irt::qss3_integrate_and_fire>);
        static_assert(irt::has_output_port<irt::qss3_integrate_and_fire>);
        static_assert(
          irt::has_observation_function<irt::qss3_integrate_and_fire>);

        static_assert(
          irt::has_initialize_function<irt::qss1_threshold_crossing>);
        static_assert(irt::has_lambda_function<irt::qss1_threshold_crossing>);
        static_assert(
          irt::has_transition_function<irt::qss1_threshold_crossing>);
        static_assert(irt::has_input_port<irt::qss1_threshold_crossing>);
        static_assert(irt::has_output_port<irt::qss1_threshold_crossing>);
        static_assert(
          irt::has_observation_function<irt::qss1_threshold_crossing>);

        static_assert(
          irt::has_initialize_function<irt::qss2_threshold_crossing>);
        static_assert(irt::has_lambda_function<irt::qss2_threshold_crossing>);
        static_assert(
          irt::has_transition_function<irt::qss2_threshold_crossing>);
        static_assert(irt::has_input_port<irt::qss2_threshold_crossing>);
        static_assert(irt::has_output_port<irt::qss2_threshold_crossing>);
        static_assert(
          irt::has_observation_function<irt::qss2_threshold_crossing>);

        static_assert(
          irt::has_initialize_function<irt::qss3_threshold_crossing>);
        static_assert(irt::has_lambda_function<irt::qss3_threshold_crossing>);
        static_assert(
          irt::has_transition_function<irt::qss3_threshold_crossing>);
        static_assert(irt::has_input_port<irt::qss3_threshold_crossing>);
        static_assert(irt::has_output_port<irt::qss3_threshold_crossing>);
        static_assert(
          irt::has_observation_function<irt::qss3_threshold_crossing>);

        static_assert(irt::has_initialize_function<irt::qss1_pwm>);
        static_assert(irt::has_lambda_function<irt::qss1_pwm>);
        static_assert(irt::has_transition_function<irt::qss1_pwm>);
        static_assert(irt::has_input_port<irt::qss1_pwm>);
        static_assert(irt::has_output_port<irt::qss1_pwm>);
        static_assert(irt::has_observation_function<irt::qss1_pwm>);

        static_assert(irt::has_initialize_function<irt::qss2_pwm>);
        static_assert(irt::has_lambda_function<irt::qss2_pwm>);
        static_assert(irt::has_transition_function<irt::qss2_pwm>);
        static_assert(irt::has_input_port<irt::qss2_pwm>);
        static_assert(irt::has_output_port<irt::qss2_pwm>);
        static_assert(irt::has_observation_function<irt::qss2_pwm>);

        static_assert(irt::has_initialize_function<irt::qss3_pwm>);
        static_assert(irt::has_lambda_function<irt::qss3_pwm>);
        static_assert(irt::has_transition_function<irt::qss3_pwm>);
        static_assert(irt::has_input_port<irt::qss3_pwm>);
        static_assert(irt::has_output_port<irt::qss3_pwm>);
        static_assert(irt::has_observation_function<irt::qss3_pwm>);

        static_assert(irt::has_initialize_function<irt::qss1_sqrt>);
        static_assert(irt::has_lambda_function<irt::qss1_sqrt>);
        static_assert(irt::has_transition_function<irt::qss1_sqrt>);
        static_assert(irt::has_input_port<irt::qss1_sqrt>);
        static_assert(irt::has_output_port<irt::qss1_sqrt>);
        static_assert(irt::has_observation_function<irt::qss1_sqrt>);

        static_assert(irt::has_initialize_function<irt::qss2_sqrt>);
        static_assert(irt::has_lambda_function<irt::qss2_sqrt>);
        static_assert(irt::has_transition_function<irt::qss2_sqrt>);
        static_assert(irt::has_input_port<irt::qss2_sqrt>);
        static_assert(irt::has_output_port<irt::qss2_sqrt>);
        static_assert(irt::has_observation_function<irt::qss2_sqrt>);

        static_assert(irt::has_initialize_function<irt::qss3_sqrt>);
        static_assert(irt::has_lambda_function<irt::qss3_sqrt>);
        static_assert(irt::has_transition_function<irt::qss3_sqrt>);
        static_assert(irt::has_input_port<irt::qss3_sqrt>);
        static_assert(irt::has_output_port<irt::qss3_sqrt>);
        static_assert(irt::has_observation_function<irt::qss3_sqrt>);

        static_assert(irt::has_initialize_function<irt::qss1_atan>);
        static_assert(irt::has_lambda_function<irt::qss1_atan>);
        static_assert(irt::has_transition_function<irt::qss1_atan>);
        static_assert(irt::has_input_port<irt::qss1_atan>);
        static_assert(irt::has_output_port<irt::qss1_atan>);
        static_assert(irt::has_observation_function<irt::qss1_atan>);

        static_assert(irt::has_initialize_function<irt::qss2_atan>);
        static_assert(irt::has_lambda_function<irt::qss2_atan>);
        static_assert(irt::has_transition_function<irt::qss2_atan>);
        static_assert(irt::has_input_port<irt::qss2_atan>);
        static_assert(irt::has_output_port<irt::qss2_atan>);
        static_assert(irt::has_observation_function<irt::qss2_atan>);

        static_assert(irt::has_initialize_function<irt::qss3_atan>);
        static_assert(irt::has_lambda_function<irt::qss3_atan>);
        static_assert(irt::has_transition_function<irt::qss3_atan>);
        static_assert(irt::has_input_port<irt::qss3_atan>);
        static_assert(irt::has_output_port<irt::qss3_atan>);
        static_assert(irt::has_observation_function<irt::qss3_atan>);

        static_assert(irt::has_initialize_function<irt::qss1_tan>);
        static_assert(irt::has_lambda_function<irt::qss1_tan>);
        static_assert(irt::has_transition_function<irt::qss1_tan>);
        static_assert(irt::has_input_port<irt::qss1_tan>);
        static_assert(irt::has_output_port<irt::qss1_tan>);
        static_assert(irt::has_observation_function<irt::qss1_tan>);

        static_assert(irt::has_initialize_function<irt::qss2_tan>);
        static_assert(irt::has_lambda_function<irt::qss2_tan>);
        static_assert(irt::has_transition_function<irt::qss2_tan>);
        static_assert(irt::has_input_port<irt::qss2_tan>);
        static_assert(irt::has_output_port<irt::qss2_tan>);
        static_assert(irt::has_observation_function<irt::qss2_tan>);

        static_assert(irt::has_initialize_function<irt::qss3_tan>);
        static_assert(irt::has_lambda_function<irt::qss3_tan>);
        static_assert(irt::has_transition_function<irt::qss3_tan>);
        static_assert(irt::has_input_port<irt::qss3_tan>);
        static_assert(irt::has_output_port<irt::qss3_tan>);
        static_assert(irt::has_observation_function<irt::qss3_tan>);

        static_assert(irt::has_initialize_function<irt::qss1_tanh>);
        static_assert(irt::has_lambda_function<irt::qss1_tanh>);
        static_assert(irt::has_transition_function<irt::qss1_tanh>);
        static_assert(irt::has_input_port<irt::qss1_tanh>);
        static_assert(irt::has_output_port<irt::qss1_tanh>);
        static_assert(irt::has_observation_function<irt::qss1_tanh>);

        static_assert(irt::has_initialize_function<irt::qss2_tanh>);
        static_assert(irt::has_lambda_function<irt::qss2_tanh>);
        static_assert(irt::has_transition_function<irt::qss2_tanh>);
        static_assert(irt::has_input_port<irt::qss2_tanh>);
        static_assert(irt::has_output_port<irt::qss2_tanh>);
        static_assert(irt::has_observation_function<irt::qss2_tanh>);

        static_assert(irt::has_initialize_function<irt::qss3_tanh>);
        static_assert(irt::has_lambda_function<irt::qss3_tanh>);
        static_assert(irt::has_transition_function<irt::qss3_tanh>);
        static_assert(irt::has_input_port<irt::qss3_tanh>);
        static_assert(irt::has_output_port<irt::qss3_tanh>);
        static_assert(irt::has_observation_function<irt::qss3_tanh>);

        static_assert(irt::has_initialize_function<irt::qss1_sigmoid>);
        static_assert(irt::has_lambda_function<irt::qss1_sigmoid>);
        static_assert(irt::has_transition_function<irt::qss1_sigmoid>);
        static_assert(irt::has_input_port<irt::qss1_sigmoid>);
        static_assert(irt::has_output_port<irt::qss1_sigmoid>);
        static_assert(irt::has_observation_function<irt::qss1_sigmoid>);

        static_assert(irt::has_initialize_function<irt::qss2_sigmoid>);
        static_assert(irt::has_lambda_function<irt::qss2_sigmoid>);
        static_assert(irt::has_transition_function<irt::qss2_sigmoid>);
        static_assert(irt::has_input_port<irt::qss2_sigmoid>);
        static_assert(irt::has_output_port<irt::qss2_sigmoid>);
        static_assert(irt::has_observation_function<irt::qss2_sigmoid>);

        static_assert(irt::has_initialize_function<irt::qss3_sigmoid>);
        static_assert(irt::has_lambda_function<irt::qss3_sigmoid>);
        static_assert(irt::has_transition_function<irt::qss3_sigmoid>);
        static_assert(irt::has_input_port<irt::qss3_sigmoid>);
        static_assert(irt::has_output_port<irt::qss3_sigmoid>);
        static_assert(irt::has_observation_function<irt::qss3_sigmoid>);

        static_assert(irt::has_initialize_function<irt::qss1_division>);
        static_assert(irt::has_lambda_function<irt::qss1_division>);
        static_assert(irt::has_transition_function<irt::qss1_division>);
        static_assert(irt::has_input_port<irt::qss1_division>);
        static_assert(irt::has_output_port<irt::qss1_division>);
        static_assert(irt::has_observation_function<irt::qss1_division>);

        static_assert(irt::has_initialize_function<irt::qss2_division>);
        static_assert(irt::has_lambda_function<irt::qss2_division>);
        static_assert(irt::has_transition_function<irt::qss2_division>);
        static_assert(irt::has_input_port<irt::qss2_division>);
        static_assert(irt::has_output_port<irt::qss2_division>);
        static_assert(irt::has_observation_function<irt::qss2_division>);

        static_assert(irt::has_initialize_function<irt::qss3_division>);
        static_assert(irt::has_lambda_function<irt::qss3_division>);
        static_assert(irt::has_transition_function<irt::qss3_division>);
        static_assert(irt::has_input_port<irt::qss3_division>);
        static_assert(irt::has_output_port<irt::qss3_division>);
        static_assert(irt::has_observation_function<irt::qss3_division>);

        static_assert(irt::has_initialize_function<irt::qss1_atan2>);
        static_assert(irt::has_lambda_function<irt::qss1_atan2>);
        static_assert(irt::has_transition_function<irt::qss1_atan2>);
        static_assert(irt::has_input_port<irt::qss1_atan2>);
        static_assert(irt::has_output_port<irt::qss1_atan2>);
        static_assert(irt::has_observation_function<irt::qss1_atan2>);

        static_assert(irt::has_initialize_function<irt::qss2_atan2>);
        static_assert(irt::has_lambda_function<irt::qss2_atan2>);
        static_assert(irt::has_transition_function<irt::qss2_atan2>);
        static_assert(irt::has_input_port<irt::qss2_atan2>);
        static_assert(irt::has_output_port<irt::qss2_atan2>);
        static_assert(irt::has_observation_function<irt::qss2_atan2>);

        static_assert(irt::has_initialize_function<irt::qss3_atan2>);
        static_assert(irt::has_lambda_function<irt::qss3_atan2>);
        static_assert(irt::has_transition_function<irt::qss3_atan2>);
        static_assert(irt::has_input_port<irt::qss3_atan2>);
        static_assert(irt::has_output_port<irt::qss3_atan2>);
        static_assert(irt::has_observation_function<irt::qss3_atan2>);

        static_assert(irt::has_initialize_function<irt::qss1_saturation>);
        static_assert(irt::has_lambda_function<irt::qss1_saturation>);
        static_assert(irt::has_transition_function<irt::qss1_saturation>);
        static_assert(irt::has_input_port<irt::qss1_saturation>);
        static_assert(irt::has_output_port<irt::qss1_saturation>);
        static_assert(irt::has_observation_function<irt::qss1_saturation>);

        static_assert(irt::has_initialize_function<irt::qss2_saturation>);
        static_assert(irt::has_lambda_function<irt::qss2_saturation>);
        static_assert(irt::has_transition_function<irt::qss2_saturation>);
        static_assert(irt::has_input_port<irt::qss2_saturation>);
        static_assert(irt::has_output_port<irt::qss2_saturation>);
        static_assert(irt::has_observation_function<irt::qss2_saturation>);

        static_assert(irt::has_initialize_function<irt::qss3_saturation>);
        static_assert(irt::has_lambda_function<irt::qss3_saturation>);
        static_assert(irt::has_transition_function<irt::qss3_saturation>);
        static_assert(irt::has_input_port<irt::qss3_saturation>);
        static_assert(irt::has_output_port<irt::qss3_saturation>);
        static_assert(irt::has_observation_function<irt::qss3_saturation>);

        static_assert(irt::has_initialize_function<irt::qss1_dead_zone>);
        static_assert(irt::has_lambda_function<irt::qss1_dead_zone>);
        static_assert(irt::has_transition_function<irt::qss1_dead_zone>);
        static_assert(irt::has_input_port<irt::qss1_dead_zone>);
        static_assert(irt::has_output_port<irt::qss1_dead_zone>);
        static_assert(irt::has_observation_function<irt::qss1_dead_zone>);

        static_assert(irt::has_initialize_function<irt::qss2_dead_zone>);
        static_assert(irt::has_lambda_function<irt::qss2_dead_zone>);
        static_assert(irt::has_transition_function<irt::qss2_dead_zone>);
        static_assert(irt::has_input_port<irt::qss2_dead_zone>);
        static_assert(irt::has_output_port<irt::qss2_dead_zone>);
        static_assert(irt::has_observation_function<irt::qss2_dead_zone>);

        static_assert(irt::has_initialize_function<irt::qss3_dead_zone>);
        static_assert(irt::has_lambda_function<irt::qss3_dead_zone>);
        static_assert(irt::has_transition_function<irt::qss3_dead_zone>);
        static_assert(irt::has_input_port<irt::qss3_dead_zone>);
        static_assert(irt::has_output_port<irt::qss3_dead_zone>);
        static_assert(irt::has_observation_function<irt::qss3_dead_zone>);

        static_assert(irt::has_initialize_function<irt::qss1_abs>);
        static_assert(irt::has_lambda_function<irt::qss1_abs>);
        static_assert(irt::has_transition_function<irt::qss1_abs>);
        static_assert(irt::has_input_port<irt::qss1_abs>);
        static_assert(irt::has_output_port<irt::qss1_abs>);
        static_assert(irt::has_observation_function<irt::qss1_abs>);

        static_assert(irt::has_initialize_function<irt::qss2_abs>);
        static_assert(irt::has_lambda_function<irt::qss2_abs>);
        static_assert(irt::has_transition_function<irt::qss2_abs>);
        static_assert(irt::has_input_port<irt::qss2_abs>);
        static_assert(irt::has_output_port<irt::qss2_abs>);
        static_assert(irt::has_observation_function<irt::qss2_abs>);

        static_assert(irt::has_initialize_function<irt::qss3_abs>);
        static_assert(irt::has_lambda_function<irt::qss3_abs>);
        static_assert(irt::has_transition_function<irt::qss3_abs>);
        static_assert(irt::has_input_port<irt::qss3_abs>);
        static_assert(irt::has_output_port<irt::qss3_abs>);
        static_assert(irt::has_observation_function<irt::qss3_abs>);

        static_assert(irt::has_initialize_function<irt::qss1_sign>);
        static_assert(irt::has_lambda_function<irt::qss1_sign>);
        static_assert(irt::has_transition_function<irt::qss1_sign>);
        static_assert(irt::has_input_port<irt::qss1_sign>);
        static_assert(irt::has_output_port<irt::qss1_sign>);
        static_assert(irt::has_observation_function<irt::qss1_sign>);

        static_assert(irt::has_initialize_function<irt::qss2_sign>);
        static_assert(irt::has_lambda_function<irt::qss2_sign>);
        static_assert(irt::has_transition_function<irt::qss2_sign>);
        static_assert(irt::has_input_port<irt::qss2_sign>);
        static_assert(irt::has_output_port<irt::qss2_sign>);
        static_assert(irt::has_observation_function<irt::qss2_sign>);

        static_assert(irt::has_initialize_function<irt::qss3_sign>);
        static_assert(irt::has_lambda_function<irt::qss3_sign>);
        static_assert(irt::has_transition_function<irt::qss3_sign>);
        static_assert(irt::has_input_port<irt::qss3_sign>);
        static_assert(irt::has_output_port<irt::qss3_sign>);
        static_assert(irt::has_observation_function<irt::qss3_sign>);

        static_assert(irt::has_initialize_function<irt::qss1_hysteresis>);
        static_assert(irt::has_lambda_function<irt::qss1_hysteresis>);
        static_assert(irt::has_transition_function<irt::qss1_hysteresis>);
        static_assert(irt::has_input_port<irt::qss1_hysteresis>);
        static_assert(irt::has_output_port<irt::qss1_hysteresis>);
        static_assert(irt::has_observation_function<irt::qss1_hysteresis>);

        static_assert(irt::has_initialize_function<irt::qss2_hysteresis>);
        static_assert(irt::has_lambda_function<irt::qss2_hysteresis>);
        static_assert(irt::has_transition_function<irt::qss2_hysteresis>);
        static_assert(irt::has_input_port<irt::qss2_hysteresis>);
        static_assert(irt::has_output_port<irt::qss2_hysteresis>);
        static_assert(irt::has_observation_function<irt::qss2_hysteresis>);

        static_assert(irt::has_initialize_function<irt::qss3_hysteresis>);
        static_assert(irt::has_lambda_function<irt::qss3_hysteresis>);
        static_assert(irt::has_transition_function<irt::qss3_hysteresis>);
        static_assert(irt::has_input_port<irt::qss3_hysteresis>);
        static_assert(irt::has_output_port<irt::qss3_hysteresis>);
        static_assert(irt::has_observation_function<irt::qss3_hysteresis>);

        static_assert(irt::has_initialize_function<irt::qss1_minimum>);
        static_assert(irt::has_lambda_function<irt::qss1_minimum>);
        static_assert(irt::has_transition_function<irt::qss1_minimum>);
        static_assert(irt::has_input_port<irt::qss1_minimum>);
        static_assert(irt::has_output_port<irt::qss1_minimum>);
        static_assert(irt::has_observation_function<irt::qss1_minimum>);

        static_assert(irt::has_initialize_function<irt::qss2_minimum>);
        static_assert(irt::has_lambda_function<irt::qss2_minimum>);
        static_assert(irt::has_transition_function<irt::qss2_minimum>);
        static_assert(irt::has_input_port<irt::qss2_minimum>);
        static_assert(irt::has_output_port<irt::qss2_minimum>);
        static_assert(irt::has_observation_function<irt::qss2_minimum>);

        static_assert(irt::has_initialize_function<irt::qss3_minimum>);
        static_assert(irt::has_lambda_function<irt::qss3_minimum>);
        static_assert(irt::has_transition_function<irt::qss3_minimum>);
        static_assert(irt::has_input_port<irt::qss3_minimum>);
        static_assert(irt::has_output_port<irt::qss3_minimum>);
        static_assert(irt::has_observation_function<irt::qss3_minimum>);

        static_assert(irt::has_initialize_function<irt::qss1_maximum>);
        static_assert(irt::has_lambda_function<irt::qss1_maximum>);
        static_assert(irt::has_transition_function<irt::qss1_maximum>);
        static_assert(irt::has_input_port<irt::qss1_maximum>);
        static_assert(irt::has_output_port<irt::qss1_maximum>);
        static_assert(irt::has_observation_function<irt::qss1_maximum>);

        static_assert(irt::has_initialize_function<irt::qss2_maximum>);
        static_assert(irt::has_lambda_function<irt::qss2_maximum>);
        static_assert(irt::has_transition_function<irt::qss2_maximum>);
        static_assert(irt::has_input_port<irt::qss2_maximum>);
        static_assert(irt::has_output_port<irt::qss2_maximum>);
        static_assert(irt::has_observation_function<irt::qss2_maximum>);

        static_assert(irt::has_initialize_function<irt::qss3_maximum>);
        static_assert(irt::has_lambda_function<irt::qss3_maximum>);
        static_assert(irt::has_transition_function<irt::qss3_maximum>);
        static_assert(irt::has_input_port<irt::qss3_maximum>);
        static_assert(irt::has_output_port<irt::qss3_maximum>);
        static_assert(irt::has_observation_function<irt::qss3_maximum>);

        static_assert(irt::has_initialize_function<irt::qss1_wrap>);
        static_assert(irt::has_lambda_function<irt::qss1_wrap>);
        static_assert(irt::has_transition_function<irt::qss1_wrap>);
        static_assert(irt::has_input_port<irt::qss1_wrap>);
        static_assert(irt::has_output_port<irt::qss1_wrap>);
        static_assert(irt::has_observation_function<irt::qss1_wrap>);

        static_assert(irt::has_initialize_function<irt::qss2_wrap>);
        static_assert(irt::has_lambda_function<irt::qss2_wrap>);
        static_assert(irt::has_transition_function<irt::qss2_wrap>);
        static_assert(irt::has_input_port<irt::qss2_wrap>);
        static_assert(irt::has_output_port<irt::qss2_wrap>);
        static_assert(irt::has_observation_function<irt::qss2_wrap>);

        static_assert(irt::has_initialize_function<irt::qss3_wrap>);
        static_assert(irt::has_lambda_function<irt::qss3_wrap>);
        static_assert(irt::has_transition_function<irt::qss3_wrap>);
        static_assert(irt::has_input_port<irt::qss3_wrap>);
        static_assert(irt::has_output_port<irt::qss3_wrap>);
        static_assert(irt::has_observation_function<irt::qss3_wrap>);

        static_assert(irt::has_initialize_function<irt::qss1_power>);
        static_assert(irt::has_lambda_function<irt::qss1_power>);
        static_assert(irt::has_transition_function<irt::qss1_power>);
        static_assert(irt::has_input_port<irt::qss1_power>);
        static_assert(irt::has_output_port<irt::qss1_power>);
        static_assert(irt::has_observation_function<irt::qss1_power>);

        static_assert(irt::has_initialize_function<irt::qss1_square>);
        static_assert(irt::has_lambda_function<irt::qss1_square>);
        static_assert(irt::has_transition_function<irt::qss1_square>);
        static_assert(irt::has_input_port<irt::qss1_square>);
        static_assert(irt::has_output_port<irt::qss1_square>);
        static_assert(irt::has_observation_function<irt::qss1_square>);

        static_assert(irt::has_initialize_function<irt::qss1_sum_2>);
        static_assert(irt::has_lambda_function<irt::qss1_sum_2>);
        static_assert(irt::has_transition_function<irt::qss1_sum_2>);
        static_assert(irt::has_input_port<irt::qss1_sum_2>);
        static_assert(irt::has_output_port<irt::qss1_sum_2>);
        static_assert(irt::has_observation_function<irt::qss1_sum_2>);

        static_assert(irt::has_initialize_function<irt::qss1_sum_3>);
        static_assert(irt::has_lambda_function<irt::qss1_sum_3>);
        static_assert(irt::has_transition_function<irt::qss1_sum_3>);
        static_assert(irt::has_input_port<irt::qss1_sum_3>);
        static_assert(irt::has_output_port<irt::qss1_sum_3>);
        static_assert(irt::has_observation_function<irt::qss1_sum_3>);

        static_assert(irt::has_initialize_function<irt::qss1_sum_4>);
        static_assert(irt::has_lambda_function<irt::qss1_sum_4>);
        static_assert(irt::has_transition_function<irt::qss1_sum_4>);
        static_assert(irt::has_input_port<irt::qss1_sum_4>);
        static_assert(irt::has_output_port<irt::qss1_sum_4>);
        static_assert(irt::has_observation_function<irt::qss1_sum_4>);

        static_assert(irt::has_initialize_function<irt::qss1_wsum_2>);
        static_assert(irt::has_lambda_function<irt::qss1_wsum_2>);
        static_assert(irt::has_transition_function<irt::qss1_wsum_2>);
        static_assert(irt::has_input_port<irt::qss1_wsum_2>);
        static_assert(irt::has_output_port<irt::qss1_wsum_2>);
        static_assert(irt::has_observation_function<irt::qss1_wsum_2>);

        static_assert(irt::has_initialize_function<irt::qss1_wsum_3>);
        static_assert(irt::has_lambda_function<irt::qss1_wsum_3>);
        static_assert(irt::has_transition_function<irt::qss1_wsum_3>);
        static_assert(irt::has_input_port<irt::qss1_wsum_3>);
        static_assert(irt::has_output_port<irt::qss1_wsum_3>);
        static_assert(irt::has_observation_function<irt::qss1_wsum_3>);

        static_assert(irt::has_initialize_function<irt::qss1_wsum_4>);
        static_assert(irt::has_lambda_function<irt::qss1_wsum_4>);
        static_assert(irt::has_transition_function<irt::qss1_wsum_4>);
        static_assert(irt::has_input_port<irt::qss1_wsum_4>);
        static_assert(irt::has_output_port<irt::qss1_wsum_4>);
        static_assert(irt::has_observation_function<irt::qss1_wsum_4>);

        static_assert(irt::has_initialize_function<irt::qss1_inverse>);
        static_assert(irt::has_lambda_function<irt::qss1_inverse>);
        static_assert(irt::has_transition_function<irt::qss1_inverse>);
        static_assert(irt::has_input_port<irt::qss1_inverse>);
        static_assert(irt::has_output_port<irt::qss1_inverse>);
        static_assert(irt::has_observation_function<irt::qss1_inverse>);

        static_assert(irt::has_initialize_function<irt::qss1_integer>);
        static_assert(irt::has_lambda_function<irt::qss1_integer>);
        static_assert(irt::has_transition_function<irt::qss1_integer>);
        static_assert(irt::has_input_port<irt::qss1_integer>);
        static_assert(irt::has_output_port<irt::qss1_integer>);
        static_assert(irt::has_observation_function<irt::qss1_integer>);

        static_assert(irt::has_initialize_function<irt::qss1_compare>);
        static_assert(irt::has_lambda_function<irt::qss1_compare>);
        static_assert(irt::has_transition_function<irt::qss1_compare>);
        static_assert(irt::has_input_port<irt::qss1_compare>);
        static_assert(irt::has_output_port<irt::qss1_compare>);
        static_assert(irt::has_observation_function<irt::qss1_compare>);

        static_assert(irt::has_initialize_function<irt::qss1_integrator>);
        static_assert(irt::has_lambda_function<irt::qss1_integrator>);
        static_assert(irt::has_transition_function<irt::qss1_integrator>);
        static_assert(irt::has_input_port<irt::qss1_integrator>);
        static_assert(irt::has_output_port<irt::qss1_integrator>);
        static_assert(irt::has_observation_function<irt::qss1_integrator>);

        static_assert(irt::has_initialize_function<irt::qss2_multiplier>);
        static_assert(irt::has_lambda_function<irt::qss2_multiplier>);
        static_assert(irt::has_transition_function<irt::qss2_multiplier>);
        static_assert(irt::has_input_port<irt::qss2_multiplier>);
        static_assert(irt::has_output_port<irt::qss2_multiplier>);
        static_assert(irt::has_observation_function<irt::qss2_multiplier>);

        static_assert(irt::has_initialize_function<irt::logical_and_2>);
        static_assert(irt::has_lambda_function<irt::logical_and_2>);
        static_assert(irt::has_transition_function<irt::logical_and_2>);
        static_assert(irt::has_input_port<irt::logical_and_2>);
        static_assert(irt::has_output_port<irt::logical_and_2>);
        static_assert(irt::has_observation_function<irt::logical_and_2>);

        static_assert(irt::has_initialize_function<irt::logical_invert>);
        static_assert(irt::has_lambda_function<irt::logical_invert>);
        static_assert(irt::has_transition_function<irt::logical_invert>);
        static_assert(irt::has_input_port<irt::logical_invert>);
        static_assert(irt::has_output_port<irt::logical_invert>);
        static_assert(irt::has_observation_function<irt::logical_invert>);

        static_assert(irt::has_initialize_function<irt::accumulator_2>);
        static_assert(not irt::has_lambda_function<irt::accumulator_2>);
        static_assert(irt::has_transition_function<irt::accumulator_2>);
        static_assert(irt::has_input_port<irt::accumulator_2>);
        static_assert(not irt::has_output_port<irt::accumulator_2>);
        static_assert(irt::has_observation_function<irt::accumulator_2>);

        static_assert(irt::has_initialize_function<irt::hsm_wrapper>);
        static_assert(irt::has_lambda_function<irt::hsm_wrapper>);
        static_assert(irt::has_transition_function<irt::hsm_wrapper>);
        static_assert(irt::has_input_port<irt::hsm_wrapper>);
        static_assert(irt::has_output_port<irt::hsm_wrapper>);
        static_assert(irt::has_observation_function<irt::hsm_wrapper>);

        static_assert(irt::has_initialize_function<irt::queue>);
        static_assert(irt::has_lambda_function<irt::queue>);
        static_assert(irt::has_transition_function<irt::queue>);
        static_assert(irt::has_input_port<irt::queue>);
        static_assert(irt::has_output_port<irt::queue>);
        static_assert(not irt::has_observation_function<irt::queue>);

        static_assert(irt::has_initialize_function<irt::dynamic_queue>);
        static_assert(irt::has_lambda_function<irt::dynamic_queue>);
        static_assert(irt::has_transition_function<irt::dynamic_queue>);
        static_assert(irt::has_input_port<irt::dynamic_queue>);
        static_assert(irt::has_output_port<irt::dynamic_queue>);
        static_assert(not irt::has_observation_function<irt::dynamic_queue>);

        static_assert(irt::has_initialize_function<irt::priority_queue>);
        static_assert(irt::has_lambda_function<irt::priority_queue>);
        static_assert(irt::has_transition_function<irt::priority_queue>);
        static_assert(irt::has_input_port<irt::priority_queue>);
        static_assert(irt::has_output_port<irt::priority_queue>);
        static_assert(not irt::has_observation_function<irt::priority_queue>);
    };

    "time"_test = [] {
        expect(irt::time_domain<irt::time>::infinity >
               irt::time_domain<irt::time>::zero);
        expect(irt::time_domain<irt::time>::zero >
               irt::time_domain<irt::time>::negative_infinity);
    };

    "path: extensions of the observation files"_test = [] {
        using irt::file_type;

        irt::path p{ "dir/run-0-0001.irb" };
        expect(p.has_extension() == file_type::irb_file);

        p.replace_extension(file_type::irt_manifest_file);
        expect(eq(p.sv(), std::string_view{ "dir/run-0-0001.manifest.jsonl" }));
        expect(p.has_extension() == file_type::irt_manifest_file);

        p.replace_extension(file_type::data_file);
        expect(eq(p.sv(), std::string_view{ "dir/run-0-0001.csv" }));
    };

    "message"_test = [] {
        using namespace irt::literals;

        {
            irt::message vdouble({ 0, 0, 0 });
            expect(eq(vdouble[0], 0.0_r));
            expect(eq(vdouble[1], 0.0_r));
            expect(eq(vdouble[2], 0.0_r));
        }

        {
            irt::message vdouble({ 1.0_r });
            expect(eq(vdouble[0], 1.0_r));
            expect(eq(vdouble[1], 0.0_r));
            expect(eq(vdouble[2], 0.0_r));
        }

        {
            irt::message vdouble({ 0.0_r, 1.0_r });
            expect(eq(vdouble[0], 0.0_r));
            expect(eq(vdouble[1], 1.0_r));
            expect(eq(vdouble[2], 0.0_r));
        }

        {
            irt::message vdouble({ 0.0_r, 0.0_r, 1.0_r });
            expect(eq(vdouble[0], 0.0_r));
            expect(eq(vdouble[1], 0.0_r));
            expect(eq(vdouble[2], 1.0_r));
        }
    };

    "heap_order"_test = [] {
        irt::heap h(4u);

        auto i1 = h.alloc(0.0, irt::model_id{ 0 });
        auto i2 = h.alloc(1.0, irt::model_id{ 1 });
        auto i3 = h.alloc(-1.0, irt::model_id{ 2 });
        auto i4 = h.alloc(2.0, irt::model_id{ 3 });

        expect(h[i1].tn == 0);
        expect(h[i2].tn == 1);
        expect(h[i3].tn == -1);
        expect(h[i4].tn == 2);

        expect(h.top() == i3);
        h.pop();
        expect(h.top() == i1);
        h.pop();
        expect(h.top() == i2);
        h.pop();
        expect(h.top() == i4);
        h.pop();

        expect(h.empty());
        expect(!h.full());
    };

    "heap_insert_pop"_test = [] {
        irt::heap h(4u);

        auto i1 = h.alloc(0, irt::model_id{ 0 });
        auto i2 = h.alloc(1, irt::model_id{ 1 });
        auto i3 = h.alloc(-1, irt::model_id{ 2 });
        auto i4 = h.alloc(2, irt::model_id{ 3 });

        expect(i1 != irt::invalid_heap_handle);
        expect(i2 != irt::invalid_heap_handle);
        expect(i3 != irt::invalid_heap_handle);
        expect(i4 != irt::invalid_heap_handle);

        expect(!h.empty());
        expect(h.top() == i3);

        h.pop(); // remove i3
        h.pop(); // rmeove i1

        expect(h.top() == i2);

        h[i3].tn = -10;
        h.insert(i3);
        h[i1].tn = -1;
        h.insert(i1);

        // h[i3].tn = -10;
        // h.insert(i3);

        // h[i1].tn = -1;
        // h.insert(i1);

        expect(h.top() == i3);
        h.pop();

        expect(h.top() == i1);
        h.pop();

        expect(h.top() == i2);
        h.pop();

        expect(h.top() == i4);
        h.pop();

        expect(h.empty());
    };

    "heap_with_equality"_test = [] {
        using namespace irt::literals;

        irt::heap h(256u);

        for (int t = 0; t < 100; ++t)
            h.alloc(irt::to_real(t), static_cast<irt::model_id>(t));

        expect(h.size() == 100_ul);

        h.alloc(50, irt::model_id{ 502 });
        h.alloc(50, irt::model_id{ 503 });
        h.alloc(50, irt::model_id{ 504 });

        expect(h.size() == 103_ul);

        for (irt::time t = 0, e = 50; t < e; ++t) {
            expect(h[h.top()].tn == t);
            h.pop();
        }

        expect(h[h.top()].tn == 50);
        h.pop();
        expect(h[h.top()].tn == 50);
        h.pop();
        expect(h[h.top()].tn == 50);
        h.pop();
        expect(h[h.top()].tn == 50);
        h.pop();

        for (irt::time t = 51, e = 100; t < e; ++t) {
            expect(h[h.top()].tn == t);
            h.pop();
        }
    };

    "heap_remove"_test = [] {
        using namespace irt::literals;

        irt::heap h(256u);

        for (int t = 0; t < 100; ++t)
            h.alloc(irt::to_real(t), static_cast<irt::model_id>(t));

        expect(h.size() == 100_ul);

        for (irt::u32 i = 0u; i < 100u; i += 2u)
            h.remove(i);

        expect(h[h.top()].tn == 1.0);

        for (irt::u32 i = 0u; i < 100u; i += 2u)
            h.reintegrate(irt::to_real(i), i);

        expect(h.size() == 100_u) << fatal;

        for (int t = 0; t < 100; ++t) {
            expect(h[h.top()].tn == static_cast<irt::real>(t));
            h.pop();
        }
    };

    "heap-middle-decrease"_test = [] {
        using namespace irt::literals;

        irt::heap h(256u);

        for (int t = 0; t < 100; ++t)
            h.alloc(irt::to_real(t), static_cast<irt::model_id>(t));

        expect(h.size() == 100_ul);

        for (irt::time t = 0, e = 50; t < e; ++t) {
            expect(h[h.top()].tn == t);
            h.pop();
        }

        expect(h[h.top()].tn == 50.0);
        constexpr irt::u32 move = 99u;

        h.decrease(0.0, move);
        expect(eq(h.top(), move));
        expect(eq(h[h.top()].tn, 0.0));
    };

    "hierarchy-simple"_test = [] {
        struct data_type {
            explicit data_type(int i_) noexcept
              : i(i_)
            {}

            int                       i;
            irt::hierarchy<data_type> d;
        };

        irt::vector<data_type> data(256, irt::reserve_tag);
        data_type              parent(999);
        parent.d.set_id(&parent);

        data.emplace_back(0);
        data[0].d.set_id(&parent);

        for (int i = 0; i < 15; ++i) {
            data.emplace_back(i + 1);
            data[i].d.set_id(&data[i]);
            data[i].d.parent_to(parent.d);
            expect(data[i].d.parented_by(parent.d));
        }

        expect(parent.d.get_parent() == nullptr);
        expect(parent.d.get_child() != nullptr);

        auto* child = parent.d.get_child();
        expect(child->d.get_child() == nullptr);

        int   i       = 1;
        auto* sibling = child->d.get_sibling();
        while (sibling) {
            ++i;

            sibling = sibling->d.get_sibling();
        }

        expect(i == 15);
    };

    "simulation-dispatch"_test = [] {
        irt::simulation sim;

        auto& dyn1 = sim.alloc<irt::qss1_sum_2>();
        (void)sim.alloc<irt::qss1_integrator>();
        (void)sim.alloc<irt::qss1_multiplier>();

        auto& mdl = irt::get_model(dyn1);

        irt::dispatch(mdl,
                      []([[maybe_unused]] auto& dyns) { std::cout << "ok"; });

        auto ret = irt::dispatch(
          mdl, []([[maybe_unused]] const auto& dyns) -> int { return 1; });

        expect(ret == 1);

        auto ret_2 = irt::dispatch(
          mdl, []([[maybe_unused]] const auto& dyns) { return 579.0; });

        expect(eq(ret_2, 579.0));
    };

    "constant_simulation"_test = [] {
        irt::on_error_callback = irt::debug::breakpoint;
        fmt::print("constant_simulation\n");
        irt::simulation sim;

        expect(sim.can_alloc(3));

        auto& cnt = sim.alloc<irt::counter>();
        auto& c1  = sim.alloc<irt::constant>();
        auto& c2  = sim.alloc<irt::constant>();

        get_p(sim, c1).set_constant(0, 0);
        get_p(sim, c2).set_constant(0, 0);

        expect(!!sim.connect_dynamics(c1, 0, cnt, 0));
        expect(!!sim.connect_dynamics(c2, 0, cnt, 0));

        expect(!!sim.initialize());

        do {
            expect(!!sim.run());
        } while (not sim.current_time_expired());

        expect(eq(cnt.event_number, static_cast<irt::i64>(2)));
    };

    "observation_simulation"_test = [] {
        irt::simulation sim;

        expect(sim.can_alloc(4));

        auto& cnt = sim.alloc<irt::qss1_sum_3>();
        auto& c1  = sim.alloc<irt::constant>();
        auto& c2  = sim.alloc<irt::constant>();
        auto& c3  = sim.alloc<irt::constant>();

        get_p(sim, c1).set_constant(10, 0);
        get_p(sim, c2).set_constant(100, 1);
        get_p(sim, c3).set_constant(1000, 2);

        expect(!!sim.connect_dynamics(c1, 0, cnt, 0));
        expect(!!sim.connect_dynamics(c2, 0, cnt, 1));
        expect(!!sim.connect_dynamics(c3, 0, cnt, 2));

        sim.limits.set_bound(0, 3);

        expect(sim.observe(get_model(cnt), 1.0).has_value());
        expect(!!sim.initialize());

        do {
            expect(!!sim.run());
        } while (not sim.current_time_expired());

        expect(sim.finalize().has_value());

        expect(fatal(eq(sim.observers.size(), 1u)));
        expect(fatal(sim.observers.begin() != sim.observers.end()));

        sim.tick_resamplers();

        irt::vector<irt::resampled_sample> h;
        expect(
          sim
            .drain_observations([&](const irt::observation_batch& b) noexcept {
                h.insert(h.end(), b.points.begin(), b.points.end());
                return irt::success();
            })
            .has_value());

        expect(fatal(eq(h.ssize(), 3)));
        expect(eq(h[0].value, 10.0));
        expect(eq(h[0].t, 0.0));
        expect(eq(h[1].value, 110.0));
        expect(eq(h[1].t, 1.0));
        expect(eq(h[2].value, 1110.0));
        expect(eq(h[2].t, 2.0));
    };

    "resampler_outputs"_test = [] {
        irt::resampler r(0.5, irt::interpolate_type::none);

        const irt::raw_sample first[] = { { 0.0, 1.0 }, { 0.5, 2.0 } };
        r.tick(first, 0.5);

        // The last sample waits for a later date.
        expect(fatal(eq(r.output().size(), 1u)));
        expect(eq(r.output()[0].t, 0.0));
        expect(eq(r.output_index(), 0u));
        expect(eq(r.raw_output().size(), 2u));
        expect(eq(r.raw_output_index(), 0u));

        r.clear_outputs();
        expect(r.output().empty() and r.raw_output().empty());
        expect(eq(r.output_index(), 1u));
        expect(eq(r.raw_output_index(), 2u));

        const irt::raw_sample second[] = { { 1.0, 4.0 } };
        r.tick(second, irt::time_domain<irt::time>::infinity);

        expect(fatal(eq(r.output().size(), 2u)));
        expect(eq(r.output()[0].value, 2.0));
        expect(eq(r.output()[1].value, 4.0));
        expect(eq(r.output_index(), 1u));
        expect(eq(r.point_count(), 3u));
        expect(eq(r.raw_output_index(), 2u));

        r.reset();
        expect(eq(r.point_count(), 0u));
        expect(r.output().empty() and r.raw_output().empty());
    };

    "observer_last_value"_test = [] {
        irt::observer obs(irt::undefined<irt::model_id>());
        expect(eq(obs.last_value(), 0.0));

        expect(obs.observe(irt::raw_sample(1.0, 3.0)) ==
               irt::buffer_status::ok);
        expect(eq(obs.last_value(), 3.0));

        const auto copy = obs;
        expect(eq(copy.last_value(), 3.0));

        obs.reset();
        expect(eq(obs.last_value(), 0.0));
    };

    "cross_simulation"_test = [] {
        fmt::print("cross_simulation\n");
        irt::simulation sim;

        expect(sim.can_alloc(3));

        auto& cnt   = sim.alloc<irt::counter>();
        auto& fn    = sim.alloc<irt::time_func>();
        auto& cross = sim.alloc<irt::qss1_cross>();

        get_p(sim, fn).set_time_func(0, 0.1, 2);
        get_p(sim, cross).set_cross(0);

        expect(sim.connect_dynamics(fn, 0, cross, 0).has_value());
        expect(sim.connect_dynamics(cross, 0, cnt, 0).has_value());

        sim.limits.set_duration(0, 10);
        expect(sim.initialize().has_value());

        do {
            expect(sim.run().has_value());
        } while (not sim.current_time_expired());

        expect(
          eq(cnt.event_number, static_cast<decltype(cnt.event_number)>(1)));
    };

    "hsm_automata"_test = [] {
        irt::hierarchical_state_machine            hsmw;
        irt::hierarchical_state_machine::execution exec;
        irt::external_source                       srcs;

        expect(!!hsmw.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsmw.set_state(1u, 0u));

        hsmw.states[1u].condition.set(3u, 7u);
        hsmw.states[1u].if_transition = 2u;

        expect(!!hsmw.set_state(2u, 0u));
        hsmw.states[2u].enter_action.set_output(
          irt::hierarchical_state_machine::variable::port_0, 1.f);

        expect(!!hsmw.start(exec, srcs));

        expect((int)exec.current_state == 1);
        exec.values = 0b00000011;

        expect(eq(exec.messages, 0));

        const auto processed = hsmw.dispatch(
          irt::hierarchical_state_machine::event_type::input_changed, exec,
          srcs);
        expect(!!processed);
        expect(processed.value() == true);

        expect(eq(exec.messages, 1));
    };

    "hsm_automata_timer"_test = [] {
        irt::hierarchical_state_machine            hsmw;
        irt::hierarchical_state_machine::execution exec;
        irt::external_source                       srcs;

        expect(!!hsmw.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsmw.set_state(1u, 0u));

        hsmw.states[1u]
          .condition
          .type = irt::hierarchical_state_machine::condition_type::port;
        hsmw.states[1u].condition.set(3u, 7u);
        hsmw.states[1u].if_transition = 2u;

        expect(!!hsmw.set_state(2u, 0u));
        hsmw.states[2u].enter_action.set_affect(
          irt::hierarchical_state_machine::variable::var_timer, 1.f);
        hsmw.states[2u].condition.set_timer();
        hsmw.states[2u].if_transition = 3u;

        expect(!!hsmw.set_state(3u, 0u));
        hsmw.states[2u].enter_action.set_output(
          irt::hierarchical_state_machine::variable::port_0, 1.f);

        expect(!!hsmw.start(exec, srcs));

        expect((int)exec.current_state == 1);
        exec.values = 0b00000011;

        expect(eq(exec.messages, 0));

        const auto processed = hsmw.dispatch(
          irt::hierarchical_state_machine::event_type::input_changed, exec,
          srcs);
        expect(!!processed);
        expect(processed.value() == true);
        expect((int)exec.current_state == 2);

        expect(eq(exec.messages, 1));
    };

    "hsm_condition_greater_registers"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        h.states[1u].enter_action.set_affect(var::var_r1, 1.0f);
        h.states[1u].if_transition = 2u;

        h.constants[0] = 0.5f;
        expect(!!h.set_state(2u, 0u));
        h.states[2u].condition.set_greater(var::var_r1, var::hsm_constant_0);
        h.states[2u].if_action.set_output(var::port_0, 1.0f);
        h.states[2u].else_action.set_output(var::port_0, 0.0f);
        h.states[2u].if_transition   = 3u;
        h.states[2u].else_transition = 3u;

        expect(!!h.set_state(3u, 0u)); // terminal
        expect(!!h.start(exec, srcs));

        const auto p1 = h.dispatch(hsm_t::event_type::internal, exec, srcs);
        expect(!!p1);

        const auto p2 = h.dispatch(hsm_t::event_type::internal, exec, srcs);
        expect(!!p2);

        expect(eq(exec.messages, 1));
        expect(approx((double)exec.message_values[0], 1.0, 1e-5));
    };

    "hsm_seed_and_negative_branch"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        h.constants[0] = 0.1f;
        h.constants[1] = 1.0f;
        h.constants[2] = -0.1f;

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        h.states[0u].enter_action.set_affect(var::var_r1, var::hsm_constant_1);

        expect(!!h.set_state(1u, 0u));
        h.states[1u].enter_action.set_affect(var::var_r2, -5.0f);
        h.states[1u].if_transition = 2u;

        expect(!!h.set_state(2u, 0u));
        h.states[2u].condition.set_greater(var::var_r2, var::hsm_constant_0);
        h.states[2u].if_transition   = 3u;
        h.states[2u].else_transition = 4u;

        expect(!!h.set_state(3u, 0u));

        expect(!!h.set_state(4u, 0u));
        h.states[4u].condition.set_less(var::var_r2, var::hsm_constant_2);
        h.states[4u].if_action.set_output(var::port_0, var::var_r2);
        h.states[4u].else_action.set_output(var::port_0, 0.0f);
        h.states[4u].if_transition   = 5u;
        h.states[4u].else_transition = 5u;

        expect(!!h.set_state(5u, 0u));
        expect(!!h.start(exec, srcs));

        const auto p1 = h.dispatch(hsm_t::event_type::internal, exec, srcs);
        expect(!!p1);

        const auto p2 = h.dispatch(hsm_t::event_type::internal, exec, srcs);
        expect(!!p2);

        const auto p3 = h.dispatch(hsm_t::event_type::internal, exec, srcs);
        expect(!!p3);
    };

    "hsm_omega_b_revision_cycle"_test = [] {
        irt::simulation sim(
          irt::simulation_reserve_definition(),
          irt::external_source_reserve_definition{ .constant_nb = 2 });

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);
        expect(sim.srcs.constant_sources.can_alloc(2u) >> fatal);

        // Three observations at t=1,2,3:
        //   obs1 = 1.50 -> error = 1.50 - 1.00(seed) = +0.50 -> REVISE (+)
        //   obs2 = 1.52 -> error = 1.52 - 1.50        = +0.02 -> HOLD
        //   obs3 = 1.00 -> error = 1.00 - 1.50        = -0.50 -> REVISE (-)
        auto& cst_value  = sim.srcs.constant_sources.alloc();
        cst_value.length = 3;
        cst_value.buffer = { 1.50, 1.52, 1.00 };

        auto& cst_ta  = sim.srcs.constant_sources.alloc();
        cst_ta.length = 3;
        cst_ta.buffer = { 1.0, 1.0, 1.0 };

        auto& cnt = sim.alloc<irt::counter>();

        auto& gen = sim.alloc<irt::generator>();
        get_p(sim, gen)
          .clear()
          .set_generator_ta(irt::source_type::constant,
                            sim.srcs.constant_sources.get_id(cst_ta))
          .set_generator_value(irt::source_type::constant,
                               sim.srcs.constant_sources.get_id(cst_value));

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        auto& hsm = sim.hsms.alloc();
        using var = irt::hierarchical_state_machine::variable;

        hsm.constants[0] = 0.1f;  // +epsilon_0
        hsm.constants[1] = 1.0f;  // seed lambda_hat
        hsm.constants[2] = -0.1f; // -epsilon_0

        // State 0 (top): seed the belief on entry -- confirmed active
        // during start().
        expect(!!hsm.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));
        hsm.states[0u].enter_action.set_affect(var::var_r1,
                                               var::hsm_constant_1);

        // State 1: wait for the dual-wired observation (port_0 AND port_1).
        // Corrected mask: port_0 -> bit 3, port_1 -> bit 2 => 0b1100.
        expect(!!hsm.set_state(1u, 0u));
        hsm.states[1u].condition.set(0b1100u, 0b1100u);
        hsm.states[1u].if_transition = 2u;

        // State 2: error = observed(port_1) - belief(var_r1).
        expect(!!hsm.set_state(2u, 0u));
        hsm.states[2u].enter_action.set_affect(var::var_r2, var::port_1);
        hsm.states[2u].exit_action.set_minus(var::var_r2, var::var_r1);
        hsm.states[2u].if_transition = 3u;

        // State 3: positive branch, error > +epsilon_0.
        expect(!!hsm.set_state(3u, 0u));
        hsm.states[3u].condition.set_greater(var::var_r2, var::hsm_constant_0);
        hsm.states[3u].if_transition   = 5u;
        hsm.states[3u].else_transition = 4u;

        // State 4: negative branch, error < -epsilon_0.
        expect(!!hsm.set_state(4u, 0u));
        hsm.states[4u].condition.set_less(var::var_r2, var::hsm_constant_2);
        hsm.states[4u].if_transition   = 5u;
        hsm.states[4u].else_transition = 1u; // hold: back to waiting

        // State 5: shared revision -- emit, return to waiting.
        expect(!!hsm.set_state(5u, 0u));
        hsm.states[5u].enter_action.set_plus(var::var_r1, var::var_r2);
        hsm.states[5u].exit_action.set_output(var::port_0, var::var_r1);
        hsm.states[5u].if_transition = 1u;

        auto& hsmw = sim.alloc<irt::hsm_wrapper>();
        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(hsm)));

        expect(!!sim.connect_dynamics(gen, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(gen, 0, hsmw, 1));
        expect(!!sim.connect_dynamics(hsmw, 0, cnt, 0));

        sim.limits.set_bound(0, 4);

        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;
        // int         iter = 0;

        do {
            st = sim.run();
            expect(!!st);
            // fmt::print("iter={:2} hsm_state={} r1={:6.3f} r2={:6.3f} "
            //            "events={} last={:6.3f}\n",
            //            iter++,
            //            (int)hsmw.exec.current_state,
            //            hsmw.exec.r1,
            //            hsmw.exec.r2,
            //            cnt.event_number,
            //            cnt.last_value);
        } while (not sim.current_time_expired());

        // fmt::print("final: events={} last={:6.3f} r1={:6.3f}\n",
        //            cnt.event_number,
        //            cnt.last_value,
        //            hsmw.exec.r1);

        expect(eq(cnt.event_number, 2));
        expect(eq(cnt.last_value, 1.0));
        expect(eq(hsmw.exec.r1, 1.0));
    };

    "hsm_hierarchy_exit_effect_enter_order"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        // root 0 > A 1 > A1 2, and B 3. A leaves to B on a message on
        // port_0 while A1 waits its timer.
        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u, 2u));
        expect(!!h.set_state(2u, 1u));
        expect(!!h.set_state(3u, 0u));
        expect(!!h.check());

        h.states[1u].condition.set(0b1000u, 0b1000u);
        h.states[1u].if_transition = 3u;
        h.states[1u].if_action.set_output(var::port_0, 3.f);
        h.states[1u].exit_action.set_output(var::port_0, 2.f);

        h.states[2u].enter_action.set_affect(var::var_timer, 5.f);
        h.states[2u].condition.set_timer();
        h.states[2u].if_transition = 2u;
        h.states[2u].exit_action.set_output(var::port_0, 1.f);

        h.states[3u].enter_action.set_output(var::port_0, 4.f);

        expect(!!h.start(exec, srcs));
        expect(eq((int)exec.current_state, 2));
        expect(eq(exec.timers[2], 5.0));

        exec.values    = 0b1000;
        const auto ret = h.dispatch(hsm_t::event_type::input_changed, exec,
                                    srcs);
        expect(ret.has_value() and *ret);
        expect(eq((int)exec.current_state, 3));

        // Exit of the leaf, exit of its super state, effect, enter.
        expect(eq(exec.messages, 4) >> fatal);
        expect(eq(exec.message_values[0], 1.0));
        expect(eq(exec.message_values[1], 2.0));
        expect(eq(exec.message_values[2], 3.0));
        expect(eq(exec.message_values[3], 4.0));
        expect(eq(exec.next_timer(), irt::time_domain<irt::time>::infinity));
    };

    "hsm_hierarchy_completion_of_composite"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        // root 0 > A 1 > (A1 2 -> A2 3), and B 4: A2 is a final state, the
        // completion of A goes to B.
        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u, 2u));
        expect(!!h.set_state(2u, 1u));
        expect(!!h.set_state(3u, 1u));
        expect(!!h.set_state(4u, 0u));

        h.states[1u].if_transition = 4u;
        h.states[2u].if_transition = 3u;

        expect(!!h.start(exec, srcs));
        expect(eq((int)exec.current_state, 2));

        expect(!!h.run_to_completion(exec, false, srcs));
        expect(eq((int)exec.current_state, 4));
    };

    "hsm_hierarchy_self_transition_of_composite"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        // root 0 > A 1 > (A1 2, A2 3): A restarts on a message, the
        // external transition exits A and enters A and its initial state.
        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u, 2u));
        expect(!!h.set_state(2u, 1u));
        expect(!!h.set_state(3u, 1u));

        h.states[1u].condition.set(0b1000u, 0b1000u);
        h.states[1u].if_transition = 1u;
        h.states[1u].enter_action.set_plus(var::var_i1, 1);

        h.states[2u].condition.set(0b0100u, 0b0100u);
        h.states[2u].if_transition = 3u;
        h.states[2u].enter_action.set_plus(var::var_i2, 1);

        expect(!!h.start(exec, srcs));
        exec.values = 0b0100;
        expect(!!h.dispatch(hsm_t::event_type::input_changed, exec, srcs));
        expect(eq((int)exec.current_state, 3));

        exec.values = 0b1000;
        expect(!!h.dispatch(hsm_t::event_type::input_changed, exec, srcs));
        expect(eq((int)exec.current_state, 2));
        expect(eq(exec.i1, 2));
        expect(eq(exec.i2, 2));
    };

    "hsm_hierarchy_check"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;

        // The invalid machines are expected errors.
        const auto callback = std::exchange(irt::on_error_callback, nullptr);

        {
            hsm_t h;
            expect(not h.check());
        }

        {
            hsm_t h;
            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u));
            h.states[2u].super_id = 1u;
            expect(not h.check());
            h.states[1u].sub_id = 2u;
            expect(!!h.check());
        }

        {
            hsm_t h;
            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u, 2u));
            expect(!!h.set_state(2u, 1u, 1u));
            h.states[1u].super_id = 2u;
            expect(not h.check());
        }

        {
            hsm_t h;
            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            for (irt::u8 i = 1u; i != hsm_t::max_depth; ++i)
                expect(!!h.set_state(i, i - 1u, i + 1u));
            h.states[hsm_t::max_depth - 1].sub_id = hsm_t::invalid_state_id;
            expect(!!h.check());

            h.states[hsm_t::max_depth - 1].sub_id = hsm_t::max_depth;
            expect(!!h.set_state(hsm_t::max_depth, hsm_t::max_depth - 1));
            expect(not h.check());
        }

        {
            hsm_t h;
            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u));
            h.states[1u].if_transition = 0u;
            expect(not h.check());
        }

        irt::on_error_callback = callback;
    };

    "hsm_hierarchy_dispatch_returns_source_error"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        h.states[1u].condition.set(0b1000u, 0b1000u);
        expect(!!h.start(exec, srcs));

        // The source of the execution references no external source: the
        // error must not be taken for an unconsumed event.
        h.flags.set(hsm_t::option::use_source);
        exec.values = 0b1000;

        const auto callback = std::exchange(irt::on_error_callback, nullptr);
        const auto ret      = h.dispatch(hsm_t::event_type::input_changed, exec,
                                         srcs);
        irt::on_error_callback = callback;

        expect(not ret.has_value());
        expect(eq((int)exec.current_state, 1));
    };

    "hsm_hierarchy_devs_nested_timers"_test = [] {
        using var = hsm_bench::var;

        // Marche leaves at t = 9 while Tic loops every 2.
        hsm_bench b;
        b.machine.states[1u].enter_action.set_affect(var::var_timer, 9.f);
        b.machine.states[1u].condition.set_timer();

        expect(run_until(b.sim, 8.5) >> fatal);
        expect(eq(b.ticks.event_number, 4));
        expect(eq(b.stops.event_number, 0));

        hsm_bench c;
        c.machine.states[1u].enter_action.set_affect(var::var_timer, 9.f);
        c.machine.states[1u].condition.set_timer();

        expect(run_until(c.sim, 20.0) >> fatal);
        expect(eq(c.ticks.event_number, 4));
        expect(eq(c.stops.event_number, 1));
        expect(eq((int)c.hsmw.exec.current_state, 3));
        expect(eq(c.hsmw.sigma, irt::time_domain<irt::time>::infinity));
    };

    "hsm_hierarchy_devs_interruption"_test = [] {
        // A message on x[0] at t = 3.5 stops Marche and its Tic loop.
        hsm_bench b;
        b.machine.states[1u].condition.set(0b1000u, 0b1000u);
        b.send_at(3.5, 0);

        expect(run_until(b.sim, 10.0) >> fatal);
        expect(eq(b.ticks.event_number, 1));
        expect(eq(b.stops.event_number, 1));
        expect(eq((int)b.hsmw.exec.current_state, 3));
        expect(
          eq(b.hsmw.exec.next_timer(), irt::time_domain<irt::time>::infinity));
    };

    "hsm_completion_goes_before_the_next_timer"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > Y 1 > (A 2 -> M 3 -> Z 5), and C 4: the timers of Y and A
        // expire together at t = 2. Z is entered before the timer of Y leaves
        // it, so each state sends its message.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u, 2u));
        expect(!!h.set_state(2u, 1u));
        expect(!!h.set_state(3u, 1u));
        expect(!!h.set_state(5u, 1u));
        expect(!!h.set_state(4u, 0u));

        h.states[1u].enter_action.set_affect(var::var_timer, 2.f);
        h.states[1u].condition.set_timer();
        h.states[1u].if_transition = 4u;

        h.states[2u].enter_action.set_affect(var::var_timer, 2.f);
        h.states[2u].condition.set_timer();
        h.states[2u].if_transition = 3u;
        h.states[2u].exit_action.set_output(var::port_0, 1.f);

        h.states[3u].if_transition = 5u;
        h.states[3u].exit_action.set_output(var::port_0, 1.f);

        h.states[5u].exit_action.set_output(var::port_0, 1.f);

        h.states[4u].enter_action.set_output(var::port_0, 1.f);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq((int)hsmw.exec.current_state, 4));
        expect(eq(out.event_number, 4));
    };

    "hsm_completion_goes_before_the_message"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        // root 0 > Y 1 > (A 2 -> M 3 -> Z 5), and C 4: A leaves on its timer,
        // M has a completion transition and Y leaves on a message.
        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u, 2u));
        expect(!!h.set_state(2u, 1u));
        expect(!!h.set_state(3u, 1u));
        expect(!!h.set_state(5u, 1u));
        expect(!!h.set_state(4u, 0u));

        h.states[1u].condition.set(0b1000u, 0b1000u);
        h.states[1u].if_transition = 4u;

        h.states[2u].enter_action.set_affect(var::var_timer, 2.f);
        h.states[2u].condition.set_timer();
        h.states[2u].if_transition = 3u;

        h.states[3u].if_transition = 5u;
        h.states[5u].enter_action.set_plus(var::var_i1, 1);

        expect(!!h.start(exec, srcs));
        exec.advance_timers(2.0);
        exec.values.set(3, true);
        expect(!!h.run_to_completion(exec, true, srcs));

        expect(eq((int)exec.current_state, 4));
        expect(eq(exec.i1, 1));
    };

    "hsm_message_goes_before_the_condition_of_the_current_state"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        // root 0 > Y 1 > (S 2 -> T 3), and C 4: the condition of S reads the
        // value of the message, and Y leaves on the same message.
        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u, 2u));
        expect(!!h.set_state(2u, 1u));
        expect(!!h.set_state(3u, 1u));
        expect(!!h.set_state(4u, 0u));

        h.states[1u].condition.set(0b1000u, 0b1000u);
        h.states[1u].if_transition = 4u;

        h.states[2u].condition.set_greater(var::port_0, 0.f);
        h.states[2u].if_transition = 3u;

        h.states[3u].enter_action.set_plus(var::var_i1, 1);

        exec.clear();
        expect(!!h.start(exec, srcs));
        expect(!!h.run_to_completion(exec, false, srcs));
        expect(eq((int)exec.current_state, 2));

        exec.ports[3] = 1.0;
        exec.values.set(3, true);
        expect(!!h.run_to_completion(exec, true, srcs));
        expect(eq((int)exec.current_state, 4));
        expect(eq(exec.i1, 0));
    };

    "hsm_condition_is_checked_after_an_internal_transition"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t                h;
        hsm_t::execution     exec;
        irt::external_source srcs;

        // root 0 > Y 1 > (S 2 -> T 3): a message runs the internal transition
        // of Y, and its action validates the condition of S.
        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u, 2u));
        expect(!!h.set_state(2u, 1u));
        expect(!!h.set_state(3u, 1u));

        h.states[1u].condition.set(0b1000u, 0b1000u);
        h.states[1u].if_action.set_plus(var::var_i1, 1);

        h.states[2u].condition.set_equal_to(var::var_i1, 1);
        h.states[2u].if_transition = 3u;

        expect(!!h.start(exec, srcs));
        expect(!!h.run_to_completion(exec, false, srcs));
        expect(eq((int)exec.current_state, 2));

        exec.values.set(3, true);
        expect(!!h.run_to_completion(exec, true, srcs));
        expect(eq((int)exec.current_state, 3));
    };

    "hsm_timer_and_message_at_the_same_time"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > Y 1 > (A 2 -> M 3 -> Z 5), and C 4: the timer of A and the
        // message of x[0] are both at t = 2. The kernel delivers the message
        // at the next step of t, after the internal transition of the machine.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           out  = sim.alloc<irt::counter>();
        auto&           cst  = sim.alloc<irt::constant>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u, 2u));
        expect(!!h.set_state(2u, 1u));
        expect(!!h.set_state(3u, 1u));
        expect(!!h.set_state(5u, 1u));
        expect(!!h.set_state(4u, 0u));

        h.states[1u].condition.set(0b1000u, 0b1000u);
        h.states[1u].if_transition = 4u;

        h.states[2u].enter_action.set_affect(var::var_timer, 2.f);
        h.states[2u].condition.set_timer();
        h.states[2u].if_transition = 3u;
        h.states[2u].exit_action.set_output(var::port_0, 1.f);

        h.states[3u].if_transition = 5u;
        h.states[3u].exit_action.set_output(var::port_0, 1.f);

        h.states[5u].exit_action.set_output(var::port_0, 1.f);

        h.states[4u].enter_action.set_output(var::port_0, 1.f);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        get_p(sim, cst).set_constant(1.0, 2.0);
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));
        expect(!!sim.connect_dynamics(cst, 0, hsmw, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq((int)hsmw.exec.current_state, 4));
        expect(eq(out.event_number, 4));
    };

    "hsm_hierarchy_devs_unconsumed_message_keeps_timer"_test = [] {
        // Marche waits x[1]: the message on x[0] at t = 3.5 is lost and
        // Tic still wakes up at t = 4.
        hsm_bench b;
        b.machine.states[1u].condition.set(0b0100u, 0b0100u);
        b.send_at(3.5, 0);

        expect(run_until(b.sim, 4.5) >> fatal);
        expect(eq(b.ticks.event_number, 2));
        expect(eq(b.stops.event_number, 0));
        expect(eq((int)b.hsmw.exec.current_state, 2));
    };

    "hsm_hierarchy_devs_completion"_test = [] {
        using var = hsm_bench::var;

        // Tic goes to the final state 4 at t = 2: the completion of Marche
        // goes to Arret at the same date.
        hsm_bench b;
        expect(!!b.machine.set_state(4u, 1u));
        b.machine.states[2u].if_transition = 4u;
        b.machine.states[2u].if_action.set_output(var::port_0, 1.f);

        expect(run_until(b.sim, 3.0) >> fatal);
        expect(eq(b.ticks.event_number, 1));
        expect(eq(b.stops.event_number, 1));
        expect(eq((int)b.hsmw.exec.current_state, 3));
    };

    "hsm_hierarchy_devs_timer_before_message"_test = [] {
        using var = hsm_bench::var;

        // At t = 2, Tic wakes up to Wait 4 which waits x[0], then the message
        // of the same date is dispatched to Wait.
        hsm_bench b;
        expect(!!b.machine.set_state(4u, 1u));
        b.machine.states[2u].if_transition = 4u;
        b.machine.states[2u].if_action.clear();
        b.machine.states[4u].condition.set(0b1000u, 0b1000u);
        b.machine.states[4u].if_transition = 3u;
        b.machine.states[4u].if_action.set_output(var::port_1, 1.f);
        b.send_at(2.0, 0);

        expect(run_until(b.sim, 3.0) >> fatal);
        expect(eq(b.ticks.event_number, 0));
        expect(eq(b.stops.event_number, 1));
        expect(eq((int)b.hsmw.exec.current_state, 3));
    };

    "hsm_completion_cycle_check"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        const auto cycle = irt::simulation_errc::hsm_completion_cycle_error;
        const auto none  = irt::simulation_errc::none;

        // root 0 > (A 1, B 2): the completion transitions without guard
        // A -> B -> A never stop.
        {
            hsm_t h;
            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u));
            expect(!!h.set_state(2u, 0u));
            h.states[1u].if_transition = 2u;
            h.states[2u].if_transition = 1u;
            expect(h.find_error() == cycle);

            // A chain is valid.
            h.states[2u].if_transition = hsm_t::invalid_state_id;
            expect(h.find_error() == none);

            // A self-transition re-enters A for ever.
            h.states[2u].if_transition = 2u;
            expect(h.find_error() == cycle);
        }

        // root 0 > (S 1 > A 2, B 3): A has no transition, its completion
        // goes to S, which leaves to B, and B goes back to S, so to A.
        {
            hsm_t h;
            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u, 2u));
            expect(!!h.set_state(2u, 1u));
            expect(!!h.set_state(3u, 0u));
            h.states[1u].if_transition = 3u;
            h.states[3u].if_transition = 1u;
            expect(h.find_error() == cycle);

            // A port condition of A does not stop its completion: it is
            // ignored, and A then has no completion event at all.
            h.states[2u].condition.set(0b1000u, 0b1000u);
            expect(h.find_error() == none);
        }

        // A guard or a timer makes the transition uncertain.
        {
            hsm_t h;
            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u));
            expect(!!h.set_state(2u, 0u));
            h.states[1u].condition.set_equal_to(var::var_i1, 0);
            h.states[1u].if_transition = 2u;
            h.states[2u].if_transition = 1u;
            expect(h.find_error() == none);

            h.states[1u].condition.set_timer();
            expect(h.find_error() == none);
        }
    };

    "hsm_run_to_completion_is_bounded"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        const auto callback = std::exchange(irt::on_error_callback, nullptr);

        // root 0 > (A 1, B 2): the guards of A and B are always true.
        {
            hsm_t                h;
            hsm_t::execution     exec;
            irt::external_source srcs;

            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u));
            expect(!!h.set_state(2u, 0u));
            h.states[1u].condition.set_equal_to(var::var_i1, 0);
            h.states[1u].if_transition = 2u;
            h.states[2u].condition.set_equal_to(var::var_i1, 0);
            h.states[2u].if_transition = 1u;

            expect(h.find_error() == irt::simulation_errc::none);
            expect(!!h.start(exec, srcs));
            expect(not h.run_to_completion(exec, false, srcs));
        }

        // root 0 > (A 1, B 2): two timers of null duration.
        {
            hsm_t                h;
            hsm_t::execution     exec;
            irt::external_source srcs;

            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u));
            expect(!!h.set_state(2u, 0u));
            for (const irt::u8 i : { 1u, 2u }) {
                h.states[i].enter_action.set_affect(var::var_timer, 0.f);
                h.states[i].condition.set_timer();
            }
            h.states[1u].if_transition = 2u;
            h.states[2u].if_transition = 1u;

            expect(h.find_error() == irt::simulation_errc::none);
            expect(!!h.start(exec, srcs));
            expect(not h.run_to_completion(exec, false, srcs));
        }

        irt::on_error_callback = callback;

        // root 0 > (A 1, B 2, C 3): A goes to B while i1 <= 9 and B
        // increments i1, then A leaves to C.
        {
            hsm_t                h;
            hsm_t::execution     exec;
            irt::external_source srcs;

            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u));
            expect(!!h.set_state(2u, 0u));
            expect(!!h.set_state(3u, 0u));
            h.states[1u].condition.set_less_equal(var::var_i1, 9);
            h.states[1u].if_transition   = 2u;
            h.states[1u].else_transition = 3u;
            h.states[2u].enter_action.set_plus(var::var_i1, 1);
            h.states[2u].if_transition = 1u;
            h.states[3u].condition.set(0b1000u, 0b1000u);

            expect(h.find_error() == irt::simulation_errc::none);
            expect(!!h.start(exec, srcs));
            expect(!!h.run_to_completion(exec, false, srcs));
            expect(eq((int)exec.current_state, 3));
            expect(eq(exec.i1, 10));
        }
    };

    "hsm_output_overflow"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // A hierarchy of n levels whose enter actions send a message each.
        const auto make = [](hsm_t& h, const irt::u8 n) {
            for (irt::u8 i = 0u; i != n; ++i) {
                const auto super = i == 0u ? hsm_t::invalid_state_id
                                           : static_cast<irt::u8>(i - 1u);
                const auto sub   = i + 1u == n ? hsm_t::invalid_state_id
                                               : static_cast<irt::u8>(i + 1u);
                expect(!!h.set_state(i, super, sub));
                h.states[i].enter_action.set_output(var::port_0, 1.f);
            }

            h.states[n - 1u].condition.set(0b1000u, 0b1000u);
        };

        {
            hsm_t                h;
            hsm_t::execution     exec;
            irt::external_source srcs;

            make(h, 4u);
            expect(!!h.start(exec, srcs));
            expect(eq(exec.messages, 4));
            expect(not exec.message_overflow);
        }

        {
            hsm_t                h;
            hsm_t::execution     exec;
            irt::external_source srcs;

            make(h, 5u);

            const auto callback = std::exchange(irt::on_error_callback,
                                                nullptr);
            expect(not h.start(exec, srcs));
            irt::on_error_callback = callback;

            expect(exec.message_overflow);
        }
    };

    "hsm_output_overflow_stops_the_simulation"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > (A 1 -> B 2 -> C 3 -> D 4): at t = 1, the timer of A
        // runs the exit of A, B and C and the enter of B, C and D.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        for (const irt::u8 i : { 1u, 2u, 3u, 4u })
            expect(!!h.set_state(i, 0u));

        h.states[1u].enter_action.set_affect(var::var_timer, 1.f);
        h.states[1u].condition.set_timer();
        h.states[1u].if_transition = 2u;
        h.states[1u].exit_action.set_output(var::port_0, 1.f);

        for (const irt::u8 i : { 2u, 3u }) {
            h.states[i].enter_action.set_output(var::port_0, 1.f);
            h.states[i].exit_action.set_output(var::port_0, 1.f);
            h.states[i].if_transition = i + 1u;
        }

        h.states[4u].enter_action.set_output(var::port_0, 1.f);
        h.states[4u].condition.set(0b1000u, 0b1000u);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        const auto callback = std::exchange(irt::on_error_callback, nullptr);
        expect(not run_until(sim, 10.0));
        irt::on_error_callback = callback;

        expect(eq(out.event_number, 0));
    };

    "hsm_port_reads_the_maximum_of_the_bag"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > (A 1, B 2): A goes to B if port_0 > 2. Two constants send
        // 1 and 3 on x[0] at t = 1, in both orders of the bag.
        for (const auto three_first : { true, false }) {
            irt::simulation sim{ irt::simulation_reserve_definition(),
                                 irt::external_source_reserve_definition() };
            auto&           h    = sim.hsms.alloc();
            auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
            auto&           out  = sim.alloc<irt::counter>();
            auto&           c1   = sim.alloc<irt::constant>();
            auto&           c2   = sim.alloc<irt::constant>();

            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            expect(!!h.set_state(1u, 0u));
            expect(!!h.set_state(2u, 0u));
            h.states[1u].condition.set_greater(var::port_0, 2.f);
            h.states[1u].if_transition = 2u;
            h.states[2u].enter_action.set_output(var::port_0, 1.f);
            h.states[2u].condition.set(0b1000u, 0b1000u);

            get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
            get_p(sim, c1).set_constant(three_first ? 3.0 : 1.0, 1.0);
            get_p(sim, c2).set_constant(three_first ? 1.0 : 3.0, 1.0);
            expect(!!sim.connect_dynamics(c1, 0, hsmw, 0));
            expect(!!sim.connect_dynamics(c2, 0, hsmw, 0));
            expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

            expect(run_until(sim, 10.0) >> fatal);
            expect(eq((int)hsmw.exec.current_state, 2)) << three_first;
            expect(eq(out.event_number, 1)) << three_first;
        }
    };

    "hsm_output_flush_does_not_run_the_machine"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > A 1: the guard of A is always true and its self-transition
        // sends a message. The flush of this message must not fire it again.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        h.states[1u].condition.set_equal_to(var::var_i1, 0);
        h.states[1u].if_transition = 1u;
        h.states[1u].if_action.set_output(var::port_0, 1.f);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        sim.limits.set_bound(0, 10.0);
        expect(!!sim.srcs.prepare() >> fatal);
        expect(!!sim.initialize() >> fatal);

        // Without the fix, the simulation stays at t = 0 forever.
        for (auto step = 0; step < 100 and not sim.current_time_expired();
             ++step)
            expect(!!sim.run() >> fatal);

        expect(sim.current_time_expired() >> fatal);
        expect(eq(out.event_number, 1));
    };

    "hsm_negative_initial_timer"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > (A 1, B 2): the enter action of A arms the timer to -3.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        expect(!!h.set_state(2u, 0u));
        h.states[1u].enter_action.set_affect(var::var_timer, -3.f);
        h.states[1u].condition.set_timer();
        h.states[1u].if_transition = 2u;
        h.states[2u].enter_action.set_output(var::port_0, 1.f);
        h.states[2u].condition.set(0b1000u, 0b1000u);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        sim.limits.set_bound(0, 10.0);
        expect(!!sim.srcs.prepare() >> fatal);
        expect(!!sim.initialize() >> fatal);
        expect(eq(hsmw.sigma, 0.0));

        while (not sim.current_time_expired())
            expect(!!sim.run() >> fatal);

        expect(eq((int)hsmw.exec.current_state, 2));
        expect(eq(out.event_number, 1));
    };

    "hsm_infinite_timeout_is_passive"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        {
            irt::parameter p;
            p.set_hsm_wrapper(0, 0, 0.0, 0.0,
                              std::numeric_limits<irt::real>::quiet_NaN());
            expect(irt::time_domain<irt::time>::is_infinity(
              p.reals[irt::hsm_wrapper_tag::timer]));
        }

        // root 0 > (A 1, B 2): A has a timer condition but no enter action,
        // its timer is the timeout of the parameter.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        expect(!!h.set_state(2u, 0u));
        h.states[1u].condition.set_timer();
        h.states[1u].if_transition = 2u;
        h.states[2u].enter_action.set_output(var::port_0, 1.f);
        h.states[2u].condition.set(0b1000u, 0b1000u);

        get_p(sim, hsmw)
          .set_hsm_wrapper(ordinal(sim.hsms.get_id(h)))
          .set_hsm_wrapper(0, 0, 0.0, 0.0,
                           irt::time_domain<irt::time>::infinity);
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq((int)hsmw.exec.current_state, 1));
        expect(eq(out.event_number, 0));
    };

    "output_port_sends_several_messages_in_one_step"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;

        // State 1 goes to state 2 at once: the exit action sends 8 and the
        // enter action sends 1 on the same port. Both messages arrive in the
        // same bag and the receiver reads the maximum of the bag.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           r    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           recv = sim.alloc<irt::hsm_wrapper>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        expect(!!h.set_state(2u, 0u));
        h.states[1u].exit_action.set_output(hsm_t::variable::port_0, 8.f);
        h.states[2u].enter_action.set_output(hsm_t::variable::port_0, 1.f);
        h.states[1u].if_transition = 2u;

        expect(!!r.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!r.set_state(1u, 0u));
        r.states[1u].condition.set(0b1000u, 0b1000u);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        get_p(sim, recv).set_hsm_wrapper(ordinal(sim.hsms.get_id(r)));
        expect(!!sim.connect_dynamics(hsmw, 0, recv, 0));
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq(out.event_number, 2));
        expect(eq(out.sum_values, irt::real(9)));
        expect(eq(recv.exec.ports[3], irt::real(8)));
    };

    "message_at_the_date_of_an_emission"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // At t = 1, the quantizer receives 1 from a constant, then 3 from a
        // wrapper at the date of the emission of the level 1: both levels
        // must leave the quantizer.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           cst  = sim.alloc<irt::constant>();
        auto&           q    = sim.alloc<irt::qss1_quantizer>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        expect(!!h.set_state(2u, 0u));
        h.states[1u].enter_action.set_affect(var::var_timer, 1.f);
        h.states[1u].condition.set_timer();
        h.states[1u].if_transition = 2u;
        h.states[1u].if_action.set_output(var::port_0, 3.f);
        h.states[2u].condition.set(0b1000u, 0b1000u);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        get_p(sim, cst).set_constant(1.0, 1.0);
        expect(!!sim.connect_dynamics(cst, 0, q, 0));
        expect(!!sim.connect_dynamics(hsmw, 0, q, 0));
        expect(!!sim.connect_dynamics(q, 0, out, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq(out.event_number, 2));
        expect(eq(out.last_value, irt::real(3)));
    };

    "hsm_timer_and_bag_in_the_same_transition"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > (A 1, B 2, C 3, D 4): at t = 1, the timer of A goes to B
        // and a constant sends 3 on x[0]. The completion of B reads port_0
        // before the bag: B goes to D, which sends 6.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           cst  = sim.alloc<irt::constant>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        for (const irt::u8 i : { 1u, 2u, 3u, 4u })
            expect(!!h.set_state(i, 0u));

        h.states[1u].enter_action.set_affect(var::var_timer, 1.f);
        h.states[1u].condition.set_timer();
        h.states[1u].if_transition = 2u;
        h.states[2u].condition.set_greater(var::port_0, 2.f);
        h.states[2u].if_transition   = 3u;
        h.states[2u].else_transition = 4u;
        h.states[3u].enter_action.set_output(var::port_0, 5.f);
        h.states[3u].condition.set(0b0001u, 0b0001u);
        h.states[4u].enter_action.set_output(var::port_0, 6.f);
        h.states[4u].condition.set(0b0001u, 0b0001u);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        get_p(sim, cst).set_constant(3.0, 1.0);
        expect(!!sim.connect_dynamics(cst, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq((int)hsmw.exec.current_state, 4));
        expect(eq(out.event_number, 1));
        expect(eq(out.last_value, irt::real(6)));
    };

    "messages_of_a_timer_and_of_a_source_in_the_same_bag"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // At t = 1, the timer of W1 sends 1 to x[0] of W2, and a constant
        // sends to W3, which sends 2 to x[1] of W2. Both messages arrive in
        // the same bag: the completion of W2 reads port_1 = 2 and sends 5.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h1  = sim.hsms.alloc();
        auto&           h2  = sim.hsms.alloc();
        auto&           h3  = sim.hsms.alloc();
        auto&           w1  = sim.alloc<irt::hsm_wrapper>();
        auto&           w2  = sim.alloc<irt::hsm_wrapper>();
        auto&           w3  = sim.alloc<irt::hsm_wrapper>();
        auto&           cst = sim.alloc<irt::constant>();
        auto&           out = sim.alloc<irt::counter>();

        expect(!!h1.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h1.set_state(1u, 0u));
        expect(!!h1.set_state(2u, 0u));
        h1.states[1u].enter_action.set_affect(var::var_timer, 1.f);
        h1.states[1u].condition.set_timer();
        h1.states[1u].if_transition = 2u;
        h1.states[1u].if_action.set_output(var::port_0, 1.f);
        h1.states[2u].condition.set(0b0001u, 0b0001u);

        expect(!!h3.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h3.set_state(1u, 0u));
        expect(!!h3.set_state(2u, 0u));
        h3.states[1u].condition.set(0b1000u, 0b1000u);
        h3.states[1u].if_transition = 2u;
        h3.states[1u].if_action.set_output(var::port_0, 2.f);
        h3.states[2u].condition.set(0b0001u, 0b0001u);

        expect(!!h2.set_state(0u, hsm_t::invalid_state_id, 1u));
        for (const irt::u8 i : { 1u, 2u, 3u, 4u })
            expect(!!h2.set_state(i, 0u));
        h2.states[1u].condition.set(0b1000u, 0b1000u);
        h2.states[1u].if_transition = 2u;
        h2.states[2u].condition.set_greater_equal(var::port_1, 2.f);
        h2.states[2u].if_transition   = 3u;
        h2.states[2u].else_transition = 4u;
        h2.states[3u].enter_action.set_output(var::port_0, 5.f);
        h2.states[3u].condition.set(0b0001u, 0b0001u);
        h2.states[4u].enter_action.set_output(var::port_0, 6.f);
        h2.states[4u].condition.set(0b0001u, 0b0001u);

        get_p(sim, w1).set_hsm_wrapper(ordinal(sim.hsms.get_id(h1)));
        get_p(sim, w2).set_hsm_wrapper(ordinal(sim.hsms.get_id(h2)));
        get_p(sim, w3).set_hsm_wrapper(ordinal(sim.hsms.get_id(h3)));
        get_p(sim, cst).set_constant(1.0, 1.0);
        expect(!!sim.connect_dynamics(cst, 0, w3, 0));
        expect(!!sim.connect_dynamics(w1, 0, w2, 0));
        expect(!!sim.connect_dynamics(w3, 0, w2, 1));
        expect(!!sim.connect_dynamics(w2, 0, out, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq((int)w2.exec.current_state, 3));
        expect(eq(out.event_number, 1));
        expect(eq(out.last_value, irt::real(5)));
    };

    "hsm_initial_completion_leaves_in_the_first_bag"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > (A 1, B 2): the enter action of A sends 1, the completion
        // of A goes to B, whose enter action sends 2. Both messages leave in
        // the first step.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        expect(!!h.set_state(2u, 0u));
        h.states[1u].enter_action.set_output(var::port_0, 1.f);
        h.states[1u].if_transition = 2u;
        h.states[2u].enter_action.set_output(var::port_0, 2.f);
        h.states[2u].condition.set(0b1000u, 0b1000u);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        sim.limits.set_bound(0, 10.0);
        expect(!!sim.srcs.prepare() >> fatal);
        expect(!!sim.initialize() >> fatal);
        expect(eq((int)hsmw.exec.current_state, 2));
        expect(eq(hsmw.exec.messages, 2));

        expect(!!sim.run() >> fatal);
        expect(eq(out.event_number, 2));
        expect(eq(out.sum_values, irt::real(3)));
    };

    "hsm_condition_compares_the_exact_values"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        const auto inf = irt::time_domain<irt::time>::infinity;

        std::array<irt::real, hsm_t::max_constants> constants{};
        constants[0] = 2.0;

        hsm_t::execution        e;
        hsm_t::condition_action c;

        // A real and an integer of the same value are equal, in both orders.
        e.r1 = 3.0;
        e.i1 = 3;
        c.set_equal_to(var::var_r1, var::var_i1);
        expect(c.check(constants, e));
        c.set_equal_to(var::var_i1, var::var_r1);
        expect(c.check(constants, e));
        c.set_less_equal(var::var_r1, var::var_i1);
        expect(c.check(constants, e));
        c.set_equal_to(var::var_r1, 3);
        expect(c.check(constants, e));

        // The integer is not truncated to compare with a real.
        e.r1 = 3.5;
        c.set_equal_to(var::var_i1, var::var_r1);
        expect(not c.check(constants, e));
        c.set_less(var::var_i1, var::var_r1);
        expect(c.check(constants, e));

        // A constant of the machine.
        e.r1 = 2.0;
        c.set_equal_to(var::var_r1, var::hsm_constant_0);
        expect(c.check(constants, e));

        // All the bits of the mantissa are compared.
        e.r1 = 1.0000001;
        e.r2 = 1.0;
        c.set_greater(var::var_r1, var::var_r2);
        expect(c.check(constants, e));
        e.r1 = 0.25;
        e.r2 = 0.25000000001;
        c.set_less(var::var_r1, var::var_r2);
        expect(c.check(constants, e));

        // The infinities.
        e.r2 = -inf;
        c.set_less_equal(var::var_r2, 3.f);
        expect(c.check(constants, e));
        e.r2 = inf;
        c.set_greater(var::var_r2, 3.f);
        expect(c.check(constants, e));
    };

    "hsm_integer_actions_read_their_own_variable"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h = sim.hsms.alloc();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));

        const auto run = [&](const hsm_t::state_action& action) noexcept {
            hsm_t::execution e;
            e.i1                      = 12;
            e.i2                      = 10;
            h.states[1u].enter_action = action;
            expect(!!h.start(e, sim.srcs) >> fatal);
            return e;
        };

        hsm_t::state_action a;

        a.set_modulus(var::var_i2, 7);
        expect(eq(run(a).i2, 3));
        a.set_bit_and(var::var_i2, 6);
        expect(eq(run(a).i2, 2));
        a.set_bit_or(var::var_i2, 1);
        expect(eq(run(a).i2, 11));
        a.set_bit_xor(var::var_i2, 1);
        expect(eq(run(a).i2, 11));

        // As divides: a null divisor does not stop the process.
        a.set_modulus(var::var_i1, 0);
        expect(eq(run(a).i1, INT32_MAX));
        a.set_modulus(var::var_i2, 0);
        expect(eq(run(a).i2, INT32_MAX));
    };

    "hsm_nan_delay_arms_nothing"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // The default timer is infinite: infinity * 0 is NaN.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h = sim.hsms.alloc();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!h.set_state(1u, 0u));
        h.states[1u].enter_action.set_multiplies(var::var_timer, 0.f);
        h.states[1u].condition.set_timer();

        hsm_t::execution e;
        expect(!!h.start(e, sim.srcs) >> fatal);
        expect(irt::time_domain<irt::time>::is_infinity(e.timers[1]));
    };

    "hsm_output_overflow_in_a_confluent_transition"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // root 0 > (A 1, B 2, C 3): at t = 1, the timer of A sends 3 messages
        // (if, exit, enter of B) and the bag sends 2 more (if of B, enter of
        // C) in the same bag: the 4 places of the buffer are not enough.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           h    = sim.hsms.alloc();
        auto&           hsmw = sim.alloc<irt::hsm_wrapper>();
        auto&           cst  = sim.alloc<irt::constant>();
        auto&           out  = sim.alloc<irt::counter>();

        expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
        for (const irt::u8 i : { 1u, 2u, 3u })
            expect(!!h.set_state(i, 0u));

        h.states[1u].enter_action.set_affect(var::var_timer, 1.f);
        h.states[1u].condition.set_timer();
        h.states[1u].if_transition = 2u;
        h.states[1u].if_action.set_output(var::port_0, 1.f);
        h.states[1u].exit_action.set_output(var::port_0, 2.f);
        h.states[2u].enter_action.set_output(var::port_0, 3.f);
        h.states[2u].condition.set(0b1000u, 0b1000u);
        h.states[2u].if_transition = 3u;
        h.states[2u].if_action.set_output(var::port_0, 4.f);
        h.states[3u].enter_action.set_output(var::port_0, 5.f);
        h.states[3u].condition.set(0b0001u, 0b0001u);

        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(h)));
        get_p(sim, cst).set_constant(3.0, 1.0);
        expect(!!sim.connect_dynamics(cst, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(hsmw, 0, out, 0));

        sim.limits.set_bound(0, 10.0);
        expect(!!sim.srcs.prepare() >> fatal);
        expect(!!sim.initialize() >> fatal);

        const auto callback = std::exchange(irt::on_error_callback, nullptr);
        auto       ret      = irt::error_code{};
        do {
            if (auto r = sim.run(); not r) {
                ret = r.error();
                break;
            }
        } while (not sim.current_time_expired());
        irt::on_error_callback = callback;

        expect(ret == irt::error_code(
                        irt::simulation_errc::hsm_output_overflow_error));
        expect(eq(out.event_number, 0));
    };

    "hsm_output_buffer_is_emptied_before_a_confluent_transition"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        // W: root 0 > (A 1, B 2, C 3, D 4). At t = 1, the timer of A sends
        // 1, 2, 3, 4 (if and exit of A, enter of B, if of the completion of
        // B). V sends at t = 1 too: its message arrives in the bag of the step
        // where W sends its 4 messages, and C goes to D, which sends 5 and 6.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           hw  = sim.hsms.alloc();
        auto&           hv  = sim.hsms.alloc();
        auto&           w   = sim.alloc<irt::hsm_wrapper>();
        auto&           v   = sim.alloc<irt::hsm_wrapper>();
        auto&           out = sim.alloc<irt::counter>();

        expect(!!hw.set_state(0u, hsm_t::invalid_state_id, 1u));
        for (const irt::u8 i : { 1u, 2u, 3u, 4u })
            expect(!!hw.set_state(i, 0u));

        hw.states[1u].enter_action.set_affect(var::var_timer, 1.f);
        hw.states[1u].condition.set_timer();
        hw.states[1u].if_transition = 2u;
        hw.states[1u].if_action.set_output(var::port_0, 1.f);
        hw.states[1u].exit_action.set_output(var::port_0, 2.f);
        hw.states[2u].enter_action.set_output(var::port_0, 3.f);
        hw.states[2u].if_transition = 3u;
        hw.states[2u].if_action.set_output(var::port_0, 4.f);
        hw.states[3u].condition.set(0b1000u, 0b1000u);
        hw.states[3u].if_transition = 4u;
        hw.states[3u].if_action.set_output(var::port_0, 5.f);
        hw.states[4u].enter_action.set_output(var::port_0, 6.f);
        hw.states[4u].condition.set(0b0001u, 0b0001u);

        expect(!!hv.set_state(0u, hsm_t::invalid_state_id, 1u));
        expect(!!hv.set_state(1u, 0u));
        expect(!!hv.set_state(2u, 0u));
        hv.states[1u].enter_action.set_affect(var::var_timer, 1.f);
        hv.states[1u].condition.set_timer();
        hv.states[1u].if_transition = 2u;
        hv.states[1u].if_action.set_output(var::port_0, 7.f);
        hv.states[2u].condition.set(0b0001u, 0b0001u);

        get_p(sim, w).set_hsm_wrapper(ordinal(sim.hsms.get_id(hw)));
        get_p(sim, v).set_hsm_wrapper(ordinal(sim.hsms.get_id(hv)));
        expect(!!sim.connect_dynamics(v, 0, w, 0));
        expect(!!sim.connect_dynamics(w, 0, out, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq((int)w.exec.current_state, 4));
        expect(eq(out.event_number, 6));
        expect(eq(out.sum_values, irt::real(21)));
    };

    "qss_integrator_at_integer_dates"_test = [] {
        // The derivative changes at the integer dates and, with a quantum of 1
        // and slopes of 1 or 2, the integrator crosses its levels at the same
        // dates: the trajectory is checked against the exact integral.
        static constexpr irt::real slopes[] = { 1, 1, -1, 2, 0, -2, 0 };

        const auto exact = [](const irt::real t) noexcept {
            irt::real x = 0;
            for (int k = 0; k != std::ssize(slopes); ++k)
                x += slopes[k] *
                     std::clamp(t - static_cast<irt::real>(k + 1), 0.0, 1.0);
            return x;
        };

        struct emission {
            irt::real t;
            irt::real value;

            bool operator==(const emission&) const = default;
        };

        const auto run =
          [&]<typename Integrator>(irt::vector<emission>& emissions) noexcept {
              irt::simulation sim{ irt::simulation_reserve_definition(),
                                   irt::external_source_reserve_definition{
                                     .constant_nb = 1 } };
              auto&           cst = sim.srcs.constant_sources.alloc();
              cst.length          = std::size(slopes);
              std::copy_n(slopes, std::size(slopes), cst.buffer.begin());

              auto& gen   = sim.alloc<irt::generator>();
              auto& integ = sim.alloc<Integrator>();
              auto& out   = sim.alloc<irt::counter>();
              get_p(sim, gen).clear().set_generator_ta(1.0).set_generator_value(
                irt::source_type::constant,
                sim.srcs.constant_sources.get_id(cst));
              get_p(sim, integ).set_integrator(0.0, 1.0);
              expect(!!sim.connect_dynamics(gen, 0, integ, 0));
              expect(!!sim.connect_dynamics(integ, 0, out, 0));

              sim.limits.set_bound(0, 7.5);
              expect(!!sim.srcs.prepare() >> fatal);
              expect(!!sim.initialize() >> fatal);

              auto events = out.event_number;
              do {
                  expect(!!sim.run() >> fatal);
                  const auto t = sim.current_time();
                  if (sim.current_time_expired())
                      break;

                  // QSS2 and QSS3 cross some levels at dates which are not
                  // dyadic (t = 13/3): their values are rounded.
                  if (get_model(integ).tl == t)
                      expect(std::abs(integ.X - exact(t)) < 1e-12)
                        << "at t =" << t;

                  if (out.event_number != events) {
                      events = out.event_number;
                      emissions.emplace_back(t, out.last_value);
                  }
              } while (true);
          };

        irt::vector<emission> qss1, qss2, qss3;
        run.template          operator()<irt::qss1_integrator>(qss1);
        run.template          operator()<irt::qss2_integrator>(qss2);
        run.template          operator()<irt::qss3_integrator>(qss3);

        static constexpr emission expected[] = { { 0, 0 },   { 2, 1 },
                                                 { 3, 2 },   { 4, 1 },
                                                 { 4.5, 2 }, { 5, 3 },
                                                 { 6.5, 2 }, { 7, 1 } };
        // std::ranges::equal on a local type crashes gcc 11.
        expect(eq(qss1.ssize(), std::ssize(expected)) >> fatal);
        for (int i = 0; i != std::ssize(expected); ++i)
            expect(qss1[i] == expected[i]) << "emission" << i;
        for (const auto& e : expected)
            expect(eq(e.value, exact(e.t)));
    };

    "have_true_message"_test = [] {
        const irt::message msgs[] = { { 0.0 }, { 2.0 }, { 0.0 } };

        expect(not irt::have_true_message({}));
        expect(not irt::have_true_message(std::span(msgs).first(1)));
        expect(irt::have_true_message(msgs));
    };

    "logical_and_accumulator_do_not_depend_on_the_bag_order"_test = [] {
        // At t = 1, two constants send a, then b, on the same port: the
        // transition must not depend on the order of the connections.
        struct result {
            irt::real or_value;
            irt::real invert_value;
            irt::real accumulated;

            bool operator==(const result&) const = default;
        };

        const auto run = [](const bool swap) noexcept -> result {
            irt::simulation sim{ irt::simulation_reserve_definition(),
                                 irt::external_source_reserve_definition() };
            auto&           a     = sim.alloc<irt::constant>();
            auto&           b     = sim.alloc<irt::constant>();
            auto&           one   = sim.alloc<irt::constant>();
            auto&           lor   = sim.alloc<irt::logical_or_2>();
            auto&           inv   = sim.alloc<irt::logical_invert>();
            auto&           acc   = sim.alloc<irt::accumulator_2>();
            auto&           c_or  = sim.alloc<irt::counter>();
            auto&           c_inv = sim.alloc<irt::counter>();

            get_p(sim, a).set_constant(0.0, 1.0);
            get_p(sim, b).set_constant(5.0, 1.0);
            get_p(sim, one).set_constant(1.0, 1.0);

            auto& first  = swap ? b : a;
            auto& second = swap ? a : b;
            for (auto* src : { &first, &second }) {
                expect(!!sim.connect_dynamics(*src, 0, lor, 0));
                expect(!!sim.connect_dynamics(*src, 0, inv, 0));
                expect(!!sim.connect_dynamics(*src, 0, acc, 2));
            }
            expect(!!sim.connect_dynamics(one, 0, acc, 0));
            expect(!!sim.connect_dynamics(lor, 0, c_or, 0));
            expect(!!sim.connect_dynamics(inv, 0, c_inv, 0));

            expect(run_until(sim, 10.0) >> fatal);

            return { c_or.last_value, c_inv.last_value, acc.number };
        };

        const auto direct  = run(false);
        const auto swapped = run(true);
        expect(direct == swapped);
        expect(eq(direct.or_value, irt::real(1)));
        expect(eq(direct.invert_value, irt::real(0)));
        expect(eq(direct.accumulated, irt::real(5)));
    };

    "generator_time_advance_port"_test = [] {
        // At t = 0.5, the generator (ta = 1) receives 3 on x_t: the next
        // values leave at 3.5, 4.5, ..., 9.5.
        irt::simulation sim{ irt::simulation_reserve_definition(),
                             irt::external_source_reserve_definition() };
        auto&           cst = sim.alloc<irt::constant>();
        auto&           gen = sim.alloc<irt::generator>();
        auto&           out = sim.alloc<irt::counter>();

        get_p(sim, cst).set_constant(3.0, 0.5);
        get_p(sim, gen).clear().set_generator_ta(1.0);
        expect(!!sim.connect_dynamics(cst, 0, gen, irt::generator::x_t));
        expect(!!sim.connect_dynamics(gen, 0, out, 0));

        expect(run_until(sim, 10.0) >> fatal);
        expect(eq(out.event_number, 7));
    };

    "bag_order_is_the_total_order_of_ieee_754"_test = [] {
        const auto inf = std::numeric_limits<irt::real>::infinity();
        const auto nan = std::numeric_limits<irt::real>::quiet_NaN();

        std::array<irt::message, 8> bag{
            irt::message{ 2.0 },      irt::message{ nan },
            irt::message{ +0.0 },     irt::message{ -inf },
            irt::message{ -0.0 },     irt::message{ -1.0 },
            irt::message{ 1.0, 3.0 }, irt::message{ 1.0, -3.0 }
        };

        irt::detail::sort_bag(bag);

        expect(eq(bag[0][0], -inf));
        expect(eq(bag[1][0], irt::real(-1)));
        expect(std::signbit(bag[2][0]) and bag[2][0] == 0.0);
        expect(not std::signbit(bag[3][0]) and bag[3][0] == 0.0);
        expect(eq(bag[4][1], irt::real(-3)));
        expect(eq(bag[5][1], irt::real(3)));
        expect(eq(bag[6][0], irt::real(2)));
        expect(std::isnan(bag[7][0]));
    };

    "bag_is_sorted_by_values"_test = [] {
        // At t = 1, two constants send 5 and 2 to a queue: the first message
        // to leave is 2, whatever the order of the connections.
        const auto first_output = [](const bool swap) noexcept -> irt::real {
            irt::simulation sim{ irt::simulation_reserve_definition(),
                                 irt::external_source_reserve_definition() };
            auto&           a   = sim.alloc<irt::constant>();
            auto&           b   = sim.alloc<irt::constant>();
            auto&           q   = sim.alloc<irt::queue>();
            auto&           out = sim.alloc<irt::counter>();

            get_p(sim, a).set_constant(5.0, 1.0);
            get_p(sim, b).set_constant(2.0, 1.0);
            expect(!!sim.connect_dynamics(swap ? b : a, 0, q, 0));
            expect(!!sim.connect_dynamics(swap ? a : b, 0, q, 0));
            expect(!!sim.connect_dynamics(q, 0, out, 0));

            sim.limits.set_bound(0, 10.0);
            expect(!!sim.srcs.prepare() >> fatal);
            expect(!!sim.initialize() >> fatal);

            while (out.event_number == 0 and not sim.current_time_expired())
                expect(!!sim.run() >> fatal);

            return out.last_value;
        };

        expect(eq(first_output(false), irt::real(2)));
        expect(eq(first_output(true), irt::real(2)));
    };

    "hsm_hierarchy_devs_invalid_machine"_test = [] {
        hsm_bench b;
        b.machine.states[1u].sub_id = hsm_bench::hsm_t::invalid_state_id;

        const auto callback = std::exchange(irt::on_error_callback, nullptr);
        expect(not run_until(b.sim, 1.0));
        irt::on_error_callback = callback;
    };

    "queue_sends_one_message_by_event"_test = [] {
        // Two generators send 10 and 20 at t = 1, 2, ...: the queue receives
        // two messages at the same date and must send the two values.
        irt::simulation sim(
          irt::simulation_reserve_definition(),
          irt::external_source_reserve_definition{ .constant_nb = 2 });

        const irt::real v10[] = { 10.0 };
        const irt::real v20[] = { 20.0 };
        auto&           cst10 = sim.srcs.constant_sources.alloc(v10);
        auto&           cst20 = sim.srcs.constant_sources.alloc(v20);

        auto& gen1 = sim.alloc<irt::generator>();
        auto& gen2 = sim.alloc<irt::generator>();
        auto& q    = sim.alloc<irt::queue>();
        auto& cnt  = sim.alloc<irt::counter>();

        get_p(sim, gen1).clear().set_generator_ta(1.0).set_generator_value(
          irt::source_type::constant, sim.srcs.constant_sources.get_id(cst10));
        get_p(sim, gen2).clear().set_generator_ta(1.0).set_generator_value(
          irt::source_type::constant, sim.srcs.constant_sources.get_id(cst20));
        get_p(sim, q).set_queue(2.0);

        expect(!!sim.connect_dynamics(gen1, 0, q, 0));
        expect(!!sim.connect_dynamics(gen2, 0, q, 0));
        expect(!!sim.connect_dynamics(q, 0, cnt, 0));

        // The messages of t = 1, 2, 3 leave at t = 3, 4, 5.
        sim.limits.set_bound(0, 5.5);
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        do {
            expect(!!sim.run());
        } while (not sim.current_time_expired());

        expect(eq(cnt.event_number, static_cast<irt::i64>(6)));
        expect(eq(cnt.sum_values, 90.0));
    };

    "hsm_simulation"_test = [] {
        irt::simulation sim(
          irt::simulation_reserve_definition(),
          irt::external_source_reserve_definition{ .constant_nb = 2 });

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);
        expect(sim.srcs.constant_sources.can_alloc(2u) >> fatal);
        auto& cst_value  = sim.srcs.constant_sources.alloc();
        cst_value.length = 10;
        cst_value.buffer = { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0 };

        auto& cst_ta  = sim.srcs.constant_sources.alloc();
        cst_ta.length = 10;
        cst_ta.buffer = { 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1. };

        auto& cst_1 = sim.alloc<irt::constant>();
        get_p(sim, cst_1).set_constant(1, 0);

        auto& cnt = sim.alloc<irt::counter>();

        auto& gen = sim.alloc<irt::generator>();
        get_p(sim, gen)
          .clear()
          .set_generator_ta(irt::source_type::constant,
                            sim.srcs.constant_sources.get_id(cst_ta))
          .set_generator_value(irt::source_type::constant,
                               sim.srcs.constant_sources.get_id(cst_value));

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        expect(eq(sim.hsms.size(), 0u));
        auto& hsm = sim.hsms.alloc();
        expect(eq(sim.hsms.size(), 1u));

        expect(!!hsm.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsm.set_state(1u, 0u));
        hsm.states[1u].condition.set(0b1100u, 0b1100u);
        hsm.states[1u].if_transition = 2u;

        expect(!!hsm.set_state(2u, 0u));
        hsm.states[2u].enter_action.set_output(
          irt::hierarchical_state_machine::variable::port_0, 1.0f);

        auto& hsmw = sim.alloc<irt::hsm_wrapper>();
        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(hsm)));

        expect(!!sim.connect_dynamics(gen, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(gen, 0, hsmw, 1));
        expect(!!sim.connect_dynamics(hsmw, 0, cnt, 0));

        sim.limits.set_bound(0, 10);

        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;

        do {
            st = sim.run();
            expect(!!st);
        } while (not sim.current_time_expired());

        expect(eq(cnt.event_number, static_cast<irt::i64>(1)));
    };

    "hsm_enter_exit_simulation"_test = [] {
        irt::simulation sim(
          irt::simulation_reserve_definition(),
          irt::external_source_reserve_definition{ .constant_nb = 2 });

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);
        expect(sim.srcs.constant_sources.can_alloc(2u) >> fatal);
        auto& cst_value  = sim.srcs.constant_sources.alloc();
        cst_value.length = 10;
        cst_value.buffer = { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0 };

        auto& cst_ta  = sim.srcs.constant_sources.alloc();
        cst_ta.length = 10;
        cst_ta.buffer = { 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1. };

        auto& cst_1                = sim.alloc<irt::constant>();
        get_p(sim, cst_1).reals[0] = 1.0;

        auto& cnt = sim.alloc<irt::counter>();

        auto& gen = sim.alloc<irt::generator>();

        get_p(sim, gen)
          .clear()
          .set_generator_value(irt::source_type::constant,
                               sim.srcs.constant_sources.get_id(cst_value))
          .set_generator_ta(irt::source_type::constant,
                            sim.srcs.constant_sources.get_id(cst_ta));

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        auto& hsm = sim.hsms.alloc();

        expect(!!hsm.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsm.set_state(1u, 0u));
        hsm.states[1u].enter_action.set_affect(
          irt::hierarchical_state_machine::variable::var_i1, 1.0f);
        hsm.states[1u].exit_action.set_plus(
          irt::hierarchical_state_machine::variable::var_i1, 10.0f);

        hsm.states[1u].condition.set(0b1100u, 0b1100u);
        hsm.states[1u].if_transition = 2u;

        expect(!!hsm.set_state(2u, 0u));
        hsm.states[2u].enter_action.set_output(
          irt::hierarchical_state_machine::variable::port_0, 1.0f);

        auto& hsmw = sim.alloc<irt::hsm_wrapper>();
        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(hsm)));

        expect(!!sim.connect_dynamics(gen, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(gen, 0, hsmw, 1));
        expect(!!sim.connect_dynamics(hsmw, 0, cnt, 0));

        sim.limits.set_bound(0, 10);
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;

        do {
            st = sim.run();
            expect(!!st);
        } while (not sim.current_time_expired());

        expect(eq(hsmw.exec.i1, 11));

        expect(eq(cnt.event_number, static_cast<irt::i64>(1)));
    };

    "hsm_timer_simulation"_test = [] {
        irt::simulation sim;

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);

        auto& cnt = sim.alloc<irt::counter>();
        auto& gen = sim.alloc<irt::constant>();

        get_p(sim, gen).reals[0]    = 1.0;
        get_p(sim, gen).reals[1]    = 5.0;
        get_p(sim, gen).integers[0] = ordinal(
          irt::constant::init_type::constant);

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        auto& hsm = sim.hsms.alloc();

        expect(!!hsm.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsm.set_state(1u, 0u));
        hsm.states[1u].condition.set(0b1100u, 0b1100u);
        hsm.states[1u].if_transition = 2u;

        expect(!!hsm.set_state(2u, 0u));
        hsm.states[2u].enter_action.set_affect(
          irt::hierarchical_state_machine::variable::var_timer, 10.f);
        hsm.states[2u].condition.set_timer();
        hsm.states[2u].if_transition = 3u;

        expect(!!hsm.set_state(3u, 0u));
        hsm.states[3u].enter_action.set_output(
          irt::hierarchical_state_machine::variable::port_0, 1.f);

        auto& hsmw = sim.alloc<irt::hsm_wrapper>();
        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(hsm)));

        expect(!!sim.connect_dynamics(gen, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(gen, 0, hsmw, 1));
        expect(!!sim.connect_dynamics(hsmw, 0, cnt, 0));

        sim.limits.set_bound(0, 20);
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;

        do {
            st = sim.run();
            expect(!!st);
        } while (not sim.current_time_expired());

        expect(eq(cnt.event_number, static_cast<irt::i64>(1)));
    };

    "hsm_timer_stop_and_restart_simulation"_test = [] {
        irt::simulation sim;

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);

        auto& cnt                    = sim.alloc<irt::counter>();
        auto& gen1                   = sim.alloc<irt::constant>();
        get_p(sim, gen1).reals[0]    = 1.0;
        get_p(sim, gen1).reals[1]    = 5.0;
        get_p(sim, gen1).integers[0] = ordinal(
          irt::constant::init_type::constant);
        auto& gen2                   = sim.alloc<irt::constant>();
        get_p(sim, gen2).reals[0]    = 1.0;
        get_p(sim, gen2).reals[1]    = 12.0;
        get_p(sim, gen2).integers[0] = ordinal(
          irt::constant::init_type::constant);

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        auto& hsm = sim.hsms.alloc();

        expect(!!hsm.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsm.set_state(1u, 0u));
        hsm.states[1u].condition.set(0b1100u, 0b1100u);
        hsm.states[1u].if_transition = 2u;

        expect(!!hsm.set_state(2u, 0u));
        hsm.states[2u].enter_action.set_affect(
          irt::hierarchical_state_machine::variable::var_timer, 4.f);
        hsm.states[2u].condition.set_timer();
        hsm.states[2u].if_transition = 3u;

        expect(!!hsm.set_state(3u, 0u));
        hsm.states[3u].enter_action.set_output(
          irt::hierarchical_state_machine::variable::port_0, 1.f);

        auto& hsmw = sim.alloc<irt::hsm_wrapper>();
        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(hsm)));

        expect(!!sim.connect_dynamics(gen1, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(gen2, 0, hsmw, 1));
        expect(!!sim.connect_dynamics(hsmw, 0, cnt, 0));

        sim.limits.set_bound(0, 20);
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;

        do {
            st = sim.run();
            expect(!!st);
        } while (not sim.current_time_expired());

        expect(eq(cnt.event_number, static_cast<irt::i64>(1)));
    };

    "hsm_timer_stop_simulation"_test = [] {
        irt::simulation sim;

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);

        auto& cnt                    = sim.alloc<irt::counter>();
        auto& gen1                   = sim.alloc<irt::constant>();
        get_p(sim, gen1).reals[0]    = 1.0;
        get_p(sim, gen1).reals[1]    = 5.0;
        get_p(sim, gen1).integers[0] = ordinal(
          irt::constant::init_type::constant);
        auto& gen2                   = sim.alloc<irt::constant>();
        get_p(sim, gen2).reals[0]    = 1.0;
        get_p(sim, gen2).reals[1]    = 12.0;
        get_p(sim, gen2).integers[0] = ordinal(
          irt::constant::init_type::constant);

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        auto& hsm = sim.hsms.alloc();

        expect(!!hsm.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsm.set_state(1u, 0u));
        hsm.states[1u].condition.set(0b0011u, 0b0011u);
        hsm.states[1u].if_transition = 2u;

        expect(!!hsm.set_state(2u, 0u));
        hsm.states[2u].enter_action.set_affect(
          irt::hierarchical_state_machine::variable::var_timer, 10.f);
        hsm.states[2u].condition.set_timer();
        hsm.states[2u].if_transition = 3u;
        hsm.states[2u].if_transition = 4u;

        expect(!!hsm.set_state(3u, 0u));
        hsm.states[3u].enter_action.set_output(
          irt::hierarchical_state_machine::variable::port_0, 1.0f);

        expect(!!hsm.set_state(4u, 0u));

        auto& hsmw = sim.alloc<irt::hsm_wrapper>();
        get_p(sim, hsmw).set_hsm_wrapper(ordinal(sim.hsms.get_id(hsm)));

        expect(!!sim.connect_dynamics(gen1, 0, hsmw, 0));
        expect(!!sim.connect_dynamics(gen2, 0, hsmw, 1));
        expect(!!sim.connect_dynamics(hsmw, 0, cnt, 0));

        sim.limits.set_bound(0, 20);
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;

        do {
            st = sim.run();
            expect(!!st);
        } while (not sim.current_time_expired());

        expect(eq(cnt.event_number, static_cast<irt::i64>(0)));
    };

    "hsm_constant_access"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t h;
        h.constants = { 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0 };

        hsm_t::execution e;
        e.clear();
        e.messages = 0;

        hsm_t::state_action a;
        a.set_output(var::port_0, var::hsm_constant_2);

        h.affect_action(a, e);

        const double got = e.messages > 0 ? e.message_values[0] : -999.0;

        expect(eq(got, 12.0));
    };

    "hsm_condition_type_mask"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;

        hsm_t::condition_action c;
        c.set(0b1000, 0b1111);

        const auto [p, m] = c.get_bitset();

        expect(eq(m.to_ulong(), 0b1111u));
        expect(eq(p.to_ulong(), 0b1000u));
    };

    "hsm_set_unset_values"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        hsm_t            h;
        hsm_t::execution e;
        e.clear();

        hsm_t::state_action a;
        a.set_setport(var::port_0); // must remove bit 3
        h.affect_action(a, e);

        expect(eq(e.values.to_ulong(), 0b1000u));

        e.values.reset();
        e.values.set(3, true);
        hsm_t::state_action u;
        u.set_unsetport(var::port_0);
        h.affect_action(u, e);

        expect(eq(e.values.to_ulong(), 0u));
    };

    "hsm_two_input_selector"_test = [] {
        using hsm_t = irt::hierarchical_state_machine;
        using var   = hsm_t::variable;

        auto run_variant = [](irt::i32 prefer_fine_flag, double coarse_value,
                              double fine_value) {
            hsm_t                h;
            hsm_t::execution     exec;
            irt::external_source srcs;

            expect(!!h.set_state(0u, hsm_t::invalid_state_id, 1u));
            h.states[0u].enter_action.set_affect(var::var_i1, prefer_fine_flag);

            expect(!!h.set_state(1u, 0u));
            h.states[1u].condition.set(0b1100u, 0b1100u);
            h.states[1u].if_transition = 2u;

            expect(!!h.set_state(2u, 0u));
            h.states[2u].enter_action.set_affect(var::var_r1, var::port_0);
            h.states[2u].exit_action.set_affect(var::var_r2, var::port_1);
            h.states[2u].if_transition = 3u;

            expect(!!h.set_state(3u, 0u));
            h.states[3u].condition.set_equal_to(var::var_i1, 1);
            h.states[3u].if_transition   = 4u;
            h.states[3u].else_transition = 5u;

            expect(!!h.set_state(4u, 0u));
            h.states[4u].enter_action.set_output(var::port_0, var::var_r2);
            h.states[4u].if_transition = 6u;

            expect(!!h.set_state(5u, 0u));
            h.states[5u].enter_action.set_output(var::port_0, var::var_r1);
            h.states[5u].if_transition = 6u;

            expect(!!h.set_state(6u, 0u));

            expect(!!h.start(exec, srcs));

            exec.values = 0b00001100;

            exec.ports[3] = coarse_value; // port_0
            exec.ports[2] = fine_value;   // port_1

            const auto p1 = h.dispatch(hsm_t::event_type::internal, exec, srcs);
            expect(!!p1);

            const auto p2 = h.dispatch(hsm_t::event_type::internal, exec, srcs);
            expect(!!p2);

            const auto p3 = h.dispatch(hsm_t::event_type::internal, exec, srcs);
            expect(!!p3);
        };

        run_variant(0, 6.0, 11.0); // prefer coarse -> expect emitted value 6.0
        run_variant(1, 6.0, 11.0); // prefer fine   -> expect emitted value 11.0
    };

    "generator_counter_simluation"_test = [] {
        fmt::print("generator_counter_simluation\n");
        irt::simulation sim(
          irt::simulation_reserve_definition(),
          irt::external_source_reserve_definition{ .constant_nb = 2 });

        expect(sim.can_alloc(2));

        expect(sim.srcs.constant_sources.can_alloc(2u));
        auto& cst_value  = sim.srcs.constant_sources.alloc();
        cst_value.buffer = { 1., 2., 3., 4., 5., 6., 7., 8., 9., 10. };
        cst_value.length = 10;

        auto& cst_ta  = sim.srcs.constant_sources.alloc();
        cst_ta.buffer = { 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1. };
        cst_ta.length = 10;

        auto& gen = sim.alloc<irt::generator>();
        auto& cnt = sim.alloc<irt::counter>();

        get_p(sim, gen)
          .clear()
          .set_generator_value(irt::source_type::constant,
                               sim.srcs.constant_sources.get_id(cst_value))
          .set_generator_ta(irt::source_type::constant,
                            sim.srcs.constant_sources.get_id(cst_ta));

        expect(!!sim.connect_dynamics(gen, 0, cnt, 0));

        sim.limits.set_bound(0, 10);
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;

        do {
            st = sim.run();
            expect(!!st);
        } while (not sim.current_time_expired());

        expect(eq(cnt.event_number, static_cast<irt::i64>(9)));
    };

    "boolean_simulation"_test = [] {
        irt::simulation sim(
          irt::simulation_reserve_definition(),
          irt::external_source_reserve_definition{ .constant_nb = 2 });

        expect(sim.srcs.constant_sources.can_alloc(2u));
        auto& cst_value  = sim.srcs.constant_sources.alloc();
        cst_value.buffer = { 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0 };
        cst_value.length = 10;

        auto& cst_ta  = sim.srcs.constant_sources.alloc();
        cst_ta.buffer = { 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1. };
        cst_ta.length = 10;

        auto& gen   = sim.alloc<irt::generator>();
        auto& l_and = sim.alloc<irt::logical_and_2>();
        auto& l_or  = sim.alloc<irt::logical_or_2>();

        get_p(sim, gen)
          .clear()
          .set_generator_value(irt::source_type::constant,
                               sim.srcs.constant_sources.get_id(cst_value))
          .set_generator_ta(irt::source_type::constant,
                            sim.srcs.constant_sources.get_id(cst_ta));

        expect(!!sim.connect_dynamics(gen, 0, l_and, 0));
        expect(!!sim.connect_dynamics(l_and, 0, l_or, 0));

        get_p(sim, l_and).integers[0] = false;
        get_p(sim, l_and).integers[1] = true;

        get_p(sim, l_or).integers[0] = false;
        get_p(sim, l_or).integers[1] = false;

        expect(sim.observe(irt::get_model(l_and)).has_value());
        auto* obs = sim.observers.try_to_get(irt::get_model(l_and).obs_id);

        expect(fatal(obs != nullptr));
        file_output fo_a(sim, irt::get_model(l_and).obs_id,
                         "boolean_simulation.csv");

        sim.limits.set_bound(0, 10);
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());
        do {
            expect(!!sim.run());

            if (not sim.immediate_observers.empty())
                sim.tick_resamplers();

        } while (not sim.current_time_expired());
    };

    "time_func"_test = [] {
        fmt::print("time_func\n");
        irt::simulation sim;

        constexpr irt::real timestep{ 0.1 };

        expect(sim.can_alloc(2));

        auto& time_fun = sim.alloc<irt::time_func>();
        auto& cnt      = sim.alloc<irt::counter>();

        get_p(sim, time_fun).set_time_func(timestep, timestep, 1);

        expect(!!sim.connect_dynamics(time_fun, 0, cnt, 0));

        irt::real c = 0;
        sim.limits.set_bound(0, 30);
        expect(!!sim.initialize());
        do {
            expect(!!sim.run());
            if (not sim.current_time_expired())
                expect(
                  eq(time_fun.value, sim.current_time() * sim.current_time()));
            c++;
        } while (not sim.current_time_expired());

        // One step per timestep: the counter receives the value in the step
        // of its emission.
        expect(eq(c, sim.limits.duration() / timestep));
    };

    "time_func_sin"_test = [] {
        fmt::print("time_func_sin\n");

        constexpr irt::real pi       = 3.141592653589793238462643383279502884;
        constexpr irt::real f0       = irt::real(0.1);
        constexpr irt::real timestep = 0.1;

        irt::simulation sim;

        expect(sim.can_alloc(2));

        auto& time_fun = sim.alloc<irt::time_func>();
        auto& cnt      = sim.alloc<irt::counter>();

        get_p(sim, time_fun).set_time_func(timestep, timestep, 0);

        expect(!!sim.connect_dynamics(time_fun, 0, cnt, 0));

        sim.limits.set_bound(0, 30);
        irt::real c = irt::zero;

        expect(!!sim.initialize());
        do {
            expect(!!sim.run() >> fatal);
            if (not sim.current_time_expired())
                expect(eq(time_fun.value,
                          std::sin(irt::two * pi * f0 * sim.current_time())));
            c++;
        } while (not sim.current_time_expired());

        expect(eq(c, sim.limits.duration() / timestep));
    };

    "abstract_sin"_test = [] {
        irt::simulation           sim;
        irt::abstract_sin<1>&     d1  = sim.alloc<irt::abstract_sin<1>>();
        irt::abstract_sin<2>&     d2  = sim.alloc<irt::abstract_sin<2>>();
        irt::abstract_sin<3>&     d3  = sim.alloc<irt::abstract_sin<3>>();
        irt::abstract_wsum<3, 3>& sum = sim.alloc<irt::abstract_wsum<3, 3>>();

        expect(sim.connect(d1.y[0], sim.models.get_id(irt::get_model(sum)), 0)
                 .has_value());
        expect(sim.connect(d2.y[0], sim.models.get_id(irt::get_model(sum)), 1)
                 .has_value());
        expect(sim.connect(d3.y[0], sim.models.get_id(irt::get_model(sum)), 2)
                 .has_value());

        d1.value[0] = std::numbers::pi / 4.0;
        d2.value[0] = std::numbers::pi / 4.0;
        d2.value[1] = 0.5;
        d3.value[0] = std::numbers::pi / 4.0;
        d3.value[1] = 0.5;
        d3.value[2] = 0.0;

        // sim.emitting_output_ports.clear();
        // d1.lambda(sim);
        // d2.lambda(sim);
        // d3.lambda(sim);
        // expect(eq(sim.emitting_output_ports.ssize(), 3));

        // expect(
        //   irt::almost_equal(sim.emitting_output_ports[0].msg[0],
        //   .707, 1.e-2));

        // expect(
        //   irt::almost_equal(sim.emitting_output_ports[1].msg[0],
        //   .707, 1.e-2));
        // expect(
        //   irt::almost_equal(sim.emitting_output_ports[1].msg[1],
        //   .354, 1.e-2));

        // expect(
        //   irt::almost_equal(sim.emitting_output_ports[2].msg[0],
        //   .707, 1.e-2));
        // expect(
        //   irt::almost_equal(sim.emitting_output_ports[2].msg[1],
        //   .354, 1.e-2));
        // expect(
        //   irt::almost_equal(sim.emitting_output_ports[2].msg[2],
        //   -.176, 1.e-2));
    };

    "id_data_array"_test = [] {
        enum class my_id : irt::u32;

        using my_data = irt::id_data_array<
          void, my_id, irt::allocator<irt::new_delete_memory_resource>, int,
          double>;

        my_data m1;
        expect(fatal(m1.grow<2, 1>(5)));
        const auto id1 = m1.alloc_id();
        const auto id2 = m1.alloc_id();
        const auto id3 = m1.alloc_id();
        const auto id4 = m1.alloc_id();
        expect(fatal(eq(static_cast<unsigned>(m1.size()), 4_u)));
        m1.get<int>(id1)    = 1;
        m1.get<int>(id2)    = 2;
        m1.get<int>(id3)    = 3;
        m1.get<int>(id4)    = 4;
        m1.get<double>(id1) = 1.0;
        m1.get<double>(id2) = 2.0;
        m1.get<double>(id3) = 3.0;
        m1.get<double>(id4) = 4.0;

        my_data m2(m1);
        expect(fatal(eq(static_cast<unsigned>(m1.size()), 4_u)));
        m1.get<int>(id1)    = 1;
        m1.get<int>(id2)    = 2;
        m1.get<int>(id3)    = 3;
        m1.get<int>(id4)    = 4;
        m1.get<double>(id1) = 1.0;
        m1.get<double>(id2) = 2.0;
        m1.get<double>(id3) = 3.0;
        m1.get<double>(id4) = 4.0;
        m2.get<int>(id1)    = 1;
        m2.get<int>(id2)    = 2;
        m2.get<int>(id3)    = 3;
        m2.get<int>(id4)    = 4;
        m2.get<double>(id1) = 1.0;
        m2.get<double>(id2) = 2.0;
        m2.get<double>(id3) = 3.0;
        m2.get<double>(id4) = 4.0;

        my_data m3(std::move(m2));
        expect(fatal(eq(static_cast<unsigned>(m1.size()), 4_u)));
        expect(fatal(eq(static_cast<unsigned>(m2.size()), 0_u)));
        expect(fatal(eq(static_cast<unsigned>(m3.size()), 4_u)));
        m1.get<int>(id1)    = 1;
        m1.get<int>(id2)    = 2;
        m1.get<int>(id3)    = 3;
        m1.get<int>(id4)    = 4;
        m1.get<double>(id1) = 1.0;
        m1.get<double>(id2) = 2.0;
        m1.get<double>(id3) = 3.0;
        m1.get<double>(id4) = 4.0;
        m3.get<int>(id1)    = 1;
        m3.get<int>(id2)    = 2;
        m3.get<int>(id3)    = 3;
        m3.get<int>(id4)    = 4;
        m3.get<double>(id1) = 1.0;
        m3.get<double>(id2) = 2.0;
        m3.get<double>(id3) = 3.0;
        m3.get<double>(id4) = 4.0;
    };

    "project-copy-move"_test = [] {
        irt::project pj;
        expect(fatal(pj.parameters.grow<2, 1>(2)));
        const auto pj_id_1 = pj.parameters.alloc_id();
        const auto pj_id_2 = pj.parameters.alloc_id();

        irt::project copy_pj(pj);
        expect(eq(copy_pj.parameters.size(), pj.parameters.size()));
        expect(copy_pj.parameters.exists(pj_id_1));
        expect(copy_pj.parameters.exists(pj_id_2));

        irt::project move_copy_pj(std::move(copy_pj));
        expect(copy_pj.parameters.empty());
        expect(eq(move_copy_pj.parameters.size(), pj.parameters.size()));
        expect(move_copy_pj.parameters.exists(pj_id_1));
        expect(move_copy_pj.parameters.exists(pj_id_2));

        irt::project op_copy_pj;

        op_copy_pj = pj;
        expect(eq(op_copy_pj.parameters.size(), pj.parameters.size()));
        expect(op_copy_pj.parameters.exists(pj_id_1));
        expect(op_copy_pj.parameters.exists(pj_id_2));

        irt::project op_move_copy_pj;
        op_move_copy_pj = std::move(op_copy_pj);
        expect(op_copy_pj.parameters.empty());
        expect(eq(op_move_copy_pj.parameters.size(), pj.parameters.size()));
        expect(op_move_copy_pj.parameters.exists(pj_id_1));
        expect(op_move_copy_pj.parameters.exists(pj_id_2));
    };
}
