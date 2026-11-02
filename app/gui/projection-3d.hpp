// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_APP_PROJECTION_3D_2026
#define ORG_VLEPROJECT_IRRITATOR_APP_PROJECTION_3D_2026

#include <array>
#include <cmath>

// Free of any ImGui dependency to be unit tested.

namespace irt {

/** Rotates 3D points around a center to display them in 2D: the projection
 * keeps the x and y coordinates of the rotated points.
 *
 * The angles (in radian) are changed by the user dragging the mouse with the
 * right button.
 */
class projection_3d
{
public:
    using vec3 = std::array<float, 3>;

    /** Computes the rotation @c Rz·Rx·Ry from the three @c angles around the
     * x, y and z axis. */
    void update_matrices(const vec3& angles, const vec3& center) noexcept
    {
        m_angles = angles;
        m_center = center;

        const auto cx = std::cos(angles[0]), sx = std::sin(angles[0]);
        const auto cy = std::cos(angles[1]), sy = std::sin(angles[1]);
        const auto cz = std::cos(angles[2]), sz = std::sin(angles[2]);

        const matrix rx{ 1.f, 0.f, 0.f, 0.f, cx, -sx, 0.f, sx, cx };
        const matrix ry{ cy, 0.f, sy, 0.f, 1.f, 0.f, -sy, 0.f, cy };
        const matrix rz{ cz, -sz, 0.f, sz, cz, 0.f, 0.f, 0.f, 1.f };

        m_rot = multiply(rz, multiply(rx, ry));
    }

    /** @return The rotated point @c center + R · (p - @c center). */
    constexpr vec3 compute(const float x,
                           const float y,
                           const float z) const noexcept
    {
        const auto dx = x - m_center[0];
        const auto dy = y - m_center[1];
        const auto dz = z - m_center[2];

        return vec3{
            m_center[0] + m_rot[0] * dx + m_rot[1] * dy + m_rot[2] * dz,
            m_center[1] + m_rot[3] * dx + m_rot[4] * dy + m_rot[5] * dz,
            m_center[2] + m_rot[6] * dx + m_rot[7] * dy + m_rot[8] * dz
        };
    }

    constexpr vec3 angles() const noexcept { return m_angles; }
    constexpr vec3 center() const noexcept { return m_center; }

private:
    using matrix = std::array<float, 9>;

    static constexpr matrix multiply(const matrix& a, const matrix& b) noexcept
    {
        matrix r{};

        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k)
                    r[i * 3 + j] += a[i * 3 + k] * b[k * 3 + j];

        return r;
    }

    vec3   m_angles{ 0.f, 0.f, 0.f };
    vec3   m_center{ 0.f, 0.f, 0.f };
    matrix m_rot{ 1.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f };
};

} // namespace irt

#endif
