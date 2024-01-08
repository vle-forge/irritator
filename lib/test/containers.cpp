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

template<typename T>
class scoped_vector_view
{
public:
    scoped_vector_view(irt::freelist_memory_resource& mem,
                       std::uint64_t&                 access) noexcept;

    ~scoped_vector_view() noexcept;

    auto vec() noexcept;

private:
    irt::freelist_memory_resource& m_mem;
    std::uint64_t&                 m_access;

    T*           m_vec_start{};
    std::int32_t m_vec_size{};
    std::int32_t m_vec_capacity{};
};

template<typename T>
scoped_vector_view<T>::scoped_vector_view(irt::freelist_memory_resource& mem,
                                          std::uint64_t& access) noexcept
  : m_mem{ mem }
  , m_access{ access }
{
    const auto left  = access >> 32;
    const auto mid   = (access >> 16) & 0xffff;
    const auto right = access & 0xffff;

    m_vec_capacity = static_cast<std::int32_t>(right);
    m_vec_size     = static_cast<std::int32_t>(mid);

    if (m_vec_capacity)
        m_vec_start = reinterpret_cast<T*>(m_mem.head() + left);
}

template<typename T>
scoped_vector_view<T>::~scoped_vector_view() noexcept
{
    if (m_vec_capacity) {
        auto       mem = reinterpret_cast<std::uintptr_t>(m_mem.head());
        auto       beg = reinterpret_cast<std::uintptr_t>(m_vec_start);
        const auto dif = beg - mem;

        irt::container::ensure(dif < std::numeric_limits<std::uint32_t>::max());
        irt::container::ensure(0 <= m_vec_size &&
                               m_vec_size <
                                 std::numeric_limits<std::uint16_t>::max());
        irt::container::ensure(0 <= m_vec_capacity &&
                               m_vec_capacity <
                                 std::numeric_limits<std::uint16_t>::max());

        const auto left  = static_cast<std::uint64_t>(dif);
        auto       mid   = static_cast<std::uint64_t>(m_vec_size);
        auto       right = static_cast<std::uint64_t>(m_vec_capacity);

        m_access = (left << 32) | (mid << 16) | right;
    } else {
        m_access = 0;
    }
}

template<typename T>
auto scoped_vector_view<T>::vec() noexcept
{
    return irt::vector_view<T, irt::mr_allocator>(
      &m_mem, m_vec_start, m_vec_size, m_vec_capacity);
}

