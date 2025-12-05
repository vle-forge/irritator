// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/ext.hpp>
#include <irritator/file.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>
#include <irritator/random.hpp>

#include <fmt/format.h>

#include <numbers>
#include <random>
#include <sstream>

#include <cstdio>

#include <boost/ut.hpp>

struct file_output {
    using value_type = irt::observation;

    std::FILE*     os = nullptr;
    irt::observer& obs;

    file_output(irt::observer& obs_, const char* filename) noexcept
      : os{ std::fopen(filename, "w") }
      , obs{ obs_ }
    {
        if (os)
            fmt::print(os, "t,v\n");
    }

    void push_back(const irt::observation& vec) const noexcept
    {
        if (os)
            fmt::print(os, "{},{}\n", vec.x, vec.y);
    }

    void write() noexcept
    {
        if (obs.states[irt::observer_flags::buffer_full]) {
            irt::write_interpolate_data(obs, 0.1, [&](auto t, auto v) noexcept {
                fmt::print(os, "{},{}\n", t, v);
            });
        }
    }

    void flush() noexcept
    {
        irt::flush_interpolate_data(obs, 0.1, [&](auto t, auto v) noexcept {
            fmt::print(os, "{},{}\n", t, v);
        });

        std::fflush(os);
    }

    ~file_output() noexcept
    {
        if (os)
            std::fclose(os);
    }
};

bool function_ref_called = false;

void function_ref_f() { function_ref_called = true; }

struct function_ref_class {
    bool baz_called = false;

    void baz() { baz_called = true; }

    bool qux_called = false;

    void qux() { qux_called = true; }
};

struct function_ref_multiple_operator {
    int i;

    void operator()(bool) { i = 1; }

    void operator()(double) { i++; }
};

struct global_allocator_t {
    size_t allocation_size   = 0;
    int    allocation_number = 0;
} global_allocator;

struct global_deallocator_t {
    int free_number = 0;
} global_deallocator;

void* global_alloc(size_t size) noexcept
{
    global_allocator.allocation_size += size;
    global_allocator.allocation_number++;

    fmt::print("global_alloc {} (global size: {}, number: {})\n",
               size,
               global_allocator.allocation_size,
               global_allocator.allocation_number);

    return std::malloc(size);
}

void global_free(void* ptr) noexcept
{
    if (ptr) {
        global_deallocator.free_number++;
        fmt::print(
          "global_free {} (number: {})\n", ptr, global_deallocator.free_number);
        std::free(ptr);
    }
}

struct struct_with_static_member {
    static int i;
    static int j;

    static void clear() noexcept
    {
        i = 0;
        j = 0;
    }

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
    fmt::print(
      "make_output_node_id({},{})\n", static_cast<irt::u64>(mdl), port);
    boost::ut::expect(port >= 0 && port < 8);

    auto index = irt::get_index(mdl);
    boost::ut::expect(index < 268435456u);

    fmt::print("{0:32b} <- index\n", index);
    fmt::print("{0:32b} <- port\n", port);
    fmt::print("{0:32b} <- port + 8u\n", 8u + static_cast<unsigned>(port));

    irt::u32 port_index =
      static_cast<irt::u32>(8u + static_cast<unsigned>(port)) << 28u;
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

template<typename Data>
static bool check_data_array_loop(const Data& d) noexcept
{
    using value_type = typename Data::value_type;

    irt::small_vector<const value_type*, 16> test_vec;

    if (test_vec.capacity() < d.ssize())
        return false;

    const value_type* ptr = nullptr;
    while (d.next(ptr))
        test_vec.emplace_back(ptr);

    int i = 0;
    for (const auto& elem : d) {
        if (test_vec[i] != &elem)
            return false;

        ++i;
    }

    return true;
}

template<typename Dynamics>
inline auto get_p(irt::simulation& sim, const Dynamics& d) noexcept
  -> irt::parameter&
{
    return sim.parameters[sim.get_id(d)];
}

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

