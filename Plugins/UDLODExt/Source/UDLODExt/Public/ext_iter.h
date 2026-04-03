#pragma once

#include <concepts>
#include <expected>
#include <functional>
#include <type_traits>
#include <utility>

#include "ext_array4.h"
#include "ext_buffer.h"
#include "Async/ParallelFor.h"
#include "Containers/Array.h"
#include "Containers/Deque.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Misc/AssertionMacros.h"
#include "Templates/Tuple.h"

namespace ext::iter {
namespace detail {
template <typename T>
using decay_invoke_result_t = std::decay_t<T>;

template <typename T>
struct expected_traits;

template <typename value, typename Error>
struct expected_traits<std::expected<value, Error>> {
    using value_type = value;
    using error_type = Error;
};

template <typename T>
inline constexpr bool is_expected_v = false;

template <typename value, typename Error>
inline constexpr bool is_expected_v<std::expected<value, Error>> = true;

template <typename T>
struct pair_like_traits;

template <typename K, typename V>
struct pair_like_traits<TTuple<K, V>> {
    using first_type = K;
    using second_type = V;

    static const K& first(const TTuple<K, V>& pair) { return pair.template Get<0>(); }
    static const V& second(const TTuple<K, V>& pair) { return pair.template Get<1>(); }
};

template <typename K, typename V>
struct pair_like_traits<std::pair<K, V>> {
    using first_type = K;
    using second_type = V;

