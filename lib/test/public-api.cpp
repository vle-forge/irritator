// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <cstdint>
#include <irritator/archiver.hpp>
#include <irritator/core.hpp>
#include <irritator/examples.hpp>
#include <irritator/ext.hpp>
#include <irritator/file.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>
#include <irritator/observation.hpp>

#include <fmt/format.h>

#include <filesystem>
#include <fstream>
#include <numeric>
#include <random>
#include <sstream>

#include <cstdio>

#include <boost/ut.hpp>

irt::static_memory_resource<256 * 256 * 16> mem;

struct file_output {
    using value_type = irt::observation;

    std::FILE*            os = nullptr;
    irt::observer&        obs;
    irt::interpolate_type type        = irt::interpolate_type::none;
    irt::real             time_step   = 1e-1;
    bool                  interpolate = true;

    file_output(irt::observer& obs_, const char* filename) noexcept
      : os{ nullptr }
      , obs{ obs_ }
      , type{ get_interpolate_type(obs.type) }
    {
        if (os = std::fopen(filename, "w"); os)
            fmt::print(os, "t,v\n");
    }

    void push_back(const irt::observation& vec) noexcept
    {
        fmt::print(os, "{},{}\n", vec.x, vec.y);
    }

    void write() noexcept
    {
        auto it = std::back_insert_iterator<file_output>(*this);

        if (interpolate) {
            if (obs.buffer.ssize() >= 2)
                write_interpolate_data(obs, it, time_step);
        } else {
            write_raw_data(obs, it);
        }
    }

