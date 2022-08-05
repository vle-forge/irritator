// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/examples.hpp>
#include <irritator/ext.hpp>
#include <irritator/external_source.hpp>
#include <irritator/file.hpp>
#include <irritator/io.hpp>

#include <fmt/format.h>

#include <filesystem>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>

#include <cstdio>

#include <boost/ut.hpp>

struct file_output
{
    std::FILE*  os = nullptr;
    std::string filename;

    file_output(const std::string_view name)
      : filename(name)
    {
        os = std::fopen(filename.c_str(), "w");
    }

    ~file_output()
    {
        if (os)
            std::fclose(os);
    }
};

void file_output_callback(const irt::observer& obs,
                          const irt::dynamics_type /*type*/,
                          const irt::time /*tl*/,
                          const irt::time             t,
                          const irt::observer::status s) noexcept
{
    auto* fo = reinterpret_cast<file_output*>(obs.user_data);

    switch (s) {
    case irt::observer::status::initialize:
        fmt::print(fo->os, "t,{}\n", obs.name.c_str());
        break;

    case irt::observer::status::run:
        fmt::print(fo->os, "{},{}\n", t, obs.msg.data[0]);
        break;

    case irt::observer::status::finalize:
        fmt::print(fo->os, "{},{}\n", t, obs.msg.data[0]);
        break;
    }
}

bool function_ref_called = false;

void function_ref_f() { function_ref_called = true; }

struct function_ref_class
{
    bool baz_called = false;

    void baz() { baz_called = true; }

    bool qux_called = false;

    void qux() { qux_called = true; }
};

struct function_ref_multiple_operator
{
    int i;

    void operator()(bool) { i = 1; }

    void operator()(double) { i++; }
};

static void empty_fun(irt::model_id /*id*/) noexcept {}

static irt::status run_simulation(irt::simulation& sim, const double duration_p)
{
    using namespace boost::ut;

    irt::time t        = 0;
    irt::time duration = static_cast<irt::time>(duration_p);

    expect(sim.initialize(t) == irt::status::success);

    do {
        auto status = sim.run(t);
        expect(status == irt::status::success);
    } while (t < duration);

    return irt::status::success;
}

struct global_alloc
{
    size_t allocation_size   = 0;
    int    allocation_number = 0;

    void* operator()(size_t size)
    {
        allocation_size += size;
        allocation_number++;

        fmt::print("global_alloc {} (global size: {}, number: {})\n",
                   size,
                   allocation_size,
                   allocation_number);

        return std::malloc(size);
    }
};

struct global_free
{
    int free_number = 0;

    void operator()(void* ptr)
    {
        if (ptr) {
            free_number++;
            fmt::print("global_free {} (number: {})\n", ptr, free_number);
            std::free(ptr);
        }
    }
};

static void* null_alloc(size_t /*sz*/) { return nullptr; }

static void null_free(void*) {}

struct struct_with_static_member
{
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
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = irt::get_index(mdl);
    irt_assert(index < 268435456u);

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
    irt_assert(port >= 0 && port < 8);

    irt::u32 index = irt::get_index(mdl);
    irt_assert(index < 268435456u);

    fmt::print("{0:32b} <- index\n", index);
    fmt::print("{0:32b} <- port\n", port);
    fmt::print("{0:32b} <- port + 8u\n", 8u + port);

    irt::u32 port_index = static_cast<irt::u32>(8u + port) << 28u;
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
    irt_assert(port < 8u);

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

    irt_assert(port >= 8u && port < 16u);
    port -= 8u;
    fmt::print("{0:32b} <- port - 8u\n", port);
    irt_assert(port < 8u);

    constexpr irt::u32 mask = ~(15u << 28u);
    fmt::print("{0:32b} <- mask\n", mask);

    irt::u32 index = real_node_id & mask;
    fmt::print("{0:32b} <- real_node_id & mask\n", index);

    fmt::print("index: {} port: {}\n", index, port);
    return std::make_pair(index, port);
}