    "sizeof"_test = [] {
        fmt::print("qss1_integrator   {}\n", sizeof(irt::qss1_integrator));
        fmt::print("qss1_multiplier   {}\n", sizeof(irt::qss1_multiplier));
        fmt::print("qss1_cross        {}\n", sizeof(irt::qss1_cross));
        fmt::print("qss1_power        {}\n", sizeof(irt::qss1_power));
        fmt::print("qss1_square       {}\n", sizeof(irt::qss1_square));
        fmt::print("qss1_sum_2        {}\n", sizeof(irt::qss1_sum_2));
        fmt::print("qss1_sum_3        {}\n", sizeof(irt::qss1_sum_3));
        fmt::print("qss1_sum_4        {}\n", sizeof(irt::qss1_sum_4));
        fmt::print("qss1_wsum_2       {}\n", sizeof(irt::qss1_wsum_2));
        fmt::print("qss1_wsum_3       {}\n", sizeof(irt::qss1_wsum_3));
        fmt::print("qss1_wsum_4       {}\n", sizeof(irt::qss1_wsum_4));
        fmt::print("qss1_invert       {}\n", sizeof(irt::qss1_inverse));
        fmt::print("qss1_integer      {}\n", sizeof(irt::qss1_integer));
        fmt::print("qss1_compare      {}\n", sizeof(irt::qss1_compare));
        fmt::print("qss1_sin          {}\n", sizeof(irt::qss1_sin));
        fmt::print("qss1_cos          {}\n", sizeof(irt::qss1_cos));
        fmt::print("qss1_log          {}\n", sizeof(irt::qss1_log));
        fmt::print("qss1_exp          {}\n", sizeof(irt::qss1_exp));
        fmt::print("qss2_integrator   {}\n", sizeof(irt::qss2_integrator));
        fmt::print("qss2_multiplier   {}\n", sizeof(irt::qss2_multiplier));
        fmt::print("qss2_cross        {}\n", sizeof(irt::qss2_cross));
        fmt::print("qss2_power        {}\n", sizeof(irt::qss2_power));
        fmt::print("qss2_square       {}\n", sizeof(irt::qss2_square));
        fmt::print("qss2_sum_2        {}\n", sizeof(irt::qss2_sum_2));
        fmt::print("qss2_sum_3        {}\n", sizeof(irt::qss2_sum_3));
        fmt::print("qss2_sum_4        {}\n", sizeof(irt::qss2_sum_4));
        fmt::print("qss2_wsum_2       {}\n", sizeof(irt::qss2_wsum_2));
        fmt::print("qss2_wsum_3       {}\n", sizeof(irt::qss2_wsum_3));
        fmt::print("qss2_wsum_4       {}\n", sizeof(irt::qss2_wsum_4));
        fmt::print("qss2_invert       {}\n", sizeof(irt::qss2_inverse));
        fmt::print("qss2_integer      {}\n", sizeof(irt::qss2_integer));
        fmt::print("qss2_compare      {}\n", sizeof(irt::qss2_compare));
        fmt::print("qss2_sin          {}\n", sizeof(irt::qss2_sin));
        fmt::print("qss2_cos          {}\n", sizeof(irt::qss2_cos));
        fmt::print("qss2_log          {}\n", sizeof(irt::qss2_log));
        fmt::print("qss2_exp          {}\n", sizeof(irt::qss2_exp));
        fmt::print("qss3_integrator   {}\n", sizeof(irt::qss3_integrator));
        fmt::print("qss3_multiplier   {}\n", sizeof(irt::qss3_multiplier));
        fmt::print("qss3_power        {}\n", sizeof(irt::qss3_power));
        fmt::print("qss3_square       {}\n", sizeof(irt::qss3_square));
        fmt::print("qss3_cross        {}\n", sizeof(irt::qss3_cross));
        fmt::print("qss3_sum_2        {}\n", sizeof(irt::qss3_sum_2));
        fmt::print("qss3_sum_3        {}\n", sizeof(irt::qss3_sum_3));
        fmt::print("qss3_sum_4        {}\n", sizeof(irt::qss3_sum_4));
        fmt::print("qss3_wsum_2       {}\n", sizeof(irt::qss3_wsum_2));
        fmt::print("qss3_wsum_3       {}\n", sizeof(irt::qss3_wsum_3));
        fmt::print("qss3_wsum_4       {}\n", sizeof(irt::qss3_wsum_4));
        fmt::print("qss3_invert       {}\n", sizeof(irt::qss3_inverse));
        fmt::print("qss3_integer      {}\n", sizeof(irt::qss3_integer));
        fmt::print("qss3_compare      {}\n", sizeof(irt::qss3_compare));
        fmt::print("qss3_sin          {}\n", sizeof(irt::qss3_sin));
        fmt::print("qss3_cos          {}\n", sizeof(irt::qss3_cos));
        fmt::print("qss3_log          {}\n", sizeof(irt::qss3_log));
        fmt::print("qss3_exp          {}\n", sizeof(irt::qss3_exp));
        fmt::print("counter           {}\n", sizeof(irt::counter));
        fmt::print("queue             {}\n", sizeof(irt::queue));
        fmt::print("dynamic_queue     {}\n", sizeof(irt::dynamic_queue));
        fmt::print("priority_queue    {}\n", sizeof(irt::priority_queue));
        fmt::print("generator         {}\n", sizeof(irt::generator));
        fmt::print("constant          {}\n", sizeof(irt::constant));
        fmt::print("time_func         {}\n", sizeof(irt::time_func));
        fmt::print("accumulator       {}\n", sizeof(irt::accumulator_2));
        fmt::print("hsm_wrapper       {}\n", sizeof(irt::hsm_wrapper));
        fmt::print("--------------------\n");
        fmt::print("dynamic number:   {}\n", irt::dynamics_type_size());
        fmt::print("max dynamic size: {}\n", irt::max_size_in_bytes());
        fmt::print("--------------------\n");
        fmt::print("model             {}\n", sizeof(irt::model));
        fmt::print("message           {}\n", sizeof(irt::message));
        fmt::print("observer          {}\n", sizeof(irt::observer));
        fmt::print("node              {}\n", sizeof(irt::node));
        fmt::print("parameter         {}\n", sizeof(irt::parameter));
        fmt::print("model size:       {}\n", sizeof(irt::model));
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

    "observation_message"_test = [] {
        using namespace irt::literals;

        {
            irt::observation_message vdouble({ 0 });
            expect(vdouble[0] == 0.0_r);
            expect(vdouble[1] == 0.0_r);
            expect(vdouble[2] == 0.0_r);
            expect(vdouble[3] == 0.0_r);
        }

        {
            irt::observation_message vdouble({ 1.0_r });
            expect(vdouble[0] == 1.0_r);
            expect(vdouble[1] == 0.0_r);
            expect(vdouble[2] == 0.0_r);
            expect(vdouble[3] == 0.0_r);
        }

        {
            irt::observation_message vdouble({ 0.0_r, 1.0_r });
            expect(vdouble[0] == 0.0_r);
            expect(vdouble[1] == 1.0_r);
            expect(vdouble[2] == 0.0_r);
            expect(vdouble[3] == 0.0_r);
        }

        {
            irt::observation_message vdouble({ 0.0_r, 0.0_r, 1.0_r });
            expect(vdouble[0] == 0.0_r);
            expect(vdouble[1] == 0.0_r);
            expect(vdouble[2] == 1.0_r);
            expect(vdouble[3] == 0.0_r);
        }

        {
            irt::observation_message vdouble({ 0.0_r, 0.0_r, 0.0_r, 1.0_r });
            expect(vdouble[0] == 0.0_r);
            expect(vdouble[1] == 0.0_r);
            expect(vdouble[2] == 0.0_r);
            expect(vdouble[3] == 1.0_r);
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

        expect(eq(cnt.number, static_cast<irt::i64>(2)));
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

        expect(eq(cnt.number, static_cast<decltype(cnt.number)>(1)));
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
          irt::hierarchical_state_machine::event_type::input_changed,
          exec,
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

        hsmw.states[1u].condition.type =
          irt::hierarchical_state_machine::condition_type::port;
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
          irt::hierarchical_state_machine::event_type::input_changed,
          exec,
          srcs);
        expect(!!processed);
        expect(processed.value() == true);
        expect((int)exec.current_state == 2);

        expect(eq(exec.messages, 1));
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

        expect(eq(cnt.number, static_cast<irt::i64>(1)));
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

        expect(eq(cnt.number, static_cast<irt::i64>(1)));
    };

    "hsm_timer_simulation"_test = [] {
        irt::simulation sim;

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);

        auto& cnt = sim.alloc<irt::counter>();
        auto& gen = sim.alloc<irt::constant>();

        get_p(sim, gen).reals[0] = 1.0;
        get_p(sim, gen).reals[1] = 5.0;
        get_p(sim, gen).integers[0] =
          ordinal(irt::constant::init_type::constant);

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

        expect(eq(cnt.number, static_cast<irt::i64>(1)));
    };

