// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_NODE_EDITOR_GEOMETRY_2026
#define ORG_VLEPROJECT_IRRITATOR_APP_NODE_EDITOR_GEOMETRY_2026

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>

// Free of any ImGui dependency to be unit tested: the functions work with any
// two dimensional vector type with public @c x and @c y float members.

namespace irt::node_editor {

template<typename V>
concept vec2 = requires(V v) {
    { v.x } -> std::convertible_to<float>;
    { v.y } -> std::convertible_to<float>;
};

template<vec2 V>
struct rect {
    V min;
    V max;
};

template<vec2 V>
constexpr bool contains(const rect<V>& r, const V p) noexcept
{
    return r.min.x <= p.x and p.x <= r.max.x and r.min.y <= p.y and
           p.y <= r.max.y;
}

template<vec2 V>
constexpr bool overlaps(const rect<V>& a, const rect<V>& b) noexcept
{
    return a.min.x <= b.max.x and b.min.x <= a.max.x and a.min.y <= b.max.y and
           b.min.y <= a.max.y;
}

/// Builds the rectangle from two corners in any order, for example the
/// start and the end of a mouse selection.
template<vec2 V>
constexpr rect<V> make_rect(const V a, const V b) noexcept
{
    return rect<V>{ V{ std::min(a.x, b.x), std::min(a.y, b.y) },
                    V{ std::max(a.x, b.x), std::max(a.y, b.y) } };
}

/// The four control points of the link drawn from an output pin to an input
/// pin: the tangents are horizontal to leave and enter the nodes sideways.
template<vec2 V>
struct bezier {
    V p0, p1, p2, p3;
};

template<vec2 V>
constexpr bezier<V> make_link_bezier(const V     from,
                                     const V     to,
                                     const float tangent) noexcept
{
    return bezier<V>{ from, V{ from.x + tangent, from.y },
                      V{ to.x - tangent, to.y }, to };
}

template<vec2 V>
constexpr V point_at(const bezier<V>& b, const float t) noexcept
{
    const auto u  = 1.f - t;
    const auto w0 = u * u * u;
    const auto w1 = 3.f * u * u * t;
    const auto w2 = 3.f * u * t * t;
    const auto w3 = t * t * t;

    return V{ w0 * b.p0.x + w1 * b.p1.x + w2 * b.p2.x + w3 * b.p3.x,
              w0 * b.p0.y + w1 * b.p1.y + w2 * b.p2.y + w3 * b.p3.y };
}

/// A cubic bezier lies in the convex hull of its control points: their
/// bounding box is a cheap and conservative bound to cull the links.
template<vec2 V>
constexpr rect<V> bounds(const bezier<V>& b) noexcept
{
    return rect<V>{ V{ std::min({ b.p0.x, b.p1.x, b.p2.x, b.p3.x }),
                       std::min({ b.p0.y, b.p1.y, b.p2.y, b.p3.y }) },
                    V{ std::max({ b.p0.x, b.p1.x, b.p2.x, b.p3.x }),
                       std::max({ b.p0.y, b.p1.y, b.p2.y, b.p3.y }) } };
}

template<vec2 V>
inline float distance_to_segment(const V p, const V a, const V b) noexcept
{
    const auto abx = b.x - a.x;
    const auto aby = b.y - a.y;
    const auto len = abx * abx + aby * aby;
    const auto t   = len > 0.f
                       ? std::clamp(((p.x - a.x) * abx + (p.y - a.y) * aby) / len,
                                    0.f, 1.f)
                       : 0.f;
    const auto dx  = p.x - (a.x + t * abx);
    const auto dy  = p.y - (a.y + t * aby);

    return std::sqrt(dx * dx + dy * dy);
}

/// Distance to the polyline approximation of the curve, enough to pick a
/// link with the mouse.
template<vec2 V>
inline float distance_to(const bezier<V>& b,
                         const V          p,
                         const int        segments = 16) noexcept
{
    auto ret  = std::numeric_limits<float>::infinity();
    auto prev = b.p0;

    for (int i = 1; i <= segments; ++i) {
        const auto next = point_at(b, static_cast<float>(i) /
                                        static_cast<float>(segments));
        ret             = std::min(ret, distance_to_segment(p, prev, next));
        prev            = next;
    }

    return ret;
}

/// The world to screen transformation of the canvas: @c screen = @c origin +
/// @c world * @c zoom.
template<vec2 V>
struct camera {
    V     scrolling{};
    float zoom = 1.f;

    constexpr V to_screen(const V canvas_origin, const V world) const noexcept
    {
        return V{ canvas_origin.x + scrolling.x + world.x * zoom,
                  canvas_origin.y + scrolling.y + world.y * zoom };
    }

    constexpr V to_world(const V canvas_origin, const V screen) const noexcept
    {
        return V{ (screen.x - canvas_origin.x - scrolling.x) / zoom,
                  (screen.y - canvas_origin.y - scrolling.y) / zoom };
    }

    /// Changes the zoom and keeps the world point under @c screen fixed.
    constexpr void zoom_at(const V     canvas_origin,
                           const V     screen,
                           const float new_zoom) noexcept
    {
        const auto world = to_world(canvas_origin, screen);
        zoom             = new_zoom;
        scrolling.x      = screen.x - canvas_origin.x - world.x * zoom;
        scrolling.y      = screen.y - canvas_origin.y - world.y * zoom;
    }
};

} // namespace irt::node_editor

#endif
