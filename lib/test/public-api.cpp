// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>

#include <boost/ut.hpp>

#include <fmt/format.h>

#include <cstdio>

static void
dot_graph_save(const irt::simulation& sim, std::FILE* os)
{
    /* With input and output port.

    digraph graphname{
        graph[rankdir = "LR"];
        node[shape = "record"];
        edge[];

        "sum_a"[label = "sum-a | <f0> | <f1>"];

        "sum_a":f0->int_a[id = 1];
        sum_b->int_b[label = "2-10"];
        prod->sum_b[label = "3-4"];
        prod -> "sum_a":f0[label = "3-2"];
        int_a->qua_a[label = "4-11"];
        int_a->prod[label = "4-5"];
        int_a -> "sum_a":f1[label = "4-1"];
        int_b->qua_b[label = "5-12"];
        int_b->prod[label = "5-6"];
        int_b->sum_b[label = "5-3"];
        qua_a->int_a[label = "6-7"];
        qua_b->int_b[label = "7-9"];
    }
    */

    !boost::ut::expect(os != nullptr);

    std::fputs("digraph graphname {\n", os);

    irt::output_port* output_port = nullptr;
    while (sim.output_ports.next(output_port)) {
        for (const irt::input_port_id dst : output_port->connections) {
            if (auto* input_port = sim.input_ports.try_to_get(dst);
                input_port) {
                auto* mdl_src = sim.models.try_to_get(output_port->model);
                auto* mdl_dst = sim.models.try_to_get(input_port->model);

                if (!(mdl_src && mdl_dst))

                    continue;
                if (mdl_src->name.empty())
                    fmt::print(os, "{} -> ", irt::get_key(output_port->model));
                else
                    fmt::print(os, "{} -> ", mdl_src->name.c_str());

                if (mdl_dst->name.empty())
                    fmt::print(os, "{}", irt::get_key(input_port->model));
                else
                    fmt::print(os, "{}", mdl_dst->name.c_str());

                std::fputs(" [label=\"", os);

                if (output_port->name.empty())
                    fmt::print(
                      os,
                      "{}",
                      irt::get_key(sim.output_ports.get_id(*output_port)));
                else
                    fmt::print(os, "{}", output_port->name.c_str());

                std::fputs("-", os);

                if (input_port->name.empty())
                    fmt::print(
                      os,
                      "{}",
                      irt::get_key(sim.input_ports.get_id(*input_port)));
                else
                    fmt::print(os, "{}", input_port->name.c_str());

                std::fputs("\"];\n", os);
            }
        }
    }
}
double f (double t) {return t*t;}
int
main()
{
    using namespace boost::ut;

    "model_constepxr"_test = [] {
        expect(irt::is_detected_v<irt::initialize_function_t, irt::counter> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::counter> ==
               false);
        expect(irt::is_detected_v<irt::transition_function_t, irt::counter> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::counter> ==
               true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::counter> ==
               false);

        expect(
          irt::is_detected_v<irt::initialize_function_t, irt::generator> ==
          true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::generator> ==
               true);
        expect(
          irt::is_detected_v<irt::transition_function_t, irt::generator> ==
          false);
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
        expect(irt::is_detected_v<irt::has_input_port_t, irt::adder_2> ==
               true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::adder_2> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::adder_3> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::adder_3> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::adder_3> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::adder_3> ==
               true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::adder_3> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::adder_4> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::adder_4> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::adder_4> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::adder_4> ==
               true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::adder_4> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::mult_2> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::mult_2> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::mult_2> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::mult_2> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::mult_2> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::mult_3> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::mult_3> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::mult_3> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::mult_3> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::mult_3> ==
               true);

        expect(irt::is_detected_v<irt::initialize_function_t, irt::mult_4> ==
               true);
        expect(irt::is_detected_v<irt::lambda_function_t, irt::mult_4> ==
               true);
        expect(irt::is_detected_v<irt::transition_function_t, irt::mult_4> ==
               true);
        expect(irt::is_detected_v<irt::has_input_port_t, irt::mult_4> == true);
        expect(irt::is_detected_v<irt::has_output_port_t, irt::mult_4> ==
               true);

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
        expect(irt::is_not_enough_memory(s1) == false);

        irt::status s2 = irt::status::block_allocator_not_enough_memory;
        expect(irt::is_success(s2) == false);
        expect(irt::is_bad(s2) == true);
        expect(irt::is_not_enough_memory(s2) == true);
    };

    "time"_test = [] {
        expect(irt::time_domain<irt::time>::infinity >
               irt::time_domain<irt::time>::zero);
        expect(irt::time_domain<irt::time>::zero >
               irt::time_domain<irt::time>::negative_infinity);
    };

    "small_string"_test = [] {
        irt::small_string<8> f1;
        expect(f1.capacity() == 8_ul);
        expect(f1 == "");
        expect(f1.size() == 0_ul);

        f1.append("ok");
        expect(f1 == "ok");
        expect(f1.size() == 2_ul);

        f1.append("ok");
        expect(f1 == "okok");
        expect(f1.size() == 4_ul);

        f1.append("1234");
        expect(f1 == "okok123");
        expect(f1.size() == 7_ul);

        irt::small_string<8> f2(f1);
        expect(f2 == "okok123");
        expect(f2.size() == 7_ul);

        expect(f1.c_str() != f2.c_str());

        irt::small_string<8> f3("012345678");
        expect(f3 == "0123456");
        expect(f3.size() == 7_ul);

        f3.clear();
        expect(f3 == "");
        expect(f3.size() == 0_ul);

        f3 = f2;
        expect(f3 == "okok123");
        expect(f3.size() == 7_ul);
    };

    "list"_test = [] {
        irt::flat_list<int>::allocator_type allocator;
        expect(is_success(allocator.init(32)));

        irt::flat_list<int> lst(&allocator);

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
        irt::flat_double_list<int>::allocator_type allocator;
        expect(is_success(allocator.init(32)));

        irt::flat_double_list<int> lst(&allocator);

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
            first.x = 0.f;
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
        expect(array.capacity() == 0);
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
        {
            irt::message v8(irt::i8(0), irt::i8(1), irt::i8(2), irt::i8(3));
            expect(v8.to_integer_8(0) == 0);
            expect(v8.to_integer_8(1) == 1);
            expect(v8.to_integer_8(2) == 2);
            expect(v8.to_integer_8(3) == 3);
            expect(v8.size() == 4_ul);
        }
        {
            irt::message v32(0, 1, 2, 3);
            expect(v32.to_integer_32(0) == 0);
            expect(v32.to_integer_32(1) == 1);
            expect(v32.to_integer_32(2) == 2);
            expect(v32.to_integer_32(3) == 3);
            expect(v32.size() == 4_ul);
        }
        {
            irt::message v64(
              irt::i64(0), irt::i64(1), irt::i64(2), irt::i64(3));
            expect(v64.to_integer_64(0) == 0);
            expect(v64.to_integer_64(1) == 1);
            expect(v64.to_integer_64(2) == 2);
            expect(v64.to_integer_64(3) == 3);
            expect(v64.size() == 4_ul);
        }
        {
            irt::message vfloat(0.0f, 1.0f, 2.0f, 3.0f);
            expect(vfloat.to_real_32(0) == 0.0_f);
            expect(vfloat.to_real_32(1) == 1.0_f);
            expect(vfloat.to_real_32(2) == 2.0_f);
            expect(vfloat.to_real_32(3) == 3.0_f);
            expect(vfloat.size() == 4_ul);
        }
        {
            irt::message vdouble(0.0, 1.0, 2.0, 3.0);
            expect(vdouble.to_real_64(0) == 0.0);
            expect(vdouble.to_real_64(1) == 1.0);
            expect(vdouble.to_real_64(2) == 2.0);
            expect(vdouble.to_real_64(3) == 3.0);
            expect(vdouble.size() == 4_ul);
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

        expect(i1->tn == 0.0_d);
        expect(i2->tn == 1.0_d);
        expect(i3->tn == -1.0_d);
        expect(i4->tn == 2.0_d);

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

        irt::heap::handle i1 = h.insert(0.0, irt::model_id{ 0 });
        irt::heap::handle i2 = h.insert(1.0, irt::model_id{ 1 });
        irt::heap::handle i3 = h.insert(-1.0, irt::model_id{ 2 });
        irt::heap::handle i4 = h.insert(2.0, irt::model_id{ 3 });

        expect(i1 != nullptr);
        expect(i2 != nullptr);
        expect(i3 != nullptr);
        expect(i4 != nullptr);

        expect(!h.empty());
        expect(h.top() == i3);

        h.pop(); // remove i3
        h.pop(); // rmeove i1

        expect(h.top() == i2);

        i3->tn = -10.0;
        h.insert(i3);

        i1->tn = -1.0;
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
        irt::heap h;
        h.init(256u);

        for (double t = 0; t < 100.0; ++t)
            h.insert(t, irt::model_id{ static_cast<unsigned>(t) });

        expect(h.size() == 100_ul);

        h.insert(50.0, irt::model_id{ 502 });
        h.insert(50.0, irt::model_id{ 503 });
        h.insert(50.0, irt::model_id{ 504 });

        expect(h.size() == 103_ul);

        for (double t = 0.0; t < 50.0; ++t) {
            expect(h.top()->tn == t);
            h.pop();
        }

        expect(h.top()->tn == 50.0_d);
        h.pop();
        expect(h.top()->tn == 50.0_d);
        h.pop();
        expect(h.top()->tn == 50.0_d);
        h.pop();
        expect(h.top()->tn == 50.0_d);
        h.pop();

        for (double t = 51.0; t < 100.0; ++t) {
            expect(h.top()->tn == t);
            h.pop();
        }
    };

    "constant_simulation"_test = [] {
        irt::simulation sim;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.constant_models.can_alloc(2));
        expect(sim.counter_models.can_alloc(1));

        auto& cnt = sim.counter_models.alloc();
        auto& c1 = sim.constant_models.alloc();
        auto& c2 = sim.constant_models.alloc();

        c1.value = 0.0;
        c2.value = 0.0;

        expect(sim.models.can_alloc(3));
        expect(irt::is_success(sim.alloc(cnt, sim.counter_models.get_id(cnt))));
        expect(irt::is_success(sim.alloc(c1, sim.constant_models.get_id(c1))));
        expect(irt::is_success(sim.alloc(c2, sim.constant_models.get_id(c2))));

        expect(sim.connect(c1.y[0], cnt.x[0]) == irt::status::success);
        expect(sim.connect(c2.y[0], cnt.x[0]) == irt::status::success);

        irt::time t = 0.0;
        irt::status st;

        do {
            st = sim.run(t);
            expect(irt::is_success(st));
        } while (t < sim.end);

        expect(cnt.number == 2_ul);
    };

    "cross_simulation"_test = [] {
        irt::simulation sim;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.counter_models.can_alloc(1));
        expect(sim.cross_models.can_alloc(1));
        expect(sim.constant_models.can_alloc(1));

        auto& cnt = sim.counter_models.alloc();
        auto& cross1 = sim.cross_models.alloc();
        auto& c1 = sim.constant_models.alloc();

        /*auto& value = sim.messages.alloc(3.0);
        auto& threshold = sim.messages.alloc(0.0);
        
        c1.init[0] = sim.messages.get_id(value);
        cross1.init[0] = sim.messages.get_id(threshold);*/
        c1.value = 3.0;
        cross1.threshold = 0.0;

        expect(sim.models.can_alloc(3));
        expect(irt::is_success(sim.alloc(cnt, sim.counter_models.get_id(cnt))));
        expect(irt::is_success(sim.alloc(cross1, sim.cross_models.get_id(cross1))));
        expect(irt::is_success(sim.alloc(c1, sim.constant_models.get_id(c1))));

        expect(sim.connect(c1.y[0], cross1.x[0]) == irt::status::success);
        expect(sim.connect(c1.y[0], cross1.x[1]) == irt::status::success);
        expect(sim.connect(c1.y[0], cross1.x[2]) == irt::status::success);
        expect(sim.connect(cross1.y[0], cnt.x[0]) == irt::status::success);

        irt::time t = 0.0;
        irt::status st;

        do {    
            st = sim.run(t);
            expect(irt::is_success(st));
        } while (t < sim.end);

        expect(cnt.number == 2_ul);
    };

    "generator_counter_simluation"_test = [] {
        irt::simulation sim;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.generator_models.can_alloc(1));
        expect(sim.counter_models.can_alloc(1));

        auto& gen = sim.generator_models.alloc();
        auto& cnt = sim.counter_models.alloc();

        expect(sim.models.can_alloc(2));
        expect(irt::is_success(sim.alloc(gen, sim.generator_models.get_id(gen))));
        expect(irt::is_success(sim.alloc(cnt, sim.counter_models.get_id(cnt))));

        expect(sim.connect(gen.y[0], cnt.x[0]) == irt::status::success);

        expect(sim.begin == irt::time_domain<irt::time>::zero);
        expect(sim.end == irt::time_domain<irt::time>::infinity);

        sim.end = 10.0;

        irt::time t = sim.begin;
        irt::status st;

        do {
            st = sim.run(t);
            expect(irt::is_success(st));
            expect(cnt.number <= static_cast<long unsigned>(t));
        } while (t < sim.end);

        expect(cnt.number == 9_ul);
    };

    "time_func"_test = [] {
        irt::simulation sim;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.time_func_models.can_alloc(1));
        expect(sim.counter_models.can_alloc(1));

        auto& time_fun = sim.time_func_models.alloc();
        auto& cnt = sim.counter_models.alloc();

        double f(double t);
        time_fun.f = &f;
        expect(sim.models.can_alloc(2));
        expect(irt::is_success(sim.alloc(time_fun, sim.time_func_models.get_id(time_fun))));
        expect(irt::is_success(sim.alloc(cnt, sim.counter_models.get_id(cnt))));

        expect(sim.connect(time_fun.y[0], cnt.x[0]) == irt::status::success);

        expect(sim.begin == irt::time_domain<irt::time>::zero);
        expect(sim.end == irt::time_domain<irt::time>::infinity);

        sim.end = 30.0;

        irt::time t = sim.begin;
        irt::status st;

        do {
            st = sim.run(t);
            expect(irt::is_success(st));
            expect(time_fun.value == t*t);
            expect(cnt.number <= static_cast<long unsigned>(t));
        } while (t < sim.end);


    };

    "lotka_volterra_simulation"_test = [] {
        irt::simulation sim;

        expect(irt::is_success(sim.init(16lu, 256lu)));
        expect(sim.adder_2_models.can_alloc(2));
        expect(sim.mult_2_models.can_alloc(2));
        expect(sim.integrator_models.can_alloc(2));
        expect(sim.quantifier_models.can_alloc(2));

        auto& sum_a = sim.adder_2_models.alloc();
        auto& sum_b = sim.adder_2_models.alloc();
        auto& product = sim.mult_2_models.alloc();
        auto& integrator_a = sim.integrator_models.alloc();
        auto& integrator_b = sim.integrator_models.alloc();
        auto& quantifier_a = sim.quantifier_models.alloc();
        auto& quantifier_b = sim.quantifier_models.alloc();

        //auto& init_powers = sim.messages.alloc(1.0, 1.0);
        //auto& init_weigths_a = sim.messages.alloc(2.0, -0.4);
        //auto& init_weigths_b = sim.messages.alloc(-1.0, 0.1);

        //auto& a_x0 = sim.messages.alloc(18.0);

        //auto& allow_offser_a = sim.messages.alloc(irt::i8{ 1 });
        //auto& zero_init_a = sim.messages.alloc(irt::i8{ 1 });
        //auto& quantum_a = sim.messages.alloc(0.01);
        //auto& archive_lenght_a = sim.messages.alloc(irt::i32{ 3 });

        //auto& b_x0 = sim.messages.alloc(7.0);
        //auto& allow_offser_b = sim.messages.alloc(irt::i8{ 1 });
        //auto& zero_init_b = sim.messages.alloc(irt::i8{ 1 });
        //auto& quantum_b = sim.messages.alloc(0.01);
        //auto& archive_lenght_b = sim.messages.alloc(irt::i32{ 3 });

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

        expect(sim.models.can_alloc(10));
        !expect(irt::is_success(
          sim.alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a")));
        !expect(irt::is_success(
          sim.alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b")));
        !expect(irt::is_success(
          sim.alloc(product, sim.mult_2_models.get_id(product), "prod")));
        !expect(irt::is_success(sim.alloc(
          integrator_a, sim.integrator_models.get_id(integrator_a), "int_a")));
        !expect(irt::is_success(sim.alloc(
          integrator_b, sim.integrator_models.get_id(integrator_b), "int_b")));
        !expect(irt::is_success(sim.alloc(
          quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a")));
        !expect(irt::is_success(sim.alloc(
          quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b")));

        !expect(sim.models.size() == 7_ul);
        !expect(sim.sched.size() == 7_ul);

        expect(sim.connect(sum_a.y[0], integrator_a.x[1]) ==
               irt::status::success);
        expect(sim.connect(sum_b.y[0], integrator_b.x[1]) ==
               irt::status::success);

        expect(sim.connect(integrator_a.y[0], sum_a.x[0]) ==
               irt::status::success);
        expect(sim.connect(integrator_b.y[0], sum_b.x[0]) ==
               irt::status::success);

        expect(sim.connect(integrator_a.y[0], product.x[0]) ==
               irt::status::success);
        expect(sim.connect(integrator_b.y[0], product.x[1]) ==
               irt::status::success);

        expect(sim.connect(product.y[0], sum_a.x[1]) == irt::status::success);
        expect(sim.connect(product.y[0], sum_b.x[1]) == irt::status::success);

        expect(sim.connect(quantifier_a.y[0], integrator_a.x[0]) ==
               irt::status::success);
        expect(sim.connect(quantifier_b.y[0], integrator_b.x[0]) ==
               irt::status::success);
        expect(sim.connect(integrator_a.y[0], quantifier_a.x[0]) ==
               irt::status::success);
        expect(sim.connect(integrator_b.y[0], quantifier_b.x[0]) ==
               irt::status::success);

        dot_graph_save(sim, stdout);

        irt::time t = 0.0;
        irt::status st;

        std::FILE* os = std::fopen("output.csv", "w");
        !expect(os != nullptr);
        fmt::print(os,
              "{},{},{}\n",
              "t",
              "x",
	      "y");
        do {
            st = sim.run(t);

            expect(st == irt::status::success);

            fmt::print(os,
                       "{},{},{}\n",
                       t,
                       integrator_a.last_output_value,
                       integrator_b.last_output_value);
        } while (t < 15.0);

        std::fclose(os);
    };
     "izhikevitch_simulation"_test = [] {
        irt::simulation sim;
        
        expect(irt::is_success(sim.init(64lu, 256lu)));
        expect(sim.constant_models.can_alloc(2));
        expect(sim.adder_2_models.can_alloc(3));
        expect(sim.adder_4_models.can_alloc(1));
        expect(sim.mult_2_models.can_alloc(1));
        expect(sim.integrator_models.can_alloc(2));
        expect(sim.quantifier_models.can_alloc(2));
        expect(sim.cross_models.can_alloc(2));
        expect(sim.time_func_models.can_alloc(1));

        auto& time_fun = sim.time_func_models.alloc();
        auto& constant = sim.constant_models.alloc();
        auto& constant2 = sim.constant_models.alloc();
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
        double f(double);

       
        integrator_a.current_value = 0.0;

        quantifier_a.m_adapt_state = irt::quantifier::adapt_state::possible;
        quantifier_a.m_zero_init_offset = true;
        quantifier_a.m_step_size = 0.01;
        quantifier_a.m_past_length = 3;

        integrator_b.current_value = 0.0;

        quantifier_b.m_adapt_state = irt::quantifier::adapt_state::possible;
        quantifier_b.m_zero_init_offset = true;
        quantifier_b.m_step_size = 0.01;
        quantifier_b.m_past_length = 3;

        product.input_coeffs[0] = 1.0;
        product.input_coeffs[1] = 1.0;

        sum_a.input_coeffs[0] = 1.0;
        sum_a.input_coeffs[1] = -1.0;
        sum_b.input_coeffs[0] = -a;
        sum_b.input_coeffs[1] = a*b;
        sum_c.input_coeffs[0] = 0.04;
        sum_c.input_coeffs[1] = 5.0;
        sum_c.input_coeffs[2] = 140.0;
        sum_c.input_coeffs[3] = 1.0;
        sum_d.input_coeffs[0] = 1.0;
        sum_d.input_coeffs[1] = d;

        constant.value = 1.0;
        constant2.value = c;

        cross.threshold = vt;
        cross2.threshold = vt;

        time_fun.f = &f;
     
        expect(sim.models.can_alloc(12));
        !expect(irt::is_success(
               sim.alloc(time_fun, sim.time_func_models.get_id(time_fun),"tfun")));
        !expect(irt::is_success(
               sim.alloc(constant, sim.constant_models.get_id(constant),"1.0")));
        !expect(irt::is_success(
               sim.alloc(constant2, sim.constant_models.get_id(constant2),"-56.0")));

        !expect(irt::is_success(
          sim.alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a")));
        !expect(irt::is_success(
          sim.alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b")));
        !expect(irt::is_success(
          sim.alloc(sum_c, sim.adder_4_models.get_id(sum_c), "sum_c")));

        !expect(irt::is_success(
          sim.alloc(product, sim.mult_2_models.get_id(product), "prod")));
        !expect(irt::is_success(sim.alloc(
          integrator_a, sim.integrator_models.get_id(integrator_a), "int_a")));
        !expect(irt::is_success(sim.alloc(
          integrator_b, sim.integrator_models.get_id(integrator_b), "int_b")));
        !expect(irt::is_success(sim.alloc(
          quantifier_a, sim.quantifier_models.get_id(quantifier_a), "qua_a")));
        !expect(irt::is_success(sim.alloc(
          quantifier_b, sim.quantifier_models.get_id(quantifier_b), "qua_b")));
        !expect(irt::is_success(sim.alloc(
          cross, sim.cross_models.get_id(cross), "cross")));



        !expect(sim.models.size() == 12_ul);
        !expect(sim.sched.size() == 12_ul);


        expect(sim.connect(integrator_a.y[0], cross.x[0]) ==
               irt::status::success);
        expect(sim.connect(constant2.y[0], cross.x[1]) ==
               irt::status::success);
        expect(sim.connect(integrator_a.y[0], cross.x[2]) ==
               irt::status::success);

        expect(sim.connect( cross.y[0], quantifier_a.x[0]) ==
               irt::status::success);
        expect(sim.connect( cross.y[0], product.x[0]) ==
               irt::status::success);
        expect(sim.connect( cross.y[0], product.x[1]) ==
               irt::status::success);
        expect(sim.connect( product.y[0] , sum_c.x[0]) ==
               irt::status::success);
        expect(sim.connect( cross.y[0], sum_c.x[1]) ==
               irt::status::success);
        expect(sim.connect( cross.y[0], sum_b.x[1]) ==
               irt::status::success);

        expect(sim.connect(constant.y[0] ,sum_c.x[2]) ==
               irt::status::success);
        expect(sim.connect(time_fun.y[0] ,sum_c.x[3]) ==
               irt::status::success);

        expect(sim.connect(sum_c.y[0],sum_a.x[0]) ==
               irt::status::success);
        expect(sim.connect(integrator_b.y[0],sum_a.x[1]) ==
               irt::status::success);
        expect(sim.connect(sum_a.y[0],integrator_a.x[1]) ==
               irt::status::success);
        expect(sim.connect(cross.y[0],integrator_a.x[2]) ==
               irt::status::success);
        expect(sim.connect(quantifier_a.y[0], integrator_a.x[0]) ==
               irt::status::success);

        expect(sim.connect(integrator_b.y[0],quantifier_b.x[0]) ==
               irt::status::success);
        expect(sim.connect(integrator_b.y[0],sum_b.x[0]) ==
               irt::status::success);
        expect(sim.connect(quantifier_b.y[0],integrator_b.x[0]) ==
               irt::status::success);
        expect(sim.connect(sum_b.y[0],integrator_b.x[1]) ==
               irt::status::success);



        
        dot_graph_save(sim, stdout);

        irt::time t = 0.0;
        irt::status st;

        std::FILE* os = std::fopen("output_izhikevitch.csv", "w");
        !expect(os != nullptr);
        fmt::print(os,
              "{},{},{}\n",
              "t",
              "v",
	      "u");
        do {
            st = sim.run(t);
        
            expect(st == irt::status::success);
            fmt::print(os,
                       "{},{},{}\n",
                       t,
                       integrator_a.last_output_value,
		       integrator_b.last_output_value);
        } while (t < 6.0);

        std::fclose(os);
    };
}




