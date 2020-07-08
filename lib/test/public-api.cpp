// Copyright (c) 2020 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/core.hpp>
#include <irritator/io.hpp>

#include <boost/ut.hpp>

#include <fmt/format.h>

#include <iostream>

struct file_output
{
    file_output(const char* file_path) noexcept
      : os(std::fopen(file_path, "w"))
    {}

    ~file_output() noexcept
    {
        if (os)
            std::fclose(os);
    }

    std::FILE* os = nullptr;
};

void
file_output_initialize(const irt::observer& obs, const irt::time /*t*/) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<file_output*>(obs.user_data);
    fmt::print(output->os, "t,{}\n", obs.name.c_str());
}

void
file_output_observe(const irt::observer& obs,
                    const irt::time t,
                    const irt::message& msg) noexcept
{
    if (!obs.user_data)
        return;

    auto* output = reinterpret_cast<file_output*>(obs.user_data);
    fmt::print(output->os, "{},{}\n", t, msg.real[0]);
}

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

    "array_api"_test = [] {
        struct position
        {
            position() noexcept = default;

            position(int x_, int y_)
              : x(x_)
              , y(y_)
            {}

            int x = 0, y = 0;
        };

        irt::array<position> positions;
        !expect(irt::status::success == positions.init(16u));

        expect(positions.size() == 16_u);
        expect(positions.capacity() == 16u);

        for (int i = 0; i != 16; ++i) {
            positions[i].x = i;
            positions[i].y = i;
        }

        for (int i = 0; i != 16; ++i) {
            expect(positions[i].x == i);
            expect(positions[i].y == i);
        }
    };

    "vector_api"_test = [] {
        struct position
        {
            position() noexcept = default;

            position(int x_, int y_)
              : x(x_)
              , y(y_)
            {}

            int x = 0, y = 0;
        };

        irt::vector<position> positions;
        !expect(irt::status::success == positions.init(16u));

        expect(positions.size() == 0_u);
        expect(positions.capacity() == 16u);

        {
            expect(positions.can_alloc(1u));
            auto it = positions.emplace_back(0, 1);
            expect(positions.size() == 1_u);
            expect(positions.begin() == it);
            expect(it->x == 0);
            expect(it->y == 1);
        }

        {
            auto ret = positions.try_emplace_back(2, 3);
            expect(ret.first == true);
            expect(ret.second == positions.begin() + 1);
            expect(positions.size() == 2_u);
            expect(positions.begin()->x == 0);
            expect(positions.begin()->y == 1);
            expect((positions.begin() + 1)->x == 2);
            expect((positions.begin() + 1)->y == 3);
        }

        {
            auto it = positions.erase_and_swap(positions.begin());
            expect(positions.size() == 1_u);
            expect(it == positions.begin());
            expect(it->x == 2);
            expect(it->y == 3);
        }

        positions.clear();
        expect(positions.size() == 0_ul);
        expect(positions.capacity() == 16_u);

        expect(positions.can_alloc(16u));
        for (int i = 0; i != 16; ++i)
            positions.emplace_back(i, i);

        auto ret = positions.try_emplace_back(16, 16);
        expect(ret.first == false);

        expect(positions.size() == 16_ul);
        expect(positions.capacity() == 16_u);

        auto it = positions.begin();
        while (it != positions.end())
            it = positions.erase_and_swap(it);

        expect(positions.size() == 0_ul);
        expect(positions.capacity() == 16_u);
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
        {
            irt::message vdouble;
            expect(vdouble.real[0] == 0.0);
            expect(vdouble.real[1] == 0.0);
            expect(vdouble.real[2] == 0.0);
            expect(vdouble.size() == 0_ul);
        }

        {
            irt::message vdouble(1.0);
            expect(vdouble[0] == 1.0);
            expect(vdouble.size() == 1_ul);
        }

        {
            irt::message vdouble(0.0, 1.0);
            expect(vdouble[0] == 0.0);
            expect(vdouble[1] == 1.0);
            expect(vdouble.size() == 2_ul);
        }

        {
            irt::message vdouble(1.0, 2.0, 3.0);
            expect(vdouble[0] == 1.0);
            expect(vdouble[1] == 2.0);
            expect(vdouble[2] == 3.0);
            expect(vdouble.size() == 3_ul);
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

        c1.default_value = 0.0;
        c2.default_value = 0.0;

        expect(sim.models.can_alloc(3));
        expect(irt::is_success(sim.alloc(cnt, sim.counter_models.get_id(cnt))));
        expect(irt::is_success(sim.alloc(c1, sim.constant_models.get_id(c1))));
        expect(irt::is_success(sim.alloc(c2, sim.constant_models.get_id(c2))));

        expect(sim.connect(c1.y[0], cnt.x[0]) == irt::status::success);
        expect(sim.connect(c2.y[0], cnt.x[0]) == irt::status::success);

        irt::time t = 0.0;
        expect(sim.initialize(t) == irt::status::success);

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

        // auto& value = sim.messages.alloc(3.0);
        // auto& threshold = sim.messages.alloc(0.0);
        //
        // c1.init[0] = sim.messages.get_id(value);
        // cross1.init[0] = sim.messages.get_id(threshold);

        c1.default_value = 3.0;
        cross1.default_threshold = 0.0;

        expect(sim.models.can_alloc(3));
        expect(irt::is_success(sim.alloc(cnt, sim.counter_models.get_id(cnt))));
        expect(
          irt::is_success(sim.alloc(cross1, sim.cross_models.get_id(cross1))));
        expect(irt::is_success(sim.alloc(c1, sim.constant_models.get_id(c1))));

        expect(sim.connect(c1.y[0], cross1.x[0]) == irt::status::success);
        expect(sim.connect(c1.y[0], cross1.x[1]) == irt::status::success);
        expect(sim.connect(c1.y[0], cross1.x[2]) == irt::status::success);
        expect(sim.connect(cross1.y[0], cnt.x[0]) == irt::status::success);

        irt::time t = 0.0;
        expect(sim.initialize(t) == irt::status::success);

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
        expect(
          irt::is_success(sim.alloc(gen, sim.generator_models.get_id(gen))));
        expect(irt::is_success(sim.alloc(cnt, sim.counter_models.get_id(cnt))));

        expect(sim.connect(gen.y[0], cnt.x[0]) == irt::status::success);

        expect(sim.begin == irt::time_domain<irt::time>::zero);
        expect(sim.end == irt::time_domain<irt::time>::infinity);

        sim.end = 10.0;

        irt::time t = sim.begin;
        expect(sim.initialize(t) == irt::status::success);

        irt::status st;

        do {
            st = sim.run(t);
            expect(irt::is_success(st));
            expect(cnt.number <= static_cast<irt::i64>(t));
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

        time_fun.default_f = &irt::square_time_function;

        expect(sim.models.can_alloc(2));
        expect(irt::is_success(
          sim.alloc(time_fun, sim.time_func_models.get_id(time_fun))));
        expect(irt::is_success(sim.alloc(cnt, sim.counter_models.get_id(cnt))));

        expect(sim.connect(time_fun.y[0], cnt.x[0]) == irt::status::success);

        irt::time t{ 0 };
        !expect(sim.initialize(t) == irt::status::success);
        do {
            auto st = sim.run(t);
            !expect(irt::is_success(st));
            expect(time_fun.value == t * t);
            expect(cnt.number <= static_cast<irt::i64>(t));
        } while (t < 30);
    };

    "lotka_volterra_simulation"_test = [] {
        irt::simulation sim;

        expect(irt::is_success(sim.init(32lu, 512lu)));
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

        irt::dot_writer dw(std::cout);
        dw(sim);

        file_output fo_a("lotka-volterra_a.csv");
        file_output fo_b("lotka-volterra_b.csv");
        expect(fo_a.os != nullptr);
        expect(fo_b.os != nullptr);

        auto& obs_a = sim.observers.alloc(0.01,
                                          "A",
                                          static_cast<void*>(&fo_a),
                                          file_output_initialize,
                                          &file_output_observe,
                                          nullptr);
        auto& obs_b = sim.observers.alloc(0.01,
                                          "B",
                                          static_cast<void*>(&fo_b),
                                          file_output_initialize,
                                          &file_output_observe,
                                          nullptr);

        sim.observe(sim.models.get(integrator_a.id), obs_a);
        sim.observe(sim.models.get(integrator_b.id), obs_b);

        irt::time t = 0.0;

        expect(sim.initialize(t) == irt::status::success);
        !expect(sim.sched.size() == 7_ul);

        do {
            auto st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < 15.0);
    };

    "izhikevitch_simulation"_test = [] {
        irt::simulation sim;

        expect(irt::is_success(sim.init(64lu, 256lu)));
        expect(sim.constant_models.can_alloc(3));
        expect(sim.adder_2_models.can_alloc(3));
        expect(sim.adder_4_models.can_alloc(1));
        expect(sim.mult_2_models.can_alloc(1));
        expect(sim.integrator_models.can_alloc(2));
        expect(sim.quantifier_models.can_alloc(2));
        expect(sim.cross_models.can_alloc(2));

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

        expect(sim.models.can_alloc(14));
        !expect(irt::is_success(
          sim.alloc(constant3, sim.constant_models.get_id(constant3), "tfun")));
        !expect(irt::is_success(
          sim.alloc(constant, sim.constant_models.get_id(constant), "1.0")));
        !expect(irt::is_success(sim.alloc(
          constant2, sim.constant_models.get_id(constant2), "-56.0")));

        !expect(irt::is_success(
          sim.alloc(sum_a, sim.adder_2_models.get_id(sum_a), "sum_a")));
        !expect(irt::is_success(
          sim.alloc(sum_b, sim.adder_2_models.get_id(sum_b), "sum_b")));
        !expect(irt::is_success(
          sim.alloc(sum_c, sim.adder_4_models.get_id(sum_c), "sum_c")));
        !expect(irt::is_success(
          sim.alloc(sum_d, sim.adder_2_models.get_id(sum_d), "sum_d")));

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
        !expect(irt::is_success(
          sim.alloc(cross, sim.cross_models.get_id(cross), "cross")));
        !expect(irt::is_success(
          sim.alloc(cross2, sim.cross_models.get_id(cross2), "cross2")));

        !expect(sim.models.size() == 14_ul);

        expect(sim.connect(integrator_a.y[0], cross.x[0]) ==
               irt::status::success);
        expect(sim.connect(constant2.y[0], cross.x[1]) == irt::status::success);
        expect(sim.connect(integrator_a.y[0], cross.x[2]) ==
               irt::status::success);

        expect(sim.connect(cross.y[0], quantifier_a.x[0]) ==
               irt::status::success);
        expect(sim.connect(cross.y[0], product.x[0]) == irt::status::success);
        expect(sim.connect(cross.y[0], product.x[1]) == irt::status::success);
        expect(sim.connect(product.y[0], sum_c.x[0]) == irt::status::success);
        expect(sim.connect(cross.y[0], sum_c.x[1]) == irt::status::success);
        expect(sim.connect(cross.y[0], sum_b.x[1]) == irt::status::success);

        expect(sim.connect(constant.y[0], sum_c.x[2]) == irt::status::success);
        expect(sim.connect(constant3.y[0], sum_c.x[3]) == irt::status::success);

        expect(sim.connect(sum_c.y[0], sum_a.x[0]) == irt::status::success);
        expect(sim.connect(integrator_b.y[0], sum_a.x[1]) ==
               irt::status::success);
        expect(sim.connect(cross2.y[0], sum_a.x[1]) == irt::status::success);
        expect(sim.connect(sum_a.y[0], integrator_a.x[1]) ==
               irt::status::success);
        expect(sim.connect(cross.y[0], integrator_a.x[2]) ==
               irt::status::success);
        expect(sim.connect(quantifier_a.y[0], integrator_a.x[0]) ==
               irt::status::success);

        expect(sim.connect(cross2.y[0], quantifier_b.x[0]) ==
               irt::status::success);
        expect(sim.connect(cross2.y[0], sum_b.x[0]) == irt::status::success);
        expect(sim.connect(quantifier_b.y[0], integrator_b.x[0]) ==
               irt::status::success);
        expect(sim.connect(sum_b.y[0], integrator_b.x[1]) ==
               irt::status::success);

        expect(sim.connect(cross2.y[0], integrator_b.x[2]) ==
               irt::status::success);
        expect(sim.connect(integrator_a.y[0], cross2.x[0]) ==
               irt::status::success);
        expect(sim.connect(integrator_b.y[0], cross2.x[2]) ==
               irt::status::success);
        expect(sim.connect(sum_d.y[0], cross2.x[1]) == irt::status::success);
        expect(sim.connect(integrator_b.y[0], sum_d.x[0]) ==
               irt::status::success);
        expect(sim.connect(constant.y[0], sum_d.x[1]) == irt::status::success);

        irt::dot_writer dw(std::cout);
        dw(sim);

        file_output fo_a("izhikevitch_a.csv");
        expect(fo_a.os != nullptr);

        auto& obs_a = sim.observers.alloc(0.01,
                                          "A",
                                          static_cast<void*>(&fo_a),
                                          &file_output_initialize,
                                          &file_output_observe,
                                          nullptr);

        file_output fo_b("izhikevitch_b.csv");
        expect(fo_b.os != nullptr);
        auto& obs_b = sim.observers.alloc(0.01,
                                          "B",
                                          static_cast<void*>(&fo_b),
                                          &file_output_initialize,
                                          &file_output_observe,
                                          nullptr);

        sim.observe(sim.models.get(integrator_a.id), obs_a);
        sim.observe(sim.models.get(integrator_b.id), obs_b);

        irt::time t = 0.0;

        expect(irt::status::success == sim.initialize(t));
        !expect(sim.sched.size() == 14_ul);

        do {
            irt::status st = sim.run(t);
            expect(st == irt::status::success);
        } while (t < 120);
    };
}