    void flush() noexcept
    {
        auto it = std::back_insert_iterator<file_output>(*this);

        if (interpolate) {
            flush_interpolate_data(obs, it, time_step);
        } else {
            flush_raw_data(obs, it);
        }

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

static void empty_fun(irt::model_id /*id*/) noexcept {}

static irt::status run_simulation(irt::simulation& sim, const double duration_p)
{
    using namespace boost::ut;

    sim.t              = 0;
    irt::time duration = static_cast<irt::time>(duration_p);

    expect(!!sim.initialize());

    do {
        expect(!!sim.run());
    } while (sim.t < duration);

    return irt::success();
}

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

struct leaf_tester {
    struct a_error {};
    bool make_error = false;

    leaf_tester(bool error)
      : make_error(error)
    {}

    irt::result<int> make() noexcept
    {
        if (make_error)
            return boost::leaf::new_error(a_error{});

        return 1;
    }

    static auto build_error_handlers(int& num) noexcept
    {
        return std::make_tuple([&](leaf_tester::a_error) { num = 1; });
    }
};

struct leaf_tester_2 {
    struct a_error {};
    bool make_error = false;

    leaf_tester_2(bool error)
      : make_error(error)
    {}

    irt::result<int> make() noexcept
    {
        if (make_error)
            return boost::leaf::new_error(a_error{});

        return 2;
    }

    static auto build_error_handlers(int& num) noexcept
    {
        return std::make_tuple([&](leaf_tester_2::a_error) { num = 2; });
    }
};

static auto build_error_handler(int& num) noexcept
{
    return std::make_tuple([&] { num = -1; });
}

int main()
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::on_error_callback = irt::debug::breakpoint;
#endif

    using namespace boost::ut;

    "tester_1"_test = [] {
        leaf_tester   t(true);
        leaf_tester_2 t2(false);
        int           error_sum = 0;

        irt::attempt_all(
          [&]() -> irt::status {
              irt_check(t.make());
              irt_check(t2.make());
              return irt::success();
          },

          std::tuple_cat(t.build_error_handlers(error_sum),
                         t2.build_error_handlers(error_sum),
                         build_error_handler(error_sum)));

        expect(eq(error_sum, 1));
    };

    "tester_1"_test = [] {
        leaf_tester   t(false);
        leaf_tester_2 t2(true);
        int           error_sum = 0;

        irt::attempt_all(
          [&]() -> irt::status {
              irt_check(t.make());
              irt_check(t2.make());
              return irt::success();
          },

          std::tuple_cat(t.build_error_handlers(error_sum),
                         t2.build_error_handlers(error_sum),
                         build_error_handler(error_sum)));

        expect(eq(error_sum, 2));
    };

    "tester_off"_test = [] {
        leaf_tester   t(false);
        leaf_tester_2 t2(false);
        int           error_sum = 0;

        irt::attempt_all(
          [&]() -> irt::status {
              irt_check(t.make());
              irt_check(t2.make());
              return irt::success();
          },

          std::tuple_cat(t.build_error_handlers(error_sum),
                         t2.build_error_handlers(error_sum),
                         build_error_handler(error_sum)));

        expect(eq(error_sum, 0));
    };

    "tester_unknown"_test = [] {
        leaf_tester   t(false);
        leaf_tester_2 t2(false);
        int           error_sum = 0;

        irt::attempt_all(
          [&]() -> irt::status {
              irt_check(t.make());
              irt_check(t2.make());

              return boost::leaf::new_error(123456789);
          },

          std::tuple_cat(t.build_error_handlers(error_sum),
                         t2.build_error_handlers(error_sum),
                         build_error_handler(error_sum)));

        expect(eq(error_sum, -1));
    };

    "small-function-1"_test = [] {
        double o = 15.0, p = 2.0, uu = 10.0;

        auto lambda_1 =
          +[](double x, double y) noexcept -> double { return x + y; };

        auto lambda_2 = [](double x, double z) noexcept -> double {
            return x * z;
        };

        auto lambda_3 = [o, p, uu](double x, double z) noexcept -> double {
            return o * p * uu + x + z;
        };

        auto lambda_4 = [&](double x, double z) noexcept -> double {
            return o * p * uu + x + z;
        };

        {
            irt::small_function<sizeof(lambda_1), double(double, double)> f1;

            f1 = lambda_1;
            expect(eq(f1(1.0, 2.0), 3.0));
        }

        {
            irt::small_function<sizeof(lambda_2), double(double, double)> f1;

            f1 = lambda_2;
            expect(eq(f1(3.0, 2.0), 6.0));
        }

        {
            irt::small_function<sizeof(lambda_3), double(double, double)> f1;

            f1 = lambda_3;
            expect(eq(f1(1.0, 1.0), o * p * uu + 2.0));
        }

        {
            irt::small_function<sizeof(lambda_4), double(double, double)> f1;

            f1 = lambda_4;
            expect(eq(f1(2.0, 2.0), o * p * uu + 4.0));
        }

        irt::small_function<sizeof(double) * 3, double(double, double)> f1;

        f1 = +[](double x, double y) noexcept -> double { return x + y; };
        expect(eq(f1(1.0, 2.0), 3.0));

        f1 = [](double x, double z) noexcept -> double { return x * z; };
        expect(eq(f1(3.0, 2.0), 6.0));

        f1 = [o, p, uu](double x, double z) noexcept -> double {
            return o * p * uu + x + z;
        };
        expect(eq(f1(1.0, 1.0), o * p * uu + 2.0));

        auto array = std::make_unique<double[]>(100);
        f1         = [&array](double x, double y) noexcept -> double {
            for (double i = 0; i != 100.0; i += 1.0)
                array.get()[static_cast<int>(i)] = i;

            return x + y + array.get()[99];
        };

        // small_function need to be improved for lambda move capture.
        //
        // auto array2 = std::make_unique<double[]>(100);
        // f1 = [cap = std::move(array2)](double x, double y) noexcept -> double
        // {
        //    for (double i = 0; i != 100.0; i += 1.0)
        //        cap.get()[static_cast<int>(i)] = i;
        //    return x + y + cap.get()[99];
        //};
    };

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
        fmt::print("qss1_cross      {}\n", sizeof(irt::qss1_cross));
        fmt::print("qss1_power      {}\n", sizeof(irt::qss1_power));
        fmt::print("qss1_square     {}\n", sizeof(irt::qss1_square));
        fmt::print("qss1_sum_2      {}\n", sizeof(irt::qss1_sum_2));
        fmt::print("qss1_sum_3      {}\n", sizeof(irt::qss1_sum_3));
        fmt::print("qss1_sum_4      {}\n", sizeof(irt::qss1_sum_4));
        fmt::print("qss1_wsum_2     {}\n", sizeof(irt::qss1_wsum_2));
        fmt::print("qss1_wsum_3     {}\n", sizeof(irt::qss1_wsum_3));
        fmt::print("qss1_wsum_4     {}\n", sizeof(irt::qss1_wsum_4));
        fmt::print("qss2_integrator {}\n", sizeof(irt::qss2_integrator));
        fmt::print("qss2_multiplier {}\n", sizeof(irt::qss2_multiplier));
        fmt::print("qss2_cross      {}\n", sizeof(irt::qss2_cross));
        fmt::print("qss2_power      {}\n", sizeof(irt::qss2_power));
        fmt::print("qss2_square     {}\n", sizeof(irt::qss2_square));
        fmt::print("qss2_sum_2      {}\n", sizeof(irt::qss2_sum_2));
        fmt::print("qss2_sum_3      {}\n", sizeof(irt::qss2_sum_3));
        fmt::print("qss2_sum_4      {}\n", sizeof(irt::qss2_sum_4));
        fmt::print("qss2_wsum_2     {}\n", sizeof(irt::qss2_wsum_2));
        fmt::print("qss2_wsum_3     {}\n", sizeof(irt::qss2_wsum_3));
        fmt::print("qss2_wsum_4     {}\n", sizeof(irt::qss2_wsum_4));
        fmt::print("qss3_integrator {}\n", sizeof(irt::qss3_integrator));
        fmt::print("qss3_multiplier {}\n", sizeof(irt::qss3_multiplier));
        fmt::print("qss3_power      {}\n", sizeof(irt::qss3_power));
        fmt::print("qss3_square     {}\n", sizeof(irt::qss3_square));
        fmt::print("qss3_cross      {}\n", sizeof(irt::qss3_cross));
        fmt::print("qss3_sum_2      {}\n", sizeof(irt::qss3_sum_2));
        fmt::print("qss3_sum_3      {}\n", sizeof(irt::qss3_sum_3));
        fmt::print("qss3_sum_4      {}\n", sizeof(irt::qss3_sum_4));
        fmt::print("qss3_wsum_2     {}\n", sizeof(irt::qss3_wsum_2));
        fmt::print("qss3_wsum_3     {}\n", sizeof(irt::qss3_wsum_3));
        fmt::print("qss3_wsum_4     {}\n", sizeof(irt::qss3_wsum_4));
        fmt::print("counter         {}\n", sizeof(irt::counter));
        fmt::print("queue           {}\n", sizeof(irt::queue));
        fmt::print("dynamic_queue   {}\n", sizeof(irt::dynamic_queue));
        fmt::print("priority_queue  {}\n", sizeof(irt::priority_queue));
        fmt::print("generator       {}\n", sizeof(irt::generator));
        fmt::print("constant        {}\n", sizeof(irt::constant));
        fmt::print("time_func       {}\n", sizeof(irt::time_func));
        fmt::print("accumulator     {}\n", sizeof(irt::accumulator_2));
        fmt::print("hsm_wrapper     {}\n", sizeof(irt::hsm_wrapper));
        fmt::print("model           {}\n", sizeof(irt::model));
        fmt::print("message         {}\n", sizeof(irt::message));
        fmt::print("node            {}\n", sizeof(irt::node));
        fmt::print("---------------------\n", sizeof(irt::node));
        fmt::print("dynamic number:   {}\n", irt::dynamics_type_size());
        fmt::print("max dynamic size: {}\n", irt::max_size_in_bytes());
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

    "vector-iterator-valid"_test = [] {
        irt::vector<int> vec(4);

        expect(eq(vec.ssize(), 0));
        expect(eq(vec.capacity(), 4));

        vec.emplace_back(INT32_MAX);
        irt::vector<int>::iterator it = vec.begin();

        vec.reserve(512);
        if (vec.is_iterator_valid(it))
            expect(eq(it, vec.begin()));

        expect(eq(vec.front(), INT32_MAX));

        vec.emplace_back(INT32_MIN);
        expect(eq(vec.ssize(), 2));
        expect(eq(vec.capacity(), 512));

        vec.emplace_back(INT32_MAX);
        expect(eq(vec.ssize(), 3));
        expect(eq(vec.capacity(), 512));

        vec.emplace_back(INT32_MIN);
        expect(eq(vec.ssize(), 4));
        expect(eq(vec.capacity(), 512));

        it = vec.begin() + 2;

        expect(eq(*it, INT32_MAX));
        expect(eq(vec.index_from_ptr(it), 2));
    };

    "vector-erase"_test = [] {
        struct t_1 {
            int x = 0;

            t_1() noexcept = default;

            t_1(int x_) noexcept
              : x(x_)
            {}
        };

        irt::vector<t_1> v_1(10, 10);
        std::iota(v_1.begin(), v_1.end(), 0);

        expect(v_1.is_iterator_valid(v_1.begin()));

        expect(eq(v_1[0].x, 0));
        expect(eq(v_1[9].x, 9));
        v_1.erase(v_1.begin());
        expect(v_1.is_iterator_valid(v_1.begin()));

        expect(eq(v_1[0].x, 1));
        expect(eq(v_1[8].x, 9));
        expect(eq(v_1.ssize(), 9));
        v_1.erase(v_1.begin(), v_1.begin() + 5);
        expect(v_1.is_iterator_valid(v_1.begin()));

        expect(eq(v_1[0].x, 6));
        expect(eq(v_1[3].x, 9));
        expect(eq(v_1.ssize(), 4));
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
        struct toto {
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

    "vector"_test = [] {
        struct position {
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
        struct position {
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
        irt::ring_buffer<int> ring{ 10 };

        for (int i = 0; i < 9; ++i) {
            auto is_success = ring.emplace_enqueue(i);
            expect(is_success == true);
        }

        {
            auto is_success = ring.emplace_enqueue(9);
            expect(is_success == false);
        }

        expect(ring.data()[0] == 0);
        expect(ring.data()[1] == 1);
        expect(ring.data()[2] == 2);
        expect(ring.data()[3] == 3);
        expect(ring.data()[4] == 4);
        expect(ring.data()[5] == 5);
        expect(ring.data()[6] == 6);
        expect(ring.data()[7] == 7);
        expect(ring.data()[8] == 8);
        expect(ring.data()[0] == 0);

        for (int i = 10; i < 15; ++i)
            ring.force_emplace_enqueue(i);

        expect(ring.data()[0] == 11);
        expect(ring.data()[1] == 12);
        expect(ring.data()[2] == 13);
        expect(ring.data()[3] == 14);
        expect(ring.data()[4] == 4);
        expect(ring.data()[5] == 5);
        expect(ring.data()[6] == 6);
        expect(ring.data()[7] == 7);
        expect(ring.data()[8] == 8);
        expect(ring.data()[9] == 10);
    };

    "ring-buffer-front-back-access"_test = [] {
        irt::ring_buffer<int> ring(4);

        expect(ring.push_head(0) == true);
        expect(ring.push_head(-1) == true);
        expect(ring.push_head(-2) == true);
        expect(ring.push_head(-3) == false);
        expect(ring.push_head(-4) == false);

        ring.pop_tail();

        expect(ring.ssize() == 2);
        expect(ring.front() == -2);
        expect(ring.back() == -1);

        expect(ring.push_tail(1) == true);

        expect(ring.front() == -2);
        expect(ring.back() == 1);
    };

    "data_array_api"_test = [] {
        struct position {
            position() = default;
            constexpr position(float x_)
              : x(x_)
            {}

            float x;
        };

        enum class position32_id : irt::u32;
        enum class position64_id : irt::u64;

        irt::data_array<position, position32_id> small_array;
        irt::data_array<position, position64_id> array;

        expect(small_array.max_size() == 0);
        expect(small_array.max_used() == 0);
        expect(small_array.capacity() == 0);
        expect(small_array.next_key() == 1);
        expect(small_array.is_free_list_empty());

        {
            fmt::print("              u-id    idx     id    val   \n");
            fmt::print(
              "small-array {:>6} {:>6} {:>6} {:>6}\n",
              sizeof(
                irt::data_array<position, position32_id>::underlying_id_type),
              sizeof(irt::data_array<position, position32_id>::index_type),
              sizeof(irt::data_array<position, position32_id>::identifier_type),
              sizeof(irt::data_array<position, position32_id>::value_type));

            fmt::print(
              "      array {:>6} {:>6} {:>6} {:>6}\n",
              sizeof(
                irt::data_array<position, position64_id>::underlying_id_type),
              sizeof(irt::data_array<position, position64_id>::index_type),
              sizeof(irt::data_array<position, position64_id>::identifier_type),
              sizeof(irt::data_array<position, position64_id>::value_type));
        }

        small_array.reserve(3);
        expect(small_array.can_alloc(3));
        expect(small_array.max_size() == 0);
        expect(small_array.max_used() == 0);
        expect(small_array.capacity() == 3);
        expect(small_array.next_key() == 1);
        expect(small_array.is_free_list_empty());

        expect(array.max_size() == 0);
        expect(array.max_used() == 0);
        expect(array.capacity() == 0);
        expect(array.next_key() == 1);
        expect(array.is_free_list_empty());

        array.reserve(3);
        expect(array.can_alloc(3));

        expect(array.max_size() == 0);
        expect(array.max_used() == 0);
        expect(array.capacity() == 3);
        expect(array.next_key() == 1);
        expect(array.is_free_list_empty());

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

        array.reserve(3);
        expect(array.can_alloc(3));

        {
            auto& d1 = array.alloc(1.f);
            auto& d2 = array.alloc(2.f);
            auto& d3 = array.alloc(3.f);

            expect(check_data_array_loop(array));

            expect(array.max_size() == 3);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 4);
            expect(array.is_free_list_empty());

            array.free(d1);

            expect(check_data_array_loop(array));

            expect(array.max_size() == 2);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 4);
            expect(!array.is_free_list_empty());

            array.free(d2);

            expect(check_data_array_loop(array));

            expect(array.max_size() == 1);
            expect(array.max_used() == 3);
            expect(array.capacity() == 3);
            expect(array.next_key() == 4);
            expect(!array.is_free_list_empty());

            array.free(d3);

            expect(check_data_array_loop(array));

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

            expect(check_data_array_loop(array));
        }
    };

    "id-data-array"_test = [] {
        struct pos3d {
            pos3d() = default;

            pos3d(float x_, float y_, float z_)
              : x(x_)
              , y(y_)
              , z(z_)
            {}

            float x, y, z;
        };

        struct color {
            std::uint32_t rgba;
        };

        using name = irt::small_string<15>;

        enum class ex1_id : uint32_t;

        irt::id_data_array<ex1_id, irt::default_allocator, pos3d, color, name>
          d;
        d.reserve(1024);
        expect(ge(d.capacity(), 1024u));
        expect(fatal(d.can_alloc(1)));

        const auto id =
          d.alloc([](const auto /*id*/, auto& p, auto& c, auto& n) noexcept {
              p = pos3d(0.f, 0.f, 0.f);
              c = color{ 123u };
              n = "HelloWorld!";
          });

        expect(eq(d.ssize(), 1));

        const auto idx = irt::get_index(id);
        expect(eq(idx, 0));

        d.for_each([](const auto /*id*/,
                      const auto& p,
                      const auto& c,
                      const auto& n) noexcept {
            expect(eq(p.x, 0.f));
            expect(eq(p.x, 0.f));
            expect(eq(p.x, 0.f));
            expect(eq(123u, c.rgba));
            expect(n.sv() == "HelloWorld!");
        });

        d.free(id);
        expect(eq(d.ssize(), 0));

        const auto id1 =
          d.alloc([](const auto /*id*/, auto& p, auto& c, auto& n) noexcept {
              p = pos3d(0.f, 0.f, 0.f);
              c = color{ 123u };
              n = "HelloWorld!";
          });

        const auto id2 =
          d.alloc([](const auto /*id*/, auto& p, auto& c, auto& n) noexcept {
              p = pos3d(0.f, 0.f, 0.f);
              c = color{ 123u };
              n = "HelloWorld!";
          });

        const auto idx1 = irt::get_index(id1);
        expect(eq(idx1, 0));
        const auto idx2 = irt::get_index(id2);
        expect(eq(idx2, 1));
        expect(eq(d.ssize(), 2));

        d.for_each([](const auto /*id*/,
                      const auto& p,
                      const auto& c,
                      const auto& n) noexcept {
            expect(eq(p.x, 0.f));
            expect(eq(p.x, 0.f));
            expect(eq(p.x, 0.f));
            expect(eq(123u, c.rgba));
            expect(n.sv() == "HelloWorld!");
        });

        d.clear();
        expect(eq(d.ssize(), 0));
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
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble[3] == 0.0_r);
        }

        {
            irt::observation_message vdouble({ 1.0_r });
            assert(vdouble[0] == 1.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble[3] == 0.0_r);
        }

        {
            irt::observation_message vdouble({ 0.0_r, 1.0_r });
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 1.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble[3] == 0.0_r);
        }

        {
            irt::observation_message vdouble({ 0.0_r, 0.0_r, 1.0_r });
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 1.0_r);
            assert(vdouble[3] == 0.0_r);
        }

