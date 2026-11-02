// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "node-editor-geometry.hpp"
#include "projection-3d.hpp"

#include <boost/ut.hpp>

#include <array>
#include <numbers>
#include <span>

struct v2 {
    float x = 0.f;
    float y = 0.f;
};

using rect_v2   = irt::node_editor::rect<v2>;
using camera_v2 = irt::node_editor::camera<v2>;

int main()
{
    using namespace boost::ut;
    using namespace irt::node_editor;

    "geometry: rectangles"_test = [] {
        const auto a = make_rect(v2{ 10.f, 10.f }, v2{ 0.f, 0.f });
        expect(eq(a.min.x, 0.f) and eq(a.max.y, 10.f));

        expect(contains(a, v2{ 5.f, 5.f }));
        expect(not contains(a, v2{ 11.f, 5.f }));

        expect(overlaps(a, rect_v2{ v2{ 9.f, 9.f }, v2{ 20.f, 20.f } }));
        expect(not overlaps(a, rect_v2{ v2{ 11.f, 0.f }, v2{ 20.f, 5.f } }));
    };

    "geometry: the link bezier"_test = [] {
        const auto b = make_link_bezier(v2{ 0.f, 0.f }, v2{ 100.f, 50.f },
                                        25.f);

        expect(eq(point_at(b, 0.f).x, 0.f));
        expect(eq(point_at(b, 1.f).x, 100.f) and eq(point_at(b, 1.f).y, 50.f));

        // The tangents leave and enter the nodes horizontally.
        expect(eq(b.p1.y, 0.f) and eq(b.p2.y, 50.f));

        const auto box = bounds(b);
        for (auto i = 0; i <= 10; ++i)
            expect(contains(box, point_at(b, static_cast<float>(i) / 10.f)));

        expect(lt(distance_to(b, point_at(b, 0.5f)), 0.5f));
        expect(gt(distance_to(b, v2{ 0.f, 100.f }), 40.f));
    };

    "geometry: the segment distance"_test = [] {
        expect(
          eq(distance_to_segment(v2{ 5.f, 3.f }, v2{}, v2{ 10.f, 0.f }), 3.f));
        expect(
          eq(distance_to_segment(v2{ -4.f, 3.f }, v2{}, v2{ 10.f, 0.f }), 5.f));
        expect(eq(distance_to_segment(v2{ 3.f, 4.f }, v2{}, v2{}), 5.f));
    };

    "geometry: the camera"_test = [] {
        auto cam = camera_v2{ .scrolling = v2{ 20.f, 10.f }, .zoom = 0.5f };
        const auto origin = v2{ 100.f, 100.f };

        const auto s = cam.to_screen(origin, v2{ 40.f, 40.f });
        expect(eq(s.x, 140.f) and eq(s.y, 130.f));

        const auto w = cam.to_world(origin, s);
        expect(eq(w.x, 40.f) and eq(w.y, 40.f));

        // The world point under the mouse stays under it while zooming.
        const auto mouse  = v2{ 250.f, 180.f };
        const auto before = cam.to_world(origin, mouse);
        cam.zoom_at(origin, mouse, 1.f);
        const auto after = cam.to_world(origin, mouse);

        expect(eq(cam.zoom, 1.f));
        expect(lt(std::abs(before.x - after.x), 1e-4f));
        expect(lt(std::abs(before.y - after.y), 1e-4f));
    };
    "layout: the grid"_test = [] {
        expect(eq(grid_columns(0u), 1u));
        expect(eq(grid_columns(1u), 1u));
        expect(eq(grid_columns(4u), 2u));
        expect(eq(grid_columns(5u), 3u));

        const auto p = grid_position(4u, 3u, v2{ 10.f, 20.f });
        expect(eq(p.x, 10.f) and eq(p.y, 20.f));
    };

    "layout: two linked nodes reach the ideal distance"_test = [] {
        std::array<v2, 2> centers{ v2{ 0.f, 0.f }, v2{ 1000.f, 0.f } };
        std::array<v2, 2> moves{};
        const auto        edges = [](auto&& fn) { fn(0u, 1u); };

        for (int i = 0; i < 200; ++i)
            force_directed_step(std::span<v2>(centers), std::span<v2>(moves),
                                edges, 100.f, 50.f * (1.f - i / 200.f));

        const auto d = std::abs(centers[1].x - centers[0].x);
        expect(lt(std::abs(d - 100.f), 1.f)) << d;
    };

    "layout: superposed nodes are separated"_test = [] {
        std::array<v2, 3> centers{};
        std::array<v2, 3> moves{};
        const auto        edges = [](auto&& /*fn*/) {};

        for (int i = 0; i < 50; ++i)
            force_directed_step(std::span<v2>(centers), std::span<v2>(moves),
                                edges, 100.f, 10.f);

        for (std::size_t a = 0; a < 3; ++a)
            for (std::size_t b = a + 1; b < 3; ++b) {
                const auto dx = centers[a].x - centers[b].x;
                const auto dy = centers[a].y - centers[b].y;
                expect(gt(std::sqrt(dx * dx + dy * dy), 50.f));
            }
    };

    "projection: null angles keep the points"_test = [] {
        irt::projection_3d p;
        p.update_matrices({ 0.f, 0.f, 0.f }, { 1.f, 2.f, 3.f });

        const auto r = p.compute(4.f, -5.f, 6.f);
        expect(eq(r[0], 4.f) and eq(r[1], -5.f) and eq(r[2], 6.f));
    };

    "projection: a quarter turn around y"_test = [] {
        irt::projection_3d p;
        p.update_matrices({ 0.f, std::numbers::pi_v<float> / 2.f, 0.f },
                          { 1.f, 2.f, 3.f });

        const auto r = p.compute(2.f, 2.f, 3.f);
        expect(lt(std::abs(r[0] - 1.f), 1e-5f));
        expect(lt(std::abs(r[1] - 2.f), 1e-5f));
        expect(lt(std::abs(r[2] - 2.f), 1e-5f));
    };

    "projection: the rotation keeps the distances"_test = [] {
        irt::projection_3d p;
        p.update_matrices({ 0.3f, -1.2f, 2.5f }, { 1.f, 2.f, 3.f });

        const auto c = p.compute(1.f, 2.f, 3.f);
        expect(lt(std::abs(c[0] - 1.f), 1e-5f));
        expect(lt(std::abs(c[1] - 2.f), 1e-5f));
        expect(lt(std::abs(c[2] - 3.f), 1e-5f));

        const auto r  = p.compute(4.f, -2.f, 3.f);
        const auto dx = r[0] - 1.f;
        const auto dy = r[1] - 2.f;
        const auto dz = r[2] - 3.f;
        expect(
          lt(std::abs(std::sqrt(dx * dx + dy * dy + dz * dz) - 5.f), 1e-4f));
    };
}