    "hsm_timer_stop_and_restart_simulation"_test = [] {
        irt::simulation sim;

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);

        auto& cnt                 = sim.alloc<irt::counter>();
        auto& gen1                = sim.alloc<irt::constant>();
        get_p(sim, gen1).reals[0] = 1.0;
        get_p(sim, gen1).reals[1] = 5.0;
        get_p(sim, gen1).integers[0] =
          ordinal(irt::constant::init_type::constant);
        auto& gen2                = sim.alloc<irt::constant>();
        get_p(sim, gen2).reals[0] = 1.0;
        get_p(sim, gen2).reals[1] = 12.0;
        get_p(sim, gen2).integers[0] =
          ordinal(irt::constant::init_type::constant);

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

        expect(eq(cnt.number, static_cast<irt::i64>(1)));
    };

    "hsm_timer_stop_simulation"_test = [] {
        irt::simulation sim;

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);

        auto& cnt                 = sim.alloc<irt::counter>();
        auto& gen1                = sim.alloc<irt::constant>();
        get_p(sim, gen1).reals[0] = 1.0;
        get_p(sim, gen1).reals[1] = 5.0;
        get_p(sim, gen1).integers[0] =
          ordinal(irt::constant::init_type::constant);
        auto& gen2                = sim.alloc<irt::constant>();
        get_p(sim, gen2).reals[0] = 1.0;
        get_p(sim, gen2).reals[1] = 12.0;
        get_p(sim, gen2).integers[0] =
          ordinal(irt::constant::init_type::constant);

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

        expect(eq(cnt.number, static_cast<irt::i64>(0)));
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

        expect(eq(cnt.number, static_cast<irt::i64>(9)));
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

        auto&       obs = sim.observers.alloc();
        file_output fo_a(obs, "boolean_simulation.csv");
        sim.observe(irt::get_model(l_and), obs);

        sim.limits.set_bound(0, 10);
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());
        do {
            expect(!!sim.run());
            fo_a.write();
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

        const auto value =
          (irt::real{ 2.0 } * sim.limits.duration() / timestep -
           irt::real{ 1.0 });
        expect(eq(c, value));
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

        expect(eq(c,
                  (irt::real{ 2.0 } * sim.limits.duration() / timestep -
                   irt::real{ 1.0 })));
    };

    "abstract_sin"_test = [] {
        irt::simulation           sim;
        irt::abstract_sin<1>&     d1  = sim.alloc<irt::abstract_sin<1>>();
        irt::abstract_sin<2>&     d2  = sim.alloc<irt::abstract_sin<2>>();
        irt::abstract_sin<3>&     d3  = sim.alloc<irt::abstract_sin<3>>();
        irt::abstract_wsum<3, 3>& sum = sim.alloc<irt::abstract_wsum<3, 3>>();

        sim.connect(d1.y[0], sim.models.get_id(irt::get_model(sum)), 0);
        sim.connect(d2.y[0], sim.models.get_id(irt::get_model(sum)), 1);
        sim.connect(d3.y[0], sim.models.get_id(irt::get_model(sum)), 2);

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

    "external_source"_test = [] {
        std::error_code   ec;
        std::stringstream ofs_b;
        std::stringstream ofs_t;

        std::default_random_engine gen(1234);
        std::poisson_distribution  dist(4.0);

        irt::generate_random_file(
          ofs_b, gen, dist, 1024, irt::random_file_type::binary);

        auto str_b = ofs_b.str();
        expect(str_b.size() == static_cast<size_t>(1024) * 8);

        irt::generate_random_file(
          ofs_t, gen, dist, 1024, irt::random_file_type::text);

        auto str_t = ofs_b.str();
        expect(str_t.size() > static_cast<size_t>(1024) * 2);
    };

    "binary-memory-io"_test = [] {
        auto f = irt::memory::make(256, irt::open_mode::write);

        expect(f.has_value()) << fatal;
        expect(eq(f->data.ssize(), 256));
        expect(eq(f->data.capacity(), 256));
        expect(eq(f->tell(), 0));
        expect(eq(f->length(), 256));

        irt::u8  a = 0xfe;
        irt::u16 b = 0xfedc;
        irt::u32 c = 0xfedcba98;
        irt::u64 d = 0xfedcba9876543210;

        expect(!!f->write(a));
        expect(!!f->write(b));
        expect(!!f->write(c));
        expect(!!f->write(d));

        expect(eq(f->data.ssize(), 256));
        expect(eq(f->data.capacity(), 256));
        expect(eq(f->tell(), 8 + 4 + 2 + 1));
        expect(eq(f->length(), 256));

        f->rewind();

        expect(f->tell() == 0);
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
}
