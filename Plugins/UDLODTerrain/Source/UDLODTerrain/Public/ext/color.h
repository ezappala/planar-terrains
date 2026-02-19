#pragma once

#include <MathUtil.h>
#include <Math/Vector.h>

namespace ext::color {
template <typename T> requires std::floating_point<T>
UE::Math::TVector<T> rgb_to_lab(UE::Math::TVector<T> rgb) {
    const double r_norm = rgb.X / 255.;
    const double g_norm = rgb.Y / 255.;
    const double b_norm = rgb.Z / 255.;

    const double r_linear = r_norm <= 0.04045 ? r_norm / 12.92 : FMath::Pow((r_norm + 0.055) / 1.055, 2.4);
    const double g_linear = g_norm <= 0.04045 ? g_norm / 12.92 : FMath::Pow((g_norm + 0.055) / 1.055, 2.4);
    const double b_linear = b_norm <= 0.04045 ? b_norm / 12.92 : FMath::Pow((b_norm + 0.055) / 1.055, 2.4);

    const double x = r_linear * 0.4124564 + g_linear * 0.3575761 + b_linear * 0.1804375;
    const double y = r_linear * 0.2126729 + g_linear * 0.7151522 + b_linear * 0.0721690;
    const double z = r_linear * 0.0193339 + g_linear * 0.1191920 + b_linear * 0.9502278;

    // D65 reference white points
    constexpr double xn = 0.95047;
    constexpr double yn = 1.0;
    constexpr double zn = 1.08883;

    const double xr = x / xn;
    const double yr = y / yn;
    const double zr = z / zn;

    auto f = [](const double t) -> double {
        if (t > 0.008856) {
            return TMathUtil<double>::Cbrt(t);
        }
        return 7.787 * t + 16.0 / 116.0;
    };

    const double fx = f(xr);
    const double fy = f(yr);
    const double fz = f(zr);

    double l = 116.0 * fy - 16.0;
    double a = 500.0 * (fx - fy);
    double b = 200.0 * (fy - fz);

    return UE::Math::TVector<T>{static_cast<T>(l), static_cast<T>(a), static_cast<T>(b)};
}
} // namespace ext::color
