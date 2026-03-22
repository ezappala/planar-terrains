#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_parallel.h"
#include "HAL/PlatformProcess.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessParallelMapFilterMapPreservesInputOrderTest,
    "UDLODPreprocessor.Preprocess.Parallel.MapFilterMapPreservesInputOrder",
    TestFlags)

bool FPreprocessParallelMapFilterMapPreservesInputOrderTest::RunTest(const FString& Parameters) {
    const TArray input{0, 1, 2, 3, 4, 5};

    const auto result = par_iter_map_filter_map<int32, FString>(
        input,
        [](const int32 value) -> std::expected<TOptional<int32>, FString> {
            FPlatformProcess::Sleep(static_cast<float>((5 - value) * 0.001));
            if (value % 2 == 0) { return TOptional{value}; }
            return NullOpt;
        },
        [](
        std::expected<TOptional<int32>, FString> r)
        -> TOptional<std::expected<int32, FString>> {
            if (!r.has_value()) {
                return TOptional<std::expected<int32, FString>>{
                    std::unexpected{MoveTemp(r.error())}};
            }
            if (!r.value().IsSet()) { return NullOpt; }
            return TOptional<std::expected<int32, FString>>{
                r.value().GetValue()};
        });

    TestTrue(TEXT("Map/filter/map succeeds"), result.has_value());
    if (!result.has_value()) {
        return false;
    }

    const TArray<int32>& output = result.value();
    TestEqual(TEXT("Only even values are emitted"), output.Num(), 3);
    if (output.Num() != 3) {
        return false;
    }

    TestEqual(TEXT("First emitted value preserves input order"), output[0], 0);
    TestEqual(TEXT("Second emitted value preserves input order"), output[1], 2);
    TestEqual(TEXT("Third emitted value preserves input order"), output[2], 4);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessParallelMapFilterMapPropagatesErrorsTest,
    "UDLODPreprocessor.Preprocess.Parallel.MapFilterMapPropagatesErrors",
    TestFlags)

bool FPreprocessParallelMapFilterMapPropagatesErrorsTest::RunTest(const FString& Parameters) {
    const TArray input{0, 1, 2, 3};

    const auto result = par_iter_map_filter_map<int32, FString>(
        input,
        [](const int32 value) -> std::expected<TOptional<int32>, FString> {
            if (value == 2) { return std::unexpected{FString(TEXT("boom"))}; }
            return TOptional{value};
        },
        [](
        std::expected<TOptional<int32>, FString> r)
        -> TOptional<std::expected<int32, FString>> {
            if (!r.has_value()) {
                return TOptional<std::expected<int32, FString>>{
                    std::unexpected{MoveTemp(r.error())}};
            }
            if (!r.value().IsSet()) { return NullOpt; }
            return TOptional<std::expected<int32, FString>>{
                r.value().GetValue()};
        });

    TestFalse(TEXT("Map/filter/map surfaces errors"), result.has_value());
    if (result.has_value()) {
        return false;
    }

    TestEqual(TEXT("The first error is preserved"), result.error(), FString(TEXT("boom")));
    return true;
}

#endif
