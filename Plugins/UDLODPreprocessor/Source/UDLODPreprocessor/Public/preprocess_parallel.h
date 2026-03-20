#pragma once
#include "Async/ParallelFor.h"

namespace preprocess {
template <typename T>
    requires GdalType<T> && Copy<T>
TArray<T> par_iter_map(TArray<T> input, TFunction<T(T)> func) {
    TArray<T> output;
    output.SetNum(input.Num());
    ParallelFor(
        input.Num(),
        [&input, &output, &func](int32 index) { output[index] = func(input[index]); });
    return output;
}

template <typename T, typename ErrT>
    requires GdalType<T> && Copy<T>
TArray<T> par_iter_filter_map(
    TArray<T> input,
    TFunction<std::expected<T, ErrT>(T)> func
) {
    TArray<T> output;
    output.Reserve(input.Num());
    ParallelFor(
        input.Num(),
        [&input, &output, &func](int32 index) {
            auto result = func(input[index]);
            if (result.has_value()) { output.Add(result.value()); }
        });
    return output;
}

template <typename T, typename ErrT>
TArray<T> par_iter_map_filter_map(
    TArray<T> input,
    TFunction<std::expected<TOptional<T>, ErrT>(T)> map_func,
    TFunction<TOptional<std::expected<T, ErrT>>(std::expected<TOptional<T>, ErrT>)> filter_map_func
) {
    TArray<T> output;
    output.Reserve(input.Num());
    ParallelFor(
        input.Num(),
        [&input, &output, &map_func, &filter_map_func](int32 index) {
            auto mapped = map_func(input[index]);
            auto result = filter_map_func(mapped);
            if (result.IsSet()) { output.Add(result.GetValue().value()); }
        });
    return output;
}

template <typename T, typename ErrT>
void par_iter_try_for_each(
    TArray<T> input,
    TFunction<std::expected<T, ErrT>(T)> func
) {
    ParallelFor(
        input.Num(),
        [&input, &func](int32 index) {
            auto result = func(input[index]);
            if (!result.has_value()) {
                checkf(
                    false,
                    TEXT("Error in parallel iteration at index %d: %s"),
                    index,
                    *result.error().ToString());
            }
        });
}

template <typename T, typename ExpT, typename ErrT>
void par_iter_try_for_each(
    TArray<T> input,
    TFunction<std::expected<ExpT, ErrT>(T)> func
) {
    ParallelFor(
        input.Num(),
        [&input, &func](int32 index) {
            auto result = func(input[index]);
            if (!result.has_value()) {
                checkf(
                    false,
                    TEXT("Error in parallel iteration at index %d: %s"),
                    index,
                    *result.error().ToString());
            }
        });
}
} // namespace preprocess
