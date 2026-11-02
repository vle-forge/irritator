// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/container.hpp>
#include <irritator/core.hpp>

#include <boost/ut.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

/// A bump allocator over a caller buffer: it stands for the memory of a host
/// application (for example @c FMemory in Unreal Engine).
struct arena {
    std::byte*  begin         = nullptr;
    std::size_t size          = 0;
    std::size_t used          = 0;
    int         allocations   = 0;
    int         deallocations = 0;

    bool owns(const void* p) const noexcept
    {
        const auto* b = static_cast<const std::byte*>(p);
        return begin <= b and b < begin + size;
    }
};

static arena* current_arena = nullptr;

static void* arena_allocate(std::size_t bytes, std::size_t alignment) noexcept
{
    void*       p     = current_arena->begin + current_arena->used;
    std::size_t space = current_arena->size - current_arena->used;

    if (not std::align(alignment, bytes, p, space))
        return nullptr;

    current_arena->used = static_cast<std::size_t>(
      static_cast<std::byte*>(p) + bytes - current_arena->begin);
    ++current_arena->allocations;
    return p;
}

static void arena_deallocate(void* p,
                             std::size_t /*bytes*/,
                             std::size_t /*alignment*/) noexcept
{
    if (current_arena->owns(p))
        ++current_arena->deallocations;
}

static bool install(arena& a) noexcept
{
    current_arena = &a;
    return irt::new_delete_memory_resource::instance().set_functions(
      { &arena_allocate, &arena_deallocate });
}

static bool uninstall() noexcept
{
    return irt::new_delete_memory_resource::instance().set_functions({});
}

struct alignas(64) cache_line {
    float v = 0.f;
};

enum class item_id : std::uint32_t;

int main()
{
    using namespace boost::ut;

    "containers"_test = [] {
        std::array<std::byte, 64u * 1024u> buffer;
        arena                              a{ buffer.data(), buffer.size() };

        expect(install(a) >> fatal);

        {
            irt::vector<int> v;
            for (int i = 0; i < 100; ++i)
                v.emplace_back(i);
            expect(a.owns(v.data()));

            irt::data_array<cache_line, item_id> d;
            expect(d.reserve(16) >> fatal);
            auto& x = d.alloc();
            expect(a.owns(&x));
            expect(eq(reinterpret_cast<std::uintptr_t>(&x) % 64u,
                      std::uintptr_t{ 0 }));

            irt::id_data_array<void, item_id,
                               irt::allocator<irt::new_delete_memory_resource>,
                               cache_line, int>
              ida;
            expect(ida.reserve(16) >> fatal);
            const auto id = ida.alloc_id();
            auto&      c  = ida.get<cache_line>(id);
            expect(a.owns(&c));
            expect(eq(reinterpret_cast<std::uintptr_t>(&c) % 64u,
                      std::uintptr_t{ 0 }));

            irt::ring_buffer<double> r(8);
            expect(r.push_tail(1.0));
            expect(a.owns(&r.front()));

            irt::table<int, float> t;
            t.set(1, 1.f);
            t.set(2, 2.f);
            expect(a.owns(t.data.data()));

            expect(not uninstall());
        }

        expect(gt(a.allocations, 0));
        expect(eq(a.allocations, a.deallocations));
        expect(uninstall() >> fatal);
    };

    "simulation"_test = [] {
        static std::array<std::byte, 64u * 1024u * 1024u> buffer;
        arena a{ buffer.data(), buffer.size() };

        expect(install(a) >> fatal);

        {
            irt::simulation sim;
            expect(sim.can_alloc(3) >> fatal);

            auto& cnt = sim.alloc<irt::counter>();
            auto& c1  = sim.alloc<irt::constant>();
            auto& c2  = sim.alloc<irt::constant>();
            sim.parameters[sim.get_id(c1)].set_constant(0, 0);
            sim.parameters[sim.get_id(c2)].set_constant(0, 0);

            expect(!!sim.connect_dynamics(c1, 0, cnt, 0));
            expect(!!sim.connect_dynamics(c2, 0, cnt, 0));
            expect(!!sim.initialize() >> fatal);

            do {
                expect(!!sim.run() >> fatal);
            } while (not sim.current_time_expired());

            expect(eq(cnt.event_number, static_cast<irt::i64>(2)));
        }

        expect(gt(a.allocations, 0));
        expect(eq(a.allocations, a.deallocations));
        expect(uninstall() >> fatal);
    };
}