        {
            irt::observation_message vdouble({ 0.0_r, 0.0_r, 0.0_r, 1.0_r });
            assert(vdouble[0] == 0.0_r);
            assert(vdouble[1] == 0.0_r);
            assert(vdouble[2] == 0.0_r);
            assert(vdouble[3] == 1.0_r);
        }
    };

    "heap_order"_test = [] {
        irt::heap h;
        expect(h.reserve(4u));

        auto i1 = h.insert(0.0, irt::model_id{ 0 });
        auto i2 = h.insert(1.0, irt::model_id{ 1 });
        auto i3 = h.insert(-1.0, irt::model_id{ 2 });
        auto i4 = h.insert(2.0, irt::model_id{ 3 });
        expect(h.full());

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
        irt::heap h;
        expect(h.reserve(4u));

        auto i1 = h.insert(0, irt::model_id{ 0 });
        auto i2 = h.insert(1, irt::model_id{ 1 });
        auto i3 = h.insert(-1, irt::model_id{ 2 });
        auto i4 = h.insert(2, irt::model_id{ 3 });

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

        irt::heap h;
        expect(h.reserve(256u));

        for (int t = 0; t < 100; ++t)
            h.insert(irt::to_real(t), static_cast<irt::model_id>(t));

        expect(h.size() == 100_ul);

        h.insert(50, irt::model_id{ 502 });
        h.insert(50, irt::model_id{ 503 });
        h.insert(50, irt::model_id{ 504 });

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

    "hierarchy-simple"_test = [] {
        struct data_type {
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
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

    "input-output"_test = [] {
        irt::vector<char> out;
        irt::cache_rw     cache;

        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            sim.alloc<irt::qss1_integrator>();
            sim.alloc<irt::qss1_multiplier>();
            sim.alloc<irt::qss1_cross>();
            sim.alloc<irt::qss1_filter>();
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
            sim.alloc<irt::qss2_filter>();
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
            sim.alloc<irt::qss3_cross>();
            sim.alloc<irt::qss3_filter>();
            sim.alloc<irt::qss3_power>();
            sim.alloc<irt::qss3_square>();
            sim.alloc<irt::qss3_sum_2>();
            sim.alloc<irt::qss3_sum_3>();
            sim.alloc<irt::qss3_sum_4>();
            sim.alloc<irt::qss3_wsum_2>();
            sim.alloc<irt::qss3_wsum_3>();
            sim.alloc<irt::qss3_wsum_4>();
            sim.alloc<irt::counter>();
            sim.alloc<irt::queue>();
            sim.alloc<irt::dynamic_queue>();
            sim.alloc<irt::priority_queue>();
            sim.alloc<irt::generator>();
            sim.alloc<irt::constant>();
            sim.alloc<irt::time_func>();
            sim.alloc<irt::accumulator_2>();
            sim.alloc<irt::logical_and_2>();
            sim.alloc<irt::logical_and_3>();
            sim.alloc<irt::logical_or_2>();
            sim.alloc<irt::logical_or_3>();
            sim.alloc<irt::logical_invert>();
            sim.alloc<irt::hsm_wrapper>();

            irt::json_archiver j;
            expect(!!j.simulation_save(
              sim,
              cache,
              out,
              irt::json_archiver::print_option::indent_2_one_line_array));

            expect(out.size() > 0);
        }

        try {
            std::error_code ec;
            auto            temp = std::filesystem::temp_directory_path(ec);
            temp /= "unit-test.irt";

            if (std::ofstream ofs(temp); ofs.is_open()) {
                fmt::print("`{}`\n", std::string_view(out.data(), out.size()));
                ofs << std::string_view(out.data(), out.size()) << '\n';
            }
        } catch (...) {
        }

        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            auto in = std::span(out.data(), out.size());

            irt::json_archiver j;
            expect(!!j.simulation_load(sim, cache, in));
            expect(eq(sim.models.size(), 50u));
        }
    };

    "constant_simulation"_test = [] {
        irt::on_error_callback = irt::debug::breakpoint;
        fmt::print("constant_simulation\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

        expect(sim.can_alloc(3));

        auto& cnt = sim.alloc<irt::counter>();
        auto& c1  = sim.alloc<irt::constant>();
        auto& c2  = sim.alloc<irt::constant>();

        c1.default_value = 0.0;
        c2.default_value = 0.0;

        expect(!!sim.connect(c1, 0, cnt, 0));
        expect(!!sim.connect(c2, 0, cnt, 0));

        sim.t = irt::zero;
        expect(!!sim.initialize());

        do {
            expect(!!sim.run());
        } while (not irt::time_domain<irt::time>::is_infinity(sim.t));

        expect(eq(cnt.number, static_cast<irt::i64>(2)));
    };

    "cross_simulation"_test = [] {
        fmt::print("cross_simulation\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

        expect(sim.can_alloc(3));

        auto& cnt    = sim.alloc<irt::counter>();
        auto& cross1 = sim.alloc<irt::qss1_cross>();
        auto& c1     = sim.alloc<irt::constant>();

        c1.default_value         = 3.0;
        cross1.default_threshold = 0.0;

        expect(!!sim.connect(c1, 0, cross1, 0));
        expect(!!sim.connect(c1, 0, cross1, 1));
        expect(!!sim.connect(c1, 0, cross1, 2));
        expect(!!sim.connect(cross1, 0, cnt, 0));

        sim.t = 0.0;
        expect(!!sim.initialize());

        do {
            expect(!!sim.run());
        } while (!irt::time_domain<irt::time>::is_infinity(sim.t));

        expect(eq(cnt.number, static_cast<decltype(cnt.number)>(1)));
    };

    "hsm_automata"_test = [] {
        irt::hierarchical_state_machine            hsmw;
        irt::hierarchical_state_machine::execution exec;

        expect(!!hsmw.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsmw.set_state(1u, 0u));

        hsmw.states[1u].condition.type =
          irt::hierarchical_state_machine::condition_type::port;
        hsmw.states[1u].condition.set(3u, 7u);
        hsmw.states[1u].if_transition = 2u;

        expect(!!hsmw.set_state(2u, 0u));
        hsmw.states[2u].enter_action.type =
          irt::hsm_wrapper::hsm::action_type::output;
        hsmw.states[2u].enter_action.var1 =
          irt::hierarchical_state_machine::variable::port_0;
        hsmw.states[2u].enter_action.var2 =
          irt::hierarchical_state_machine::variable::constant_r;
        hsmw.states[2u].enter_action.constant.f = 1.f;

        expect(!!hsmw.start(exec));

        expect((int)exec.current_state == 1);
        exec.values = 0b00000011;

        expect(exec.outputs.ssize() == 0);

        const auto processed = hsmw.dispatch(
          irt::hierarchical_state_machine::event_type::input_changed, exec);
        expect(!!processed);
        expect(processed.value() == true);

        expect(exec.outputs.ssize() == 1);
    };

    "hsm_automata_timer"_test = [] {
        irt::hierarchical_state_machine            hsmw;
        irt::hierarchical_state_machine::execution exec;

        expect(!!hsmw.set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsmw.set_state(1u, 0u));

        hsmw.states[1u].condition.type =
          irt::hierarchical_state_machine::condition_type::port;
        hsmw.states[1u].condition.set(3u, 7u);
        hsmw.states[1u].if_transition = 2u;

        expect(!!hsmw.set_state(2u, 0u));
        hsmw.states[2u].enter_action.type =
          irt::hsm_wrapper::hsm::action_type::affect;
        hsmw.states[2u].enter_action.var1 =
          irt::hierarchical_state_machine::variable::var_timer;
        hsmw.states[2u].enter_action.var2 =
          irt::hierarchical_state_machine::variable::constant_r;
        hsmw.states[2u].enter_action.constant.f = 1.f;
        hsmw.states[2u].condition.type =
          irt::hierarchical_state_machine::condition_type::sigma;
        hsmw.states[2u].if_transition = 3u;

        expect(!!hsmw.set_state(3u, 0u));
        hsmw.states[2u].enter_action.type =
          irt::hsm_wrapper::hsm::action_type::output;
        hsmw.states[2u].enter_action.var1 =
          irt::hierarchical_state_machine::variable::port_0;
        hsmw.states[2u].enter_action.var2 =
          irt::hierarchical_state_machine::variable::constant_r;
        hsmw.states[2u].enter_action.constant.f = 1.f;

        expect(!!hsmw.start(exec));

        expect((int)exec.current_state == 1);
        exec.values = 0b00000011;

        expect(exec.outputs.ssize() == 0);

        const auto processed = hsmw.dispatch(
          irt::hierarchical_state_machine::event_type::input_changed, exec);
        expect(!!processed);
        expect(processed.value() == true);
        expect((int)exec.current_state == 2);

        expect(exec.outputs.ssize() == 1);
    };

    "hsm_simulation"_test = [] {
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

        expect((sim.can_alloc(3)) >> fatal);
        expect((sim.hsms.can_alloc(1)) >> fatal);
        expect(sim.srcs.constant_sources.can_alloc(2u) >> fatal);
        auto& cst_value  = sim.srcs.constant_sources.alloc();
        cst_value.length = 10;
        cst_value.buffer = { 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0 };

        auto& cst_ta  = sim.srcs.constant_sources.alloc();
        cst_ta.length = 10;
        cst_ta.buffer = { 1., 1., 1., 1., 1., 1., 1., 1., 1., 1., 1. };

        auto& cst_1         = sim.alloc<irt::constant>();
        cst_1.default_value = 1.0;

        auto& cnt = sim.alloc<irt::counter>();

        auto& gen          = sim.alloc<irt::generator>();
        gen.default_offset = 0;
        gen.flags.set(irt::generator::option::ta_use_source);
        gen.flags.set(irt::generator::option::value_use_source);
        gen.default_source_value.id =
          irt::ordinal(sim.srcs.constant_sources.get_id(cst_value));
        gen.default_source_value.type = irt::source::source_type::constant;
        gen.default_source_ta.id =
          irt::ordinal(sim.srcs.constant_sources.get_id(cst_ta));
        gen.default_source_ta.type = irt::source::source_type::constant;

        expect(sim.hsms.can_alloc());
        expect(sim.models.can_alloc());

        auto& hsm  = sim.alloc<irt::hsm_wrapper>();
        auto* hsmw = sim.hsms.try_to_get(hsm.id);
        expect((hsmw != nullptr) >> fatal);

        expect(!!hsmw->set_state(
          0u, irt::hierarchical_state_machine::invalid_state_id, 1u));

        expect(!!hsmw->set_state(1u, 0u));
        hsmw->states[1u].condition.type =
          irt::hierarchical_state_machine::condition_type::port;
        hsmw->states[1u].condition.set(0b0011u, 0b0011u);
        hsmw->states[1u].if_transition = 2u;

        expect(!!hsmw->set_state(2u, 0u));
        hsmw->states[2u].enter_action.type =
          irt::hsm_wrapper::hsm::action_type::output;
        hsmw->states[2u].enter_action.var1 =
          irt::hierarchical_state_machine::variable::port_0;
        hsmw->states[2u].enter_action.var2 =
          irt::hierarchical_state_machine::variable::constant_r;
        hsmw->states[2u].enter_action.constant.f = 1.0f;

        expect(!!sim.connect(gen, 0, hsm, 0));
        expect(!!sim.connect(gen, 0, hsm, 1));
        expect(!!sim.connect(hsm, 0, cnt, 0));

        sim.t = 0.0;
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;

        do {
            st = sim.run();
            expect(!!st);
        } while (sim.t < 10);

        expect(cnt.number == static_cast<irt::i64>(1));
    };

    "generator_counter_simluation"_test = [] {
        fmt::print("generator_counter_simluation\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        gen.default_offset = 0;
        gen.flags.set(irt::generator::option::ta_use_source);
        gen.flags.set(irt::generator::option::value_use_source);
        gen.flags.set(irt::generator::option::stop_on_error);
        gen.default_source_value.id =
          irt::ordinal(sim.srcs.constant_sources.get_id(cst_value));
        gen.default_source_value.type = irt::source::source_type::constant;
        gen.default_source_ta.id =
          irt::ordinal(sim.srcs.constant_sources.get_id(cst_ta));
        gen.default_source_ta.type = irt::source::source_type::constant;

        expect(!!sim.connect(gen, 0, cnt, 0));

        sim.t = 0.0;
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());

        irt::status st;

        do {
            st = sim.run();
            expect(!!st);
        } while (sim.t < 10.0);

        expect(eq(cnt.number, static_cast<irt::i64>(10)));
    };

    "boolean_simulation"_test = [] {
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        gen.default_source_value.id =
          irt::ordinal(sim.srcs.constant_sources.get_id(cst_value));
        gen.default_source_value.type = irt::source::source_type::constant;
        gen.default_source_ta.id =
          irt::ordinal(sim.srcs.constant_sources.get_id(cst_ta));
        gen.default_source_ta.type = irt::source::source_type::constant;

        expect(!!sim.connect(gen, 0, l_and, 0));
        expect(!!sim.connect(l_and, 0, l_or, 0));

        l_and.default_values[0] = false;
        l_and.default_values[1] = true;

        l_or.default_values[0] = false;
        l_or.default_values[1] = false;

        auto& obs = sim.observers.alloc();
        sim.observe(irt::get_model(l_and), obs);

        sim.t           = 0;
        irt::real value = 0.0;
        expect(!!sim.srcs.prepare());
        expect(!!sim.initialize());
        do {
            auto old_t = sim.t;
            expect(!!sim.run());

            if (old_t != sim.t) {
                expect(eq(obs.buffer.ssize(), 1)) << fatal;
                for (auto v : obs.buffer) {
                    expect(eq(v[0], old_t));
                    expect(eq(v[1], value));
                }
                value = value == 0. ? 1.0 : 0.;
                obs.buffer.clear();
            }
        } while (sim.t < 10.0);
    };

    "time_func"_test = [] {
        fmt::print("time_func\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

        const irt::real duration{ 30 };

        expect(sim.can_alloc(2));

        auto& time_fun = sim.alloc<irt::time_func>();
        auto& cnt      = sim.alloc<irt::counter>();

        time_fun.default_f     = &irt::square_time_function;
        time_fun.default_sigma = irt::to_real(0.1);

        expect(!!sim.connect(time_fun, 0, cnt, 0));

        irt::real c = 0;
        sim.t       = 0;
        expect(!!sim.initialize());
        do {
            expect(!!sim.run());
            expect(time_fun.value == sim.t * sim.t);
            c++;
        } while (sim.t < duration);

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
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

        expect(sim.can_alloc(2));

        auto& time_fun = sim.alloc<irt::time_func>();
        auto& cnt      = sim.alloc<irt::counter>();

        time_fun.default_f     = &irt::sin_time_function;
        time_fun.default_sigma = irt::real(0.1);

        expect(!!sim.connect(time_fun, 0, cnt, 0));

        sim.t                    = 0;
        const irt::real duration = 30;
        irt::real       c        = irt::zero;

        expect(!!sim.initialize());
        do {
            expect(!!sim.run() >> fatal);
            expect(time_fun.value == std::sin(irt::two * pi * f0 * sim.t));
            c++;
        } while (sim.t < duration);
        expect(c == (irt::real{ 2.0 } * duration / time_fun.default_sigma -
                     irt::real{ 1.0 }));
    };

    "lotka_volterra_simulation_qss1"_test = [] {
        fmt::print("lotka_volterra_simulation_qss1\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        expect(!!sim.connect(sum_a, 0, integrator_a, 0));
        expect(!!sim.connect(sum_b, 0, integrator_b, 0));

        expect(!!sim.connect(integrator_a, 0, sum_a, 0));
        expect(!!sim.connect(integrator_b, 0, sum_b, 0));

        expect(!!sim.connect(integrator_a, 0, product, 0));
        expect(!!sim.connect(integrator_b, 0, product, 1));

        expect(!!sim.connect(product, 0, sum_a, 1));
        expect(!!sim.connect(product, 0, sum_b, 1));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "lotka-volterra-qss1_a.csv");
        auto&       obs_b = sim.observers.alloc();
        file_output fo_b(obs_b, "lotka-volterra-qss1_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        sim.t = irt::real(0);

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run();
            expect(!!st);

            fo_a.write();
            fo_b.write();
        } while (sim.t < irt::real(15));

        fo_a.flush();
        fo_b.flush();
    };

    "lotka_volterra_simulation_qss2"_test = [] {
        fmt::print("lotka_volterra_simulation_qss2\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        expect(!!sim.connect(sum_a, 0, integrator_a, 0));
        expect(!!sim.connect(sum_b, 0, integrator_b, 0));

        expect(!!sim.connect(integrator_a, 0, sum_a, 0));
        expect(!!sim.connect(integrator_b, 0, sum_b, 0));

        expect(!!sim.connect(integrator_a, 0, product, 0));
        expect(!!sim.connect(integrator_b, 0, product, 1));

        expect(!!sim.connect(product, 0, sum_a, 1));
        expect(!!sim.connect(product, 0, sum_b, 1));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "lotka-volterra-qss2_a.csv");
        auto&       obs_b = sim.observers.alloc();
        file_output fo_b(obs_b, "lotka-volterra-qss2_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        sim.t = irt::real(0);

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run();
            expect(!!st);

            fo_a.write();
            fo_b.write();
        } while (sim.t < irt::real(15));

        fo_a.flush();
        fo_b.flush();
    };

    "lif_simulation_qss1"_test = [] {
        fmt::print("lif_simulation_qss1\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        expect(!!sim.connect(cross, 0, integrator, 1));
        expect(!!sim.connect(cross, 1, sum, 0));
        expect(!!sim.connect(integrator, 0, cross, 0));
        expect(!!sim.connect(integrator, 0, cross, 2));
        expect(!!sim.connect(constant_cross, 0, cross, 1));
        expect(!!sim.connect(constant, 0, sum, 1));
        expect(!!sim.connect(sum, 0, integrator, 0));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "lif-qss1.csv");
        expect(fo_a.os != nullptr);

        sim.observe(irt::get_model(integrator), obs_a);

        sim.t = 0.0;

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run();
            expect(!!st);

            fo_a.write();
        } while (sim.t < irt::time{ 100.0 });

        fo_a.flush();
    };

    "lif_simulation_qss2"_test = [] {
        fmt::print("lif_simulation_qss2\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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
        expect(!!sim.connect(cross, 0, integrator, 1));
        expect(!!sim.connect(cross, 1, sum, 0));

        expect(!!sim.connect(integrator, 0, cross, 0));
        expect(!!sim.connect(integrator, 0, cross, 2));
        expect(!!sim.connect(constant_cross, 0, cross, 1));
        expect(!!sim.connect(constant, 0, sum, 1));
        expect(!!sim.connect(sum, 0, integrator, 0));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "lif-qss2.csv");
        expect(fo_a.os != nullptr);

        sim.observe(irt::get_model(integrator), obs_a);

        sim.t = irt::time(0);

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            // printf("--------------------------------------------\n");
            auto st = sim.run();
            expect(!!st);

            fo_a.write();
        } while (sim.t < irt::time(100));

        fo_a.flush();
    };

    "izhikevich_simulation_qss1"_test = [] {
        fmt::print("izhikevich_simulation_qss1\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        expect(!!sim.connect(integrator_a, 0, cross, 0));
        expect(!!sim.connect(constant2, 0, cross, 1));
        expect(!!sim.connect(integrator_a, 0, cross, 2));

        expect(!!sim.connect(cross, 1, product, 0));
        expect(!!sim.connect(cross, 1, product, 1));
        expect(!!sim.connect(product, 0, sum_c, 0));
        expect(!!sim.connect(cross, 1, sum_c, 1));
        expect(!!sim.connect(cross, 1, sum_b, 1));

        expect(!!sim.connect(constant, 0, sum_c, 2));
        expect(!!sim.connect(constant3, 0, sum_c, 3));

        expect(!!sim.connect(sum_c, 0, sum_a, 0));
        // expect(sim.connect(integrator_b, 0, sum_a, 1) ==
        // irt::status::success);
        expect(!!sim.connect(cross2, 1, sum_a, 1));
        expect(!!sim.connect(sum_a, 0, integrator_a, 0));
        expect(!!sim.connect(cross, 0, integrator_a, 1));

        expect(!!sim.connect(cross2, 1, sum_b, 0));
        expect(!!sim.connect(sum_b, 0, integrator_b, 0));

        expect(!!sim.connect(cross2, 0, integrator_b, 1));
        expect(!!sim.connect(integrator_a, 0, cross2, 0));
        expect(!!sim.connect(integrator_b, 0, cross2, 2));
        expect(!!sim.connect(sum_d, 0, cross2, 1));
        expect(!!sim.connect(integrator_b, 0, sum_d, 0));
        expect(!!sim.connect(constant, 0, sum_d, 1));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "izhikevitch-qss1_a.csv");
        expect(fo_a.os != nullptr);

        auto&       obs_b = sim.observers.alloc();
        file_output fo_b(obs_b, "izhikevitch-qss1_b.csv");
        expect(fo_b.os != nullptr);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        sim.t = irt::real(0);

        expect(!!sim.initialize());
        expect((sim.sched.size() == 12_ul) >> fatal);

        do {
            irt::status st = sim.run();
            expect(!!st);

            fo_a.write();
            fo_b.write();
        } while (sim.t < irt::time(140));

        fo_a.flush();
        fo_b.flush();
    };

    "izhikevich_simulation_qss2"_test = [] {
        fmt::print("izhikevich_simulation_qss2\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        expect(!!sim.connect(integrator_a, 0, cross, 0));
        expect(!!sim.connect(constant2, 0, cross, 1));
        expect(!!sim.connect(integrator_a, 0, cross, 2));

        expect(!!sim.connect(cross, 1, product, 0));
        expect(!!sim.connect(cross, 1, product, 1));
        expect(!!sim.connect(product, 0, sum_c, 0));
        expect(!!sim.connect(cross, 1, sum_c, 1));
        expect(!!sim.connect(cross, 1, sum_b, 1));

        expect(!!sim.connect(constant, 0, sum_c, 2));
        expect(!!sim.connect(constant3, 0, sum_c, 3));

        expect(!!sim.connect(sum_c, 0, sum_a, 0));
        // expect(sim.connect(integrator_b, 0, sum_a, 1) ==
        // irt::status::success);
        expect(!!sim.connect(cross2, 1, sum_a, 1));
        expect(!!sim.connect(sum_a, 0, integrator_a, 0));
        expect(!!sim.connect(cross, 0, integrator_a, 1));

        expect(!!sim.connect(cross2, 1, sum_b, 0));
        expect(!!sim.connect(sum_b, 0, integrator_b, 0));

        expect(!!sim.connect(cross2, 0, integrator_b, 1));
        expect(!!sim.connect(integrator_a, 0, cross2, 0));
        expect(!!sim.connect(integrator_b, 0, cross2, 2));
        expect(!!sim.connect(sum_d, 0, cross2, 1));
        expect(!!sim.connect(integrator_b, 0, sum_d, 0));
        expect(!!sim.connect(constant, 0, sum_d, 1));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "izhikevitch-qss2_a.csv");
        expect(fo_a.os != nullptr);

        auto&       obs_b = sim.observers.alloc();
        file_output fo_b(obs_b, "izhikevitch-qss2_b.csv");
        expect(fo_b.os != nullptr);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        sim.t = irt::zero;

        expect(!!sim.initialize());
        expect((sim.sched.size() == 12_ul) >> fatal);

        do {
            irt::status st = sim.run();
            expect(!!st);

            fo_a.write();
            fo_b.write();
        } while (sim.t < irt::time(140));

        fo_a.flush();
        fo_b.flush();
    };

    "lotka_volterra_simulation_qss3"_test = [] {
        fmt::print("lotka_volterra_simulation_qss3\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        expect(!!sim.connect(sum_a, 0, integrator_a, 0));
        expect(!!sim.connect(sum_b, 0, integrator_b, 0));

        expect(!!sim.connect(integrator_a, 0, sum_a, 0));
        expect(!!sim.connect(integrator_b, 0, sum_b, 0));

        expect(!!sim.connect(integrator_a, 0, product, 0));
        expect(!!sim.connect(integrator_b, 0, product, 1));

        expect(!!sim.connect(product, 0, sum_a, 1));
        expect(!!sim.connect(product, 0, sum_b, 1));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "lotka-volterra-qss3_a.csv");
        expect(fo_a.os != nullptr);
        auto&       obs_b = sim.observers.alloc();
        file_output fo_b(obs_b, "lotka-volterra-qss3_b.csv");
        expect(fo_b.os != nullptr);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        sim.t = irt::zero;

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run();
            expect(!!st);

            fo_a.write();
            fo_b.write();
        } while (sim.t < irt::time(15));

        fo_a.flush();
        fo_b.flush();
    };

    "lif_simulation_qss3"_test = [] {
        fmt::print("lif_simulation_qss3\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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
        expect(!!sim.connect(cross, 0, integrator, 1));
        expect(!!sim.connect(cross, 1, sum, 0));

        expect(!!sim.connect(integrator, 0, cross, 0));
        expect(!!sim.connect(integrator, 0, cross, 2));
        expect(!!sim.connect(constant_cross, 0, cross, 1));
        expect(!!sim.connect(constant, 0, sum, 1));
        expect(!!sim.connect(sum, 0, integrator, 0));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "lif-qss3.csv");
        expect(fo_a.os != nullptr);

        sim.observe(irt::get_model(integrator), obs_a);

        sim.t = irt::zero;

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            // printf("--------------------------------------------\n");
            auto st = sim.run();
            expect(!!st);
            fo_a.write();
        } while (sim.t < irt::time(100));

        fo_a.flush();
    };

    "izhikevich_simulation_qss3"_test = [] {
        fmt::print("izhikevich_simulation_qss3\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        expect(!!sim.connect(integrator_a, 0, cross, 0));
        expect(!!sim.connect(constant2, 0, cross, 1));
        expect(!!sim.connect(integrator_a, 0, cross, 2));

        expect(!!sim.connect(cross, 1, product, 0));
        expect(!!sim.connect(cross, 1, product, 1));
        expect(!!sim.connect(product, 0, sum_c, 0));
        expect(!!sim.connect(cross, 1, sum_c, 1));
        expect(!!sim.connect(cross, 1, sum_b, 1));

        expect(!!sim.connect(constant, 0, sum_c, 2));
        expect(!!sim.connect(constant3, 0, sum_c, 3));

        expect(!!sim.connect(sum_c, 0, sum_a, 0));
        // expect(sim.connect(integrator_b, 0, sum_a, 1) ==
        // irt::status::success);
        expect(!!sim.connect(cross2, 1, sum_a, 1));
        expect(!!sim.connect(sum_a, 0, integrator_a, 0));
        expect(!!sim.connect(cross, 0, integrator_a, 1));

        expect(!!sim.connect(cross2, 1, sum_b, 0));
        expect(!!sim.connect(sum_b, 0, integrator_b, 0));

        expect(!!sim.connect(cross2, 0, integrator_b, 1));
        expect(!!sim.connect(integrator_a, 0, cross2, 0));
        expect(!!sim.connect(integrator_b, 0, cross2, 2));
        expect(!!sim.connect(sum_d, 0, cross2, 1));
        expect(!!sim.connect(integrator_b, 0, sum_d, 0));
        expect(!!sim.connect(constant, 0, sum_d, 1));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "izhikevitch-qss3_a.csv");
        expect(fo_a.os != nullptr);

        auto&       obs_b = sim.observers.alloc();
        file_output fo_b(obs_b, "izhikevitch-qss3_b.csv");
        expect(fo_b.os != nullptr);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        sim.t = irt::zero;

        expect(!!sim.initialize());
        expect((sim.sched.size() == 12_ul) >> fatal);

        do {
            irt::status st = sim.run();
            expect(!!st);

            fo_a.write();
            fo_b.write();
        } while (sim.t < irt::time(140));

        fo_a.flush();
        fo_b.flush();
    };

    "van_der_pol_simulation_qss3"_test = [] {
        fmt::print("van_der_pol_simulation_qss3\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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

        expect(!!sim.connect(integrator_b, 0, integrator_a, 0));
        expect(!!sim.connect(sum, 0, integrator_b, 0));

        expect(!!sim.connect(integrator_b, 0, sum, 0));
        expect(!!sim.connect(product2, 0, sum, 1));
        expect(!!sim.connect(integrator_a, 0, sum, 2));

        expect(!!sim.connect(integrator_b, 0, product1, 0));
        expect(!!sim.connect(integrator_a, 0, product1, 1));
        expect(!!sim.connect(product1, 0, product2, 0));
        expect(!!sim.connect(integrator_a, 0, product2, 1));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "van_der_pol_qss3_a.csv");
        expect(fo_a.os != nullptr);

        auto&       obs_b = sim.observers.alloc();
        file_output fo_b(obs_b, "van_der_pol_qss3_b.csv");
        expect(fo_b.os != nullptr);

        sim.observe(irt::get_model(integrator_a), obs_a);
        sim.observe(irt::get_model(integrator_b), obs_b);

        sim.t = 0.0;
        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run();
            expect(!!st);

            fo_a.write();
            fo_b.write();
        } while (sim.t < irt::time(1500.0));

        fo_a.flush();
        fo_b.flush();
    };

    "neg_lif_simulation_qss1"_test = [] {
        fmt::print("neg_lif_simulation_qss1\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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
        expect(!!sim.connect(cross, 0, integrator, 1));
        expect(!!sim.connect(cross, 1, sum, 0));
        expect(!!sim.connect(integrator, 0, cross, 0));
        expect(!!sim.connect(integrator, 0, cross, 2));
        expect(!!sim.connect(constant_cross, 0, cross, 1));
        expect(!!sim.connect(constant, 0, sum, 1));
        expect(!!sim.connect(sum, 0, integrator, 0));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "neg-lif-qss1.csv");
        expect(fo_a.os != nullptr);

        sim.observe(irt::get_model(integrator), obs_a);

        sim.t = 0;

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run();
            expect(!!st);
            fo_a.write();
        } while (sim.t < irt::time(100));

        fo_a.flush();
    };

    "neg_lif_simulation_qss2"_test = [] {
        fmt::print("neg_lif_simulation_qss2\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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
        expect(!!sim.connect(cross, 0, integrator, 1));
        expect(!!sim.connect(cross, 1, sum, 0));
        expect(!!sim.connect(integrator, 0, cross, 0));
        expect(!!sim.connect(integrator, 0, cross, 2));
        expect(!!sim.connect(constant_cross, 0, cross, 1));
        expect(!!sim.connect(constant, 0, sum, 1));
        expect(!!sim.connect(sum, 0, integrator, 0));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "neg-lif-qss2.csv");
        expect(fo_a.os != nullptr);

        sim.observe(irt::get_model(integrator), obs_a);

        sim.t = 0;

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run();
            expect(!!st);

            fo_a.write();
        } while (sim.t < irt::time(100.0));

        fo_a.flush();
    };

    "neg_lif_simulation_qss3"_test = [] {
        fmt::print("neg_lif_simulation_qss3\n");
        mem.reset();
        irt::simulation sim(&mem, mem.capacity());

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
        expect(!!sim.connect(cross, 0, integrator, 1));
        expect(!!sim.connect(cross, 1, sum, 0));
        expect(!!sim.connect(integrator, 0, cross, 0));
        expect(!!sim.connect(integrator, 0, cross, 2));
        expect(!!sim.connect(constant_cross, 0, cross, 1));
        expect(!!sim.connect(constant, 0, sum, 1));
        expect(!!sim.connect(sum, 0, integrator, 0));

        auto&       obs_a = sim.observers.alloc();
        file_output fo_a(obs_a, "neg-lif-qss3.csv");
        expect(fo_a.os != nullptr);

        sim.observe(irt::get_model(integrator), obs_a);

        sim.t = 0;

        expect(!!sim.initialize());
        expect((sim.sched.size() == 5_ul) >> fatal);

        do {
            auto st = sim.run();
            expect(!!st);
            fo_a.write();
        } while (sim.t < irt::time(100.0));

        fo_a.flush();
    };

    "all"_test = [] {
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_lotka_volterra<1>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }

        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_negative_lif<1>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_lif<1>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_van_der_pol<1>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_izhikevich<1>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }

        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_lotka_volterra<2>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_negative_lif<2>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_lif<2>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_van_der_pol<2>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_izhikevich<2>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }

        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_lotka_volterra<3>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_negative_lif<3>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_lif<3>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_van_der_pol<3>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
        {
            mem.reset();
            irt::simulation sim(&mem, mem.capacity());

            expect(!!irt::example_qss_izhikevich<3>(sim, empty_fun));
            expect(!!run_simulation(sim, 30.));
        }
    };

    // "memory"_test = [] {
    //     {
    //         irt::g_alloc_fn = global_alloc;
    //         irt::g_free_fn  = global_free;

    //                 mem.reset();
    // irt::simulation sim(&mem, mem.capacity());

    //     }

    //     fmt::print("memory: {}/{}\n",
    //                global_allocator.allocation_number,
    //                global_deallocator.free_number);

    //     expect(global_allocator.allocation_size > 0);
    //     expect(global_allocator.allocation_number ==
    //            global_deallocator.free_number);

    //     irt::g_alloc_fn = nullptr;
    //     irt::g_free_fn  = nullptr;

    //     irt::g_alloc_fn = irt::malloc_wrapper;
    //     irt::g_free_fn  = irt::free_wrapper;
    // };

    // "null_memory"_test = [] {
    //     auto old = std::exchange(irt::on_error_callback, nullptr);

    //     irt::g_alloc_fn = null_alloc;
    //     irt::g_free_fn  = null_free;

    //             mem.reset();
    // irt::simulation sim(&mem, mem.capacity());

    //     irt::g_alloc_fn = irt::malloc_wrapper;
    //     irt::g_free_fn  = irt::free_wrapper;

    //     std::exchange(irt::on_error_callback, old);
    // };

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
        auto f = irt::memory::make(
          256, irt::open_mode::write, [](irt::memory::error_code) noexcept {});

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

        irt::u8  a_w = f->data[0];
        irt::u16 b_w = *(reinterpret_cast<irt::u16*>(&f->data[1]));
        irt::u32 c_w = *(reinterpret_cast<irt::u32*>(&f->data[3]));
        irt::u64 d_w = *(reinterpret_cast<irt::u64*>(&f->data[7]));

        expect(eq(a, a_w));
        expect(eq(b, b_w));
        expect(eq(c, c_w));
        expect(eq(d, d_w));

        f->rewind();

        assert(f->tell() == 0);
    };

    "binary-file-io"_test = [] {
        std::error_code ec;
        auto            file_path = std::filesystem::temp_directory_path(ec);

        file_path /= "irritator.txt";

        {
            auto f = irt::file::open(file_path.string().c_str(),
                                     irt::open_mode::write);
            expect(!!f);
            expect(f->length() == 0);

            irt::u8  a = 0xfe;
            irt::u16 b = 0xfedc;
            irt::u32 c = 0xfedcba98;
            irt::u64 d = 0xfedcba9876543210;

            expect(!!f->write(a));
            expect(!!f->write(b));
            expect(!!f->write(c));
            expect(!!f->write(d));

            expect(f->tell() == 15);
        }

        {
            auto f =
              irt::file::open(file_path.string().c_str(), irt::open_mode::read);
            expect(!!f);
            expect(f->length() == 15);

            irt::u8  a   = 0xfe;
            irt::u16 b   = 0xfedc;
            irt::u32 c   = 0xfedcba98;
            irt::u64 d   = 0xfedcba9876543210;
            irt::u8  a_w = 0;
            irt::u16 b_w = 0;
            irt::u32 c_w = 0;
            irt::u64 d_w = 0;

            expect(!!f->read(a_w));
            expect(!!f->read(b_w));
            expect(!!f->read(c_w));
            expect(!!f->read(d_w));

            expect(a == a_w);
            expect(b == b_w);
            expect(c == c_w);
            expect(d == d_w);

            expect(f->tell() == 15);

            f->rewind();

            expect(f->tell() == 0);
        }

        std::filesystem::remove(file_path, ec);
    };

    "memory"_test = [] {
        auto mem = irt::memory::make(2040, irt::open_mode::write);
        expect(mem.has_value());

        expect(mem->write(0x00112233));
        expect(mem->write(0x44556677));
        expect(eq(mem->data.ssize(), 2040));
        expect(eq(mem->pos, 8));

        mem->rewind();

        irt::u32 a, b;
        expect(mem->read(a));
        expect(mem->read(b));

        expect(a == 0x00112233);
        expect(b == 0x44556677);
    };

    // "modeling-copy"_test = [] {
    //     irt::modeling             mod;
    //     irt::modeling_initializer mod_init;
    //
    //     mod.init(mod_init);
    //     mod.fill_internal_components();
    //
    //     auto& main_compo = mod.components.alloc();
    //     main_compo.name.assign("New component");
    //     main_compo.type = irt::component_type::none;
    //
    //     auto& main_tree = mod.tree_nodes.alloc(
    //       mod.components.get_id(main_compo),
    //       irt::undefined<irt::child_id>());
    //     mod.head = mod.tree_nodes.get_id(main_tree);
    //
    //     irt::simple_component* lotka = nullptr;
    //     while (mod.components.next(lotka))
    //         if (lotka->type == irt::component_type::qss1_lotka_volterra)
    //             break;
    //
    //     expect(lotka != nullptr);
    //
    //     auto lotka_id = mod.components.get_id(*lotka);
    //
    //     for (int i = 0; i < 2; ++i) {
    //         irt::tree_node_id a;
    //         mod.make_tree_from(*lotka, &a);
    //
    //         auto& child_a     = main_compo.children.alloc(lotka_id);
    //         auto& tree        = mod.tree_nodes.get(a);
    //         tree.id_in_parent = main_compo.children.get_id(child_a);
    //         tree.tree.set_id(&tree);
    //         tree.tree.parent_to(main_tree.tree);
    //     }
    //
    //     irt::simulation             sim;
    //     irt::modeling_to_simulation cache;
    //
    //     expect(mod.can_export_to(cache, sim) == true);
    //     !!expect(mod.export_to(cache, sim));
    //     expect(sim.models.ssize() == 2 * lotka->children.ssize());
    // };

    "archive"_test = [] {
        irt::vector<irt::u8> data;
        {
            auto m = irt::memory::make(256 * 8, irt::open_mode::write);
            expect(m.has_value()) << fatal;

            irt::simulation sim(
              irt::simulation_memory_requirement(1024 * 1024 * 8));
            irt::binary_archiver bin;

            (void)sim.alloc<irt::qss1_sum_2>();
            (void)sim.alloc<irt::qss1_integrator>();
            (void)sim.alloc<irt::qss1_multiplier>();

            expect(!!bin.simulation_save(sim, *m));

            data.resize(static_cast<int>(m->pos));
            std::copy_n(m->data.data(), m->pos, data.data());
        }

        expect(data.size() > 0);

        {
            auto m = irt::memory::make(data.size(), irt::open_mode::read);
            expect(m.has_value()) << fatal;
            irt::simulation sim(
              irt::simulation_memory_requirement(1024 * 1024 * 8));
            irt::binary_archiver bin;

            std::copy_n(data.data(), data.size(), m->data.data());
            m->pos = 0;

            expect(!!bin.simulation_load(sim, *m));
            expect(sim.models.size() == 3u);
            expect(sim.hsms.size() == 0u);
        }
    };
}
