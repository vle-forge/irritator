// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/ut.hpp>

#include <irritator/error.hpp>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ios>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

using namespace boost::ut;

namespace {

using irt::modeling_errc;
using irt::project_errc;
using irt::simulation_errc;

constexpr irt::error_code range_err{ simulation_errc::range_error };
constexpr irt::error_code file_err{ modeling_errc::file_error };

/**
 * Counts live instances: detects leaks, double destruction and values built
 * or destroyed at the wrong moment by expected<T>.
 */
struct tracker {
    static inline int alive = 0;

    int v = 0;

    explicit tracker(int x) noexcept
      : v(x)
    {
        ++alive;
    }

    tracker(const tracker& o) noexcept
      : v(o.v)
    {
        ++alive;
    }

    tracker(tracker&& o) noexcept
      : v(o.v)
    {
        o.v = -1;
        ++alive;
    }

    tracker& operator=(const tracker&) noexcept = default;

    tracker& operator=(tracker&& o) noexcept
    {
        v   = o.v;
        o.v = -1;
        return *this;
    }

    ~tracker() { --alive; }

    friend bool operator==(const tracker& a, const tracker& b) noexcept
    {
        return a.v == b.v;
    }
};

/** A type that can neither be copied nor moved. */
struct pinned {
    explicit pinned(int x) noexcept
      : v(x)
    {}

    pinned(const pinned&)            = delete;
    pinned& operator=(const pinned&) = delete;