int main()
{
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
        fmt::print("qss1_integrator {}\n", sizeof(irt::qss1_integrator));
        fmt::print("qss1_multiplier {}\n", sizeof(irt::qss1_multiplier));
        fmt::print("qss1_cross {}\n", sizeof(irt::qss1_cross));
        fmt::print("qss1_power {}\n", sizeof(irt::qss1_power));
        fmt::print("qss1_square {}\n", sizeof(irt::qss1_square));
        fmt::print("qss1_sum_2 {}\n", sizeof(irt::qss1_sum_2));
        fmt::print("qss1_sum_3 {}\n", sizeof(irt::qss1_sum_3));
        fmt::print("qss1_sum_4 {}\n", sizeof(irt::qss1_sum_4));
        fmt::print("qss1_wsum_2 {}\n", sizeof(irt::qss1_wsum_2));
        fmt::print("qss1_wsum_3 {}\n", sizeof(irt::qss1_wsum_3));
        fmt::print("qss1_wsum_4 {}\n", sizeof(irt::qss1_wsum_4));
        fmt::print("qss2_integrator {}\n", sizeof(irt::qss2_integrator));
        fmt::print("qss2_multiplier {}\n", sizeof(irt::qss2_multiplier));
        fmt::print("qss2_cross {}\n", sizeof(irt::qss2_cross));
        fmt::print("qss2_power {}\n", sizeof(irt::qss2_power));
        fmt::print("qss2_square {}\n", sizeof(irt::qss2_square));
        fmt::print("qss2_sum_2 {}\n", sizeof(irt::qss2_sum_2));
        fmt::print("qss2_sum_3 {}\n", sizeof(irt::qss2_sum_3));
        fmt::print("qss2_sum_4 {}\n", sizeof(irt::qss2_sum_4));
        fmt::print("qss2_wsum_2 {}\n", sizeof(irt::qss2_wsum_2));
        fmt::print("qss2_wsum_3 {}\n", sizeof(irt::qss2_wsum_3));
        fmt::print("qss2_wsum_4 {}\n", sizeof(irt::qss2_wsum_4));
        fmt::print("qss3_integrator {}\n", sizeof(irt::qss3_integrator));
        fmt::print("qss3_multiplier {}\n", sizeof(irt::qss3_multiplier));
        fmt::print("qss3_power {}\n", sizeof(irt::qss3_power));
        fmt::print("qss3_square {}\n", sizeof(irt::qss3_square));
        fmt::print("qss3_cross {}\n", sizeof(irt::qss3_cross));
        fmt::print("qss3_sum_2 {}\n", sizeof(irt::qss3_sum_2));
        fmt::print("qss3_sum_3 {}\n", sizeof(irt::qss3_sum_3));
        fmt::print("qss3_sum_4 {}\n", sizeof(irt::qss3_sum_4));
        fmt::print("qss3_wsum_2 {}\n", sizeof(irt::qss3_wsum_2));
        fmt::print("qss3_wsum_3 {}\n", sizeof(irt::qss3_wsum_3));
        fmt::print("qss3_wsum_4 {}\n", sizeof(irt::qss3_wsum_4));
        fmt::print("integrator {}\n", sizeof(irt::integrator));
        fmt::print("quantifier {}\n", sizeof(irt::quantifier));
        fmt::print("adder_2 {}\n", sizeof(irt::adder_2));
        fmt::print("adder_3 {}\n", sizeof(irt::adder_3));
        fmt::print("adder_4 {}\n", sizeof(irt::adder_4));
        fmt::print("mult_2 {}\n", sizeof(irt::mult_2));
        fmt::print("mult_3 {}\n", sizeof(irt::mult_3));
        fmt::print("mult_4 {}\n", sizeof(irt::mult_4));
        fmt::print("counter {}\n", sizeof(irt::counter));
        fmt::print("queue {}\n", sizeof(irt::queue));
        fmt::print("dynamic_queue {}\n", sizeof(irt::dynamic_queue));
        fmt::print("priority_queue {}\n", sizeof(irt::priority_queue));
        fmt::print("generator {}\n", sizeof(irt::generator));
        fmt::print("constant {}\n", sizeof(irt::constant));
        fmt::print("cross {}\n", sizeof(irt::cross));
        fmt::print("time_func {}\n", sizeof(irt::time_func));
        fmt::print("accumulator {}\n", sizeof(irt::accumulator_2));
        fmt::print("hsm_wrapper {}\n", sizeof(irt::hsm_wrapper));
        fmt::print("model {}\n", sizeof(irt::model));
        fmt::print("input_port {}\n", sizeof(irt::input_port));
        fmt::print("output_port {}\n", sizeof(irt::output_port));
        fmt::print("dynamic number: {}\n", irt::dynamics_type_size());
        fmt::print("max dynamic size: {}\n", irt::max_size_in_bytes());
        fmt::print("model size: {}\n", sizeof(irt::model));
    };

    "model_constepxr"_test = [] {
        expect(irt::is_detected_v<irt::initialize_function_t, irt::counter> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::counter> ==
               false);
        expect(irt::is_detected_v<irt::transition_function_t, irt::counter> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::counter> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::counter> ==
               false);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::generator> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::generator> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::generator> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::generator> ==
               false);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::generator> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::adder_2> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::adder_2> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::adder_2> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::adder_2> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::adder_2> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::adder_3> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::adder_3> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::adder_3> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::adder_3> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::adder_3> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::adder_4> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::adder_4> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::adder_4> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::adder_4> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::adder_4> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::mult_2> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::mult_2> == true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::mult_2> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::mult_2> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::mult_2> == true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::mult_3> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::mult_3> == true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::mult_3> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::mult_3> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::mult_3> == true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::mult_4> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::mult_4> == true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::mult_4> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::mult_4> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::mult_4> == true);

        expect(
          irt::is_detected_v<irt::initialize_function_t, irt::integrator> ==
          true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::integrator> ==
               true);
        expect(
          irt::is_detected_v<irt::transition_function_t, irt::integrator> ==
          true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::integrator> ==
               true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::integrator> ==
               true);

        expect(
          irt::is_detected_v<irt::initialize_function_t, irt::quantifier> ==
          true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::quantifier> ==
               true);
        expect(
          irt::is_detected_v<irt::transition_function_t, irt::quantifier> ==
          true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::quantifier> ==
               true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::quantifier> ==
               true);
    };

    "status"_test = [] {
        irt::status s1 = irt::status::success;
        expect(irt::is_success(s1) == true);
        expect(irt::is_bad(s1) == false);

        irt::status s2 = irt::status::block_allocator_not_enough_memory;
        expect(irt::is_success(s2) == false);
        expect(irt::is_bad(s2) == true);
    };

    "function_ref"_test = [] {
        {
            irt::function_ref<void(void)> fr = function_ref_f;
            fr();
            expect(function_ref_called == true);
        }

        {
            function_ref_class o;
            auto               x = &function_ref_class::baz;
            irt::function_ref<void(function_ref_class&)> fr = x;
            fr(o);
            expect(o.baz_called);
            x  = &function_ref_class::qux;
            fr = x;
            fr(o);
            expect(o.qux_called);
        }

        {
            auto                     x  = [] { return 42; };
            irt::function_ref<int()> fr = x;
            expect(fr() == 42);
        }

        {
            int                       i  = 0;
            auto                      x  = [&i] { i = 42; };
            irt::function_ref<void()> fr = x;
            fr();
            expect(i == 42);
        }

        {
            function_ref_multiple_operator ops;
            ops.i = 0;
            irt::function_ref<void(bool)>   b1(ops);
            irt::function_ref<void(double)> b2(ops);

            b1(true);
            b2(0.0);

            expect(ops.i == 2);
        }
    };

    "time"_test = [] {
        expect(irt::time_domain<irt::time>::infinity >
               irt::time_domain<irt::time>::zero);
        expect(irt::time_domain<irt::time>::zero >
               irt::time_domain<irt::time>::negative_infinity);
    };

    "small-vector<T>"_test = [] {
        irt::small_vector<int, 8> v;
        expect(v.empty());
        expect(v.capacity() == 8);
        v.emplace_back(0);
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        v.emplace_back(4);
        v.emplace_back(5);
        v.emplace_back(6);
        v.emplace_back(7);
        expect(v.size() == 8);
        expect(v.full());
        expect(!v.empty());
        expect(v[0] == 0);
        expect(v[1] == 1);
        expect(v[2] == 2);
        expect(v[3] == 3);
        expect(v[4] == 4);
        expect(v[5] == 5);
        expect(v[6] == 6);
        expect(v[7] == 7);
        v.swap_pop_back(0);
        expect(v.size() == 7);
        expect(!v.full());
        expect(!v.empty());
        expect(v[0] == 7);
        expect(v[1] == 1);
        expect(v[2] == 2);
        expect(v[3] == 3);
        expect(v[4] == 4);
        expect(v[5] == 5);
        expect(v[6] == 6);
        v.swap_pop_back(6);
        expect(v.size() == 6);
        expect(!v.full());
        expect(!v.empty());
        expect(v[0] == 7);
        expect(v[1] == 1);
        expect(v[2] == 2);
        expect(v[3] == 3);
        expect(v[4] == 4);
        expect(v[5] == 5);

        irt::small_vector<int, 8> v2;
        v2 = v;
        v2[0] *= 2;
        expect(v2[0] == 14);
        expect(v2[1] == 1);
        expect(v2[2] == 2);
        expect(v2[3] == 3);
        expect(v2[4] == 4);
        expect(v2[5] == 5);
    };

    "vector<T>"_test = [] {
        irt::vector<int> v(8);
        expect(v.empty());
        expect(v.capacity() == 8);
        v.emplace_back(0);
        v.emplace_back(1);
        v.emplace_back(2);
        v.emplace_back(3);
        v.emplace_back(4);
        v.emplace_back(5);
        v.emplace_back(6);
        v.emplace_back(7);
        expect(v.size() == 8);
        expect(v.full());
        expect(!v.empty());
        expect(v[0] == 0);
        expect(v[1] == 1);
        expect(v[2] == 2);
        expect(v[3] == 3);
        expect(v[4] == 4);
        expect(v[5] == 5);
        expect(v[6] == 6);
        expect(v[7] == 7);
        v.swap_pop_back(0);
        expect(v.size() == 7);
        expect(!v.full());
        expect(!v.empty());
        expect(v[0] == 7);
        expect(v[1] == 1);
        expect(v[2] == 2);
        expect(v[3] == 3);
        expect(v[4] == 4);
        expect(v[5] == 5);
        expect(v[6] == 6);
        v.swap_pop_back(6);
        expect(v.size() == 6);
        expect(!v.full());
        expect(!v.empty());
        expect(v[0] == 7);
        expect(v[1] == 1);
        expect(v[2] == 2);
        expect(v[3] == 3);
        expect(v[4] == 4);
        expect(v[5] == 5);

        irt::vector<int> v2(8);
        v2 = v;
        v2[0] *= 2;
        expect(v2[0] == 14);
        expect(v2[1] == 1);
        expect(v2[2] == 2);
        expect(v2[3] == 3);
        expect(v2[4] == 4);
        expect(v2[5] == 5);
    };

    "vector-erase"_test = [] {
        struct t_1
        {
            int x = 0;

            t_1() noexcept = default;

            t_1(int x_) noexcept
              : x(x_)
            {}
        };

        irt::vector<t_1> v_1(10, 10);
        std::iota(v_1.begin(), v_1.end(), 0);

        assert(v_1[0].x == 0);
        assert(v_1[9].x == 9);
        v_1.erase(v_1.begin());
        assert(v_1[0].x == 1);
        assert(v_1[8].x == 9);
        assert(v_1.ssize() == 9);
        v_1.erase(v_1.begin(), v_1.begin() + 5);
        assert(v_1[0].x == 6);
        assert(v_1[3].x == 9);
        assert(v_1.ssize() == 4);
    };

    "vector-static-member"_test = [] {
        struct_with_static_member::clear();

        irt::vector<struct_with_static_member> v;
        v.reserve(4);

        expect(v.ssize() == 0);
        expect(v.capacity() >= 4);

        v.emplace_back();
        expect(struct_with_static_member::i == 1);
        expect(struct_with_static_member::j == 0);

        v.emplace_back();
        v.emplace_back();
        v.emplace_back();
        expect(struct_with_static_member::i == 4);
        expect(struct_with_static_member::j == 0);

        v.pop_back();
        expect(struct_with_static_member::i == 4);
        expect(struct_with_static_member::j == 1);

        v.swap_pop_back(2);
        expect(struct_with_static_member::i == 4);
        expect(struct_with_static_member::j == 2);

        v.swap_pop_back(0);
        expect(struct_with_static_member::i == 4);
        expect(struct_with_static_member::j == 3);

        expect(std::ssize(v) == 1);
    };

    "small-vector-no-trivial"_test = [] {
        struct toto
        {
            int i;

            toto(int i_) noexcept
              : i(i_)
            {}

            ~toto() noexcept { i = 0; }
        };

        irt::small_vector<toto, 4> v;
        v.emplace_back(10);
        expect(v.data()[0].i == 10);

        irt::small_vector<toto, 4> v2 = v;
        v2.emplace_back(100);

        expect(v.data()[0].i == 10);
        expect(v2.data()[0].i == 10);
        expect(v2.data()[1].i == 100);
    };

    "small-vector-static-member"_test = [] {
        struct_with_static_member::clear();

        irt::small_vector<struct_with_static_member, 4> v;
        v.emplace_back();
        expect(struct_with_static_member::i == 1);
        expect(struct_with_static_member::j == 0);

        v.emplace_back();
        v.emplace_back();
        v.emplace_back();
        expect(struct_with_static_member::i == 4);
        expect(struct_with_static_member::j == 0);

        v.pop_back();
        expect(struct_with_static_member::i == 4);
        expect(struct_with_static_member::j == 1);

        v.swap_pop_back(2);
        expect(struct_with_static_member::i == 4);
        expect(struct_with_static_member::j == 2);

        v.swap_pop_back(0);
        expect(struct_with_static_member::i == 4);
        expect(struct_with_static_member::j == 3);

        expect(std::ssize(v) == 1);
    };

    "small_string"_test = [] {
        irt::small_string<8> f1;
        expect(f1.capacity() == 8);
        expect(f1 == "");
        expect(f1.ssize() == 0);

        f1 = "ok";
        expect(f1 == "ok");
        expect(f1.ssize() == 2);

        f1 = "okok";
        expect(f1 == "okok");
        expect(f1.ssize() == 4);

        f1 = "okok123456";
        expect(f1 == "okok123");
        expect(f1.ssize() == 7);

        irt::small_string<8> f2(f1);
        expect(f2 == "okok123");
        expect(f2.ssize() == 7);

        expect(f1.c_str() != f2.c_str());

        irt::small_string<8> f3("012345678");
        expect(f3 == "0123456");
        expect(f3.ssize() == 7);

        f3.clear();
        expect(f3 == "");
        expect(f3.ssize() == 0);

        f3 = f2;
        expect(f3 == "okok123");
        expect(f3.ssize() == 7);

        irt::small_string<8> f4;
        std::string_view     t0 = "012345678";
        std::string_view     t1 = "okok123";

        f4 = t0;
        expect(f4 == "0123456");
        expect(f4.ssize() == 7);

        f4 = t1;
        expect(f4 == "okok123");
        expect(f4.ssize() == 7);
    };

    "list"_test = [] {
        irt::block_allocator<irt::list_view_node<int>> allocator;
        expect(is_success(allocator.init(32)));

        irt::u64       id = static_cast<irt::u64>(-1);
        irt::list_view lst(allocator, id);

        lst.emplace_front(5);
        lst.emplace_front(4);
        lst.emplace_front(3);
        lst.emplace_front(2);
        lst.emplace_front(1);

        {
            int i = 1;
            for (auto it = lst.begin(); it != lst.end(); ++it)
                expect(*it == i++);
        }

        lst.pop_front();

        {
            int i = 2;
            for (auto it = lst.begin(); it != lst.end(); ++it)
                expect(*it == i++);
        }
    };

    "double_list"_test = [] {
        irt::block_allocator<irt::list_view_node<int>> allocator;
        expect(is_success(allocator.init(32)));

        irt::u64       id = static_cast<irt::u64>(-1);
        irt::list_view lst(allocator, id);

        expect(lst.empty());
        expect(lst.begin() == lst.end());

        lst.emplace_front(0);
        expect(lst.begin() == --lst.end());
        expect(++lst.begin() == lst.end());

        lst.clear();
        expect(lst.empty());
        expect(lst.begin() == lst.end());

        lst.emplace_front(5);
        lst.emplace_front(4);
        lst.emplace_front(3);
        lst.emplace_front(2);
        lst.emplace_front(1);
        lst.emplace_back(6);
        lst.emplace_back(7);
        lst.emplace_back(8);

        {
            int i = 1;
            for (auto it = lst.begin(); it != lst.end(); ++it)
                expect(*it == i++);
        }

        lst.pop_front();

        {
            int i = 2;
            for (auto it = lst.begin(); it != lst.end(); ++it)
                expect(*it == i++);
        }

        {
            auto it = lst.begin();
            expect(*it == 2);

            --it;
            expect(it == lst.end());

            --it;
            expect(it == --lst.end());
        }

        {
            auto it = lst.end();
            expect(it == lst.end());

            --it;
            expect(*it == 8);

            --it;
            expect(*it == 7);
        }

        lst.emplace(lst.begin(), 10);
        expect(*lst.begin() == 10);

        {
            auto it = lst.begin();
            ++it;

            it = lst.emplace(it, 11);
            expect(*it == 11);
            expect(*lst.begin() == 10);
        }
    };

    "vector"_test = [] {
        struct position
        {
            position() noexcept = default;

            position(float x_, float y_) noexcept
              : x(x_)
              , y(y_)
            {}

            float x = 0, y = 0;
        };

        irt::vector<position> pos(4, 4);
        pos[0].x = 0;
        pos[1].x = 1;
        pos[2].x = 2;
        pos[3].x = 3;

        pos.emplace_back(4.f, 0.f);
        expect((pos.size() == 5) >> fatal);
        expect((pos.capacity() == 4 + 4 / 2));
    };

    "table"_test = [] {
        struct position
        {
            position() noexcept = default;
            position(float x_) noexcept
              : x(x_)
            {}

            float x = 0;
        };

        irt::table<int, position> tbl;
        tbl.data.reserve(10);

        tbl.data.emplace_back(4, 4.f);
        tbl.data.emplace_back(3, 3.f);
        tbl.data.emplace_back(2, 2.f);
        tbl.data.emplace_back(1, 1.f);
        tbl.sort();
        expect(tbl.data.size() == 4);
        expect(tbl.data.capacity() == 10);
        tbl.set(0, 0.f);

        expect(tbl.data.size() == 5);
        expect(tbl.data.capacity() == 10);
        expect(tbl.data[0].id == 0);
        expect(tbl.data[1].id == 1);
        expect(tbl.data[2].id == 2);
        expect(tbl.data[3].id == 3);
        expect(tbl.data[4].id == 4);
        expect(tbl.data[0].value.x == 0.f);
        expect(tbl.data[1].value.x == 1.f);
        expect(tbl.data[2].value.x == 2.f);
        expect(tbl.data[3].value.x == 3.f);
        expect(tbl.data[4].value.x == 4.f);
    };

    "ring-buffer"_test = [] {
        int                   buffer[10];
        irt::ring_buffer<int> ring{ buffer, irt::length(buffer) };

        for (int i = 0; i < 9; ++i) {
            auto is_success = ring.emplace_enqueue(i);
            expect(is_success == true);
        }

        {
            auto is_success = ring.emplace_enqueue(9);
            expect(is_success == false);
        }

        expect(buffer[0] == 0);
        expect(buffer[1] == 1);
        expect(buffer[2] == 2);
        expect(buffer[3] == 3);
        expect(buffer[4] == 4);
        expect(buffer[5] == 5);
        expect(buffer[6] == 6);
        expect(buffer[7] == 7);
        expect(buffer[8] == 8);
        expect(buffer[0] == 0);

        for (int i = 10; i < 15; ++i)
            ring.force_emplace_enqueue(i);

        expect(buffer[0] == 11);
        expect(buffer[1] == 12);
        expect(buffer[2] == 13);
        expect(buffer[3] == 14);
        expect(buffer[4] == 4);
        expect(buffer[5] == 5);
        expect(buffer[6] == 6);
        expect(buffer[7] == 7);
        expect(buffer[8] == 8);
        expect(buffer[9] == 10);
    };

    "ring-buffer-front-back-access"_test = [] {
        int                   buffer[4];
        irt::ring_buffer<int> ring(buffer, irt::length(buffer));

        expect(ring.push_front(0) == true);
        expect(ring.push_front(-1) == true);
        expect(ring.push_front(-2) == true);
        expect(ring.push_front(-3) == false);
        expect(ring.push_front(-4) == false);

        ring.pop_back();

        expect(ring.ssize() == 2);
        expect(ring.front() == -2);
        expect(ring.back() == -1);

        expect(ring.push_back(1) == true);

        expect(ring.front() == -2);
        expect(ring.back() == 1);
    };

    "data_array_api"_test = [] {
        struct position
        {
            position() = default;
            constexpr position(float x_)
              : x(x_)
            {}

            float x;
        };

        enum class position_id : std::uint64_t
        {
        };

        irt::data_array<position, position_id> array;

        expect(array.max_size() == 0);
        expect(array.max_used() == 0);
        expect(array.capacity() == 0);
        expect(array.next_key() == 1);
        expect(array.is_free_list_empty());

        bool is_init = irt::is_success(array.init(3));

        expect(array.max_size() == 0);
        expect(array.max_used() == 0);
        expect(array.capacity() == 3);
        expect(array.next_key() == 1);
        expect(array.is_free_list_empty());

        expect(is_init);

        {
            auto& first = array.alloc();
            first.x     = 0.f;
            expect(array.max_size() == 1);
            expect(array.max_used() == 1);
            expect(array.capacity() == 3);
            expect(array.next_key() == 2);
            expect(array.is_free_list_empty());

            auto& second = array.alloc();
            expect(array.max_size() == 2);
            expect(array.max_used() == 2);
            expect(array.capacity() == 3);
            expect(array.next_key() == 3);
            expect(array.is_free_list_empty());

            second.x = 1.f;

            auto& third = array.alloc();
            expect(array.max_size() == 3);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 4);
            expect(array.is_free_list_empty());

            third.x = 2.f;

            expect(array.full());
        }

        array.clear();

        expect(array.max_size() == 0);
        expect(array.max_used() == 0);
        expect(array.capacity() == 3);
        expect(array.next_key() == 1);
        expect(array.is_free_list_empty());

        is_init = irt::is_success(array.init(3));

        {
            auto& d1 = array.alloc(1.f);
            auto& d2 = array.alloc(2.f);
            auto& d3 = array.alloc(3.f);

            expect(array.max_size() == 3);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 4);
            expect(array.is_free_list_empty());

            array.free(d1);

            expect(array.max_size() == 2);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 4);
            expect(!array.is_free_list_empty());

            array.free(d2);

            expect(array.max_size() == 1);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 4);
            expect(!array.is_free_list_empty());

            array.free(d3);
            expect(array.max_size() == 0);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 4);
            expect(!array.is_free_list_empty());

            auto& n1 = array.alloc();
            auto& n2 = array.alloc();
            auto& n3 = array.alloc();

            expect(irt::get_index(array.get_id(n1)) == 2_u);
            expect(irt::get_index(array.get_id(n2)) == 1_u);
            expect(irt::get_index(array.get_id(n3)) == 0_u);

            expect(array.max_size() == 3);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 7);
            expect(array.is_free_list_empty());
        }
    };

    "message"_test = [] {
        using namespace irt::literals;

        {
            irt::message vdouble;
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble.size() == 0);
            assert(vdouble.ssize() == 0);
        }

        {
            irt::message vdouble(1.0_r);
            assert(vdouble[0] == 1.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble.size() == 1);
            assert(vdouble.ssize() == 1);
        }

        {
            irt::message vdouble(0.0_r, 1.0_r);
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 1.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble.size() == 2);
            assert(vdouble.ssize() == 2);
        }

        {
            irt::message vdouble(0.0_r, 0.0_r, 1.0_r);
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 1.0_r);
            assert(vdouble.size() == 3);
            assert(vdouble.ssize() == 3);
        }
    };

    "observation_message"_test = [] {
        using namespace irt::literals;

        {
            irt::observation_message vdouble;
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble[3] == 0.0_r);
            assert(vdouble.size() == 0);
            assert(vdouble.ssize() == 0);
        }

        {
            irt::observation_message vdouble(1.0_r);
            assert(vdouble[0] == 1.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble[3] == 0.0_r);
            assert(vdouble.size() == 1);
            assert(vdouble.ssize() == 1);
        }

        {
            irt::observation_message vdouble(0.0_r, 1.0_r);
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 1.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble[3] == 0.0_r);
            assert(vdouble.size() == 2);
            assert(vdouble.ssize() == 2);
        }

        {
            irt::observation_message vdouble(0.0_r, 0.0_r, 1.0_r);
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 1.0_r);
            assert(vdouble[3] == 0.0_r);
            assert(vdouble.size() == 3);
            assert(vdouble.ssize() == 3);
        }

        {
            irt::observation_message vdouble(0.0_r, 0.0_r, 0.0_r, 1.0_r);
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble[3] == 1.0_r);
            assert(vdouble.size() == 4);
            assert(vdouble.ssize() == 4);
        }
    };

    "heap_order"_test = [] {
        irt::heap h;
        h.init(4u);

        irt::heap::handle i1 = h.insert(0.0, irt::model_id{ 0 });
        irt::heap::handle i2 = h.insert(1.0, irt::model_id{ 1 });
        irt::heap::handle i3 = h.insert(-1.0, irt::model_id{ 2 });
        irt::heap::handle i4 = h.insert(2.0, irt::model_id{ 3 });
        expect(h.full());

        expect(i1->tn == 0);
        expect(i2->tn == 1);
        expect(i3->tn == -1);
        expect(i4->tn == 2);

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
        irt::heap h;
        h.init(4u);

        irt::heap::handle i1 = h.insert(0, irt::model_id{ 0 });
        irt::heap::handle i2 = h.insert(1, irt::model_id{ 1 });
        irt::heap::handle i3 = h.insert(-1, irt::model_id{ 2 });
        irt::heap::handle i4 = h.insert(2, irt::model_id{ 3 });

        expect(i1 != nullptr);
        expect(i2 != nullptr);
        expect(i3 != nullptr);
        expect(i4 != nullptr);

        expect(!h.empty());
        expect(h.top() == i3);

        h.pop(); // remove i3
        h.pop(); // rmeove i1

        expect(h.top() == i2);

        i3->tn = -10;
        h.insert(i3);

        i1->tn = -1;
        h.insert(i1);

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

        irt::heap h;
        h.init(256u);

        for (int t = 0; t < 100; ++t) {
            h.insert(irt::to_real(t), static_cast<irt::model_id>(t));
        }

        expect(h.size() == 100_ul);

        h.insert(50, irt::model_id{ 502 });
        h.insert(50, irt::model_id{ 503 });
        h.insert(50, irt::model_id{ 504 });

        expect(h.size() == 103_ul);

        for (irt::time t = 0, e = 50; t < e; ++t) {
            expect(h.top()->tn == t);
            h.pop();
        }

        expect(h.top()->tn == 50);
        h.pop();
        expect(h.top()->tn == 50);
        h.pop();
        expect(h.top()->tn == 50);
        h.pop();
        expect(h.top()->tn == 50);
        h.pop();

        for (irt::time t = 51, e = 100; t < e; ++t) {
            expect(h.top()->tn == t);
            h.pop();
        }
    };

    "hierarchy-simple"_test = [] {
        struct data_type
        {
            data_type(int i_) noexcept
              : i(i_)
            {}

            int                       i;
            irt::hierarchy<data_type> d;
        };

        irt::vector<data_type> data(256);
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
        sim.init(64u, 256u);
        auto& dyn1 = sim.alloc<irt::qss1_sum_2>();
        (void)sim.alloc<irt::qss1_integrator>();
        (void)sim.alloc<irt::qss1_multiplier>();

        auto& mdl = irt::get_model(dyn1);

        irt::dispatch(mdl,
                      []([[maybe_unused]] auto& dyns) { std::cout << "ok"; });

        auto ret =
          irt::dispatch(mdl, []([[maybe_unused]] const auto& dyns) -> int {
              std::cout << "ok";
              return 1;
          });

        expect(ret == 1);

        auto ret_2 = irt::dispatch(
          mdl,
          []([[maybe_unused]] const auto& dyns, int v1, double v2) {
              std::cout << "ok" << v1 << ' ' << v2;
              return v2 + v1;
          },
          123,
          456.0);

        expect(ret_2 == 579.0);
    };

    "input-output"_test = [] {
        std::string str;
        str.reserve(4096u);

        {
            irt::simulation      sim;
            irt::external_source srcs;
            expect(irt::is_success(sim.init(64lu, 4096lu)));
            expect(irt::is_success(srcs.init(64lu)));

            sim.alloc<irt::qss1_integrator>();
            sim.alloc<irt::qss1_multiplier>();
            sim.alloc<irt::qss1_cross>();
            sim.alloc<irt::qss1_power>();
            sim.alloc<irt::qss1_square>();
            sim.alloc<irt::qss1_sum_2>();
            sim.alloc<irt::qss1_sum_3>();
            sim.alloc<irt::qss1_sum_4>();
            sim.alloc<irt::qss1_wsum_2>();
            sim.alloc<irt::qss1_wsum_3>();
            sim.alloc<irt::qss1_wsum_4>();
            sim.alloc<irt::qss2_integrator>();
            sim.alloc<irt::qss2_multiplier>();
            sim.alloc<irt::qss2_cross>();
            sim.alloc<irt::qss2_power>();
            sim.alloc<irt::qss2_square>();
            sim.alloc<irt::qss2_sum_2>();
            sim.alloc<irt::qss2_sum_3>();
            sim.alloc<irt::qss2_sum_4>();
            sim.alloc<irt::qss2_wsum_2>();
            sim.alloc<irt::qss2_wsum_3>();
            sim.alloc<irt::qss2_wsum_4>();
            sim.alloc<irt::qss3_integrator>();
            sim.alloc<irt::qss3_multiplier>();
            sim.alloc<irt::qss3_power>();
            sim.alloc<irt::qss3_square>();
            sim.alloc<irt::qss3_cross>();
            sim.alloc<irt::qss3_sum_2>();
            sim.alloc<irt::qss3_sum_3>();
            sim.alloc<irt::qss3_sum_4>();
            sim.alloc<irt::qss3_wsum_2>();
            sim.alloc<irt::qss3_wsum_3>();
            sim.alloc<irt::qss3_wsum_4>();
            sim.alloc<irt::integrator>();
            sim.alloc<irt::quantifier>();
            sim.alloc<irt::adder_2>();
            sim.alloc<irt::adder_3>();
            sim.alloc<irt::adder_4>();
            sim.alloc<irt::mult_2>();
            sim.alloc<irt::mult_3>();
            sim.alloc<irt::mult_4>();
            sim.alloc<irt::counter>();
            sim.alloc<irt::queue>();
            sim.alloc<irt::dynamic_queue>();
            sim.alloc<irt::priority_queue>();
            sim.alloc<irt::generator>();
            sim.alloc<irt::constant>();
            sim.alloc<irt::cross>();
            sim.alloc<irt::time_func>();
            sim.alloc<irt::accumulator_2>();
            sim.alloc<irt::hsm_wrapper>();

            std::ostringstream os;
            irt::writer        w(os);

            expect(irt::is_success(w(sim, srcs)));
            str = os.str();
        }

        expect(!str.empty());
        fmt::print("[\n{}\n]\n", str);

        {
            std::istringstream is(str);

            irt::simulation      sim;
            irt::external_source srcs;
            expect(irt::is_success(sim.init(64lu, 32lu)));
            // expect(irt::is_success(srcs.init(64u)));

            irt::reader r(is);
            expect(irt::is_success(r(sim, srcs)));

            expect(sim.models.size() == 51);
        }

        {
            std::istringstream is(str);
            int                i = 0;

            irt::simulation      sim;
            irt::external_source srcs;
            expect(irt::is_success(sim.init(64lu, 32lu)));

            irt::reader r(is);
            expect(irt::is_success(
              r(sim, srcs, [&i](irt::model_id /*id*/) { ++i; })));
            expect(i == 51);

            expect(sim.models.size() == 51);
        }

        {
            std::string string_error{
                "0 0 0 0\n1\n0 5 6 qss1_integrator A B C\n"
            };
            std::istringstream   is{ string_error };
            irt::simulation      sim;
            irt::external_source srcs;

            expect(irt::is_success(sim.init(64lu, 32lu)));

            irt::is_fatal_breakpoint = false;

            irt::reader r(is);
            expect(irt::is_bad(r(sim, srcs)));
            expect(r.line_error() == 3);
            expect(r.column_error() <= 23); /* linux/win: 22 macos: 23 */
            expect(r.model_error == 0);
            expect(r.connection_error == 0);

            expect(r.get_position(0).x == 5.f);
            expect(r.get_position(0).y == 6.f);

            irt::is_fatal_breakpoint = true;
        }
    };

    "constant_simulation"_test = [] {
        fmt::print("constant_simulation\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.can_alloc(3));

        auto& cnt = sim.alloc<irt::counter>();
        auto& c1  = sim.alloc<irt::constant>();
        auto& c2  = sim.alloc<irt::constant>();

        c1.default_value = 0.0;
        c2.default_value = 0.0;

        expect(sim.connect(c1, 0, cnt, 0) == irt::status::success);
        expect(sim.connect(c2, 0, cnt, 0) == irt::status::success);

        irt::time t = 0.0;
        expect(sim.initialize(t) == irt::status::success);

        irt::status st;

        do {
            st = sim.run(t);
            expect(irt::is_success(st));
        } while (!irt::time_domain<irt::time>::is_infinity(t));

        expect(cnt.number == static_cast<irt::i64>(2));
    };

    "cross_simulation"_test = [] {
        fmt::print("cross_simulation\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.can_alloc(3));

        auto& cnt    = sim.alloc<irt::counter>();
        auto& cross1 = sim.alloc<irt::cross>();
        auto& c1     = sim.alloc<irt::constant>();

        c1.default_value         = 3.0;
        cross1.default_threshold = 0.0;

        expect(sim.connect(c1, 0, cross1, 0) == irt::status::success);
        expect(sim.connect(c1, 0, cross1, 1) == irt::status::success);
        expect(sim.connect(c1, 0, cross1, 2) == irt::status::success);
        expect(sim.connect(cross1, 0, cnt, 0) == irt::status::success);

        irt::time t = 0.0;
        expect(sim.initialize(t) == irt::status::success);

        irt::status st;

        do {
            st = sim.run(t);
            expect(irt::is_success(st));
        } while (!irt::time_domain<irt::time>::is_infinity(t));

        expect(cnt.number == static_cast<irt::i64>(2));
    };

    "hsm_automata"_test = [] {
        irt::hierarchical_state_machine hsmw;
        hsmw.states.resize(3);

        hsmw.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u);

        hsmw.set_state(1u, 0u);
        hsmw.states[1u].input_changed_action.value_condition_1 = 3u;
        hsmw.states[1u].input_changed_action.value_mask_1      = 7u;
        hsmw.states[1u].input_changed_action.transition_1      = 2u;

        hsmw.set_state(2u, 0u);
        hsmw.states[2u].enter_action.type =
          irt::hsm_wrapper::hsm::action_type_output;
        hsmw.states[2u].enter_action.parameter_1 = 0u;
        hsmw.states[2u].enter_action.parameter_2 = 1u;

        // irt_breakpoint();
        hsmw.start();

        fmt::print("1. current state: {}\n",
                   (unsigned)hsmw.get_current_state());

        expect((int)hsmw.get_current_state() == 1);
        hsmw.values = 0b00000011;

        expect(hsmw.outputs.ssize() == 0);

        const auto processed = hsmw.dispatch(
          irt::hierarchical_state_machine::event_type_input_changed);

        expect(processed.first == irt::status::success);
        expect(processed.second == true);

        fmt::print("2. current state: {}\n",
                   (unsigned)hsmw.get_current_state());

        expect(hsmw.outputs.ssize() == 1);
    };

    "hsm_simulation"_test = [] {
        irt::simulation      sim;
        irt::external_source srcs;
        sim.source_dispatch = srcs;

        expect((irt::is_success(sim.init(16lu, 256lu))) >> fatal);
        expect((irt::is_success(srcs.init(4lu))) >> fatal);
        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);

        expect(srcs.constant_sources.can_alloc(2u));
        auto& cst_value  = srcs.constant_sources.alloc(32);
        cst_value.buffer = { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0 };

        auto& cst_ta  = srcs.constant_sources.alloc(32);
        cst_ta.buffer = { 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1. };

        auto& cst_1         = sim.alloc<irt::constant>();
        cst_1.default_value = 1.0;

        auto& cnt = sim.alloc<irt::counter>();

        auto& gen = sim.alloc<irt::generator>();
        gen.default_source_value.id =
          irt::ordinal(srcs.constant_sources.get_id(cst_value));
        gen.default_source_value.type =
          irt::ordinal(irt::external_source_type::constant);
        gen.default_source_ta.id =
          irt::ordinal(srcs.constant_sources.get_id(cst_ta));
        gen.default_source_ta.type =
          irt::ordinal(irt::external_source_type::constant);

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        auto& hsm  = sim.alloc<irt::hsm_wrapper>();
        auto* hsmw = sim.hsms.try_to_get(hsm.id);
        expect((hsmw != nullptr) >> fatal);
        hsmw->states.resize(2);

        hsmw->set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u);
        hsmw->states[0u].input_changed_action.value_condition_1 = 3u;
        hsmw->states[0u].input_changed_action.transition_1      = 1u;

        hsmw->set_state(1u, 0u);
        hsmw->states[1u].enter_action.type =
          irt::hsm_wrapper::hsm::action_type_output;
        hsmw->states[1u].enter_action.parameter_1 = 0u;
        hsmw->states[1u].enter_action.parameter_2 = 1u;

        hsmw->states[1u].enter_action.type =
          irt::hsm_wrapper::hsm::action_type_output;
        hsmw->states[1u].enter_action.parameter_1 = 0u;
        hsmw->states[1u].enter_action.parameter_2 = 1u;

        expect(sim.connect(gen, 0, hsm, 0) == irt::status::success);
        expect(sim.connect(gen, 0, hsm, 1) == irt::status::success);
        expect(sim.connect(hsm, 0, cnt, 0) == irt::status::success);

        irt::time t = 0.0;
        expect(sim.initialize(t) == irt::status::success);

        irt::status st;

        do {
            st = sim.run(t);
            expect(irt::is_success(st));
        } while (t < 10);

        expect(cnt.number == static_cast<irt::i64>(1));
    };

    "generator_counter_simluation"_test = [] {
        fmt::print("generator_counter_simluation\n");
        irt::simulation      sim;
        irt::external_source srcs;
        sim.source_dispatch = srcs;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(irt::is_success(srcs.init(4lu)));
        expect(sim.can_alloc(2));

        expect(srcs.constant_sources.can_alloc(2u));
        auto& cst_value  = srcs.constant_sources.alloc(32);
        cst_value.buffer = { 1., 2., 3., 4., 5., 6., 7., 8., 9., 10. };

        auto& cst_ta  = srcs.constant_sources.alloc(32);
        cst_ta.buffer = { 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1. };

        auto& gen = sim.alloc<irt::generator>();
        auto& cnt = sim.alloc<irt::counter>();

        gen.default_source_value.id =
          irt::ordinal(srcs.constant_sources.get_id(cst_value));
        gen.default_source_value.type =
          irt::ordinal(irt::external_source_type::constant);
        gen.default_source_ta.id =
          irt::ordinal(srcs.constant_sources.get_id(cst_ta));
        gen.default_source_ta.type =
          irt::ordinal(irt::external_source_type::constant);

        expect(sim.connect(gen, 0, cnt, 0) == irt::status::success);

        irt::time t = 0.0;
        expect(sim.initialize(t) == irt::status::success);

        irt::status st;

        do {
            st = sim.run(t);
            expect(irt::is_success(st));
        } while (t < 10);

        expect(cnt.number == static_cast<irt::i64>(10));
    };

    "time_func"_test = [] {
        fmt::print("time_func\n");
        irt::simulation sim;
        const irt::real duration{ 30 };

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.can_alloc(2));

        auto& time_fun = sim.alloc<irt::time_func>();
        auto& cnt      = sim.alloc<irt::counter>();

        time_fun.default_f     = &irt::square_time_function;
        time_fun.default_sigma = irt::to_real(0.1);

        expect(sim.connect(time_fun, 0, cnt, 0) == irt::status::success);

        irt::time t = 0;
        irt::real c = 0;
        expect(sim.initialize(t) == irt::status::success);
        do {
            auto st = sim.run(t);
            expect(irt::is_success(st) == true);
            expect(time_fun.value == t * t);
            c++;
        } while (t < duration);

        const auto value =
          (irt::real{ 2.0 } * duration / time_fun.default_sigma -
           irt::real{ 1.0 });
        expect(c == value);
    };

    "time_func_sin"_test = [] {
        fmt::print("time_func_sin\n");
#if irt_have_numbers == 1
        constexpr irt::real pi = std::numbers::pi_v<irt::real>;
#else
        // std::acos(-1) is not a constexpr in MVSC 2019
        constexpr irt::real pi = 3.141592653589793238462643383279502884;
#endif

        const irt::real f0 = irt::real(0.1);
        irt::simulation sim;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.can_alloc(2));

        auto& time_fun = sim.alloc<irt::time_func>();
        auto& cnt      = sim.alloc<irt::counter>();

        time_fun.default_f     = &irt::sin_time_function;
        time_fun.default_sigma = irt::real(0.1);

        expect(sim.connect(time_fun, 0, cnt, 0) == irt::status::success);

        irt::time       t        = 0;
        const irt::real duration = 30;
        irt::real       c        = irt::zero;

        expect(sim.initialize(t) == irt::status::success);
        do {
            auto st = sim.run(t);
            expect((irt::is_success(st)) >> fatal);
            expect(time_fun.value == std::sin(irt::two * pi * f0 * t));
            c++;
        } while (t < duration);
        expect(c == (irt::real{ 2.0 } * duration / time_fun.default_sigma -
                     irt::real{ 1.0 }));
    };

    "lotka_volterra_simulation"_test = [] {
        fmt::print("lotka_volterra_simulation\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(8));

        auto& sum_a        = sim.alloc<irt::adder_2>();
        auto& sum_b        = sim.alloc<irt::adder_2>();
        auto& product      = sim.alloc<irt::mult_2>();
        auto& integrator_a = sim.alloc<irt::integrator>();
        auto& integrator_b = sim.alloc<irt::integrator>();
        auto& quantifier_a = sim.alloc<irt::quantifier>();
        auto& quantifier_b = sim.alloc<irt::quantifier>();

        integrator_a.default_current_value = 18;

        quantifier_a.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_a.default_zero_init_offset = true;
        quantifier_a.default_step_size        = irt::to_real(0.01);
        quantifier_a.default_past_length      = 3;

        integrator_b.default_current_value = 7;

        quantifier_b.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_b.default_zero_init_offset = true;
        quantifier_b.default_step_size        = irt::to_real(0.01);
        quantifier_b.default_past_length      = 3;

        product.default_input_coeffs[0] = irt::real(1.0);
        product.default_input_coeffs[1] = irt::real(1.0);
        sum_a.default_input_coeffs[0]   = irt::real(2.0);
        sum_a.default_input_coeffs[1]   = irt::real(-0.4);
        sum_b.default_input_coeffs[0]   = irt::real(-1.0);
        sum_b.default_input_coeffs[1]   = irt::real(0.1);

        expect((sim.models.size() == 7_ul) >> fatal);

        expect(sim.connect(sum_a, 0, integrator_a, 1) == irt::status::success);
        expect(sim.connect(sum_b, 0, integrator_b, 1) == irt::status::success);

        expect(sim.connect(integrator_a, 0, sum_a, 0) == irt::status::success);
        expect(sim.connect(integrator_b, 0, sum_b, 0) == irt::status::success);

        expect(sim.connect(integrator_a, 0, product, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_b, 0, product, 1) ==
               irt::status::success);

        expect(sim.connect(product, 0, sum_a, 1) == irt::status::success);
        expect(sim.connect(product, 0, sum_b, 1) == irt::status::success);

        expect(sim.connect(quantifier_a, 0, integrator_a, 0) ==
               irt::status::success);
        expect(sim.connect(quantifier_b, 0, integrator_b, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_a, 0, quantifier_a, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_b, 0, quantifier_b, 0) ==
               irt::status::success);

        file_output fo_a("lotka-volterra_a.csv");
        file_output fo_b("lotka-volterra_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t = 0;

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 7_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::real(15));
    };

    "izhikevitch_simulation"_test = [] {
        fmt::print("izhikevitch_simulation\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(64lu, 256lu)));
        expect(sim.models.can_alloc(14));

        auto& constant     = sim.alloc<irt::constant>();
        auto& constant2    = sim.alloc<irt::constant>();
        auto& constant3    = sim.alloc<irt::constant>();
        auto& sum_a        = sim.alloc<irt::adder_2>();
        auto& sum_b        = sim.alloc<irt::adder_2>();
        auto& sum_c        = sim.alloc<irt::adder_4>();
        auto& sum_d        = sim.alloc<irt::adder_2>();
        auto& product      = sim.alloc<irt::mult_2>();
        auto& integrator_a = sim.alloc<irt::integrator>();
        auto& integrator_b = sim.alloc<irt::integrator>();
        auto& quantifier_a = sim.alloc<irt::quantifier>();
        auto& quantifier_b = sim.alloc<irt::quantifier>();
        auto& cross        = sim.alloc<irt::cross>();
        auto& cross2       = sim.alloc<irt::cross>();

        irt::real a  = irt::real(0.2);
        irt::real b  = irt::real(2.0);
        irt::real c  = irt::real(-56.0);
        irt::real d  = irt::real(-16.0);
        irt::real I  = irt::real(-99.0);
        irt::real vt = irt::real(30.0);

        constant.default_value  = irt::real(1.0);
        constant2.default_value = c;
        constant3.default_value = I;

        cross.default_threshold  = vt;
        cross2.default_threshold = vt;

        integrator_a.default_current_value = irt::real(0.0);

        quantifier_a.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_a.default_zero_init_offset = true;
        quantifier_a.default_step_size        = irt::real(0.01);
        quantifier_a.default_past_length      = 3;

        integrator_b.default_current_value = irt::real(0.0);

        quantifier_b.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_b.default_zero_init_offset = true;
        quantifier_b.default_step_size        = irt::real(0.01);
        quantifier_b.default_past_length      = 3;

        product.default_input_coeffs[0] = 1.0;
        product.default_input_coeffs[1] = 1.0;

        sum_a.default_input_coeffs[0] = irt::real(1.0);
        sum_a.default_input_coeffs[1] = irt::real(-1.0);
        sum_b.default_input_coeffs[0] = irt::real(-a);
        sum_b.default_input_coeffs[1] = irt::real(a * b);
        sum_c.default_input_coeffs[0] = irt::real(0.04);
        sum_c.default_input_coeffs[1] = irt::real(5.0);
        sum_c.default_input_coeffs[2] = irt::real(140.0);
        sum_c.default_input_coeffs[3] = irt::real(1.0);
        sum_d.default_input_coeffs[0] = irt::real(1.0);
        sum_d.default_input_coeffs[1] = irt::real(d);

        expect((sim.models.size() == 14_ul) >> fatal);

        expect(sim.connect(integrator_a, 0, cross, 0) == irt::status::success);
        expect(sim.connect(constant2, 0, cross, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, cross, 2) == irt::status::success);

        expect(sim.connect(cross, 0, quantifier_a, 0) == irt::status::success);
        expect(sim.connect(cross, 0, product, 0) == irt::status::success);
        expect(sim.connect(cross, 0, product, 1) == irt::status::success);
        expect(sim.connect(product, 0, sum_c, 0) == irt::status::success);
        expect(sim.connect(cross, 0, sum_c, 1) == irt::status::success);
        expect(sim.connect(cross, 0, sum_b, 1) == irt::status::success);

        expect(sim.connect(constant, 0, sum_c, 2) == irt::status::success);
        expect(sim.connect(constant3, 0, sum_c, 3) == irt::status::success);

        expect(sim.connect(sum_c, 0, sum_a, 0) == irt::status::success);
        expect(sim.connect(cross2, 0, sum_a, 1) == irt::status::success);
        expect(sim.connect(sum_a, 0, integrator_a, 1) == irt::status::success);
        expect(sim.connect(cross, 0, integrator_a, 2) == irt::status::success);
        expect(sim.connect(quantifier_a, 0, integrator_a, 0) ==
               irt::status::success);

        expect(sim.connect(cross2, 0, quantifier_b, 0) == irt::status::success);
        expect(sim.connect(cross2, 0, sum_b, 0) == irt::status::success);
        expect(sim.connect(quantifier_b, 0, integrator_b, 0) ==
               irt::status::success);
        expect(sim.connect(sum_b, 0, integrator_b, 1) == irt::status::success);

        expect(sim.connect(cross2, 0, integrator_b, 2) == irt::status::success);
        expect(sim.connect(integrator_a, 0, cross2, 0) == irt::status::success);
        expect(sim.connect(integrator_b, 0, cross2, 2) == irt::status::success);
        expect(sim.connect(sum_d, 0, cross2, 1) == irt::status::success);
        expect(sim.connect(integrator_b, 0, sum_d, 0) == irt::status::success);
        expect(sim.connect(constant, 0, sum_d, 1) == irt::status::success);

        file_output fo_a("izhikevitch_a.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        file_output fo_b("izhikevitch_b.csv");
        expect(fo_b.os != nullptr);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t = irt::real(0);

        expect(irt::status::success == sim.initialize(t));
        expect((sim.sched.size() == 14_ul) >> fatal);

        do {
            irt::status st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::real(120));
    };

    "lotka_volterra_simulation_qss1"_test = [] {
        fmt::print("lotka_volterra_simulation_qss1\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum_a        = sim.alloc<irt::qss1_wsum_2>();
        auto& sum_b        = sim.alloc<irt::qss1_wsum_2>();
        auto& product      = sim.alloc<irt::qss1_multiplier>();
        auto& integrator_a = sim.alloc<irt::qss1_integrator>();
        auto& integrator_b = sim.alloc<irt::qss1_integrator>();

        integrator_a.default_X  = irt::real(18.0);
        integrator_a.default_dQ = irt::real(0.1);

        integrator_b.default_X  = irt::real(7.0);
        integrator_b.default_dQ = irt::real(0.1);

        sum_a.default_input_coeffs[0] = irt::real(2.0);
        sum_a.default_input_coeffs[1] = irt::real(-0.4);
        sum_b.default_input_coeffs[0] = irt::real(-1.0);
        sum_b.default_input_coeffs[1] = irt::real(0.1);

        expect((sim.models.size() == 5_ul) >> fatal);

        expect(sim.connect(sum_a, 0, integrator_a, 0) == irt::status::success);
        expect(sim.connect(sum_b, 0, integrator_b, 0) == irt::status::success);

        expect(sim.connect(integrator_a, 0, sum_a, 0) == irt::status::success);
        expect(sim.connect(integrator_b, 0, sum_b, 0) == irt::status::success);

        expect(sim.connect(integrator_a, 0, product, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_b, 0, product, 1) ==
               irt::status::success);

        expect(sim.connect(product, 0, sum_a, 1) == irt::status::success);
        expect(sim.connect(product, 0, sum_b, 1) == irt::status::success);

        file_output fo_a("lotka-volterra-qss1_a.csv");
        file_output fo_b("lotka-volterra-qss1_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t = irt::real(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::real(15));
    };

    "lotka_volterra_simulation_qss2"_test = [] {
        fmt::print("lotka_volterra_simulation_qss2\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum_a        = sim.alloc<irt::qss2_wsum_2>();
        auto& sum_b        = sim.alloc<irt::qss2_wsum_2>();
        auto& product      = sim.alloc<irt::qss2_multiplier>();
        auto& integrator_a = sim.alloc<irt::qss2_integrator>();
        auto& integrator_b = sim.alloc<irt::qss2_integrator>();

        integrator_a.default_X  = irt::real(18.0);
        integrator_a.default_dQ = irt::real(0.1);

        integrator_b.default_X  = irt::real(7.0);
        integrator_b.default_dQ = irt::real(0.1);

        sum_a.default_input_coeffs[0] = irt::real(2.0);
        sum_a.default_input_coeffs[1] = irt::real(-0.4);
        sum_b.default_input_coeffs[0] = irt::real(-1.0);
        sum_b.default_input_coeffs[1] = irt::real(0.1);

        expect((sim.models.size() == 5_ul) >> fatal);

        expect(sim.connect(sum_a, 0, integrator_a, 0) == irt::status::success);
        expect(sim.connect(sum_b, 0, integrator_b, 0) == irt::status::success);

        expect(sim.connect(integrator_a, 0, sum_a, 0) == irt::status::success);
        expect(sim.connect(integrator_b, 0, sum_b, 0) == irt::status::success);

        expect(sim.connect(integrator_a, 0, product, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_b, 0, product, 1) ==
               irt::status::success);

        expect(sim.connect(product, 0, sum_a, 1) == irt::status::success);
        expect(sim.connect(product, 0, sum_b, 1) == irt::status::success);

        file_output fo_a("lotka-volterra-qss2_a.csv");
        file_output fo_b("lotka-volterra-qss2_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t = irt::real(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::real(15));
    };

    "lif_simulation_qss"_test = [] {
        fmt::print("lif_simulation_qss\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(6));

        auto& sum            = sim.alloc<irt::adder_2>();
        auto& quantifier     = sim.alloc<irt::quantifier>();
        auto& integrator     = sim.alloc<irt::integrator>();
        auto& I              = sim.alloc<irt::time_func>();
        auto& constant_cross = sim.alloc<irt::constant>();
        auto& cross          = sim.alloc<irt::cross>();

        irt::real tau = irt::real(10.0);
        irt::real Vt  = irt::real(1.0);
        irt::real V0  = irt::real(10.0);
        irt::real Vr  = irt::real(-V0);

        sum.default_input_coeffs[0] = irt::real(-1) / tau;
        sum.default_input_coeffs[1] = V0 / tau;

        constant_cross.default_value = Vr;

        integrator.default_current_value = irt::real(0);

        quantifier.default_adapt_state = irt::quantifier::adapt_state::possible;
        quantifier.default_zero_init_offset = true;
        quantifier.default_step_size        = irt::real(0.1);
        quantifier.default_past_length      = 3;

        I.default_f             = &irt::sin_time_function;
        I.default_sigma         = quantifier.default_step_size;
        cross.default_threshold = Vt;

        expect((sim.models.size() == 6_ul) >> fatal);

        expect(sim.connect(quantifier, 0, integrator, 0) ==
               irt::status::success);
        expect(sim.connect(sum, 0, integrator, 1) == irt::status::success);
        expect(sim.connect(cross, 0, integrator, 2) == irt::status::success);
        expect(sim.connect(cross, 0, quantifier, 0) == irt::status::success);
        expect(sim.connect(cross, 0, sum, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 2) == irt::status::success);
        expect(sim.connect(constant_cross, 0, cross, 1) ==
               irt::status::success);
        expect(sim.connect(I, 0, sum, 1) == irt::status::success);

        file_output fo_a("lif-qss.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        sim.observe(irt::get_model(integrator), obs_a);

        irt::time t(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 6_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(100));
    };

    "lif_simulation_qss1"_test = [] {
        fmt::print("lif_simulation_qss1\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum            = sim.alloc<irt::qss1_wsum_2>();
        auto& integrator     = sim.alloc<irt::qss1_integrator>();
        auto& constant       = sim.alloc<irt::constant>();
        auto& constant_cross = sim.alloc<irt::constant>();
        auto& cross          = sim.alloc<irt::qss1_cross>();

        irt::real tau = irt::real(10.0);
        irt::real Vt  = irt::real(1.0);
        irt::real V0  = irt::real(10.0);
        irt::real Vr  = irt::real(-V0);

        sum.default_input_coeffs[0] = irt::real(-1) / tau;
        sum.default_input_coeffs[1] = V0 / tau;

        constant.default_value       = irt::real(1);
        constant_cross.default_value = Vr;

        integrator.default_X  = irt::real(0);
        integrator.default_dQ = irt::real(0.001);

        cross.default_threshold = Vt;

        expect((sim.models.size() == 5_ul) >> fatal);

        // Connections

        expect(sim.connect(cross, 0, integrator, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 2) == irt::status::success);
        expect(sim.connect(constant_cross, 0, cross, 1) ==
               irt::status::success);
        expect(sim.connect(constant, 0, sum, 1) == irt::status::success);
        expect(sim.connect(sum, 0, integrator, 0) == irt::status::success);

        file_output fo_a("lif-qss1.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        sim.observe(irt::get_model(integrator), obs_a);

        irt::time t = 0.0;

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time{ 100.0 });
    };

    "lif_simulation_qss2"_test = [] {
        fmt::print("lif_simulation_qss2\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum            = sim.alloc<irt::qss2_wsum_2>();
        auto& integrator     = sim.alloc<irt::qss2_integrator>();
        auto& constant       = sim.alloc<irt::constant>();
        auto& constant_cross = sim.alloc<irt::constant>();
        auto& cross          = sim.alloc<irt::qss2_cross>();

        irt::real tau = irt::real(10.0);
        irt::real Vt  = irt::real(1.0);
        irt::real V0  = irt::real(10.0);
        irt::real Vr  = irt::real(-V0);

        sum.default_input_coeffs[0] = irt::real(-1.0) / tau;
        sum.default_input_coeffs[1] = V0 / tau;

        constant.default_value       = irt::real(1.0);
        constant_cross.default_value = Vr;

        integrator.default_X  = irt::real(0.0);
        integrator.default_dQ = irt::real(0.001);

        cross.default_threshold = Vt;

        expect((sim.models.size() == 5_ul) >> fatal);

        // Connections

        // expect(sim.connect(cross.y[1], integrator.x[0]) ==
        // irt::status::success);
        expect(sim.connect(cross, 0, integrator, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum, 0) == irt::status::success);

        expect(sim.connect(integrator, 0, cross, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 2) == irt::status::success);
        expect(sim.connect(constant_cross, 0, cross, 1) ==
               irt::status::success);
        expect(sim.connect(constant, 0, sum, 1) == irt::status::success);
        expect(sim.connect(sum, 0, integrator, 0) == irt::status::success);

        file_output fo_a("lif-qss2.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        sim.observe(irt::get_model(integrator), obs_a);

        irt::time t = irt::time(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            // printf("--------------------------------------------\n");
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(100));
    };

    "izhikevich_simulation_qss1"_test = [] {
        fmt::print("izhikevich_simulation_qss1\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(128lu, 256)));
        expect(sim.can_alloc(12));

        auto& constant     = sim.alloc<irt::constant>();
        auto& constant2    = sim.alloc<irt::constant>();
        auto& constant3    = sim.alloc<irt::constant>();
        auto& sum_a        = sim.alloc<irt::qss1_wsum_2>();
        auto& sum_b        = sim.alloc<irt::qss1_wsum_2>();
        auto& sum_c        = sim.alloc<irt::qss1_wsum_4>();
        auto& sum_d        = sim.alloc<irt::qss1_wsum_2>();
        auto& product      = sim.alloc<irt::qss1_multiplier>();
        auto& integrator_a = sim.alloc<irt::qss1_integrator>();
        auto& integrator_b = sim.alloc<irt::qss1_integrator>();
        auto& cross        = sim.alloc<irt::qss1_cross>();
        auto& cross2       = sim.alloc<irt::qss1_cross>();

        irt::real a  = irt::real(0.2);
        irt::real b  = irt::real(2.0);
        irt::real c  = irt::real(-56.0);
        irt::real d  = irt::real(-16.0);
        irt::real I  = irt::real(-99.0);
        irt::real vt = irt::real(30.0);

        constant.default_value  = irt::real(1.0);
        constant2.default_value = c;
        constant3.default_value = I;

        cross.default_threshold  = vt;
        cross2.default_threshold = vt;

        integrator_a.default_X  = irt::real(0.0);
        integrator_a.default_dQ = irt::real(0.01);

        integrator_b.default_X  = irt::real(0.0);
        integrator_b.default_dQ = irt::real(0.01);

        sum_a.default_input_coeffs[0] = irt::real(1.0);
        sum_a.default_input_coeffs[1] = irt::real(-1.0);
        sum_b.default_input_coeffs[0] = -a;
        sum_b.default_input_coeffs[1] = a * b;
        sum_c.default_input_coeffs[0] = irt::real(0.04);
        sum_c.default_input_coeffs[1] = irt::real(5.0);
        sum_c.default_input_coeffs[2] = irt::real(140.0);
        sum_c.default_input_coeffs[3] = irt::real(1.0);
        sum_d.default_input_coeffs[0] = irt::real(1.0);
        sum_d.default_input_coeffs[1] = d;

        expect((sim.models.size() == 12_ul) >> fatal);

        expect(sim.connect(integrator_a, 0, cross, 0) == irt::status::success);
        expect(sim.connect(constant2, 0, cross, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, cross, 2) == irt::status::success);

        expect(sim.connect(cross, 1, product, 0) == irt::status::success);
        expect(sim.connect(cross, 1, product, 1) == irt::status::success);
        expect(sim.connect(product, 0, sum_c, 0) == irt::status::success);
        expect(sim.connect(cross, 1, sum_c, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum_b, 1) == irt::status::success);

        expect(sim.connect(constant, 0, sum_c, 2) == irt::status::success);
        expect(sim.connect(constant3, 0, sum_c, 3) == irt::status::success);

        expect(sim.connect(sum_c, 0, sum_a, 0) == irt::status::success);
        // expect(sim.connect(integrator_b, 0, sum_a, 1) ==
        // irt::status::success);
        expect(sim.connect(cross2, 1, sum_a, 1) == irt::status::success);
        expect(sim.connect(sum_a, 0, integrator_a, 0) == irt::status::success);
        expect(sim.connect(cross, 0, integrator_a, 1) == irt::status::success);

        expect(sim.connect(cross2, 1, sum_b, 0) == irt::status::success);
        expect(sim.connect(sum_b, 0, integrator_b, 0) == irt::status::success);

        expect(sim.connect(cross2, 0, integrator_b, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, cross2, 0) == irt::status::success);
        expect(sim.connect(integrator_b, 0, cross2, 2) == irt::status::success);
        expect(sim.connect(sum_d, 0, cross2, 1) == irt::status::success);
        expect(sim.connect(integrator_b, 0, sum_d, 0) == irt::status::success);
        expect(sim.connect(constant, 0, sum_d, 1) == irt::status::success);

        file_output fo_a("izhikevitch-qss1_a.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        file_output fo_b("izhikevitch-qss1_b.csv");
        expect(fo_b.os != nullptr);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t = irt::real(0);

        expect(irt::status::success == sim.initialize(t));
        expect((sim.sched.size() == 12_ul) >> fatal);

        do {
            irt::status st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(140));
    };

    "izhikevich_simulation_qss2"_test = [] {
        fmt::print("izhikevich_simulation_qss2\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(64lu, 256lu)));
        expect(sim.can_alloc(12));

        auto& constant     = sim.alloc<irt::constant>();
        auto& constant2    = sim.alloc<irt::constant>();
        auto& constant3    = sim.alloc<irt::constant>();
        auto& sum_a        = sim.alloc<irt::qss2_wsum_2>();
        auto& sum_b        = sim.alloc<irt::qss2_wsum_2>();
        auto& sum_c        = sim.alloc<irt::qss2_wsum_4>();
        auto& sum_d        = sim.alloc<irt::qss2_wsum_2>();
        auto& product      = sim.alloc<irt::qss2_multiplier>();
        auto& integrator_a = sim.alloc<irt::qss2_integrator>();
        auto& integrator_b = sim.alloc<irt::qss2_integrator>();
        auto& cross        = sim.alloc<irt::qss2_cross>();
        auto& cross2       = sim.alloc<irt::qss2_cross>();

        irt::real a  = irt::real(0.2);
        irt::real b  = irt::real(2.0);
        irt::real c  = irt::real(-56.0);
        irt::real d  = irt::real(-16.0);
        irt::real I  = irt::real(-99.0);
        irt::real vt = irt::real(30.0);

        constant.default_value  = irt::real(1.0);
        constant2.default_value = c;
        constant3.default_value = I;

        cross.default_threshold  = vt;
        cross2.default_threshold = vt;

        integrator_a.default_X  = irt::real(0.0);
        integrator_a.default_dQ = irt::real(0.01);

        integrator_b.default_X  = irt::real(0.0);
        integrator_b.default_dQ = irt::real(0.01);

        sum_a.default_input_coeffs[0] = irt::real(1.0);
        sum_a.default_input_coeffs[1] = irt::real(-1.0);
        sum_b.default_input_coeffs[0] = irt::real(-a);
        sum_b.default_input_coeffs[1] = irt::real(a * b);
        sum_c.default_input_coeffs[0] = irt::real(0.04);
        sum_c.default_input_coeffs[1] = irt::real(5.0);
        sum_c.default_input_coeffs[2] = irt::real(140.0);
        sum_c.default_input_coeffs[3] = irt::real(1.0);
        sum_d.default_input_coeffs[0] = irt::real(1.0);
        sum_d.default_input_coeffs[1] = irt::real(d);

        expect((sim.models.size() == 12_ul) >> fatal);

        expect(sim.connect(integrator_a, 0, cross, 0) == irt::status::success);
        expect(sim.connect(constant2, 0, cross, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, cross, 2) == irt::status::success);

        expect(sim.connect(cross, 1, product, 0) == irt::status::success);
        expect(sim.connect(cross, 1, product, 1) == irt::status::success);
        expect(sim.connect(product, 0, sum_c, 0) == irt::status::success);
        expect(sim.connect(cross, 1, sum_c, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum_b, 1) == irt::status::success);

        expect(sim.connect(constant, 0, sum_c, 2) == irt::status::success);
        expect(sim.connect(constant3, 0, sum_c, 3) == irt::status::success);

        expect(sim.connect(sum_c, 0, sum_a, 0) == irt::status::success);
        // expect(sim.connect(integrator_b, 0, sum_a, 1) ==
        // irt::status::success);
        expect(sim.connect(cross2, 1, sum_a, 1) == irt::status::success);
        expect(sim.connect(sum_a, 0, integrator_a, 0) == irt::status::success);
        expect(sim.connect(cross, 0, integrator_a, 1) == irt::status::success);

        expect(sim.connect(cross2, 1, sum_b, 0) == irt::status::success);
        expect(sim.connect(sum_b, 0, integrator_b, 0) == irt::status::success);

        expect(sim.connect(cross2, 0, integrator_b, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, cross2, 0) == irt::status::success);
        expect(sim.connect(integrator_b, 0, cross2, 2) == irt::status::success);
        expect(sim.connect(sum_d, 0, cross2, 1) == irt::status::success);
        expect(sim.connect(integrator_b, 0, sum_d, 0) == irt::status::success);
        expect(sim.connect(constant, 0, sum_d, 1) == irt::status::success);

        file_output fo_a("izhikevitch-qss2_a.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        file_output fo_b("izhikevitch-qss2_b.csv");
        expect(fo_b.os != nullptr);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t = irt::zero;

        expect(irt::status::success == sim.initialize(t));
        expect((sim.sched.size() == 12_ul) >> fatal);

        do {
            irt::status st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(140));
    };

    "lotka_volterra_simulation_qss3"_test = [] {
        fmt::print("lotka_volterra_simulation_qss3\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum_a        = sim.alloc<irt::qss3_wsum_2>();
        auto& sum_b        = sim.alloc<irt::qss3_wsum_2>();
        auto& product      = sim.alloc<irt::qss3_multiplier>();
        auto& integrator_a = sim.alloc<irt::qss3_integrator>();
        auto& integrator_b = sim.alloc<irt::qss3_integrator>();

        integrator_a.default_X  = irt::real(18.0);
        integrator_a.default_dQ = irt::real(0.1);

        integrator_b.default_X  = irt::real(7.0);
        integrator_b.default_dQ = irt::real(0.1);

        sum_a.default_input_coeffs[0] = irt::real(2.0);
        sum_a.default_input_coeffs[1] = irt::real(-0.4);
        sum_b.default_input_coeffs[0] = irt::real(-1.0);
        sum_b.default_input_coeffs[1] = irt::real(0.1);

        expect((sim.models.size() == 5_ul) >> fatal);

        expect(sim.connect(sum_a, 0, integrator_a, 0) == irt::status::success);
        expect(sim.connect(sum_b, 0, integrator_b, 0) == irt::status::success);

        expect(sim.connect(integrator_a, 0, sum_a, 0) == irt::status::success);
        expect(sim.connect(integrator_b, 0, sum_b, 0) == irt::status::success);

        expect(sim.connect(integrator_a, 0, product, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_b, 0, product, 1) ==
               irt::status::success);

        expect(sim.connect(product, 0, sum_a, 1) == irt::status::success);
        expect(sim.connect(product, 0, sum_b, 1) == irt::status::success);

        file_output fo_a("lotka-volterra-qss3_a.csv");
        file_output fo_b("lotka-volterra-qss3_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t = irt::zero;

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(15));
    };

    "lif_simulation_qss3"_test = [] {
        fmt::print("lif_simulation_qss3\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum            = sim.alloc<irt::qss3_wsum_2>();
        auto& integrator     = sim.alloc<irt::qss3_integrator>();
        auto& constant       = sim.alloc<irt::constant>();
        auto& constant_cross = sim.alloc<irt::constant>();
        auto& cross          = sim.alloc<irt::qss3_cross>();

        irt::real tau = irt::real(10.0);
        irt::real Vt  = irt::real(1.0);
        irt::real V0  = irt::real(10.0);
        irt::real Vr  = irt::real(-V0);

        sum.default_input_coeffs[0] = irt::real(-1.0) / tau;
        sum.default_input_coeffs[1] = V0 / tau;

        constant.default_value       = irt::real(1.0);
        constant_cross.default_value = Vr;

        integrator.default_X  = irt::real(0.0);
        integrator.default_dQ = irt::real(0.01);

        cross.default_threshold = Vt;

        expect((sim.models.size() == 5_ul) >> fatal);

        // Connections

        // expect(sim.connect(cross.y[1], integrator.x[0]) ==
        // irt::status::success);
        expect(sim.connect(cross, 0, integrator, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum, 0) == irt::status::success);

        expect(sim.connect(integrator, 0, cross, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 2) == irt::status::success);
        expect(sim.connect(constant_cross, 0, cross, 1) ==
               irt::status::success);
        expect(sim.connect(constant, 0, sum, 1) == irt::status::success);
        expect(sim.connect(sum, 0, integrator, 0) == irt::status::success);

        file_output fo_a("lif-qss3.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        sim.observe(irt::get_model(integrator), obs_a);

        irt::time t = irt::zero;

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            // printf("--------------------------------------------\n");
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(100));
    };

    "izhikevich_simulation_qss3"_test = [] {
        fmt::print("izhikevich_simulation_qss3\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(64lu, 256lu)));
        expect(sim.can_alloc(12));

        auto& constant     = sim.alloc<irt::constant>();
        auto& constant2    = sim.alloc<irt::constant>();
        auto& constant3    = sim.alloc<irt::constant>();
        auto& sum_a        = sim.alloc<irt::qss3_wsum_2>();
        auto& sum_b        = sim.alloc<irt::qss3_wsum_2>();
        auto& sum_c        = sim.alloc<irt::qss3_wsum_4>();
        auto& sum_d        = sim.alloc<irt::qss3_wsum_2>();
        auto& product      = sim.alloc<irt::qss3_multiplier>();
        auto& integrator_a = sim.alloc<irt::qss3_integrator>();
        auto& integrator_b = sim.alloc<irt::qss3_integrator>();
        auto& cross        = sim.alloc<irt::qss3_cross>();
        auto& cross2       = sim.alloc<irt::qss3_cross>();

        irt::real a  = irt::real(0.2);
        irt::real b  = irt::real(2.0);
        irt::real c  = irt::real(-56.0);
        irt::real d  = irt::real(-16.0);
        irt::real I  = irt::real(-99.0);
        irt::real vt = irt::real(30.0);

        constant.default_value  = irt::real(1.0);
        constant2.default_value = c;
        constant3.default_value = I;

        cross.default_threshold  = vt;
        cross2.default_threshold = vt;

        integrator_a.default_X  = irt::real(0.0);
        integrator_a.default_dQ = irt::real(0.01);

        integrator_b.default_X  = irt::real(0.0);
        integrator_b.default_dQ = irt::real(0.01);

        sum_a.default_input_coeffs[0] = irt::real(1.0);
        sum_a.default_input_coeffs[1] = irt::real(-1.0);
        sum_b.default_input_coeffs[0] = irt::real(-a);
        sum_b.default_input_coeffs[1] = irt::real(a * b);
        sum_c.default_input_coeffs[0] = irt::real(0.04);
        sum_c.default_input_coeffs[1] = irt::real(5.0);
        sum_c.default_input_coeffs[2] = irt::real(140.0);
        sum_c.default_input_coeffs[3] = irt::real(1.0);
        sum_d.default_input_coeffs[0] = irt::real(1.0);
        sum_d.default_input_coeffs[1] = irt::real(d);

        expect((sim.models.size() == 12_ul) >> fatal);

        expect(sim.connect(integrator_a, 0, cross, 0) == irt::status::success);
        expect(sim.connect(constant2, 0, cross, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, cross, 2) == irt::status::success);

        expect(sim.connect(cross, 1, product, 0) == irt::status::success);
        expect(sim.connect(cross, 1, product, 1) == irt::status::success);
        expect(sim.connect(product, 0, sum_c, 0) == irt::status::success);
        expect(sim.connect(cross, 1, sum_c, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum_b, 1) == irt::status::success);

        expect(sim.connect(constant, 0, sum_c, 2) == irt::status::success);
        expect(sim.connect(constant3, 0, sum_c, 3) == irt::status::success);

        expect(sim.connect(sum_c, 0, sum_a, 0) == irt::status::success);
        // expect(sim.connect(integrator_b, 0, sum_a, 1) ==
        // irt::status::success);
        expect(sim.connect(cross2, 1, sum_a, 1) == irt::status::success);
        expect(sim.connect(sum_a, 0, integrator_a, 0) == irt::status::success);
        expect(sim.connect(cross, 0, integrator_a, 1) == irt::status::success);

        expect(sim.connect(cross2, 1, sum_b, 0) == irt::status::success);
        expect(sim.connect(sum_b, 0, integrator_b, 0) == irt::status::success);

        expect(sim.connect(cross2, 0, integrator_b, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, cross2, 0) == irt::status::success);
        expect(sim.connect(integrator_b, 0, cross2, 2) == irt::status::success);
        expect(sim.connect(sum_d, 0, cross2, 1) == irt::status::success);
        expect(sim.connect(integrator_b, 0, sum_d, 0) == irt::status::success);
        expect(sim.connect(constant, 0, sum_d, 1) == irt::status::success);

        file_output fo_a("izhikevitch-qss3_a.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        file_output fo_b("izhikevitch-qss3_b.csv");
        expect(fo_b.os != nullptr);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t = irt::zero;

        expect(irt::status::success == sim.initialize(t));
        expect((sim.sched.size() == 12_ul) >> fatal);

        do {
            irt::status st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(140));
    };

    "van_der_pol_simulation"_test = [] {
        fmt::print("van_der_pol_simulation\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(6));

        auto& sum          = sim.alloc<irt::adder_3>();
        auto& product      = sim.alloc<irt::mult_3>();
        auto& integrator_a = sim.alloc<irt::integrator>();
        auto& integrator_b = sim.alloc<irt::integrator>();
        auto& quantifier_a = sim.alloc<irt::quantifier>();
        auto& quantifier_b = sim.alloc<irt::quantifier>();

        integrator_a.default_current_value = irt::real(0);

        quantifier_a.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_a.default_zero_init_offset = true;
        quantifier_a.default_step_size        = irt::real(0.01);
        quantifier_a.default_past_length      = 3;

        integrator_b.default_current_value = irt::real(10.0);

        quantifier_b.default_adapt_state =
          irt::quantifier::adapt_state::possible;
        quantifier_b.default_zero_init_offset = true;
        quantifier_b.default_step_size        = irt::real(0.01);
        quantifier_b.default_past_length      = 3;

        product.default_input_coeffs[0] = irt::real(1);
        product.default_input_coeffs[1] = irt::real(1);
        product.default_input_coeffs[2] = irt::real(1);

        irt::real mu(4.0);
        sum.default_input_coeffs[0] = mu;
        sum.default_input_coeffs[1] = -mu;
        sum.default_input_coeffs[2] = irt::real(-1.0);

        expect((sim.models.size() == 6_ul) >> fatal);

        expect(sim.connect(integrator_b, 0, integrator_a, 1) ==
               irt::status::success);
        expect(sim.connect(sum, 0, integrator_b, 1) == irt::status::success);

        expect(sim.connect(integrator_b, 0, sum, 0) == irt::status::success);
        expect(sim.connect(product, 0, sum, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, sum, 2) == irt::status::success);

        expect(sim.connect(integrator_b, 0, product, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_a, 0, product, 1) ==
               irt::status::success);
        expect(sim.connect(integrator_a, 0, product, 2) ==
               irt::status::success);

        expect(sim.connect(quantifier_a, 0, integrator_a, 0) ==
               irt::status::success);
        expect(sim.connect(quantifier_b, 0, integrator_b, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_a, 0, quantifier_a, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_b, 0, quantifier_b, 0) ==
               irt::status::success);

        file_output fo_a("van_der_pol_a.csv");
        file_output fo_b("van_der_pol_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 6_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(150));
    };

    "van_der_pol_simulation_qss3"_test = [] {
        fmt::print("van_der_pol_simulation_qss3\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum          = sim.alloc<irt::qss3_wsum_3>();
        auto& product1     = sim.alloc<irt::qss3_multiplier>();
        auto& product2     = sim.alloc<irt::qss3_multiplier>();
        auto& integrator_a = sim.alloc<irt::qss3_integrator>();
        auto& integrator_b = sim.alloc<irt::qss3_integrator>();

        integrator_a.default_X  = irt::real(0.0);
        integrator_a.default_dQ = irt::real(0.001);

        integrator_b.default_X  = irt::real(10.0);
        integrator_b.default_dQ = irt::real(0.001);

        irt::real mu(4);
        sum.default_input_coeffs[0] = mu;
        sum.default_input_coeffs[1] = -mu;
        sum.default_input_coeffs[2] = irt::real(-1.0);

        expect(sim.models.size() == 5_ul);

        expect(sim.connect(integrator_b, 0, integrator_a, 0) ==
               irt::status::success);
        expect(sim.connect(sum, 0, integrator_b, 0) == irt::status::success);

        expect(sim.connect(integrator_b, 0, sum, 0) == irt::status::success);
        expect(sim.connect(product2, 0, sum, 1) == irt::status::success);
        expect(sim.connect(integrator_a, 0, sum, 2) == irt::status::success);

        expect(sim.connect(integrator_b, 0, product1, 0) ==
               irt::status::success);
        expect(sim.connect(integrator_a, 0, product1, 1) ==
               irt::status::success);
        expect(sim.connect(product1, 0, product2, 0) == irt::status::success);
        expect(sim.connect(integrator_a, 0, product2, 1) ==
               irt::status::success);

        file_output fo_a("van_der_pol_qss3_a.csv");
        file_output fo_b("van_der_pol_qss3_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);
        auto& obs_b = sim.observers.alloc("B", file_output_callback, &fo_b);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        irt::time t(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(1500.0));
    };

    "neg_lif_simulation_qss1"_test = [] {
        fmt::print("neg_lif_simulation_qss1\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum            = sim.alloc<irt::qss1_wsum_2>();
        auto& integrator     = sim.alloc<irt::qss1_integrator>();
        auto& constant       = sim.alloc<irt::constant>();
        auto& constant_cross = sim.alloc<irt::constant>();
        auto& cross          = sim.alloc<irt::qss1_cross>();

        irt::real tau(10.0);
        irt::real Vt(-1.0);
        irt::real V0(-10.0);
        irt::real Vr(0.0);

        sum.default_input_coeffs[0] = irt::real(-1) / tau;
        sum.default_input_coeffs[1] = V0 / tau;

        constant.default_value       = irt::real(1);
        constant_cross.default_value = Vr;

        integrator.default_X  = irt::real(0.0);
        integrator.default_dQ = irt::real(0.001);

        cross.default_threshold = Vt;
        cross.default_detect_up = false;

        expect((sim.models.size() == 5_ul) >> fatal);

        // Connections

        // expect(sim.connect(cross.y[1], integrator.x[0]) ==
        // irt::status::success);
        expect(sim.connect(cross, 0, integrator, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 2) == irt::status::success);
        expect(sim.connect(constant_cross, 0, cross, 1) ==
               irt::status::success);
        expect(sim.connect(constant, 0, sum, 1) == irt::status::success);
        expect(sim.connect(sum, 0, integrator, 0) == irt::status::success);

        file_output fo_a("neg-lif-qss1.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        sim.observe(irt::get_model(integrator), obs_a);

        irt::time t(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(100));
    };

    "neg_lif_simulation_qss2"_test = [] {
        fmt::print("neg_lif_simulation_qss2\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum            = sim.alloc<irt::qss2_wsum_2>();
        auto& integrator     = sim.alloc<irt::qss2_integrator>();
        auto& constant       = sim.alloc<irt::constant>();
        auto& constant_cross = sim.alloc<irt::constant>();
        auto& cross          = sim.alloc<irt::qss2_cross>();

        irt::real tau(10.0);
        irt::real Vt(-1.0);
        irt::real V0(-10.0);
        irt::real Vr(0.0);

        sum.default_input_coeffs[0] = irt::real(-1.0) / tau;
        sum.default_input_coeffs[1] = V0 / tau;

        constant.default_value       = irt::real(1.0);
        constant_cross.default_value = Vr;

        integrator.default_X  = irt::real(0.0);
        integrator.default_dQ = irt::real(0.0001);

        cross.default_threshold = Vt;
        cross.default_detect_up = false;

        expect((sim.models.size() == 5_ul) >> fatal);

        // Connections

        // expect(sim.connect(cross.y[1], integrator.x[0]) ==
        // irt::status::success);
        expect(sim.connect(cross, 0, integrator, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 2) == irt::status::success);
        expect(sim.connect(constant_cross, 0, cross, 1) ==
               irt::status::success);
        expect(sim.connect(constant, 0, sum, 1) == irt::status::success);
        expect(sim.connect(sum, 0, integrator, 0) == irt::status::success);

        file_output fo_a("neg-lif-qss2.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        sim.observe(irt::get_model(integrator), obs_a);

        irt::time t(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(100.0));
    };

    "neg_lif_simulation_qss3"_test = [] {
        fmt::print("neg_lif_simulation_qss3\n");
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
        expect(sim.can_alloc(5));

        auto& sum            = sim.alloc<irt::qss3_wsum_2>();
        auto& integrator     = sim.alloc<irt::qss3_integrator>();
        auto& constant       = sim.alloc<irt::constant>();
        auto& constant_cross = sim.alloc<irt::constant>();
        auto& cross          = sim.alloc<irt::qss3_cross>();

        irt::real tau = 10;
        irt::real Vt  = -1;
        irt::real V0  = -10;
        irt::real Vr  = 0;

        sum.default_input_coeffs[0] = irt::real(-1) / tau;
        sum.default_input_coeffs[1] = V0 / tau;

        constant.default_value       = irt::real(1);
        constant_cross.default_value = Vr;

        integrator.default_X  = irt::zero;
        integrator.default_dQ = irt::to_real(0.0001);

        cross.default_threshold = Vt;
        cross.default_detect_up = false;

        expect((sim.models.size() == 5_ul) >> fatal);

        // Connections

        // expect(sim.connect(cross.y[1], integrator.x[0]) ==
        // irt::status::success);
        expect(sim.connect(cross, 0, integrator, 1) == irt::status::success);
        expect(sim.connect(cross, 1, sum, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 0) == irt::status::success);
        expect(sim.connect(integrator, 0, cross, 2) == irt::status::success);
        expect(sim.connect(constant_cross, 0, cross, 1) ==
               irt::status::success);
        expect(sim.connect(constant, 0, sum, 1) == irt::status::success);
        expect(sim.connect(sum, 0, integrator, 0) == irt::status::success);

        file_output fo_a("neg-lif-qss3.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc("A", file_output_callback, &fo_a);

        sim.observe(irt::get_model(integrator), obs_a);

        irt::time t(0);

        expect(sim.initialize(t) == irt::status::success);
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < irt::time(100.0));
    };

    "all"_test = [] {
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_lotka_volterra<1>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }

        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_negative_lif<1>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_lif<1>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_van_der_pol<1>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_izhikevich<1>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }

        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_lotka_volterra<2>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_negative_lif<2>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_lif<2>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_van_der_pol<2>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_izhikevich<2>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }

        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_lotka_volterra<3>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_negative_lif<3>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_lif<3>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_van_der_pol<3>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
        {
            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
            expect(irt::example_qss_izhikevich<3>(sim, empty_fun) ==
                   irt::status::success);
            expect(run_simulation(sim, 30.) == irt::status::success);
        }
    };

    "memory"_test = [] {
        global_alloc g_a;
        global_free  g_b;

        {
            irt::g_alloc_fn = g_a;
            irt::g_free_fn  = g_b;

            irt::simulation sim;
            expect(sim.init(30u, 30u) == irt::status::success);
        }

        fmt::print("memory: {}/{}\n", g_a.allocation_number, g_b.free_number);
        expect(g_a.allocation_size > 0);
        expect(g_a.allocation_number == g_b.free_number);

        irt::g_alloc_fn.reset();
        irt::g_free_fn.reset();
    };

    "null_memory"_test = [] {
        irt::is_fatal_breakpoint = false;
        irt::g_alloc_fn          = null_alloc;
        irt::g_free_fn           = null_free;

        irt::simulation sim;
        expect(sim.init(30u, 30u) != irt::status::success);

        irt::is_fatal_breakpoint = true;
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
        expect(str_b.size() == 1024 * 8);

        irt::generate_random_file(
          ofs_t, gen, dist, 1024, irt::random_file_type::text);

        auto str_t = ofs_b.str();
        expect(str_t.size() > 1024 * 2);
    };

    "binary-memory-io"_test = [] {
        irt::memory f(256, irt::open_mode::write);

        assert(f.data.ssize() == 256);
        assert(f.data.capacity() == 256);
        assert(f.tell() == 0);
        assert(f.length() == 256);

        irt::u8  a = 0xfe;
        irt::u16 b = 0xfedc;
        irt::u32 c = 0xfedcba98;
        irt::u64 d = 0xfedcba9876543210;

        f.write(a);
        f.write(b);
        f.write(c);
        f.write(d);

        assert(f.data.ssize() == 256);
        assert(f.data.capacity() == 256);
        assert(f.tell() == 8 + 4 + 2 + 1);
        assert(f.length() == 256);

        irt::u8  a_w = f.data[0];
        irt::u16 b_w = *(reinterpret_cast<irt::u16*>(&f.data[1]));
        irt::u32 c_w = *(reinterpret_cast<irt::u32*>(&f.data[3]));
        irt::u64 d_w = *(reinterpret_cast<irt::u64*>(&f.data[7]));

        assert(a == a_w);
        assert(b == b_w);
        assert(c == c_w);
        assert(d == d_w);

        f.rewind();

        assert(f.tell() == 0);
    };

    "binary-file-io"_test = [] {
        std::error_code ec;
        auto            file_path = std::filesystem::temp_directory_path(ec);

        file_path /= "irritator.txt";

        {
            irt::file f(file_path.string().c_str(), irt::open_mode::write);
            assert(f.length() == 0);

            irt::u8  a = 0xfe;
            irt::u16 b = 0xfedc;
            irt::u32 c = 0xfedcba98;
            irt::u64 d = 0xfedcba9876543210;

            f.write(a);
            f.write(b);
            f.write(c);
            f.write(d);

            assert(f.tell() == 15);
        }

        {
            irt::file f(file_path.string().c_str(), irt::open_mode::read);
            assert(f.length() == 15);

            irt::u8  a   = 0xfe;
            irt::u16 b   = 0xfedc;
            irt::u32 c   = 0xfedcba98;
            irt::u64 d   = 0xfedcba9876543210;
            irt::u8  a_w = 0;
            irt::u16 b_w = 0;
            irt::u32 c_w = 0;
            irt::u64 d_w = 0;

            f.read(a_w);
            f.read(b_w);
            f.read(c_w);
            f.read(d_w);

            assert(a == a_w);
            assert(b == b_w);
            assert(c == c_w);
            assert(d == d_w);

            assert(f.tell() == 15);

            f.rewind();

            assert(f.tell() == 0);
        }

        std::filesystem::remove(file_path, ec);
    };
}