    static const K& first(const std::pair<K, V>& pair) { return pair.first; }
    static const V& second(const std::pair<K, V>& pair) { return pair.second; }
};

template <typename T>
concept pair_like = requires(const T& pair) {
    typename pair_like_traits<std::decay_t<T>>::first_type;
    typename pair_like_traits<std::decay_t<T>>::second_type;
    { pair_like_traits<std::decay_t<T>>::first(pair) };
    { pair_like_traits<std::decay_t<T>>::second(pair) };
};

template <typename ContainerType, typename T>
inline constexpr bool is_supported_range_container_v =
    std::is_same_v<ContainerType, TArray<T>> ||
    std::is_same_v<ContainerType, TSet<T>> ||
    std::is_same_v<ContainerType, TDeque<T>>;

template <typename T>
void append_value(TArray<T>& output, T value) { output.Emplace(MoveTemp(value)); }

template <typename T>
void append_value(TSet<T>& output, T value) { output.Emplace(MoveTemp(value)); }

template <typename T>
void append_value(TDeque<T>& output, T value) { output.EmplaceLast(MoveTemp(value)); }

template <typename T>
int32 min_num(TConstArrayView<T> input) { return input.Num(); }

template <typename T, typename... Ts>
int32 min_num(TConstArrayView<T> input, TConstArrayView<Ts>... inputs) {
    int32 Shortest = input.Num();
    ((Shortest = FMath::Min(Shortest, inputs.Num())), ...);
    return Shortest;
}

template <typename T>
TArray<T> snapshot(const TSet<T>& input) {
    TArray<T> output;
    output.Reserve(input.Num());
    for (const T& Element : input) { output.Emplace(Element); }
    return output;
}

template <typename T>
TArray<T> snapshot(const TDeque<T>& input) {
    TArray<T> output;
    output.Reserve(input.Num());
    for (int32 index = 0; index < input.Num(); ++index) { output.Emplace(input[index]); }
    return output;
}

template <typename K, typename V>
TArray<TPair<K, V>> snapshot(const TMap<K, V>& input) {
    TArray<TPair<K, V>> output;
    output.Reserve(input.Num());
    for (const TPair<K, V>& Entry : input) { output.Emplace(Entry.Key, Entry.Value); }
    return output;
}
} // namespace detail

/**
 * Create an array of multiple arrays in lockstep. The zip function yields
 * elements until any subarray is exhausted.
 */
template <typename A, typename B>
TArray<TTuple<A, B>> zip(TConstArrayView<A> left, TConstArrayView<B> right) {
    const int32 Shortest = FMath::Min(left.Num(), right.Num());

    TArray<TTuple<A, B>> output;
    output.Reserve(Shortest);

    for (int32 index = 0; index < Shortest; ++index) { output.Emplace(left[index], right[index]); }

    return output;
}

template <typename A, typename... Ts>
TArray<TTuple<A, Ts...>> zip(TConstArrayView<A> input, TConstArrayView<Ts>... inputs) {
    const int32 shortest = detail::min_num(input, inputs...);

    TArray<TTuple<A, Ts...>> output;
    output.Reserve(shortest);

    for (int32 index = 0; index < shortest; ++index) {
        output.Emplace(input[index], inputs[index]...);
    }

    return output;
}

template <typename T1, typename T2>
TArray4D<TTuple<T1, T2>> zip(const TArray4D<T1>& left, const TArray4D<T2>& right) {
    const auto sd0 = FMath::Min(left.get_dim0(), right.get_dim0());
    const auto sd1 = FMath::Min(left.get_dim1(), right.get_dim1());
    const auto sd2 = FMath::Min(left.get_dim2(), right.get_dim2());
    const auto sd3 = FMath::Min(left.get_dim3(), right.get_dim3());

    TArray4D<TTuple<T1, T2>> output(sd0, sd1, sd2, sd3);

    using types::usize;
    for (usize i0 = 0; i0 < sd0; ++i0) {
        for (usize i1 = 0; i1 < sd1; ++i1) {
            for (usize i2 = 0; i2 < sd2; ++i2) {
                for (usize i3 = 0; i3 < sd3; ++i3) {
                    output(i0, i1, i2, i3) = TTuple<T1, T2>{
                        left(i0, i1, i2, i3),
                        right(i0, i1, i2, i3)};
                }
            }
        }
    }

    return output;
}

/**
 * Create an array of the cartesian product of two input arrays.
 */
template <typename A, typename B>
TArray<TTuple<A, B>> product(TConstArrayView<A> left, TConstArrayView<B> right) {
    TArray<TTuple<A, B>> output;
    output.Reserve(left.Num() * right.Num());

    for (const A& left_element : left) {
        for (const B& right_element : right) { output.Emplace(left_element, right_element); }
    }

    return output;
}

template <typename A, typename B, typename F>
TArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, const A&, const B&>>> zip_map(
    TConstArrayView<A> left,
    TConstArrayView<B> right,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, const A&, const B&>>;

    const int32 shortest = FMath::Min(left.Num(), right.Num());

    TArray<Ret> output;
    output.Reserve(shortest);

    for (int32 index = 0; index < shortest; ++index) {
        output.Emplace(std::invoke(func, left[index], right[index]));
    }

    return output;
}

template <typename T, typename Ret, typename F>
void map_into(TConstArrayView<T> input, TArray<Ret>& output, F&& func) {
    output.Reset();
    output.Reserve(input.Num());

    for (const T& Element : input) { output.Emplace(std::invoke(func, Element)); }
}

template <typename T, typename F>
TArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>> map(
    TConstArrayView<T> input,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>;

    TArray<Ret> output;
    output.Reserve(input.Num());

    for (const T& element : input) { output.Emplace(std::invoke(func, element)); }

    return output;
}

template <typename T, types::usize N, typename F>
TStaticArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>, N> map(
    const TStaticArray<T, N>& input,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>;

    TStaticArray<Ret, N> output;
    for (types::usize index = 0; index < N; ++index) {
        output[index] = std::invoke(func, input[index]);
    }
    return output;
}

template <typename Key, typename value, typename F>
    requires detail::pair_like<std::invoke_result_t<F&, const Key&, const value&>>
TMap<
    typename detail::pair_like_traits<detail::decay_invoke_result_t<std::invoke_result_t<F&, const
        Key&, const value&>>>::first_type,
    typename detail::pair_like_traits<detail::decay_invoke_result_t<std::invoke_result_t<F&, const
        Key&, const value&>>>::second_type> map(const TMap<Key, value>& input, F&& func) {
    using Pair = detail::decay_invoke_result_t<std::invoke_result_t<F&, const Key&, const value&>>;
    using PairTraits = detail::pair_like_traits<Pair>;
    using RetKey = PairTraits::first_type;
    using Retvalue = PairTraits::second_type;

    TMap<RetKey, Retvalue> output;

    for (const TPair<Key, value>& entry : input) {
        const Pair result = std::invoke(func, entry.Key, entry.Value);
        output.Emplace(PairTraits::first(result), PairTraits::second(result));
    }

    return output;
}

template <typename T>
TDeque<T> deque_from_array(TConstArrayView<T> input) {
    TDeque<T> output;
    for (const T& element : input) { output.EmplaceLast(element); }
    return output;
}

template <typename ContainerType, std::integral T>
    requires detail::is_supported_range_container_v<ContainerType, T>
ContainerType range(T start, T end) {
    check(start < end);

    ContainerType output;
    if constexpr (std::is_same_v<ContainerType, TArray<T>>) {
        output.Reserve(static_cast<int32>(end - start));
    } else if constexpr (std::is_same_v<ContainerType, TSet<T>>) {
        output.Reserve(static_cast<int32>(end - start));
    }

    for (T value = start; value < end; ++value) { detail::append_value(output, value); }

    return output;
}

template <typename ContainerType, std::integral T>
    requires detail::is_supported_range_container_v<ContainerType, T>
ContainerType range_inclusive(T start, T end) {
    check(start <= end);

    ContainerType output;
    if constexpr (std::is_same_v<ContainerType, TArray<T>>) {
        output.Reserve(static_cast<int32>(end - start + 1));
    } else if constexpr (std::is_same_v<ContainerType, TSet<T>>) {
        output.Reserve(static_cast<int32>(end - start + 1));
    }

    for (T value = start;; ++value) {
        detail::append_value(output, value);
        if (value == end) { break; }
    }

    return output;
}

template <std::integral T, typename F>
TArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, T>>> map_range(
    T start,
    T end,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, T>>;

    check(start < end);

    TArray<Ret> output;
    output.Reserve(static_cast<int32>(end - start));

    for (T value = start; value < end; ++value) { output.Emplace(std::invoke(func, value)); }

    return output;
}

template <std::integral T, typename F>
TArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, T>>> map_range_inclusive(
    T start,
    T end,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, T>>;

    check(start <= end);

    TArray<Ret> output;
    output.Reserve(static_cast<int32>(end - start + 1));

    for (T value = start;; ++value) {
        output.Emplace(std::invoke(func, value));
        if (value == end) { break; }
    }

    return output;
}

template <typename T>
TArray<T> rev(TConstArrayView<T> input) {
    TArray<T> output;
    output.Reserve(input.Num());

    for (int32 index = input.Num(); index-- > 0;) { output.Emplace(input[index]); }

    return output;
}

template <typename T, typename F>
TArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>> map_rev(
    TConstArrayView<T> input,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>;

    TArray<Ret> output;
    output.Reserve(input.Num());

    for (int32 index = input.Num(); index-- > 0;) {
        output.Emplace(std::invoke(func, input[index]));
    }

    return output;
}

template <std::integral T, typename F>
TArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, T>>> map_range_rev(
    T start,
    T end,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, T>>;

    check(start < end);

    TArray<Ret> output;
    output.Reserve(static_cast<int32>(end - start));

    for (T value = end; value-- > start;) { output.Emplace(std::invoke(func, value)); }

    return output;
}

template <std::integral T, typename F>
TArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, T>>> map_range_inclusive_rev(
    T start,
    T end,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, T>>;

    check(start <= end);

    TArray<Ret> output;
    output.Reserve(static_cast<int32>(end - start + 1));

    for (T value = end;; --value) {
        output.Emplace(std::invoke(func, value));
        if (value == start) { break; }
    }

    return output;
}

template <std::integral T>
TArray<T> step_by(T start, T end, T step) {
    check(start < end);
    check(step > 0);

    TArray<T> output;

    for (T value = start; value < end; value = static_cast<T>(value + step)) {
        output.Emplace(value);
    }

    return output;
}

template <typename T>
TArray<TTuple<types::usize, T>> enumerate(TConstArrayView<T> input) {
    TArray<TTuple<types::usize, T>> output;
    output.Reserve(input.Num());

    for (int32 index = 0; index < input.Num(); ++index) {
        output.Emplace(static_cast<types::usize>(index), input[index]);
    }

    return output;
}

/**
 * Return an array of indexes and values from the input array.
 * Elements are visited in the logical order of the array, which is where the
 * rightmost index is varying the fastest.
 *
 * Note that input arrays are row-major, meaning the shape is (width, height)
 * but the rightmost index is the x coordinate, which is the column. This is
 * because GDAL datasets are row-major, and we want to be able to index into
 * them directly.
 */
template <typename T>
TArray<TTuple<types::usize, types::usize, T>> indexed_iter(
    TConstArrayView<T> input,
    TTuple<types::usize, types::usize> shape
) {
    check(shape.Get<0>() > 0);

    TArray<TTuple<types::usize, types::usize, T>> output;
    output.Reserve(input.Num());

    const types::usize Width = shape.Get<0>();

    for (int32 index = 0; index < input.Num(); ++index) {
        const types::usize X = static_cast<types::usize>(index) % Width;
        const types::usize Y = static_cast<types::usize>(index) / Width;
        output.Emplace(X, Y, input[index]);
    }

    return output;
}

template <typename T, typename F>
void indexed_for_each(
    TConstArrayView<T> input,
    TTuple<types::usize, types::usize> shape,
    F&& func
) {
    check(shape.Get<0>() > 0);

    const types::usize width = shape.Get<0>();

    for (int32 index = 0; index < input.Num(); ++index) {
        const types::usize X = static_cast<types::usize>(index) % width;
        const types::usize Y = static_cast<types::usize>(index) / width;
        std::invoke(func, X, Y, input[index]);
    }
}

template <typename T>
TArray<TTuple<types::usize, types::usize, T>> indexed_iter(const Buffer<T>& input) {
    const types::usize_c shape = input.shape();
    check(shape.Get<0>() > 0);

    TArray<TTuple<types::usize, types::usize, T>> output;
    output.Reserve(input.len());

    const types::usize width = shape.Get<0>();

    for (int32 index = 0; index < input.len(); ++index) {
        const types::usize X = static_cast<types::usize>(index) % width;
        const types::usize Y = static_cast<types::usize>(index) / width;
        output.Emplace(X, Y, input.data()[index]);
    }

    return output;
}

template <typename T, typename Ret, typename F>
void filter_map_into(TConstArrayView<T> input, TArray<Ret>& output, F&& func) {
    output.Reset();
    output.Reserve(input.Num());

    for (const T& Element : input) {
        TOptional<Ret> result = std::invoke(func, Element);
        if (result.IsSet()) { output.Emplace(result.GetValue()); }
    }
}

template <typename T, typename Ret, typename F>
TArray<Ret> filter_map(TConstArrayView<T> input, F&& func) {
    TArray<Ret> output;
    filter_map_into<T, Ret>(input, output, std::forward<F>(func));
    return output;
}

template <typename T, typename F>
TArray<T> filter(TConstArrayView<T> input, F&& func) {
    TArray<T> output;
    output.Reserve(input.Num());

    for (const T& element : input) { if (std::invoke(func, element)) { output.Emplace(element); } }

    return output;
}

template <typename T, typename F>
void filter_into(TConstArrayView<T> input, TArray<T>& output, F&& func) {
    output.Reset();
    output.Reserve(input.Num());

    for (const T& element : input) { if (std::invoke(func, element)) { output.Emplace(element); } }
}

template <typename T, typename Ret, typename F>
TSet<Ret> filter_map_unique(TConstArrayView<T> input, F&& func) {
    TSet<Ret> output;
    output.Reserve(input.Num());

    for (const T& element : input) {
        TOptional<Ret> result = std::invoke(func, element);
        if (result.IsSet()) { output.Emplace(result.GetValue()); }
    }

    return output;
}

/**
 * Apply a function that returns std::expected<Ret, Err> and collect the
 * unwrapped values into a single expected.
 *
 * The first error short-circuits the collection.
 */
template <typename T, typename F>
std::expected<
    TArray<typename detail::expected_traits<detail::decay_invoke_result_t<std::invoke_result_t<F&,
        const T&>>>::value_type>,
    typename detail::expected_traits<detail::decay_invoke_result_t<std::invoke_result_t<F&, const T
        &>>>::error_type> map_try_collect(TConstArrayView<T> input, F&& func) {
    using Result = detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>;
    static_assert(
        detail::is_expected_v<Result>,
        "map_try_collect requires func to return std::expected<...>");

    using Ret = detail::expected_traits<Result>::value_type;
    // using Err = detail::expected_traits<Result>::error_type;

    TArray<Ret> output;
    output.Reserve(input.Num());

    for (const T& element : input) {
        Result mapped = std::invoke(func, element);
        if (!mapped.has_value()) { return std::unexpected(mapped.error()); }
        output.Emplace(mapped.value());
    }

    return output;
}

template <typename T>
TArray<T> drain(TArray<T>& input) {
    TArray<T> output = MoveTemp(input);
    input.Reset();
    return output;
}

template <typename T>
TArray<T> drain(TDeque<T>& input) {
    TArray<T> output;
    output.Reserve(input.Num());

    for (int32 index = 0; index < input.Num(); ++index) { output.Emplace(MoveTemp(input[index])); }

    input.Empty();
    return output;
}

template <typename ContainerType, typename T>
TArray<T> drain(ContainerType& input) {
    TArray<T> output;
    output.Reserve(input.Num());

    for (const T& element : input) { output.Emplace(element); }

    input.Empty();
    return output;
}

template <typename T, typename B, typename F>
B fold(TConstArrayView<T> input, B init, F&& func) {
    B accum = MoveTemp(init);
    for (const T& Element : input) { accum = std::invoke(func, MoveTemp(accum), Element); }
    return accum;
}

template <typename T>
TArray<T> flatten(const TArray<TArray<T>>& input) {
    int32 total_count = 0;
    for (const TArray<T>& subarr : input) { total_count += subarr.Num(); }

    TArray<T> output;
    output.Reserve(total_count);

    for (const TArray<T>& subarr : input) { output.Append(subarr); }

    return output;
}

template <typename T, typename F>
void retain(TDeque<T>& input, F&& predicate) {
    const int32 len = input.Num();
    int32 write_index = 0;

    for (int32 read_index = 0; read_index < len; ++read_index) {
        if (!std::invoke(predicate, input[read_index])) { continue; }

        if (write_index != read_index) { Swap(input[write_index], input[read_index]); }

        ++write_index;
    }

    while (input.Num() > write_index) {
        T removed;
        const bool was_removed = input.TryPopLast(removed);
        check(was_removed);
    }
}

template <typename K, typename V, typename F>
void retain(TMap<K, V>& input, F&& predicate) {
    for (auto it = input.CreateIterator(); it; ++it) {
        if (!std::invoke(predicate, it.Key(), it.Value())) { it.RemoveCurrent(); }
    }
}

template <typename T, typename F>
void retain(TArray<T>& input, F&& predicate) {
    int32 write_index = 0;

    for (int32 read_index = 0; read_index < input.Num(); ++read_index) {
        if (std::invoke(predicate, input[read_index])) {
            if (write_index != read_index) { input[write_index] = MoveTemp(input[read_index]); }
            ++write_index;
        }
    }

    if (write_index < input.Num()) { input.SetNum(write_index, EAllowShrinking::No); }
}

namespace parallel {
template <typename T, typename F>
TArray<detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>> par_map(
    TConstArrayView<T> input,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>;

    TArray<Ret> output;
    output.SetNum(input.Num(), EAllowShrinking::No);

    ParallelFor(
        input.Num(),
        [&](int32 index) { output[index] = std::invoke(func, input[index]); });

    return output;
}

template <typename T, typename F>
TSet<detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>> par_map(
    const TSet<T>& input,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>;

    const TArray<T> snapshot = detail::snapshot(input);
    TArray<Ret> mapped;
    mapped.SetNum(snapshot.Num(), EAllowShrinking::No);

    ParallelFor(
        snapshot.Num(),
        [&](int32 index) { mapped[index] = std::invoke(func, snapshot[index]); });

    TSet<Ret> output;
    output.Reserve(mapped.Num());
    for (Ret& element : mapped) { output.Emplace(MoveTemp(element)); }

    return output;
}

template <typename T, typename F>
TDeque<detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>> par_map(
    const TDeque<T>& input,
    F&& func) {
    using Ret = detail::decay_invoke_result_t<std::invoke_result_t<F&, const T&>>;

    const TArray<T> snapshot = detail::snapshot(input);
    TArray<Ret> mapped;
    mapped.SetNum(snapshot.Num(), EAllowShrinking::No);

    ParallelFor(
        snapshot.Num(),
        [&](int32 index) { mapped[index] = std::invoke(func, snapshot[index]); });

    TDeque<Ret> output;
    for (Ret& Element : mapped) { output.EmplaceLast(MoveTemp(Element)); }

    return output;
}

template <typename K, typename V, typename F>
    requires detail::pair_like<std::invoke_result_t<F&, const K&, const V&>>
TMap<
    typename detail::pair_like_traits<detail::decay_invoke_result_t<std::invoke_result_t<F&, const K
        &, const V&>>>::first_type,
    typename detail::pair_like_traits<detail::decay_invoke_result_t<std::invoke_result_t<F&, const K
        &, const V&>>>::second_type> par_map(const TMap<K, V>& input, F&& func) {
    using Pair = detail::decay_invoke_result_t<std::invoke_result_t<F&, const K&, const V&>>;
    using PairTraits = detail::pair_like_traits<Pair>;
    using RetKey = PairTraits::first_type;
    using Retvalue = PairTraits::second_type;

    const TArray<TPair<K, V>> snapshot = detail::snapshot(input);
    TArray<Pair> mapped;
    mapped.SetNum(snapshot.Num(), EAllowShrinking::No);

    ParallelFor(
        snapshot.Num(),
        [&](int32 index) {
            mapped[index] = std::invoke(func, snapshot[index].Key, snapshot[index].Value);
        });

    TMap<RetKey, Retvalue> output;
    for (const Pair& Result : mapped) {
        output.Emplace(PairTraits::first(Result), PairTraits::second(Result));
    }

    return output;
}

template <typename T, typename F>
void par_for_each(TConstArrayView<T> input, F&& func) {
    ParallelFor(input.Num(), [&](int32 index) { std::invoke(func, input[index]); });
}

template <typename T, typename F>
void par_for_each(const TSet<T>& input, F&& func) {
    const TArray<T> snapshot = detail::snapshot(input);
    ParallelFor(snapshot.Num(), [&](int32 index) { std::invoke(func, snapshot[index]); });
}

template <typename T, typename F>
void par_for_each(const TDeque<T>& input, F&& func) {
    const TArray<T> snapshot = detail::snapshot(input);
    ParallelFor(snapshot.Num(), [&](int32 index) { std::invoke(func, snapshot[index]); });
}

template <typename K, typename V, typename F>
void par_for_each(const TMap<K, V>& input, F&& func) {
    const TArray<TPair<K, V>> snapshot = detail::snapshot(input);
    ParallelFor(
        snapshot.Num(),
        [&](int32 index) { std::invoke(func, snapshot[index].Key, snapshot[index].Value); });
}
} // namespace parallel
} // namespace ext::iter
