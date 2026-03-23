#pragma once

#include <complex.h>
#include <expected>
#include <format>

#include "CoreGlobals.h"
#include "ext_array4.h"
#include "ext_buffer.h"
#include "Async/ParallelFor.h"
#include "Async/ParallelTransformReduce.h"
#include "Containers/Array.h"
#include "Containers/Deque.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Misc/ScopeLock.h"
#include "Templates/Tuple.h"

namespace ext::iter {
/**
 * Create an array of multiple arrays in lockstep. The zip function yeilds
 * elements until any subarray is exhausted.
 */
template <typename T>
TArray<TTuple<T, T>> zip(TArray<T> a, TArray<T> b) {
    auto shortest = FMath::Min(a.Num(), b.Num());
    TArray<TTuple<T, T>> output;
    output.Reserve(shortest);
    for (int32 i = 0; i < shortest; ++i) { output.Emplace(a[i], b[i]); }
    return output;
}

template <typename T, typename... Ts>
TArray<TTuple<T, Ts...>> zip(TArray<T> a, TArray<Ts>... arrays) {
    auto shortest = FMath::Min(a.Num(), arrays.Num()...);
    TArray<TTuple<T, Ts...>> output;
    output.Reserve(shortest);
    for (int32 i = 0; i < shortest; ++i) { output.Emplace(a[i], arrays[i]...); }
    return output;
}

template <typename T1, typename T2>
TArray4D<TTuple<T1, T2>> zip(TArray4D<T1> a, TArray4D<T2> b) {
    auto shortest_d0 = FMath::Min(a.get_dim0(), b.get_dim0());
    auto shortest_d1 = FMath::Min(a.get_dim1(), b.get_dim1());
    auto shortest_d2 = FMath::Min(a.get_dim2(), b.get_dim2());
    auto shortest_d3 = FMath::Min(a.get_dim3(), b.get_dim3());
    TArray4D<TTuple<T1, T2>> output(shortest_d0, shortest_d1, shortest_d2, shortest_d3);
    using types::usize;
    for (usize i0 = 0; i0 < shortest_d0; ++i0) {
        for (usize i1 = 0; i1 < shortest_d1; ++i1) {
            for (usize i2 = 0; i2 < shortest_d2; ++i2) {
                for (usize i3 = 0; i3 < shortest_d3; ++i3) {
                    output(i0, i1, i2, i3) = {a(i0, i1, i2, i3), b(i0, i1, i2, i3)};
                }
            }
        }
    }
    return output;
}

/**
 * Create an array of the "cartesian product" of two input arrays.
 */
template <typename T>
TArray<TTuple<T, T>> product(TArray<T> a, TArray<T> b) {
    TArray<TTuple<T, T>> output;
    output.Reserve(a.Num() * b.Num());
    for (int32 i = 0; i < a.Num(); ++i) {
        for (int32 j = 0; j < b.Num(); ++j) { output.Emplace(a[i], b[j]); }
    }
    return output;
}

template <typename T, typename Ret>
TArray<Ret> map(TArray<T> input, const TFunction<Ret(T)>& func) {
    TArray<Ret> output;
    output.Reserve(input.Num());
    for (int32 i = 0; i < input.Num(); ++i) { output.Emplace(func(input[i])); }
    return output;
}

template <typename T, typename ExpT, typename ErrT>
TArray<std::expected<ExpT, ErrT>> map(
    TArray<T> input,
    const TFunction<std::expected<ExpT, ErrT>(T)>& func) {
    TArray<std::expected<ExpT, ErrT>> output;
    output.Reserve(input.Num());

    for (int32 i = 0; i < input.Num(); ++i) { output.Emplace(func(input[i])); }
    return output;
}

template <typename T, typename Ret, types::usize N>
TStaticArray<Ret, N> map(TStaticArray<T, N> input, const TFunction<Ret(T)>& func) {
    TStaticArray<Ret, N> output;
    for (types::usize i = 0; i < N; ++i) { output[i] = func(input[i]); }
    return output;
}

template <typename Key, typename Value, typename RetKey, typename RetValue>
TMap<RetKey, RetValue> map(
    TMap<Key, Value> input,
    const TFunction<TTuple<RetKey, RetValue>(Key, Value)>& func
) {
    TMap<RetKey, RetValue> output;
    for (const auto& [key, value] : input) {
        const auto& [new_key, new_value] = func(key, value);
        output.Emplace(new_key, new_value);
    }
    return output;
}

template <typename T>
TDeque<T> deque_from_array(TArray<T> input) {
    TDeque<T> output;
    for (int32 i = 0; i < input.Num(); ++i) { output.EmplaceLast(input[i]); }
    return output;
}

template <typename ContainerType, typename T>
    requires std::is_arithmetic_v<T>
ContainerType range(T a, T b) {
    check(a < b);
    ContainerType output;
    for (T i = a; i < b; ++i) {
        if constexpr (
            std::is_same_v<ContainerType, TSet<T>> ||
            std::is_same_v<ContainerType, TArray<T>>
        ) {
            output.Emplace(i);
        } else if constexpr (std::is_same_v<ContainerType, TDeque<T>>) {
            output.EmplaceLast(i);
        } else {
            static_assert(
                std::is_same_v<ContainerType, TSet<T>> ||
                std::is_same_v<ContainerType, TArray<T>> ||
                std::is_same_v<ContainerType, TDeque<T>>,
                "range only supports TSet, TArray, and TDeque as ContainerType"
            );
        }
    }
    return output;
}

template <typename ContainerType, typename T>
    requires std::is_arithmetic_v<T>
ContainerType range_inclusive(T a, T b) { return range<ContainerType, T>(a, b + 1); }

template <typename T, typename Ret>
    requires std::is_arithmetic_v<T>
TArray<Ret> map_range(T a, T b, const TFunction<Ret(T)>& func) {
    check(a < b);
    TArray<Ret> output;
    for (T i = a; i < b; ++i) { output.Emplace(func(i)); }
    return output;
}

template <typename T, typename Ret>
    requires std::is_arithmetic_v<T>
TArray<Ret> map_range_inclusive(T a, T b, const TFunction<Ret(T)>& func) {
    return map_range(a, b + 1, func);
}

template <typename T>
TArray<T> rev(TArray<T> input) {
    TArray<T> output;
    output.Reserve(input.Num());
    for (int32 i = input.Num() - 1; i >= 0; --i) { output.Emplace(input[i]); }
    return output;
}

template <typename T, typename Ret>
    requires std::is_arithmetic_v<T>
TArray<Ret> map_rev(TArray<T> input, const TFunction<Ret(T)>& func) {
    TArray<Ret> output;
    output.Reserve(input.Num());
    for (int32 i = input.Num() - 1; i >= 0; --i) { output.Emplace(func(input[i])); }
    return output;
}

template <typename T, typename Ret>
    requires std::is_arithmetic_v<T>
TArray<Ret> map_range_rev(T a, T b, const TFunction<Ret(T)>& func) {
    check(a < b);
    TArray<Ret> output;
    for (T i = b - 1; i >= a; --i) { output.Emplace(func(i)); }
    return output;
}

template <typename T, typename Ret>
    requires std::is_arithmetic_v<T>
TArray<Ret> map_range_inclusive_rev(T a, T b, const TFunction<Ret(T)>& func) {
    return map_range_rev(a, b + 1, func);
}

template <typename T>
    requires std::is_arithmetic_v<T>
TArray<T> step_by(T a, T b, T step) {
    check(a < b);
    check(step > 0);
    TArray<T> output;
    if (a < b) { for (T i = a; i < b; i += step) { output.Emplace(i); } }
    return output;
}

template <typename T>
struct Enumeration {
    types::usize index;
    T element;
};

template <typename T>
TArray<Enumeration<T>> enumerate(const TArray<T>& input) {
    TArray<Enumeration<T>> output;
    output.Reserve(input.Num());
    for (int32 i = 0; i < input.Num(); ++i) { output.Emplace(i, input[i]); }
    return output;
}

/**
 * Return an array of indexes and references to elements of the input array.
 * Elements are visited in the logical order of the array, which is where the
 * rightmost index is varying the fastest.
 *
 * Note that iput arrays are row-major, meaning the shape is (width, height)
 * but the rightmost index is the x coordinate, which is the column. This is
 * because GDAL datasets are row-major, and we want to be able to index into
 * them directly.
 */
template <typename T>
TArray<TTuple<types::usize, types::usize, T>> indexed_iter(
    TArray<T> input,
    TTuple<types::usize, types::usize> shape
) {
    TArray<TTuple<types::usize, types::usize, T>> output;
    output.Reserve(input.Num());
    for (int32 i = 0; i < input.Num(); ++i) {
        // grab the correct index based on row-major shape input
        auto x = i % shape.Get<0>();
        auto y = i / shape.Get<0>();
        output.Emplace(TTuple<types::usize, types::usize, T>{x, y, input[i]});
    }
    return output;
}

template <typename T>
TArray<TTuple<types::usize, types::usize, T>> indexed_iter(Buffer<T> input) {
    return indexed_iter(input.data(), input.shape());
}

template <typename T>
TArray<T> filter(TArray<T> input, const TFunction<bool(T)>& func) {
    TArray<T> output;
    for (int32 i = 0; i < input.Num(); ++i) { if (func(input[i])) { output.Emplace(input[i]); } }
    return output;
}

template <typename T, typename Ret>
TArray<Ret> filter_map(
    TArray<T> input,
    const TFunction<TOptional<Ret>(T)>& func
) {
    TArray<Ret> output;
    for (int32 i = 0; i < input.Num(); ++i) {
        auto result = func(input[i]);
        if (result.IsSet()) { output.Emplace(result.GetValue()); }
    }
    return output;
}

template <typename T, typename Ret>
TSet<Ret> filter_map_unique(
    TArray<T> input,
    const TFunction<TOptional<Ret>(T)>& func
) {
    TSet<Ret> output;
    for (int32 i = 0; i < input.Num(); ++i) {
        auto result = func(input[i]);
        if (result.IsSet()) { output.Emplace(result.GetValue()); }
    }
    return output;
}

/**
 * From the Rust itertools crate documentaiton on try_collect:
 *
 * @code{.rs}.try_collect()@endcode
 * is a more convenient way of writing
 * @code{.rs}.collect::<Result<_, _>>()@endcode
 *
 * @par Example
 * @code{.rs}
 * use std::{fs, io};
 * use itertools::Itertools;
 * fn process_dir_entries(entries: &[fs::DirEntry]) {
 *      // ...
 * }
 *
 * fn do_stuff() -> io::Result<()> {
 *      let entries: Vec<_> = fs::read_dir(".")?.try_collect()?;
 *      process_dir_entries(&entries);
 *
 *      Ok(())
 * }
 * @endcode
 *
 * @brief Apply a function to a collection that returns a
 * @code std::expected<Ret, Err>@endcode and return a collection of all values
 * wrapped in an expected.
 *
 * @note A collection of Results are collected into  a result of a collection.
 * If any of the results is an error, the first error is returned.
 * Otherwise, a collection of all the unwrapped values is returned.
 *
 */
template <typename T, typename InErr, typename Ret, typename OutErr>
std::expected<TArray<Ret>, OutErr> map_try_collect(
    TArray<std::expected<T, InErr>> input,
    const TFunction<std::expected<Ret, OutErr>(std::expected<T, InErr>)
    >& func
) {
    TArray<Ret> output;
    output.Reserve(input.Num());
    for (int32 i = 0; i < input.Num(); ++i) {
        auto result = func(input[i]);
        if (!result.has_value()) { return std::unexpected{result.error()}; }
        output.Emplace(result.value());
    }
    return output;
}

template <typename ContainerType, typename T>
TArray<T> drain(ContainerType& input) {
    TArray<T> output;
    output.Reserve(input.Num());
    for (const auto& element : input) { output.Emplace(element); }
    input.Empty();
    return output;
}

template <typename T, typename B, typename F>
B fold(TArray<T> input, B init, F func) {
    auto accum = init;
    while (!input.IsEmpty()) {
        auto element = input.Pop();
        accum = func(accum, element);
    }
    return accum;
}

template <typename T>
TArray<T> flatten(const TArray<TArray<T>> Array) {
    TArray<T> output;
    for (const auto& subarray : Array) { output.Append(subarray); }
    return output;
}

template <typename T>
void retain(TDeque<T>& input, TFunction<bool(T)> f) {
    const auto len = input.Num();
    auto idx = 0;
    auto cur = 0;

    while (cur < len) {
        if (!f(input[cur])) {
            cur += 1;
            break;
        }
        cur += 1;
        idx += 1;
    }

    while (cur < len) {
        if (!f(input[cur])) {
            cur += 1;
            continue;
        }

        Swap(input[idx], input[cur]);
        cur += 1;
        idx += 1;
    }

    if (cur != idx) {
        // drop all elements after idx
        while (input.Num() > idx) {
            T dummy;
            if (!input.TryPopLast(dummy)) {
                UE_LOG(LogTemp, Fatal, TEXT("Failed to pop from deque while retaining!"));
            }
        }
    }
}

template <typename K, typename V>
void retain(TMap<K, V>& input, const TFunction<bool(K, V)>& f) {
    for (auto it = input.CreateIterator(); it; ++it) {
        if (!f(it.Key(), it.Value())) { it.RemoveCurrent(); }
    }
}

template <typename T>
void retain(TArray<T>& input, const TFunction<bool(T)>& f) {
    int32 idx = 0;
    for (int32 i = 0; i < input.Num(); ++i) { if (f(input[i])) { input[idx++] = input[i]; } }
    if (idx < input.Num()) { input.SetNum(idx); }
}

namespace parallel {
template <typename T>
TArray<T> par_map_impl(TArray<T> input, const TFunction<T(T)>& func) {
    TArray<T> output;
    output.SetNum(input.Num());
    ParallelFor(input.Num(), [&](int32 i) { output[i] = func(input[i]); });
    return output;
}

template <typename T>
TSet<T> par_map_impl(TArray<T> input, const TFunction<T(T)>& func) {
    TSet<T> output;
    ParallelFor(
        input.Num(),
        [&](int32 i) {
            const auto result = func(input[i]);
            // ReSharper disable once CppLocalVariableWithNonTrivialDtorIsNeverUsed
            FScopeLock lock(&output.GetCriticalSection());
            output.Emplace(result);
        });
    return output;
}

template <typename T>
TDeque<T> par_map_impl(TArray<T> input, const TFunction<T(T)>& func) {
    TDeque<T> output;
    ParallelFor(
        input.Num(),
        [&](int32 i) {
            const auto result = func(input[i]);
            // ReSharper disable once CppLocalVariableWithNonTrivialDtorIsNeverUsed
            FScopeLock lock(&output.GetCriticalSection());
            output.EmplaceLast(result);
        });
    return output;
}

template <typename K, typename V>
TMap<K, V> par_map_impl(TArray<TTuple<K, V>> input, const TFunction<TTuple<K, V>(K, V)>& func) {
    TMap<K, V> output;
    ParallelFor(
        input.Num(),
        [&](int32 i) {
            const auto& [key, value] = input[i];
            const auto& [new_key, new_value] = func(key, value);
            // ReSharper disable once CppLocalVariableWithNonTrivialDtorIsNeverUsed
            FScopeLock lock(&output.GetCriticalSection());
            output.Emplace(new_key, new_value);
        });
    return output;
}

template <typename T>
void par_for_each_impl(TArray<T> input, const TFunction<void(T)>& func) {
    ParallelFor(input.Num(), [&](int32 i) { func(input[i]); });
}

template <typename T>
void par_for_each_impl(TSet<T> input, const TFunction<void(T)>& func) {
    ParallelFor(
        input.Num(),
        [&](int32 i) {
            const auto& element = input.Array()[i];
            func(element);
        });
}

template <typename T>
void par_for_each_impl(TDeque<T> input, const TFunction<void(T)>& func) {
    ParallelFor(
        input.Num(),
        [&](int32 i) {
            const auto& element = input[i];
            func(element);
        });
}

template <typename K, typename V>
void par_for_each_impl(TMap<K, V> input, const TFunction<void(K, V)>& func) {
    auto arr = input.Array();
    ParallelFor(
        arr.Num(),
        [&](int32 i) {
            const auto& [key, value] = arr[i];
            func(key, value);
        });
}

template <template <typename> typename C, typename T>
C<T> par_map(C<T> input, const TFunction<T(T)>& func) {
    if constexpr (std::is_same_v<C<T>, TArray<T>> ||
        std::is_same_v<C<T>, TSet<T>> ||
        std::is_same_v<C<T>, TDeque<T>>) { return par_map_impl(input, func); } else {
        static_assert(
            std::is_same_v<C<T>, TArray<T>> ||
            std::is_same_v<C<T>, TSet<T>> ||
            std::is_same_v<C<T>, TDeque<T>>,
            "parallel map only supports TArray, TSet, and TDeque as "
            "ContainerType, if using TMap, did you forget to specify both key and value types?"
        );
    }
    return C<T>();
}

template <template <typename, typename> typename C, typename K, typename V>
C<K, V> par_map(C<K, V> input, const TFunction<TPair<K, V>(K, V)>& func) {
    if constexpr (std::is_same_v<C<K, V>, TMap<K, V>>) { return par_map_impl(input, func); } else {
        static_assert(
            std::is_same_v<C<K, V>, TMap<K, V>>,
            "parallel map only supports TMap as key-value container type,"
            "if using TArray, TSet, or TDeque, did you specify too many template parameters?"
        );
    }
    return C<K, V>();
}

template <template <typename> typename C, typename T>
void par_for_each(C<T> input, const TFunction<void(T)>& func) {
    if constexpr (std::is_same_v<C<T>, TArray<T>> ||
        std::is_same_v<C<T>, TSet<T>> ||
        std::is_same_v<C<T>, TDeque<T>>) { par_for_each_impl(input, func); } else {
        static_assert(
            std::is_same_v<C<T>, TArray<T>> ||
            std::is_same_v<C<T>, TSet<T>> ||
            std::is_same_v<C<T>, TDeque<T>>,
            "parallel for_each only supports TArray, TSet, and TDeque as "
            "ContainerType, if using TMap, did you forget to specify both key and value types?"
        );
    }
}

template <template <typename, typename> typename C, typename K, typename V>
void par_for_each(C<K, V> input, const TFunction<void(K, V)>& func) {
    if constexpr (std::is_same_v<C<K, V>, TMap<K, V>>) {
        par_for_each_impl<K, V>(input, func);
    } else {
        static_assert(
            std::is_same_v<C<K, V>, TMap<K, V>>,
            "parallel for_each only supports TMap as key-value container type,"
            "if using TArray, TSet, or TDeque, did you specify too many template parameters?"
        );
    }
}
} // namespace ext::iter::parallel
} // namespace ext::iter