int main()
{
    using namespace boost::ut;

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

    "vector<T>-default_allocator"_test = [] {
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

    "vector<T>-default_allocator"_test = [] {
        std::array<std::byte, 256>        buffer;
        irt::fixed_linear_memory_resource mbr{ buffer.data(), buffer.size() };

        irt::vector<int, irt::mr_allocator> v(&mbr, 8);
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

        irt::vector<int, irt::mr_allocator> v2(&mbr, 8);
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

        irt::vector<position> pos(4, 4);
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

    "data_array_api"_test = [] {
        struct position {
            position() = default;
            constexpr position(float x_)
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

    "vector_view"_test = [] {
        struct pos {
            int model;
            int port;
        };

        std::array<std::byte, 1024>   memory;
        irt::freelist_memory_resource mem{ memory.data(), memory.size() };

        pos*         data     = nullptr;
        std::int32_t size     = 0;
        std::int32_t capacity = 0;

        for (int i = 0; i < 10; ++i) {
            irt::vector_view<pos, irt::mr_allocator> view{
                &mem, data, size, capacity
            };

            view.emplace_back(123, 1);
            view.emplace_back(456, 2);
            view.emplace_back(789, 3);

            expect(eq(view[0].model, 123));
            expect(eq(view[0].port, 1));
            expect(eq(view[1].model, 456));
            expect(eq(view[1].port, 2));
            expect(eq(view[2].model, 789));
            expect(eq(view[2].port, 3));

            expect(neq(view.data(), nullptr));
            expect(eq(view.size(), 3u));
            expect(ge(view.capacity(), 3));

            view.clear();
            expect(neq(view.data(), nullptr));
            expect(eq(view.size(), 0u));
            expect(ge(view.capacity(), 3));

            view.destroy();
            expect(eq(view.data(), nullptr));
            expect(eq(view.size(), 0u));
            expect(eq(view.capacity(), 0));
        }
    };

    "vector_view_count"_test = [] {
        std::array<std::byte, 512>    memory;
        irt::freelist_memory_resource mem{ memory.data(), memory.size() };

        count_ctor_assign* data     = nullptr;
        std::int32_t       size     = 0;
        std::int32_t       capacity = 0;

        count_ctor_assign::reset();
        expect(eq(count_ctor_assign::count_ctor, 0));
        expect(eq(count_ctor_assign::count_move_ctor, 0));
        expect(eq(count_ctor_assign::count_copy_ctor, 0));
        expect(eq(count_ctor_assign::count_move_assign, 0));
        expect(eq(count_ctor_assign::count_copy_assign, 0));
        expect(eq(count_ctor_assign::count_dtor, 0));

        for (int i = 0; i < 10; ++i) {
            irt::vector_view<count_ctor_assign, irt::mr_allocator> view{
                &mem, data, size, capacity
            };

            view.emplace_back(1);
            view.emplace_back(2);
            view.emplace_back(3);

            expect(eq(count_ctor_assign::count_ctor, (i + 1) * 3));

            expect(eq(view[0].value(), 1));
            expect(eq(view[1].value(), 2));
            expect(eq(view[2].value(), 3));

            expect(neq(view.data(), nullptr));
            expect(eq(view.size(), 3u));
            expect(ge(view.capacity(), 3));

            view.clear();
            expect(eq(count_ctor_assign::count_dtor, (i + 1) * 3));

            expect(neq(view.data(), nullptr));
            expect(eq(view.size(), 0u));
            expect(ge(view.capacity(), 3));

            view.destroy();
            expect(eq(view.data(), nullptr));
            expect(eq(view.size(), 0u));
            expect(eq(view.capacity(), 0));
        }
    };

    "scoped_vector_view_count"_test = [] {
        std::array<std::byte, 512>    memory;
        irt::freelist_memory_resource mem{ memory.data(), memory.size() };

        count_ctor_assign::reset();
        expect(eq(count_ctor_assign::count_ctor, 0));
        expect(eq(count_ctor_assign::count_move_ctor, 0));
        expect(eq(count_ctor_assign::count_copy_ctor, 0));
        expect(eq(count_ctor_assign::count_move_assign, 0));
        expect(eq(count_ctor_assign::count_copy_assign, 0));
        expect(eq(count_ctor_assign::count_dtor, 0));

        for (int i = 0; i < 10; ++i) {
            std::uint64_t                         access = 0u;
            scoped_vector_view<count_ctor_assign> svv{ mem, access };

            {
                auto vec = svv.vec();

                vec.emplace_back(1);
                vec.emplace_back(2);
                vec.emplace_back(3);
            }

            expect(eq(count_ctor_assign::count_ctor, (i + 1) * 3));

            {
                auto vec = svv.vec();
                expect(eq(vec[0].value(), 1));
                expect(eq(vec[1].value(), 2));
                expect(eq(vec[2].value(), 3));

                expect(neq(vec.data(), nullptr));
                expect(eq(vec.size(), 3u));
                expect(ge(vec.capacity(), 3));
            }

            {
                auto vec = svv.vec();

                vec.clear();
                expect(eq(count_ctor_assign::count_dtor, (i + 1) * 3));

                expect(neq(vec.data(), nullptr));
                expect(eq(vec.size(), 0u));
                expect(ge(vec.capacity(), 3));
            }

            {
                auto vec = svv.vec();

                vec.destroy();
                expect(eq(vec.data(), nullptr));
                expect(eq(vec.size(), 0u));
                expect(eq(vec.capacity(), 0));
            }

            expect(!access);
        }
    };
}