    int v;
};

/** A type whose copy may throw. */
struct throwing_copy {
    throwing_copy() = default;
    throwing_copy(const throwing_copy&) {}
};

// -- error_enum / error_code -------------------------------------------------

static_assert(irt::error_enum<irt::simulation_errc>);
static_assert(irt::error_enum<irt::modeling_errc>);
static_assert(irt::error_enum<irt::project_errc>);
static_assert(!irt::error_enum<std::errc>);
static_assert(!irt::error_enum<int>);

static_assert(std::is_constructible_v<irt::error_code, simulation_errc>);
static_assert(std::is_constructible_v<irt::error_code, std::errc>);
// Unsupported enums are rejected by overload resolution, not by a hard error.
static_assert(!std::is_constructible_v<irt::error_code, std::io_errc>);
static_assert(!std::is_convertible_v<int, irt::error_code>);

static_assert(std::is_trivially_copyable_v<irt::error_code>);
static_assert(sizeof(irt::error_code) == 4);

// -- expected: layout and special members ------------------------------------

// A trivial T gives a trivial expected<T>: required to be returned in
// registers by the Itanium and Windows x64 ABIs.
static_assert(std::is_trivially_destructible_v<irt::expected<int>>);
static_assert(std::is_trivially_copyable_v<irt::expected<int>>);
static_assert(std::is_trivially_copy_constructible_v<irt::expected<int>>);
static_assert(std::is_trivially_move_constructible_v<irt::expected<int>>);
static_assert(std::is_trivially_copy_assignable_v<irt::expected<int>>);
static_assert(std::is_trivially_move_assignable_v<irt::expected<int>>);
static_assert(std::is_nothrow_copy_constructible_v<irt::expected<int>>);
static_assert(std::is_nothrow_swappable_v<irt::expected<int>>);

// Same for the other trivially copyable T (the choice is made from T alone,
// without conditionally trivial special members: they are not reliably
// supported by every compiler).
struct pod {
    int   a;
    float b;
};

static_assert(std::is_trivially_copyable_v<irt::expected<double>>);
static_assert(std::is_trivially_copyable_v<irt::expected<std::int64_t>>);
static_assert(std::is_trivially_copyable_v<irt::expected<pod>>);
static_assert(std::is_trivially_destructible_v<irt::expected<pod>>);
static_assert(!std::is_trivially_copyable_v<irt::expected<std::string>>);
static_assert(!std::is_trivially_copyable_v<irt::expected<tracker>>);

static_assert(std::is_trivially_destructible_v<irt::status>);
static_assert(std::is_trivially_copyable_v<irt::status>);
static_assert(sizeof(irt::status) == sizeof(irt::error_code));

static_assert(!std::is_trivially_destructible_v<irt::expected<std::string>>);
static_assert(std::is_copy_constructible_v<irt::expected<std::string>>);
static_assert(std::is_nothrow_move_constructible_v<irt::expected<std::string>>);
// The copy of a std::string can allocate: it is not noexcept.
static_assert(
  !std::is_nothrow_copy_constructible_v<irt::expected<std::string>>);
static_assert(
  !std::is_nothrow_copy_constructible_v<irt::expected<throwing_copy>>);

// Move-only T: expected<T> is move-only too, and says so.
static_assert(
  std::is_move_constructible_v<irt::expected<std::unique_ptr<int>>>);
static_assert(std::is_move_assignable_v<irt::expected<std::unique_ptr<int>>>);
static_assert(
  !std::is_copy_constructible_v<irt::expected<std::unique_ptr<int>>>);
static_assert(!std::is_copy_assignable_v<irt::expected<std::unique_ptr<int>>>);
static_assert(std::is_swappable_v<irt::expected<std::unique_ptr<int>>>);

// Neither copyable nor movable T.
static_assert(!std::is_move_constructible_v<irt::expected<pinned>>);
static_assert(!std::is_copy_constructible_v<irt::expected<pinned>>);
static_assert(!std::is_default_constructible_v<irt::expected<pinned>>);

// Value category of the accessors.
static_assert(
  std::is_same_v<decltype(*std::declval<irt::expected<int>>()), int&&>);
static_assert(
  std::is_same_v<decltype(*std::declval<const irt::expected<int>&>()),
                 const int&>);
static_assert(
  std::is_same_v<decltype(std::declval<irt::expected<int>>().error()),
                 irt::error_code>);

// The monadic functions change the value type.
static_assert(
  std::is_same_v<decltype(std::declval<irt::expected<int>&>().transform(
                   [](int) noexcept { return 1.5; })),
                 irt::expected<double>>);

// Values of unrelated types are not comparable (the concept makes the failed
// expression a substitution failure instead of a hard error).
template<typename A, typename B>
concept comparable = requires(const A& a, const B& b) { a == b; };

static_assert(!comparable<irt::expected<int>, irt::expected<std::string>>);
static_assert(comparable<irt::expected<int>, irt::expected<long>>);
static_assert(comparable<irt::expected<int>, int>);
static_assert(comparable<irt::expected<int>, irt::error_code>);
static_assert(!comparable<irt::expected<int>, std::string>);

// -- expected: usable in constant expressions --------------------------------

constexpr bool constexpr_scenario()
{
    irt::expected<int> a(3);
    irt::expected<int> b(range_err);

    a.swap(b);
    if (!a.has_error() || a.error() != range_err)
        return false;
    if (!b.has_value() || b.value() != 3)
        return false;

    a = 7;
    if (!a.has_value() || *a != 7)
        return false;

    a = file_err;
    if (!a.has_error() || a.error() != file_err)
        return false;

    const auto doubled = b.transform(
      [](int x) noexcept { return static_cast<double>(x) * 2.0; });
    if (!doubled.has_value() || *doubled != 6.0)
        return false;

    const auto chained = b.and_then([](int x) noexcept {
        return irt::expected<long>(static_cast<long>(x) + 1);
    });
    if (!chained.has_value() || *chained != 4L)
        return false;

    irt::status s = irt::success();
    if (!s.has_value())
        return false;

    s = range_err;
    return s.has_error() && s.error() == range_err;
}

static_assert(constexpr_scenario());

// -- helpers used by several tests -------------------------------------------

int  error_calls = 0;
void count_error() noexcept { ++error_calls; }

irt::status step(int n) noexcept
{
    if (n < 0)
        return range_err;

    return irt::success();
}

irt::status run_two(int a, int b) noexcept
{
    irt_check(step(a));
    irt_check(step(b));
    return irt::success();
}

irt::expected<int> run_value(int a) noexcept
{
    irt_check(step(a));
    return a * 2;
}

// `irt_check` must be a single statement: this does not compile with a bare
// `{ ... }` macro because of the `;` before the `else`.
irt::status run_if_else(bool c, int a) noexcept
{
    if (c)
        irt_check(step(a));
    else
        return file_err;

    return irt::success();
}

template<typename T>
void check_swap(const T& x, const T& y)
{
    using E = irt::expected<T>;

    {
        E a(x);
        E b(y);
        a.swap(b);
        expect(a.has_value() && b.has_value());
        expect(*a == y);
        expect(*b == x);
    }

    {
        E a(x);
        E b(range_err);
        a.swap(b);
        expect(a.has_error() && b.has_value());
        expect(a.error() == range_err) << "error code corrupted by swap";
        expect(*b == x);
    }

    {
        E a(range_err);
        E b(x);
        a.swap(b);
        expect(a.has_value() && b.has_error());
        expect(*a == x);
        expect(b.error() == range_err) << "error code corrupted by swap";
    }

    {
        E a(range_err);
        E b(file_err);
        a.swap(b);
        expect(a.error() == file_err);
        expect(b.error() == range_err);
    }

    {
        E a(x);
        a.swap(a);
        expect(a.has_value() && *a == x);

        E b(range_err);
        b.swap(b);
        expect(b.has_error() && b.error() == range_err);
    }

    {
        // Through the free function found by ADL.
        E a(x);
        E b(file_err);
        using std::swap;
        swap(a, b);
        expect(a.has_error() && a.error() == file_err);
        expect(b.has_value() && *b == x);
    }
}

// -- tests -------------------------------------------------------------------

void test_error_code()
{
    "error_code: default is 'no error'"_test = [] {
        constexpr irt::error_code ec{};

        expect(!static_cast<bool>(ec));
        expect(eq(ec.value(), std::int16_t{ 0 }));
        expect(ec.cat() == irt::category::generic);
    };

    "error_code: the category follows the enumeration"_test = [] {
        constexpr irt::error_code s{ simulation_errc::range_error };
        constexpr irt::error_code m{ modeling_errc::file_error };
        constexpr irt::error_code p{ project_errc::empty_project };

        expect(s.cat() == irt::category::simulation);
        expect(m.cat() == irt::category::modeling);
        expect(p.cat() == irt::category::project);

        expect(static_cast<bool>(s));
        expect(eq(s.value(),
                  static_cast<std::int16_t>(simulation_errc::range_error)));
        expect(
          eq(m.value(), static_cast<std::int16_t>(modeling_errc::file_error)));
        expect(eq(p.value(),
                  static_cast<std::int16_t>(project_errc::empty_project)));
    };

    "error_code: std::errc and (value, category) constructors"_test = [] {
        constexpr irt::error_code a{ std::errc::invalid_argument };
        constexpr irt::error_code b{ std::int16_t{ 5 },
                                     irt::category::project };

        expect(a.cat() == irt::category::generic);
        expect(eq(a.value(),
                  static_cast<std::int16_t>(std::errc::invalid_argument)));
        expect(b.cat() == irt::category::project);
        expect(eq(b.value(), std::int16_t{ 5 }));
    };

    "error_code: equality compares the value and the category"_test = [] {
        // memory_error is 1 in both enumerations.
        constexpr irt::error_code s{ simulation_errc::memory_error };
        constexpr irt::error_code m{ modeling_errc::memory_error };

        expect(eq(s.value(), m.value()));
        expect(s != m);
        expect(s == irt::error_code{ simulation_errc::memory_error });
        expect(!(s != irt::error_code{ simulation_errc::memory_error }));
    };
}

void test_make_error()
{
    "make_error: calls on_error_callback, constructors do not"_test = [] {
        irt::on_error_callback = &count_error;
        error_calls            = 0;

        const auto a = irt::make_error(simulation_errc::range_error);
        expect(eq(error_calls, 1));
        expect(a == range_err);

        const auto b = irt::make_error(std::int16_t{ 3 },
                                       irt::category::project);
        expect(eq(error_calls, 2));
        expect(b.cat() == irt::category::project);

        // The std::errc overload used to skip the callback.
        const auto c = irt::make_error(std::errc::invalid_argument);
        expect(eq(error_calls, 3));
        expect(c.cat() == irt::category::generic);

        const irt::error_code d{ simulation_errc::range_error };
        expect(eq(error_calls, 3)) << "a plain constructor must not call it";
        expect(d == range_err);

        irt::on_error_callback = nullptr;
    };
}

void test_expected_observers()
{
    "expected<T>: value state"_test = [] {
        irt::expected<int> e(42);

        expect(e.has_value());
        expect(static_cast<bool>(e));
        expect(!e.has_error());
        expect(eq(e.value(), 42));
        expect(eq(*e, 42));
        expect(eq(e.value_or(7), 42));
        expect(eq(e.unwrap_or(7), 42));
        expect(
          eq(e.unwrap_or_else([](irt::error_code) noexcept { return 9; }), 42));
    };

    "expected<T>: error state"_test = [] {
        irt::expected<int> e(range_err);

        expect(!e.has_value());
        expect(!static_cast<bool>(e));
        expect(e.has_error());
        expect(e.error() == range_err);
        expect(eq(e.value_or(7), 7));
        expect(eq(e.unwrap_or(7), 7));
        expect(
          eq(e.unwrap_or_else([](irt::error_code) noexcept { return 9; }), 9));
    };

    "expected<T>: default constructor value-initializes T"_test = [] {
        irt::expected<int> e;

        expect(e.has_value());
        expect(eq(*e, 0));
    };

    "expected<T>: in_place, operator->"_test = [] {
        irt::expected<std::string> e(std::in_place, std::size_t{ 3 }, 'a');

        expect(e.has_value());
        expect(*e == "aaa");
        expect(eq(e->size(), std::size_t{ 3 }));
    };

    "expected<T>: rvalue accessors move the value out"_test = [] {
        irt::expected<std::unique_ptr<int>> e(std::make_unique<int>(5));

        auto p = std::move(e).value();
        expect(p != nullptr);
        expect(eq(*p, 5));

        irt::expected<std::unique_ptr<int>> bad(range_err);
        auto q = std::move(bad).value_or(std::unique_ptr<int>());
        expect(q == nullptr);
    };

    "expected<T>: error() from an rvalue is a copy"_test = [] {
        expect(irt::expected<int>(range_err).error() == range_err);
        expect(irt::status(file_err).error() == file_err);
    };
}

void test_expected_special_members()
{
    "expected<T>: copy, move and assignment between all states"_test = [] {
        using E = irt::expected<tracker>;

        tracker::alive = 0;

        {
            const auto held = [](const auto&... es) {
                return (static_cast<int>(es.has_value()) + ...);
            };

            E v1(tracker(1));
            E v2(tracker(2));
            E e1(range_err);
            E e2(file_err);
            expect(eq(tracker::alive, held(v1, v2, e1, e2)));

            v1 = v2; // value <- value
            expect(v1.has_value() && v1->v == 2);
            expect(eq(tracker::alive, held(v1, v2, e1, e2)));

            v1 = e1; // value <- error
            expect(v1.has_error() && v1.error() == range_err);
            expect(eq(tracker::alive, held(v1, v2, e1, e2)));

            e2 = v2; // error <- value
            expect(e2.has_value() && e2->v == 2);
            expect(eq(tracker::alive, held(v1, v2, e1, e2)));

            e1 = e2; // error <- value (again, from another error)
            expect(e1.has_value() && e1->v == 2);
            expect(eq(tracker::alive, held(v1, v2, e1, e2)));

            e2 = v1; // value <- error
            expect(e2.has_error() && e2.error() == range_err);
            expect(eq(tracker::alive, held(v1, v2, e1, e2)));

            v1 = v1.error(); // error <- error (aliasing its own error)
            expect(v1.has_error() && v1.error() == range_err);

            E m(std::move(v2)); // move construction
            expect(m.has_value() && m->v == 2);
            expect(v2.has_value() && v2->v == -1) << "moved-from value";
            expect(eq(tracker::alive, held(v1, v2, e1, e2, m)));

            v2 = std::move(m); // value <- value (move)
            expect(v2.has_value() && v2->v == 2);
            expect(m.has_value() && m->v == -1);

            E c(e1); // copy construction of a value
            expect(c.has_value() && c->v == 2);
            E d(v1); // copy construction of an error
            expect(d.has_error() && d.error() == range_err);
            expect(eq(tracker::alive, held(v1, v2, e1, e2, m, c, d)));

            E& alias = v2; // self assignment
            v2       = alias;
            expect(v2.has_value() && v2->v == 2);
            expect(eq(tracker::alive, held(v1, v2, e1, e2, m, c, d)));
        }

        expect(eq(tracker::alive, 0)) << "leak or double destruction";
    };

    "expected<T>: assignment from a value or an error"_test = [] {
        tracker::alive = 0;

        {
            irt::expected<tracker> e(range_err);

            e = tracker(4); // error <- T&&
            expect(e.has_value() && e->v == 4);
            expect(eq(tracker::alive, 1));

            const tracker t(8);
            e = t; // value <- const T&
            expect(e->v == 8);
            expect(eq(tracker::alive, 2));

            e = range_err; // value <- error
            expect(e.has_error() && e.error() == range_err);
            expect(eq(tracker::alive, 1));

            e = file_err; // error <- error
            expect(e.error() == file_err);
            expect(eq(tracker::alive, 1));

            e = t; // error <- const T&
            expect(e.has_value() && e->v == 8);
        }

        expect(eq(tracker::alive, 0));
    };

    "expected<T>: move-only types"_test = [] {
        using E = irt::expected<std::unique_ptr<int>>;

        E a(std::make_unique<int>(1));
        E b(std::move(a));
        expect(b.has_value() && **b == 1);

        E c(range_err);
        c = std::move(b);
        expect(c.has_value() && **c == 1);

        c = range_err;
        expect(c.has_error());
    };

    "expected<T>: assigning from its own value is safe"_test = [] {
        // Longer than the small string buffer: a destroy-then-copy assignment
        // would read freed memory (reported by AddressSanitizer).
        const std::string          text(64, 'x');
        irt::expected<std::string> e(text);

        const std::string& self = *e;
        e                       = self;

        expect(e.has_value());
        expect(*e == text);
    };

    "expected<T>: assignment keeps the capacity of the contained value"_test =
      [] {
          // Copy assignment from an lvalue only: whether a move assignment
          // keeps the capacity of a destination depends on the standard
          // library (it does not with the MSVC one for a small source).
          const std::string          small("short");
          irt::expected<std::string> e(std::string(200, 'a'));
          const auto                 capacity = e->capacity();

          e = small;
          expect(*e == "short");
          expect(e->capacity() >= capacity)
            << "T::operator= should be used, not destroy + construct";

          // Same between two expected holding a value.
          irt::expected<std::string> src(small);
          irt::expected<std::string> dst(std::string(200, 'b'));
          const auto                 dst_capacity = dst->capacity();

          dst = src;
          expect(*dst == "short");
          expect(dst->capacity() >= dst_capacity)
            << "T::operator= should be used, not destroy + construct";
      };
}

void test_expected_swap()
{
    "expected<int>::swap in the four combinations"_test = [] {
        check_swap(1, 2);
    };

    "expected<std::string>::swap in the four combinations"_test = [] {
        check_swap(std::string(64, 'a'), std::string(48, 'b'));
    };

    "expected<tracker>::swap in the four combinations"_test = [] {
        tracker::alive = 0;
        {
            const tracker x(1);
            const tracker y(2);
            const int     base = tracker::alive;

            check_swap(x, y);
            expect(eq(tracker::alive, base)) << "leak or double destruction";
        }
        expect(eq(tracker::alive, 0));
    };

    "expected<unique_ptr>::swap (move-only)"_test = [] {
        using E = irt::expected<std::unique_ptr<int>>;

        E a(std::make_unique<int>(1));
        E b(range_err);
        a.swap(b);

        expect(a.has_error() && a.error() == range_err);
        expect(b.has_value() && **b == 1);

        a.swap(b);
        expect(a.has_value() && **a == 1);
        expect(b.has_error() && b.error() == range_err);
    };

    "status::swap"_test = [] {
        irt::status a = irt::success();
        irt::status b(range_err);

        a.swap(b);
        expect(a.has_error() && a.error() == range_err);
        expect(b.has_value());

        using std::swap;
        swap(a, b);
        expect(a.has_value());
        expect(b.error() == range_err);
    };
}

void test_expected_emplace()
{
    "expected<T>::emplace from an error"_test = [] {
        irt::expected<std::string> e(range_err);

        auto& ref = e.emplace(std::size_t{ 3 }, 'z');

        expect(e.has_value());
        expect(ref == "zzz");
        expect(&ref == &*e);
    };

    "expected<T>::emplace replaces a value"_test = [] {
        tracker::alive = 0;
        {
            irt::expected<tracker> e(tracker(1));

            e.emplace(5);
            expect(e.has_value() && e->v == 5);
            expect(eq(tracker::alive, 1)) << "the old value must be destroyed";
        }
        expect(eq(tracker::alive, 0));
    };

    "expected<T>::emplace from arguments referring to the current value"_test =
      [] {
          // `tail` is built from a pointer into the buffer that the previous
          // value owns and that emplace has to release.
          irt::expected<std::string> e(std::string(64, 'a') + "tail");

          e.emplace(e->c_str() + 64);

          expect(*e == "tail");
      };

    "expected<T>::emplace with a type that cannot be moved"_test = [] {
        irt::expected<pinned> e(std::in_place, 1);

        e.emplace(2);
        expect(e.has_value() && e->v == 2);

        e = range_err;
        e.emplace(3);
        expect(e.has_value() && e->v == 3);
    };
}

void test_expected_monadic()
{
    "and_then: the value type can change"_test = [] {
        const auto to_string = [](int v) noexcept {
            return irt::expected<std::string>(std::to_string(v));
        };

        irt::expected<int> ok(12);
        const auto         r = ok.and_then(to_string);
        static_assert(std::is_same_v<std::remove_const_t<decltype(r)>,
                                     irt::expected<std::string>>);
        expect(r.has_value() && *r == "12");

        irt::expected<int> bad(range_err);
        const auto         r2 = bad.and_then(to_string);
        static_assert(std::is_same_v<std::remove_const_t<decltype(r2)>,
                                     irt::expected<std::string>>);
        expect(r2.has_error() && r2.error() == range_err);
    };

    "and_then: extra arguments, const and rvalue overloads"_test = [] {
        irt::expected<int> ok(12);

        const auto r = ok.and_then(
          [](int v, int k) noexcept { return irt::expected<int>(v * k); }, 3);
        expect(r.has_value() && *r == 36);

        const irt::expected<int> c(2);
        const auto               r2 = c.and_then(
          [](const int& v) noexcept { return irt::expected<int>(v + 1); });
        expect(r2.has_value() && *r2 == 3);

        // The value is forwarded as an rvalue: a move-only value can be taken.
        irt::expected<std::unique_ptr<int>> p(std::make_unique<int>(5));
        const auto                          r3 = std::move(p).and_then(
          [](std::unique_ptr<int> q) noexcept {
              return irt::expected<int>(*q + 1);
          });
        expect(r3.has_value() && *r3 == 6);
    };

    "and_then: the callback can return a status"_test = [] {
        irt::expected<int> ok(12);

        const auto r = ok.and_then(
          [](int) noexcept -> irt::status { return irt::success(); });
        static_assert(
          std::is_same_v<std::remove_const_t<decltype(r)>, irt::status>);
        expect(r.has_value());

        const auto r2 = ok.and_then(
          [](int) noexcept -> irt::status { return file_err; });
        expect(r2.has_error() && r2.error() == file_err);
    };

    "status::and_then propagates the error of the callback"_test = [] {
        // It used to return a success whatever the callback returned.
        irt::status ok = irt::success();

        const auto r = ok.and_then(
          []() noexcept -> irt::status { return range_err; });
        expect(r.has_error()) << "the failure of f was swallowed";
        expect(r.error() == range_err);

        int calls = 0;

        const auto r2 = ok.and_then([&]() noexcept -> irt::status {
                              ++calls;
                              return range_err;
                          })
                          .and_then([&]() noexcept -> irt::status {
                              ++calls;
                              return irt::success();
                          });
        expect(eq(calls, 1)) << "the chain must stop at the first error";
        expect(r2.has_error() && r2.error() == range_err);
    };

    "status::and_then: an error skips the callback"_test = [] {
        int         calls = 0;
        irt::status bad(range_err);

        const auto r = bad.and_then([&]() noexcept {
            ++calls;
            return irt::expected<int>(1);
        });
        static_assert(
          std::is_same_v<std::remove_const_t<decltype(r)>, irt::expected<int>>);
        expect(eq(calls, 0));
        expect(r.has_error() && r.error() == range_err);

        const auto r2 = irt::success().and_then(
          [&]() noexcept { return irt::expected<int>(7); });
        expect(r2.has_value() && *r2 == 7);
    };

    "or_else: recovers from an error, keeps a value"_test = [] {
        const auto recover = [](irt::error_code) noexcept {
            return irt::expected<int>(0);
        };

        irt::expected<int> ok(5);
        irt::expected<int> bad(range_err);

        const auto r = ok.or_else(recover);
        expect(r.has_value() && *r == 5);

        const auto r2 = bad.or_else(recover);
        expect(r2.has_value() && *r2 == 0);

        const auto r3 = bad.or_else([](irt::error_code ec) noexcept {
            return irt::expected<int>(
              ec.cat() == irt::category::simulation ? 1 : 2);
        });
        expect(r3.has_value() && *r3 == 1) << "the callback receives the error";
    };

    "status::or_else"_test = [] {
        irt::status ok = irt::success();
        irt::status bad(range_err);

        int  calls = 0;
        auto count = [&](irt::error_code) noexcept {
            ++calls;
            return irt::success();
        };

        expect(ok.or_else(count).has_value());
        expect(eq(calls, 0));
        expect(bad.or_else(count).has_value());
        expect(eq(calls, 1));
    };

    "transform: the value type can change"_test = [] {
        irt::expected<int> ok(3);
        irt::expected<int> bad(range_err);

        const auto d = ok.transform([](int v) noexcept { return v * 1.5; });
        static_assert(std::is_same_v<std::remove_const_t<decltype(d)>,
                                     irt::expected<double>>);
        expect(d.has_value() && *d == 4.5);

        const auto s = ok.transform(
          [](int v) noexcept { return std::to_string(v); });
        static_assert(std::is_same_v<std::remove_const_t<decltype(s)>,
                                     irt::expected<std::string>>);
        expect(s.has_value() && *s == "3");

        const auto e = bad.transform([](int v) noexcept { return v * 1.5; });
        expect(e.has_error() && e.error() == range_err);
    };

    "transform: a callback returning void gives a status"_test = [] {
        irt::expected<int> ok(3);
        irt::expected<int> bad(range_err);

        int seen = 0;

        const auto r = ok.transform([&](int v) noexcept { seen = v; });
        static_assert(
          std::is_same_v<std::remove_const_t<decltype(r)>, irt::status>);
        expect(r.has_value());
        expect(eq(seen, 3));

        const auto r2 = bad.transform([&](int) noexcept { seen = -1; });
        expect(r2.has_error() && r2.error() == range_err);
        expect(eq(seen, 3));
    };

    "transform: rvalue overload forwards the value, map is an alias"_test = [] {
        irt::expected<std::unique_ptr<int>> p(std::make_unique<int>(9));

        const auto r = std::move(p).transform(
          [](std::unique_ptr<int> q) noexcept { return *q; });
        expect(r.has_value() && *r == 9);

        irt::expected<int> ok(4);
        const auto         m = ok.map([](int v) noexcept { return v + 1; });
        expect(m.has_value() && *m == 5);
    };

    "map_error"_test = [] {
        irt::expected<int> ok(1);
        irt::expected<int> bad(range_err);

        const auto r = ok.map_error(
          [](irt::error_code) noexcept { return file_err; });
        expect(r.has_value() && *r == 1);

        const auto r2 = bad.map_error(
          [](irt::error_code) noexcept { return file_err; });
        expect(r2.has_error() && r2.error() == file_err);

        // Anything convertible to an error_code is accepted...
        const auto r3 = bad.map_error([](irt::error_code) noexcept {
            return simulation_errc::memory_error;
        });
        expect(r3.has_error());
        expect(r3.error() == irt::error_code(simulation_errc::memory_error));

        // ... and so is status.
        irt::status s(range_err);
        const auto  r4 = s.map_error(
          [](irt::error_code) noexcept { return file_err; });
        expect(r4.has_error() && r4.error() == file_err);
        expect(irt::success()
                 .map_error([](irt::error_code) noexcept { return file_err; })
                 .has_value());
    };

    "status::unwrap_or_else calls the function on error only"_test = [] {
        int             calls = 0;
        irt::error_code seen;
        const auto      fn = [&](irt::error_code ec) noexcept {
            ++calls;
            seen = ec;
        };

        const irt::status ok = irt::success();
        ok.unwrap_or_else(fn);
        expect(eq(calls, 0));

        const irt::status bad(range_err);
        bad.unwrap_or_else(fn);
        expect(eq(calls, 1));
        expect(seen == range_err);
    };
}

void test_expected_comparison()
{
    "operator== between expected"_test = [] {
        expect(irt::expected<int>(3) == irt::expected<int>(3));
        expect(irt::expected<int>(3) != irt::expected<int>(4));
        expect(irt::expected<int>(3) == irt::expected<long>(3L));
        expect(irt::expected<int>(3) != irt::expected<int>(range_err));
        expect(irt::expected<int>(range_err) == irt::expected<int>(range_err));
        expect(irt::expected<int>(range_err) != irt::expected<int>(file_err));

        expect(irt::success() == irt::success());
        expect(irt::status(range_err) == irt::status(range_err));
        expect(irt::status(range_err) != irt::status(file_err));
        expect(irt::status(range_err) != irt::success());
    };

    "operator== with a value or an error_code"_test = [] {
        irt::expected<int> ok(3);
        irt::expected<int> bad(range_err);

        expect(ok == 3);
        expect(ok != 4);
        expect(bad != 3);

        expect(bad == range_err);
        expect(bad != file_err);
        expect(ok != range_err);
        expect(range_err == bad);

        expect(bad == simulation_errc::range_error);
        expect(irt::status(range_err) == range_err);
        expect(irt::success() != range_err);
    };
}

void test_irt_check()
{
    "irt_check returns the error of the calling function"_test = [] {
        expect(run_two(1, 2).has_value());
        expect(run_two(-1, 2).error() == range_err);
        expect(run_two(1, -1).error() == range_err);

        expect(eq(run_value(4).value(), 8));
        expect(run_value(-4).error() == range_err);
    };

    "irt_check is a single statement"_test = [] {
        expect(run_if_else(true, 1).has_value());
        expect(run_if_else(true, -1).error() == range_err);
        expect(run_if_else(false, 1).error() == file_err);
    };
}

} // namespace

int main()
{
    test_error_code();
    test_make_error();
    test_expected_observers();
    test_expected_special_members();
    test_expected_swap();
    test_expected_emplace();
    test_expected_monadic();
    test_expected_comparison();
    test_irt_check();
}