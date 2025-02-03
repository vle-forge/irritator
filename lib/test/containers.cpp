// Copyright (c) 2024 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/container.hpp>

#include <boost/ut.hpp>

#include <numeric>

struct only_copy_ctor {
    only_copy_ctor(int a_) noexcept
      : a{ a_ }
    {}
    only_copy_ctor(only_copy_ctor&&) noexcept      = default;
    only_copy_ctor(const only_copy_ctor&) noexcept = delete;

    only_copy_ctor& operator=(const only_copy_ctor&) noexcept = delete;
    only_copy_ctor& operator=(only_copy_ctor&&) noexcept      = delete;

private:
    int a;
};

struct only_move_ctor {
    only_move_ctor(int a_) noexcept
      : a{ a_ }
    {}
    only_move_ctor(only_move_ctor&&) noexcept = default;

    only_move_ctor(const only_move_ctor&) noexcept            = delete;
    only_move_ctor& operator=(const only_move_ctor&) noexcept = delete;
    only_move_ctor& operator=(only_move_ctor&&) noexcept      = delete;

private:
    int a;
};

struct count_ctor_assign {
    count_ctor_assign(int a_) noexcept
      : a{ a_ }
    {
        count_ctor++;
    }

    count_ctor_assign(count_ctor_assign&& other) noexcept
      : a{ std::exchange(other.a, 0) }
    {
        count_move_ctor++;
    }

    count_ctor_assign(const count_ctor_assign& other) noexcept
    {
        a = other.a;
        count_copy_ctor++;
    }

    count_ctor_assign& operator=(const count_ctor_assign& other) noexcept
    {
        if (this != &other) {
            a = other.a;
            count_copy_assign++;
        }
        return *this;
    }

    count_ctor_assign& operator=(count_ctor_assign&& other) noexcept
    {
        if (this != &other) {
            a = other.a;
            count_move_assign++;
        }
        return *this;
    }

    ~count_ctor_assign() noexcept { count_dtor++; }

    int value() const noexcept { return a; }

    static void reset() noexcept
    {
        count_ctor        = 0;
        count_move_ctor   = 0;
        count_copy_ctor   = 0;
        count_move_assign = 0;
        count_copy_assign = 0;
        count_dtor        = 0;
    }

    static inline int count_ctor;
    static inline int count_move_ctor;
    static inline int count_copy_ctor;
    static inline int count_move_assign;
    static inline int count_copy_assign;
    static inline int count_dtor;

private:
    int a;
};

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

