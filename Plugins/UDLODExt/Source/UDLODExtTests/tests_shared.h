#pragma once

#include "HalfspaceTypes.h"
#include "Math/UnrealMathUtility.h"
#include "MatrixTypes.h"
#include "Misc/AutomationTest.h"

namespace udlodext::tests {
inline constexpr EAutomationTestFlags TestFlags =
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::ProductFilter;

inline bool check_near(
    FAutomationTestBase& test,
    const FString& label,
    const double actual,
    const double expected,
    const double tolerance = KINDA_SMALL_NUMBER) {
    const bool is_close = FMath::IsNearlyEqual(actual, expected, tolerance);
    test.TestTrue(
        FString::Printf(TEXT("%s (expected %.6f, got %.6f)"), *label, expected, actual),
        is_close);
    return is_close;
}

inline FMatrix44f make_matrix44f(std::initializer_list<float> values) {
    check(values.size() == 16);

    FMatrix44f matrix{};
    int32 index = 0;
    for (const float value : values) {
        matrix.M[index / 4][index % 4] = value;
        ++index;
    }

    return matrix;
}

inline FMatrix44d make_matrix44d(std::initializer_list<double> values) {
    check(values.size() == 16);

    FMatrix44d matrix{};
    int32 index = 0;
    for (const double value : values) {
        matrix.M[index / 4][index % 4] = value;
        ++index;
    }

    return matrix;
}

inline bool check_halfspace(
    FAutomationTestBase& test,
    const FString& label,
    const UE::Geometry::FHalfspace3d& actual,
    const FVector4d& expected,
    const double tolerance = KINDA_SMALL_NUMBER) {
    const bool nx_ok = check_near(
        test,
        FString::Printf(TEXT("%s normal x"), *label),
        actual.Normal.X,
        expected.X,
        tolerance);
    const bool ny_ok = check_near(
        test,
        FString::Printf(TEXT("%s normal y"), *label),
        actual.Normal.Y,
        expected.Y,
        tolerance);
    const bool nz_ok = check_near(
        test,
        FString::Printf(TEXT("%s normal z"), *label),
        actual.Normal.Z,
        expected.Z,
        tolerance);
    const bool c_ok = check_near(
        test,
        FString::Printf(TEXT("%s constant"), *label),
        actual.Constant,
        expected.W,
        tolerance);
    return nx_ok && ny_ok && nz_ok && c_ok;
}
} // namespace udlodext::tests
