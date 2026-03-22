#pragma once

#include "Async/ParallelFor.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace preprocess {
template <typename T>
    requires GdalType<T> && Copy<T>
TArray<T> par_iter_map(const TArray<T>& input, TFunction<T(const T&)> func) {
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
    const TArray<T>& input,
    TFunction<std::expected<T, ErrT>(const T&)> func
) {
    TArray<TOptional<T>> ordered_results;
    ordered_results.SetNum(input.Num());

    TArray<T> output;
    output.Reserve(input.Num());
    ParallelFor(
        input.Num(),
        [&input, &ordered_results, &func](int32 index) {
            auto result = func(input[index]);
            if (result.has_value()) { ordered_results[index] = MoveTemp(result.value()); }
        });

    for (TOptional<T>& ordered_result : ordered_results) {
        if (ordered_result.IsSet()) { output.Add(MoveTemp(ordered_result.GetValue())); }
    }

    return output;
}

template <typename T, typename ErrT>
std::expected<TArray<T>, ErrT> par_iter_map_filter_map(
    const TArray<T>& input,
    TFunction<std::expected<TOptional<T>, ErrT>(const T&)> map_func,
    TFunction<TOptional<std::expected<T, ErrT>>(std::expected<TOptional<T>, ErrT>)> filter_map_func
) {
    FCriticalSection error_mutex;
    TOptional<ErrT> first_error;
    TArray<TOptional<T>> ordered_results;
    ordered_results.SetNum(input.Num());

    ParallelFor(
        input.Num(),
        [&input, &ordered_results, &map_func, &filter_map_func, &error_mutex, &first_error](
        int32 index) {
            auto mapped = map_func(input[index]);
            auto result = filter_map_func(MoveTemp(mapped));
            if (!result.IsSet()) { return; }

            auto filtered_result = MoveTemp(result.GetValue());
            if (!filtered_result.has_value()) {
                FScopeLock _(&error_mutex);
                if (!first_error.IsSet()) {
                    first_error = MoveTemp(filtered_result.error());
                }
                return;
            }

            ordered_results[index] = MoveTemp(filtered_result.value());
        });

    if (first_error.IsSet()) { return std::unexpected{MoveTemp(first_error.GetValue())}; }

    TArray<T> output;
    output.Reserve(input.Num());
    for (TOptional<T>& ordered_result : ordered_results) {
        if (ordered_result.IsSet()) { output.Add(MoveTemp(ordered_result.GetValue())); }
    }

    return output;
}

template <typename T, typename ErrT>
std::expected<void, ErrT> par_iter_try_for_each(
    const TArray<T>& input,
    TFunction<std::expected<T, ErrT>(const T&)> func
) {
    FCriticalSection error_mutex;
    TOptional<ErrT> first_error;
    ParallelFor(
        input.Num(),
        [&input, &func, &error_mutex, &first_error](int32 index) {
            auto result = func(input[index]);
            if (!result.has_value()) {
                FScopeLock _(&error_mutex);
                if (!first_error.IsSet()) {
                    first_error = MoveTemp(result.error());
                }
            }
        });
    if (first_error.IsSet()) { return std::unexpected{MoveTemp(first_error.GetValue())}; }
    return {};
}

template <typename T, typename ExpT, typename ErrT>
std::expected<void, ErrT> par_iter_try_for_each(
    const TArray<T>& input,
    TFunction<std::expected<ExpT, ErrT>(const T&)> func
) {
    FCriticalSection error_mutex;
    TOptional<ErrT> first_error;
    ParallelFor(
        input.Num(),
        [&input, &func, &error_mutex, &first_error](int32 index) {
            auto result = func(input[index]);
            if (!result.has_value()) {
                FScopeLock _(&error_mutex);
                if (!first_error.IsSet()) {
                    first_error = MoveTemp(result.error());
                }
            }
        });
    if (first_error.IsSet()) { return std::unexpected{MoveTemp(first_error.GetValue())}; }
    return {};
}
} // namespace preprocess