int main()
{
    using namespace boost::ut;

    "allocator"_test = [] {
        using a1 = irt::allocator<irt::monotonic_small_buffer<64u>>;
        using a2 = irt::allocator<irt::monotonic_small_buffer<64u>>;
        using b1 = irt::allocator<irt::monotonic_small_buffer<32u>>;
        using b2 = irt::allocator<irt::monotonic_small_buffer<32u>>;

        using sub_1 = a1::memory_resource_type;
        using sub_2 = a2::memory_resource_type;
        using sub_3 = b1::memory_resource_type;
        using sub_4 = b2::memory_resource_type;

        auto* ptr_2 = static_cast<void*>(&sub_2::instance());
        auto* ptr_3 = static_cast<void*>(&sub_3::instance());
        auto* ptr_4 = static_cast<void*>(&sub_4::instance());
        auto* ptr_1 = static_cast<void*>(&sub_1::instance());

        expect(eq(ptr_1, ptr_2));
        expect(eq(ptr_3, ptr_4));
        expect(neq(ptr_1, ptr_3));
        expect(neq(ptr_1, ptr_4));
        expect(neq(ptr_2, ptr_3));
        expect(neq(ptr_2, ptr_4));

        using id_a1 = irt::allocator<irt::monotonic_small_buffer<64u, 1>>;
        using id_a2 = irt::allocator<irt::monotonic_small_buffer<64u, 1>>;
        using id_b1 = irt::allocator<irt::monotonic_small_buffer<64u, 2>>;
        using id_b2 = irt::allocator<irt::monotonic_small_buffer<64u, 2>>;

        using id_sub_1 = id_a1::memory_resource_type;
        using id_sub_2 = id_a2::memory_resource_type;
        using id_sub_3 = id_b1::memory_resource_type;
        using id_sub_4 = id_b2::memory_resource_type;

        auto* id_ptr_2 = static_cast<void*>(&id_sub_2::instance());
        auto* id_ptr_3 = static_cast<void*>(&id_sub_3::instance());
        auto* id_ptr_4 = static_cast<void*>(&id_sub_4::instance());
        auto* id_ptr_1 = static_cast<void*>(&id_sub_1::instance());

        expect(eq(id_ptr_1, id_ptr_2));
        expect(eq(id_ptr_3, id_ptr_4));
        expect(neq(id_ptr_1, id_ptr_3));
        expect(neq(id_ptr_1, id_ptr_4));
        expect(neq(id_ptr_2, id_ptr_3));
        expect(neq(id_ptr_2, id_ptr_4));
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

        v2.erase(v2.begin());
        expect(eq(v2.ssize(), 5));
        expect(v2[0] == 1);
        expect(v2[1] == 2);
        expect(v2[2] == 3);
        expect(v2[3] == 4);
        expect(v2[4] == 5);

        v2.erase(v2.begin() + 4);
        expect(eq(v2.ssize(), 4));
        expect(v2[0] == 1);
        expect(v2[1] == 2);
        expect(v2[2] == 3);
        expect(v2[3] == 4);

        v2.erase(v2.begin() + 2);
        expect(eq(v2.ssize(), 3));
        expect(v2[0] == 1);
        expect(v2[1] == 2);
        expect(v2[2] == 4);
    };

    "vector<T>-default_allocator"_test = [] {
        irt::vector<int> v(8, irt::reserve_tag{});
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

    "vector<T>-monotonic_allocator"_test = [] {
        using fixed_alloc = irt::allocator<irt::monotonic_small_buffer<8192u>>;

        irt::vector<int, fixed_alloc> v(8, irt::reserve_tag{});
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

        irt::vector<int, fixed_alloc> v2(8, irt::reserve_tag{});
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
        irt::vector<int> vec(4, irt::reserve_tag{});

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

            explicit t_1(int x_) noexcept
              : x(x_)
            {}

            t_1& operator=(int x_) noexcept
            {
                x = x_;
                return *this;
            }
        };

        irt::vector<t_1> v_1(10);
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

            explicit toto(int i_) noexcept
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

    // "list"_test = [] {
    //     irt::block_allocator<irt::list_view_node<int>> allocator;
    //     expect(!!allocator.init(32));

    //     irt::u64       id = static_cast<irt::u64>(-1);
    //     irt::list_view lst(allocator, id);

    //     lst.emplace_front(5);
    //     lst.emplace_front(4);
    //     lst.emplace_front(3);
    //     lst.emplace_front(2);
    //     lst.emplace_front(1);

    //     {
    //         int i = 1;
    //         for (auto it = lst.begin(); it != lst.end(); ++it)
    //             expect(*it == i++);
    //     }

    //     lst.pop_front();

    //     {
    //         int i = 2;
    //         for (auto it = lst.begin(); it != lst.end(); ++it)
    //             expect(*it == i++);
    //     }
    // };

    // "double_list"_test = [] {
    //     irt::block_allocator<irt::list_view_node<int>> allocator;
    //     expect(!!allocator.init(32));

    //     irt::u64       id = static_cast<irt::u64>(-1);
    //     irt::list_view lst(allocator, id);

    //     expect(lst.empty());
    //     expect(lst.begin() == lst.end());

    //     lst.emplace_front(0);
    //     expect(lst.begin() == --lst.end());
    //     expect(++lst.begin() == lst.end());

    //     lst.clear();
    //     expect(lst.empty());
    //     expect(lst.begin() == lst.end());

    //     lst.emplace_front(5);
    //     lst.emplace_front(4);
    //     lst.emplace_front(3);
    //     lst.emplace_front(2);
    //     lst.emplace_front(1);
    //     lst.emplace_back(6);
    //     lst.emplace_back(7);
    //     lst.emplace_back(8);

    //     {
    //         int i = 1;
    //         for (auto it = lst.begin(); it != lst.end(); ++it)
    //             expect(*it == i++);
    //     }

    //     lst.pop_front();

    //     {
    //         int i = 2;
    //         for (auto it = lst.begin(); it != lst.end(); ++it)
    //             expect(*it == i++);
    //     }

    //     {
    //         auto it = lst.begin();
    //         expect(*it == 2);

    //         --it;
    //         expect(it == lst.end());

    //         --it;
    //         expect(it == --lst.end());
    //     }

    //     {
    //         auto it = lst.end();
    //         expect(it == lst.end());

    //         --it;
    //         expect(*it == 8);

    //         --it;
    //         expect(*it == 7);
    //     }

    //     lst.emplace(lst.begin(), 10);
    //     expect(*lst.begin() == 10);

    //     {
    //         auto it = lst.begin();
    //         ++it;

    //         it = lst.emplace(it, 11);
    //         expect(*it == 11);
    //         expect(*lst.begin() == 10);
    //     }
    // };

    "vector"_test = [] {
        struct position {
            position() noexcept = default;

            position(float x_, float y_) noexcept
              : x(x_)
              , y(y_)
            {}

            float x = 0, y = 0;
        };

        irt::vector<position> pos(4);
        pos[0].x = 0;
        pos[1].x = 1;
        pos[2].x = 2;
        pos[3].x = 3;

        pos.emplace_back(4.f, 0.f);
        expect((pos.size() == 5) >> fatal);
        expect((pos.capacity() == 4 + 4 / 2));
    };

    // "table"_test = [] {
    //     struct position {
    //         position() noexcept = default;
    //         position(float x_) noexcept
    //           : x(x_)
    //         {}

    //         float x = 0;
    //     };

    //     irt::table<int, position> tbl;
    //     tbl.data.reserve(10);

    //     tbl.data.emplace_back(4, 4.f);
    //     tbl.data.emplace_back(3, 3.f);
    //     tbl.data.emplace_back(2, 2.f);
    //     tbl.data.emplace_back(1, 1.f);
    //     tbl.sort();
    //     expect(tbl.data.size() == 4);
    //     expect(tbl.data.capacity() == 10);
    //     tbl.set(0, 0.f);

    //     expect(tbl.data.size() == 5);
    //     expect(tbl.data.capacity() == 10);
    //     expect(tbl.data[0].id == 0);
    //     expect(tbl.data[1].id == 1);
    //     expect(tbl.data[2].id == 2);
    //     expect(tbl.data[3].id == 3);
    //     expect(tbl.data[4].id == 4);
    //     expect(tbl.data[0].value.x == 0.f);
    //     expect(tbl.data[1].value.x == 1.f);
    //     expect(tbl.data[2].value.x == 2.f);
    //     expect(tbl.data[3].value.x == 3.f);
    //     expect(tbl.data[4].value.x == 4.f);
    // };

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

    "ring-buffer-head"_test = [] {
        irt::ring_buffer<int> ring{ 10 };

        for (int i = 0; i < 9; ++i) {
            auto is_success = ring.emplace_tail(i);
            expect(is_success == true);
        }

        {
            auto is_success = ring.emplace_tail(9);
            expect(is_success == false);
        }

        expect(eq(*ring.head(), 0));
        expect(eq(*ring.tail(), 8));

        expect(ring.data()[0] == 0);
        expect(ring.data()[1] == 1);
        expect(ring.data()[2] == 2);
        expect(ring.data()[3] == 3);
        expect(ring.data()[4] == 4);
        expect(ring.data()[5] == 5);
        expect(ring.data()[6] == 6);
        expect(ring.data()[7] == 7);
        expect(ring.data()[8] == 8);

        for (int i = 10; i < 15; ++i) {
            ring.pop_head();
            ring.emplace_tail(i);
        }

        expect(eq(*ring.head(), 5));
        expect(eq(*ring.tail(), 14));

        auto it = ring.head();
        expect(eq(*it++, 5));
        expect(eq(*it++, 6));
        expect(eq(*it++, 7));
        expect(eq(*it++, 8));
        expect(eq(*it++, 10));
        expect(eq(*it++, 11));
        expect(eq(*it++, 12));
        expect(eq(*it++, 13));
        expect(eq(*it++, 14));

        {
            auto h = ring.head();

            for (auto it = ring.begin(), et = ring.end(); it != et; ++it, ++h)
                expect(eq(*it, *h));
        }

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

    "small-ring-buffer"_test = [] {
        irt::small_ring_buffer<int, 10> ring;

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

    "small-ring-buffer-front-back-access"_test = [] {
        irt::small_ring_buffer<int, 4> ring;

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

    "id_array_api"_test = [] {
        enum class position32_id : std::uint32_t;

        irt::id_array<position32_id> ids(4);
        irt::vector<float>           x(4);
        irt::vector<float>           y(4);
        irt::vector<float>           sum(4);

        expect(eq(ids.size(), 0u));
        expect(eq(ids.max_used(), 0));
        expect(eq(ids.capacity(), 4));
        expect(eq(ids.next_key(), 1));
        expect(ids.is_free_list_empty());
        expect(ids.can_alloc(1) == true);

        auto id_1  = ids.alloc();
        auto id_2  = ids.alloc();
        auto id_3  = ids.alloc();
        auto id_4  = ids.alloc();
        auto idx_1 = irt::get_index(id_1);
        auto idx_2 = irt::get_index(id_2);
        auto idx_3 = irt::get_index(id_3);
        auto idx_4 = irt::get_index(id_4);
        expect(eq(idx_1, 0));
        expect(eq(idx_2, 1));
        expect(eq(idx_3, 2));
        expect(eq(idx_4, 3));
        expect(ids.can_alloc(1) == false);
        x[idx_1] = 1.f;
        y[idx_1] = 2.f;
        x[idx_2] = 30.f;
        y[idx_2] = 40.f;
        x[idx_3] = 500.f;
        y[idx_3] = 600.f;
        x[idx_4] = 7000.f;
        y[idx_4] = 8000.f;

        auto do_clear = [](auto& vec) noexcept {
            std::fill_n(vec.data(), vec.size(), 0.f);
        };

        auto do_sum = [&]() noexcept {
            const position32_id* id = nullptr;
            while (ids.next(id)) {
                const auto idx = irt::get_index(*id);
                sum[idx]       = x[idx] + y[idx];
            }
        };

        do_clear(sum);
        do_sum();

        expect(eq(sum[idx_1], 3.f));
        expect(eq(sum[idx_2], 70.f));
        expect(eq(sum[idx_3], 1100.f));
        expect(eq(sum[idx_4], 15000.f));

        expect(eq(ids.size(), 4u));
        expect(eq(ids.max_used(), 4));
        expect(eq(ids.capacity(), 4));
        expect(eq(ids.next_key(), 5));
        expect(ids.is_free_list_empty());

        ids.free(id_3);
        ids.free(id_4);

        expect(eq(ids.size(), 2u));
        expect(eq(ids.max_used(), 4));
        expect(eq(ids.capacity(), 4));
        expect(eq(ids.next_key(), 5));
        expect(not ids.is_free_list_empty());

        do_clear(sum);
        do_sum();

        expect(eq(sum[idx_1], 3.f));
        expect(eq(sum[idx_2], 70.f));
        expect(eq(sum[idx_3], 0.f));
        expect(eq(sum[idx_4], 0.f));

        ids.clear();
        expect(eq(ids.size(), 0u));
        expect(eq(ids.max_used(), 0));
        expect(eq(ids.capacity(), 4));
        expect(eq(ids.next_key(), 5));
        expect(ids.is_free_list_empty());

        expect(ids.can_alloc(1) == true);
    };

    "data_array_api"_test = [] {
        struct position {
            position() = default;

            explicit constexpr position(float x_)
              : x(x_)
            {}

            float x;
        };

        enum class position32_id : std::uint32_t;
        enum class position64_id : std::uint64_t;

        irt::data_array<position, position32_id> small_array{ 32 };
        irt::data_array<position, position64_id> array{ 32 };

        expect(small_array.max_size() == 0);
        expect(small_array.max_used() == 0);
        expect(small_array.capacity() == 32);
        expect(small_array.next_key() == 1);
        expect(small_array.is_free_list_empty());

        expect(small_array.max_size() == 0);
        expect(small_array.max_used() == 0);
        expect(small_array.capacity() == 32);
        expect(small_array.next_key() == 1);
        expect(small_array.is_free_list_empty());

        expect(array.max_size() == 0);
        expect(array.max_used() == 0);
        expect(array.capacity() == 32);
        expect(array.next_key() == 1);
        expect(array.is_free_list_empty());

        {
            auto& first = array.alloc();
            first.x     = 0.f;
            expect(array.max_size() == 1);
            expect(array.max_used() == 1);
            expect(array.capacity() == 32);
            expect(array.next_key() == 2);
            expect(array.is_free_list_empty());

            auto& second = array.alloc();
            expect(array.max_size() == 2);
            expect(array.max_used() == 2);
            expect(array.capacity() == 32);
            expect(array.next_key() == 3);
            expect(array.is_free_list_empty());

            second.x = 1.f;

            auto& third = array.alloc();
            expect(array.max_size() == 3);
            expect(array.max_used() == 3);
            expect(array.capacity() == 32);
            expect(array.next_key() == 4);
            expect(array.is_free_list_empty());

            third.x = 2.f;

            for (int i = array.max_size(); i < array.capacity(); ++i)
              [[maybe_unused]]
                auto x = array.alloc();

            expect(array.full());
        }

        array.clear();

        expect(array.max_size() == 0);
        expect(array.max_used() == 0);
        expect(array.capacity() == 32);
        expect(array.next_key() == 1);
        expect(array.is_free_list_empty());

        auto& d1 = array.alloc(1.f);
        auto& d2 = array.alloc(2.f);
        auto& d3 = array.alloc(3.f);

        expect(check_data_array_loop(array));

        expect(array.max_size() == 3);
        expect(array.max_used() == 3);
        expect(array.capacity() == 32);
        expect(array.next_key() == 4);
        expect(array.is_free_list_empty());

        array.free(d1);

        expect(check_data_array_loop(array));

        expect(array.max_size() == 2);
        expect(array.max_used() == 3);
        expect(array.capacity() == 32);
        expect(array.next_key() == 4);
        expect(!array.is_free_list_empty());

        array.free(d2);

        expect(check_data_array_loop(array));

        expect(array.max_size() == 1);
        expect(array.max_used() == 3);
        expect(array.capacity() == 32);
        expect(array.next_key() == 4);
        expect(!array.is_free_list_empty());

        array.free(d3);

        expect(check_data_array_loop(array));

        expect(array.max_size() == 0);
        expect(array.max_used() == 3);
        expect(array.capacity() == 32);
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
        expect(array.capacity() == 32);
        expect(array.next_key() == 7);
        expect(array.is_free_list_empty());

        expect(check_data_array_loop(array));
    };

    constexpr static int Size = 10;
    using svec                = irt::small_vector<int, Size>;
    using vec                 = irt::vector<int>;

    "test_default_constructor"_test = [] {
        svec sdata;
        vec  data;

        expect(eq(data.size(), 0u));
        expect(data.empty());
        expect(eq(data.capacity(), 0));
        expect(data.begin() == data.end());

        expect(eq(sdata.size(), 0u));
        expect(sdata.empty());
        expect(eq(sdata.capacity(), Size));
        expect(sdata.begin() == sdata.end());
    };

    "test_initializer_list_constructor"_test = [] {
        svec sdata{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        vec  data{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        expect(!data.empty());
        expect(data.full());
        expect(data.begin() != data.end());
        expect(eq(0, data.capacity() - data.ssize()));
        expect(eq(10, data.capacity()));
        expect(eq(10, data.ssize()));

        expect(!sdata.empty());
        expect(sdata.full());
        expect(sdata.begin() != sdata.end());
        expect(eq(0, sdata.capacity() - sdata.ssize()));
        expect(eq(10, sdata.capacity()));
        expect(eq(10, sdata.ssize()));

        expect(data == sdata);
    };

    "test_copy_constructor"_test = [] {
        std::vector<int> sorted_vec{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        {
            vec data(sorted_vec.begin(), sorted_vec.end());
            vec data2(data);
            expect(data2 == data);

            data2[2] = -1;
            expect(data2 != data);
        }

        {
            svec data(sorted_vec.begin(), sorted_vec.end());
            svec data2(data);
            expect(data2 == data);

            data2[2] = -1;
            expect(data2 != data);
        }
    };

    "test_move_constructor"_test = [] {
        std::vector<int> sorted_vec{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        {
            vec data(sorted_vec.begin(), sorted_vec.end());
            vec data2(std::move(data));
            expect(data.size() == 0);
            expect(data2.size() == sorted_vec.size());
            expect(data2 != data);
        }

        {
            svec data(sorted_vec.begin(), sorted_vec.end());
            svec data2(std::move(data));
            expect(data.size() == 0);
            expect(data2.size() == sorted_vec.size());
            expect(data2 != data);
        }
    };

    "test_assignment"_test = [] {
        std::vector<int> sorted_vec{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        {
            vec data(sorted_vec.begin(), sorted_vec.end());
            vec other_data;

            other_data = data;

            bool is_equal =
              std::equal(data.begin(), data.end(), other_data.begin());

            expect(is_equal);
        }

        {
            svec data(sorted_vec.begin(), sorted_vec.end());
            svec other_data;

            other_data = data;

            bool is_equal =
              std::equal(data.begin(), data.end(), other_data.begin());

            expect(is_equal);
        }
    };

    "test_move_assignment"_test = [] {
        std::vector<int> sorted_vec{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        {
            vec data(sorted_vec.begin(), sorted_vec.end());
            vec other_data;
            other_data.push_back(1);

            other_data = std::move(data);

            expect(data.size() == 0);
            expect(other_data.size() == sorted_vec.size());
            expect(data != other_data);
        }
        {
            svec data(sorted_vec.begin(), sorted_vec.end());
            svec other_data;
            other_data.push_back(1);

            other_data = std::move(data);

            expect(data.size() == 0);
            expect(other_data.size() == sorted_vec.size());
            expect(data != other_data);
        }
    };

    "test_self_assignment"_test = [] {
        std::vector<int> sorted_vec{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        {
            vec data(sorted_vec.begin(), sorted_vec.end());
            vec other_data(data);

            other_data = other_data;

            bool is_equal =
              std::equal(data.begin(), data.end(), other_data.begin());

            expect(is_equal);
        }
        {
            svec data(sorted_vec.begin(), sorted_vec.end());
            svec other_data(data);

            other_data = other_data;

            bool is_equal =
              std::equal(data.begin(), data.end(), other_data.begin());

            expect(is_equal);
        }
    };

    "test_begin"_test = [] {
        {
            vec       data(10);
            const vec constData(10);

            expect(eq(&data[0], data.begin()));
            expect(eq(&constData[0], constData.begin()));
        }
        {
            svec       data(10);
            const svec constData(10);

            expect(eq(&data[0], data.begin()));
            expect(eq(&constData[0], constData.begin()));
        }
    };

    "test_end"_test = [] {
        {
            vec       data(10);
            const vec constData(10);

            expect(eq((&data[9]) + 1, data.end()));
            expect(eq((&constData[9]) + 1, constData.end()));
        }
        {
            svec       data(10);
            const svec constData(10);

            expect(eq((&data[9]) + 1, data.end()));
            expect(eq((&constData[9]) + 1, constData.end()));
        }
    };

    "test_resize_up"_test = [] {
        const size_t INITIAL_SIZE = 5;
        const size_t NEW_SIZE     = 8;
        {

            vec data(INITIAL_SIZE);
            data.resize(NEW_SIZE);

            expect(eq(data.size(), NEW_SIZE));
        }
        {
            svec data(INITIAL_SIZE);
            data.resize(NEW_SIZE);

            expect(eq(data.size(), NEW_SIZE));
        }
    };

    "test_resize_up_value"_test = [] {
        const size_t INITIAL_SIZE  = 5;
        const size_t NEW_SIZE      = 8;
        const int    INITIAL_VALUE = 1;
        {

            vec data(INITIAL_SIZE, INITIAL_VALUE);
            data.resize(NEW_SIZE, INITIAL_VALUE);

            std::array<int, NEW_SIZE> compare_data;
            compare_data.fill(INITIAL_VALUE);

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
        {
            svec data(INITIAL_SIZE, INITIAL_VALUE);
            data.resize(NEW_SIZE, INITIAL_VALUE);

            std::array<int, NEW_SIZE> compare_data;
            compare_data.fill(INITIAL_VALUE);

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
    };

    "test_resize_excess"_test = [] {
        const size_t INITIAL_SIZE = 5;
        const size_t NEW_SIZE     = Size + 1;

        {

            vec data(INITIAL_SIZE, INITIAL_SIZE);
            data.resize(NEW_SIZE);

            expect(data.capacity() == NEW_SIZE);
        }
        {
            svec data(INITIAL_SIZE, INITIAL_SIZE);
            data.resize(NEW_SIZE);

            expect(data.capacity() == NEW_SIZE - 1);
        }
    };

    "test_resize_down"_test = [] {
        const size_t INITIAL_SIZE = 5;
        const size_t NEW_SIZE     = 2;

        {
            vec data(INITIAL_SIZE);
            data.resize(NEW_SIZE);

            expect(eq(data.size(), NEW_SIZE));
        }
        {
            svec data(INITIAL_SIZE);
            data.resize(NEW_SIZE);

            expect(eq(data.size(), NEW_SIZE));
        }
    };

    "test_resize_down_value"_test = [] {
        const size_t INITIAL_SIZE  = 5;
        const size_t NEW_SIZE      = 2;
        const int    INITIAL_VALUE = 1;

        {
            vec data(INITIAL_SIZE);
            data.resize(NEW_SIZE, INITIAL_VALUE);

            expect(eq(data.size(), NEW_SIZE));
        }
        {
            svec data(INITIAL_SIZE);
            data.resize(NEW_SIZE, INITIAL_VALUE);

            expect(eq(data.size(), NEW_SIZE));
        }
    };

    "test_push_back"_test = [] {
        std::vector<int> compare_data;
        for (int i = 0; i < Size; ++i) {
            compare_data.push_back(i);
        }

        {
            vec data;

            for (int i = 0; i < Size; ++i) {
                data.push_back(i);
            }

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
        {
            svec data;

            for (int i = 0; i < Size; ++i) {
                data.push_back(i);
            }

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
    };

    "test_push_back_literal"_test = [] {
        std::vector<int> compare_data;
        compare_data.push_back(1);
        compare_data.push_back(2);
        compare_data.push_back(3);
        compare_data.push_back(4);

        {
            vec data;

            data.push_back(1);
            data.push_back(2);
            data.push_back(3);
            data.push_back(4);

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
        {
            svec data;

            data.push_back(1);
            data.push_back(2);
            data.push_back(3);
            data.push_back(4);

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
    };

    "test_push_back_excess"_test = [] {
        {
            vec data;

            for (int i = 0; i < Size; ++i) {
                data.push_back(i);
            }

            if (data.capacity() == data.ssize())
                expect(not data.can_alloc(1));
            else
                expect(data.can_alloc(1));
        }
        {
            svec data;

            for (int i = 0; i < Size; ++i) {
                data.push_back(i);
            }

            expect(not data.can_alloc(1));
        }
    };

    "test_emplace_back"_test = [] {
        std::vector<int> compare_data;
        for (int i = 0; i < Size; ++i) {
            compare_data.emplace_back(i);
        }

        {
            vec data;

            for (int i = 0; i < Size; ++i) {
                data.emplace_back(i);
            }

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
        {
            svec data;

            for (int i = 0; i < Size; ++i) {
                data.emplace_back(i);
            }

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
    };

    "test_emplace_back_return"_test = [] {
        {
            vec data;

            data.emplace_back(24);
            auto back = data.emplace_back(42);
            expect(eq(back, data.back()));
        }
        {
            svec data;

            data.emplace_back(24);
            auto back = data.emplace_back(42);
            expect(eq(back, data.back()));
        }
    };

    "test_pop_back"_test = [] {
        std::vector<int> compare_data{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        {
            vec data(compare_data.begin(), compare_data.end());

            compare_data.pop_back();
            compare_data.pop_back();

            data.pop_back();
            data.pop_back();

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
        {
            svec data(compare_data.begin(), compare_data.end());

            compare_data.pop_back();
            compare_data.pop_back();

            data.pop_back();
            data.pop_back();

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.begin(), data.end(), compare_data.begin());

            expect(is_equal);
        }
    };

    "test_pop_back_exception"_test = [] {
        {
            vec data;

            data.resize(2);

            data.pop_back();
            data.pop_back();

            expect(data.size() == 0);
        }
        {
            svec data;

            data.resize(2);

            data.pop_back();
            data.pop_back();

            expect(data.size() == 0);
        }
    };

    "test_insert_position_value"_test = [] {
        const size_t     INITIAL_SIZE  = 5;
        const int        INITIAL_VALUE = 1;
        std::vector<int> sorted_vec{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };

        for (size_t offset = 0; offset <= INITIAL_SIZE; ++offset) {
            std::vector<int> compare_data;
            vec              data;

            data.assign(sorted_vec.begin(), sorted_vec.begin() + INITIAL_SIZE);
            compare_data.assign(sorted_vec.begin(),
                                sorted_vec.begin() + INITIAL_SIZE);
            expect(eq(compare_data.size(), data.size()));

            data.insert(data.cbegin() + offset, INITIAL_VALUE);
            compare_data.insert(compare_data.cbegin() + offset, INITIAL_VALUE);

            expect(eq(compare_data.size(), data.size()));

            bool is_equal =
              std::equal(data.cbegin(), data.cend(), compare_data.cbegin());

            expect(is_equal);
        }
    };
}
