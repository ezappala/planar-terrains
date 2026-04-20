#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include <atomic>

#include "ext_buffer.h"
#include "ext_iter.h"
#include "Containers/ArrayView.h"

using namespace udlodext::tests;

namespace {
template <typename T>
bool check_deque_equals(
    FAutomationTestBase& test,
    const FString& label,
    const TDeque<T>& actual,
    std::initializer_list<T> expected) {
    const TArray<T> expected_array(expected);
    bool matches = actual.Num() == expected_array.Num();
    test.TestEqual(
        FString::Printf(TEXT("%s length"), *label),
        actual.Num(),
        expected_array.Num());

    const int32 count = FMath::Min(actual.Num(), expected_array.Num());
    for (int32 index = 0; index < count; ++index) {
        const bool element_matches = actual[index] == expected_array[index];
        matches &= element_matches;
        test.TestTrue(
            FString::Printf(TEXT("%s element %d"), *label, index),
            element_matches);
    }

    return matches;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtIterZipAndMapHelpersTest,
    "UDLODExt.Iter.ZipAndMapHelpers",
    TestFlags)

bool FUDLODExtIterZipAndMapHelpersTest::RunTest(const FString& Parameters) {
    const TArray<int32> numbers{1, 2, 3};
    const TArray<FString> words{TEXT("one"), TEXT("two")};
    const TArray<bool> flags{true, false, true};
    const auto number_view = MakeArrayView(numbers);
    const auto word_view = MakeArrayView(words);
    const auto flag_view = MakeArrayView(flags);

    const auto zipped = ext::iter::zip(number_view, word_view);
    TestEqual(TEXT("Two-array zip stops at shortest input"), zipped.Num(), 2);
    TestEqual(TEXT("Zip first number"), zipped[0].Get<0>(), 1);
    TestEqual(TEXT("Zip first word"), zipped[0].Get<1>(), FString(TEXT("one")));
    TestEqual(TEXT("Zip second number"), zipped[1].Get<0>(), 2);
    TestEqual(TEXT("Zip second word"), zipped[1].Get<1>(), FString(TEXT("two")));

    const auto zipped_three = ext::iter::zip(number_view, word_view, flag_view);
    TestEqual(TEXT("Variadic zip stops at shortest input"), zipped_three.Num(), 2);
    TestTrue(TEXT("Variadic zip keeps the first flag"), zipped_three[0].Get<2>());
    TestFalse(TEXT("Variadic zip keeps the second flag"), zipped_three[1].Get<2>());

    TArray4D<int32> left(2, 1, 1, 2);
    left(0, 0, 0, 0) = 1;
    left(0, 0, 0, 1) = 2;
    left(1, 0, 0, 0) = 3;
    left(1, 0, 0, 1) = 4;

    TArray4D<int32> right(1, 2, 1, 3);
    right(0, 0, 0, 0) = 10;
    right(0, 0, 0, 1) = 11;
    right(0, 0, 0, 2) = 12;
    right(0, 1, 0, 0) = 20;
    right(0, 1, 0, 1) = 21;
    right(0, 1, 0, 2) = 22;

    const auto zipped4 = ext::iter::zip(left, right);
    TestEqual(TEXT("4D zip dim0"), zipped4.get_dim0(), static_cast<SIZE_T>(1));
    TestEqual(TEXT("4D zip dim1"), zipped4.get_dim1(), static_cast<SIZE_T>(1));
    TestEqual(TEXT("4D zip dim2"), zipped4.get_dim2(), static_cast<SIZE_T>(1));
    TestEqual(TEXT("4D zip dim3"), zipped4.get_dim3(), static_cast<SIZE_T>(2));
    TestEqual(TEXT("4D zip first left value"), zipped4(0, 0, 0, 0).Get<0>(), 1);
    TestEqual(TEXT("4D zip first right value"), zipped4(0, 0, 0, 0).Get<1>(), 10);
    TestEqual(TEXT("4D zip second left value"), zipped4(0, 0, 0, 1).Get<0>(), 2);
    TestEqual(TEXT("4D zip second right value"), zipped4(0, 0, 0, 1).Get<1>(), 11);

    const TArray<FString> letters{TEXT("A"), TEXT("B")};
    const auto product = ext::iter::product(number_view, MakeArrayView(letters));
    TestEqual(TEXT("Cartesian product count"), product.Num(), 6);
    TestEqual(TEXT("Product first pair number"), product[0].Get<0>(), 1);
    TestEqual(TEXT("Product first pair letter"), product[0].Get<1>(), FString(TEXT("A")));
    TestEqual(TEXT("Product last pair number"), product.Last().Get<0>(), 3);
    TestEqual(TEXT("Product last pair letter"), product.Last().Get<1>(), FString(TEXT("B")));

    const TArray<int32> offsets{10, 20, 30, 40};
    const TArray<int32> zip_mapped = ext::iter::zip_map(
        number_view,
        MakeArrayView(offsets),
        [](const int32 a, const int32 b) { return a + b; });
    TestTrue(
        TEXT("zip_map combines paired values"),
        zip_mapped == TArray<int32>{11, 22, 33});

    TArray<int32> mapped_into{999};
    ext::iter::map_into<int32, int32>(
        number_view,
        mapped_into,
        [](const int32 value) { return value * 2; });
    TestTrue(TEXT("map_into replaces prior contents"), mapped_into == TArray<int32>{2, 4, 6});

    const TArray<FString> mapped = ext::iter::map(
        number_view,
        [](const int32 value) { return FString::FromInt(value); });
    TestTrue(
        TEXT("Array map transforms each element"),
        mapped == TArray<FString>{TEXT("1"), TEXT("2"), TEXT("3")});

    TStaticArray<int32, 3> static_input;
    static_input[0] = 1;
    static_input[1] = 2;
    static_input[2] = 3;
    const auto mapped_static = ext::iter::map(
        static_input,
        [](const int32 value) { return value * value; });
    TestEqual(TEXT("Static map first element"), mapped_static[0], 1);
    TestEqual(TEXT("Static map second element"), mapped_static[1], 4);
    TestEqual(TEXT("Static map third element"), mapped_static[2], 9);

    TMap<FString, int32> map_input;
    map_input.Add(TEXT("one"), 1);
    map_input.Add(TEXT("two"), 2);
    const auto mapped_map = ext::iter::map(
        map_input,
        [](const FString& key, const int32 value) {
            return std::pair{value * 10, key.ToUpper()};
        });
    TestTrue(TEXT("Mapped TMap contains transformed first key"), mapped_map.Contains(10));
    TestTrue(TEXT("Mapped TMap contains transformed second key"), mapped_map.Contains(20));
    TestEqual(TEXT("Mapped TMap first value"), mapped_map[10], FString(TEXT("ONE")));
    TestEqual(TEXT("Mapped TMap second value"), mapped_map[20], FString(TEXT("TWO")));

    const TDeque<int32> deque = ext::iter::deque_from_array(number_view);
    return check_deque_equals(*this, TEXT("deque_from_array"), deque, {1, 2, 3});
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtIterRangeAndOrderHelpersTest,
    "UDLODExt.Iter.RangeAndOrderHelpers",
    TestFlags)

bool FUDLODExtIterRangeAndOrderHelpersTest::RunTest(const FString& Parameters) {
    const TArray<int32> array_range = ext::iter::range<TArray<int32>>(2, 5);
    TestTrue(TEXT("Array range is half-open"), array_range == TArray<int32>{2, 3, 4});

    const TSet<int32> set_range = ext::iter::range<TSet<int32>>(2, 5);
    TestEqual(TEXT("Set range count"), set_range.Num(), 3);
    TestTrue(TEXT("Set range contains 2"), set_range.Contains(2));
    TestTrue(TEXT("Set range contains 3"), set_range.Contains(3));
    TestTrue(TEXT("Set range contains 4"), set_range.Contains(4));

    const TDeque<int32> deque_range = ext::iter::range<TDeque<int32>>(2, 5);
    const bool deque_range_ok = check_deque_equals(*this, TEXT("Deque range"), deque_range, {2, 3, 4});

    const TArray<int32> inclusive_array = ext::iter::range_inclusive<TArray<int32>>(2, 4);
    TestTrue(
        TEXT("Inclusive array range includes the end value"),
        inclusive_array == TArray<int32>{2, 3, 4});

    const TSet<int32> inclusive_set = ext::iter::range_inclusive<TSet<int32>>(7, 8);
    TestEqual(TEXT("Inclusive set count"), inclusive_set.Num(), 2);
    TestTrue(TEXT("Inclusive set contains start"), inclusive_set.Contains(7));
    TestTrue(TEXT("Inclusive set contains end"), inclusive_set.Contains(8));

    const TDeque<int32> inclusive_deque = ext::iter::range_inclusive<TDeque<int32>>(1, 3);
    const bool inclusive_deque_ok = check_deque_equals(
        *this,
        TEXT("Inclusive deque range"),
        inclusive_deque,
        {1, 2, 3});

    const TArray<int32> map_range = ext::iter::map_range(
        1,
        4,
        [](const int32 value) { return value * value; });
    TestTrue(TEXT("map_range transforms each generated value"), map_range == TArray<int32>{1, 4, 9});

    const TArray<int32> map_range_inclusive = ext::iter::map_range_inclusive(
        1,
        3,
        [](const int32 value) { return value * value; });
    TestTrue(
        TEXT("map_range_inclusive includes the end value"),
        map_range_inclusive == TArray<int32>{1, 4, 9});

    const TArray<int32> reversed = ext::iter::rev(MakeArrayView(array_range));
    TestTrue(TEXT("rev flips array order"), reversed == TArray<int32>{4, 3, 2});

    const TArray<int32> map_reversed = ext::iter::map_rev(
        MakeArrayView(array_range),
        [](const int32 value) { return value * 10; });
    TestTrue(
        TEXT("map_rev iterates from the back"),
        map_reversed == TArray<int32>{40, 30, 20});

    const TArray<int32> map_range_reversed = ext::iter::map_range_rev(
        1,
        4,
        [](const int32 value) { return value; });
    TestTrue(
        TEXT("map_range_rev yields descending values"),
        map_range_reversed == TArray<int32>{3, 2, 1});

    const TArray<int32> map_range_inclusive_reversed = ext::iter::map_range_inclusive_rev(
        1,
        3,
        [](const int32 value) { return value; });
    TestTrue(
        TEXT("map_range_inclusive_rev yields descending inclusive values"),
        map_range_inclusive_reversed == TArray<int32>{3, 2, 1});

    const TArray<int32> stepped = ext::iter::step_by(0, 10, 3);
    TestTrue(TEXT("step_by advances by the requested stride"), stepped == TArray<int32>{0, 3, 6, 9});

    const auto enumerated = ext::iter::enumerate(MakeArrayView(array_range));
    TestEqual(TEXT("Enumerate preserves count"), enumerated.Num(), 3);
    TestEqual(TEXT("Enumerate first index"), enumerated[0].Get<0>(), static_cast<SIZE_T>(0));
    TestEqual(TEXT("Enumerate first value"), enumerated[0].Get<1>(), 2);
    TestEqual(TEXT("Enumerate third index"), enumerated[2].Get<0>(), static_cast<SIZE_T>(2));
    TestEqual(TEXT("Enumerate third value"), enumerated[2].Get<1>(), 4);

    return deque_range_ok && inclusive_deque_ok;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtIterIndexAndFilterHelpersTest,
    "UDLODExt.Iter.IndexAndFilterHelpers",
    TestFlags)

bool FUDLODExtIterIndexAndFilterHelpersTest::RunTest(const FString& Parameters) {
    const TArray<int32> raster{10, 11, 12, 13, 14, 15};
    const ext::types::usize_c shape{3u, 2u};
    const auto raster_view = MakeArrayView(raster);

    const auto indexed = ext::iter::indexed_iter(raster_view, shape);
    TestEqual(TEXT("indexed_iter count"), indexed.Num(), 6);
    TestEqual(TEXT("indexed_iter first x"), indexed[0].Get<0>(), static_cast<SIZE_T>(0));
    TestEqual(TEXT("indexed_iter first y"), indexed[0].Get<1>(), static_cast<SIZE_T>(0));
    TestEqual(TEXT("indexed_iter first value"), indexed[0].Get<2>(), 10);
    TestEqual(TEXT("indexed_iter fourth x"), indexed[3].Get<0>(), static_cast<SIZE_T>(0));
    TestEqual(TEXT("indexed_iter fourth y"), indexed[3].Get<1>(), static_cast<SIZE_T>(1));
    TestEqual(TEXT("indexed_iter fourth value"), indexed[3].Get<2>(), 13);

    TArray<FString> visited;
    ext::iter::indexed_for_each(
        raster_view,
        shape,
        [&visited](const SIZE_T x, const SIZE_T y, const int32 value) {
            visited.Add(FString::Printf(TEXT("%llu,%llu=%d"), x, y, value));
        });
    TestTrue(
        TEXT("indexed_for_each traverses logical x/y order"),
        visited == TArray<FString>{
            TEXT("0,0=10"),
            TEXT("1,0=11"),
            TEXT("2,0=12"),
            TEXT("0,1=13"),
            TEXT("1,1=14"),
            TEXT("2,1=15")
        });

    ext::Buffer<int32> buffer(TArray<int32>{10, 11, 12, 13, 14, 15}, shape);
    const auto indexed_buffer = ext::iter::indexed_iter(buffer);
    TestEqual(TEXT("indexed_iter(Buffer) count"), indexed_buffer.Num(), 6);
    TestEqual(TEXT("indexed_iter(Buffer) last x"), indexed_buffer.Last().Get<0>(), static_cast<SIZE_T>(2));
    TestEqual(TEXT("indexed_iter(Buffer) last y"), indexed_buffer.Last().Get<1>(), static_cast<SIZE_T>(1));
    TestEqual(TEXT("indexed_iter(Buffer) last value"), indexed_buffer.Last().Get<2>(), 15);

    TArray<int32> filter_mapped_into{999};
    ext::iter::filter_map_into<int32, int32>(
        raster_view,
        filter_mapped_into,
        [](const int32 value) -> TOptional<int32> {
            if (value % 2 == 0) { return value / 2; }
            return NullOpt;
        });
    TestTrue(
        TEXT("filter_map_into replaces the output with mapped matches"),
        filter_mapped_into == TArray<int32>{5, 6, 7});

    const auto filter_mapped = ext::iter::filter_map<int32, FString>(
        raster_view,
        [](const int32 value) -> TOptional<FString> {
            if (value >= 14) { return FString::FromInt(value); }
            return NullOpt;
        });
    TestTrue(
        TEXT("filter_map emits only populated results"),
        filter_mapped == TArray<FString>{TEXT("14"), TEXT("15")});

    const auto filtered = ext::iter::filter(
        raster_view,
        [](const int32 value) { return value % 2 == 1; });
    TestTrue(TEXT("filter keeps elements matching the predicate"), filtered == TArray<int32>{11, 13, 15});

    TArray<int32> filtered_into{999};
    ext::iter::filter_into(
        raster_view,
        filtered_into,
        [](const int32 value) { return value < 13; });
    TestTrue(TEXT("filter_into replaces the output"), filtered_into == TArray<int32>{10, 11, 12});

    const TSet<int32> unique = ext::iter::filter_map_unique<int32, int32>(
        raster_view,
        [](const int32 value) -> TOptional<int32> {
            if (value < 15) { return value % 3; }
            return NullOpt;
        });
    TestEqual(TEXT("filter_map_unique returns unique matches"), unique.Num(), 3);
    TestTrue(TEXT("filter_map_unique contains 0"), unique.Contains(0));
    TestTrue(TEXT("filter_map_unique contains 1"), unique.Contains(1));
    TestTrue(TEXT("filter_map_unique contains 2"), unique.Contains(2));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtIterCollectDrainAndRetainTest,
    "UDLODExt.Iter.CollectDrainAndRetain",
    TestFlags)

bool FUDLODExtIterCollectDrainAndRetainTest::RunTest(const FString& Parameters) {
    const TArray<int32> ok_input{1, 2, 3};
    const auto collected = ext::iter::map_try_collect(
        MakeArrayView(ok_input),
        [](const int32 value) -> std::expected<FString, FString> {
            return FString::FromInt(value * 2);
        });
    TestTrue(TEXT("map_try_collect succeeds when every mapping succeeds"), collected.has_value());
    if (!collected.has_value()) { return false; }
    TestTrue(
        TEXT("map_try_collect unwraps successful results"),
        collected.value() == TArray<FString>{TEXT("2"), TEXT("4"), TEXT("6")});

    int32 calls = 0;
    const TArray<int32> fail_input{1, 2, 3, 4};
    const auto failed = ext::iter::map_try_collect(
        MakeArrayView(fail_input),
        [&calls](const int32 value) -> std::expected<int32, FString> {
            ++calls;
            if (value == 3) { return std::unexpected(FString(TEXT("boom"))); }
            return value;
        });
    TestFalse(TEXT("map_try_collect surfaces the first error"), failed.has_value());
    if (failed.has_value()) { return false; }
    TestEqual(TEXT("map_try_collect preserves the failing error"), failed.error(), FString(TEXT("boom")));
    TestEqual(TEXT("map_try_collect stops at the first error"), calls, 3);

    TArray<int32> array_to_drain{1, 2, 3};
    const TArray<int32> drained_array = ext::iter::drain(array_to_drain);
    TestTrue(TEXT("drain(TArray) returns all elements"), drained_array == TArray<int32>{1, 2, 3});
    TestTrue(TEXT("drain(TArray) empties the source"), array_to_drain.IsEmpty());

    TDeque<int32> deque_to_drain;
    deque_to_drain.EmplaceLast(4);
    deque_to_drain.EmplaceLast(5);
    const TArray<int32> drained_deque = ext::iter::drain(deque_to_drain);
    const bool drained_deque_ok = drained_deque == TArray<int32>{4, 5};
    TestTrue(TEXT("drain(TDeque) preserves element order"), drained_deque_ok);
    TestEqual(TEXT("drain(TDeque) empties the source"), deque_to_drain.Num(), 0);

    TSet<int32> set_to_drain{7, 8};
    const TArray<int32> drained_set = ext::iter::drain<TSet<int32>, int32>(set_to_drain);
    TestEqual(TEXT("Generic drain returns every set element"), drained_set.Num(), 2);
    TestTrue(TEXT("Generic drain empties the set"), set_to_drain.IsEmpty());
    TestTrue(TEXT("Generic drain keeps 7"), drained_set.Contains(7));
    TestTrue(TEXT("Generic drain keeps 8"), drained_set.Contains(8));

    const TArray<int32> fold_input{1, 2, 3};
    const int32 folded = ext::iter::fold(
        MakeArrayView(fold_input),
        10,
        [](const int32 accum, const int32 value) { return accum + value; });
    TestEqual(TEXT("fold threads the accumulator left-to-right"), folded, 16);

    const TArray<int32> flattened = ext::iter::flatten(
        TArray<TArray<int32>>{
            TArray<int32>{1, 2},
            TArray<int32>{3},
            TArray<int32>{}
        });
    TestTrue(TEXT("flatten concatenates subarrays"), flattened == TArray<int32>{1, 2, 3});

    TDeque<int32> deque_to_retain;
    deque_to_retain.EmplaceLast(1);
    deque_to_retain.EmplaceLast(2);
    deque_to_retain.EmplaceLast(3);
    deque_to_retain.EmplaceLast(4);
    ext::iter::retain(
        deque_to_retain,
        [](const int32 value) { return value % 2 == 0; });
    const bool retained_deque_ok = check_deque_equals(
        *this,
        TEXT("retain(TDeque)"),
        deque_to_retain,
        {2, 4});

    TMap<FString, int32> map_to_retain;
    map_to_retain.Add(TEXT("one"), 1);
    map_to_retain.Add(TEXT("two"), 2);
    map_to_retain.Add(TEXT("three"), 3);
    ext::iter::retain(
        map_to_retain,
        [](const FString&, const int32 value) { return value >= 2; });
    TestEqual(TEXT("retain(TMap) removes rejected entries"), map_to_retain.Num(), 2);
    TestFalse(TEXT("retain(TMap) drops rejected key"), map_to_retain.Contains(TEXT("one")));
    TestTrue(TEXT("retain(TMap) keeps accepted key"), map_to_retain.Contains(TEXT("two")));
    TestTrue(TEXT("retain(TMap) keeps accepted key"), map_to_retain.Contains(TEXT("three")));

    TArray<int32> array_to_retain{1, 2, 3, 4, 5};
    ext::iter::retain(
        array_to_retain,
        [](const int32 value) { return value % 2 == 1; });
    TestTrue(TEXT("retain(TArray) preserves kept order"), array_to_retain == TArray<int32>{1, 3, 5});

    return drained_deque_ok && retained_deque_ok;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtIterParallelHelpersTest,
    "UDLODExt.Iter.ParallelHelpers",
    TestFlags)

bool FUDLODExtIterParallelHelpersTest::RunTest(const FString& Parameters) {
    const TArray<int32> numbers{1, 2, 3, 4};
    const TArray<int32> mapped_array = ext::iter::parallel::par_map(
        MakeArrayView(numbers),
        [](const int32 value) { return value * 2; });
    TestTrue(TEXT("par_map on arrays preserves input order"), mapped_array == TArray<int32>{2, 4, 6, 8});

    const TSet<int32> input_set{1, 2, 3};
    const TSet<int32> mapped_set = ext::iter::parallel::par_map(
        input_set,
        [](const int32 value) { return value * 3; });
    TestEqual(TEXT("par_map on sets keeps unique outputs"), mapped_set.Num(), 3);
    TestTrue(TEXT("par_map on sets contains 3"), mapped_set.Contains(3));
    TestTrue(TEXT("par_map on sets contains 6"), mapped_set.Contains(6));
    TestTrue(TEXT("par_map on sets contains 9"), mapped_set.Contains(9));

    TDeque<int32> input_deque;
    input_deque.EmplaceLast(5);
    input_deque.EmplaceLast(6);
    input_deque.EmplaceLast(7);
    const TDeque<int32> mapped_deque = ext::iter::parallel::par_map(
        input_deque,
        [](const int32 value) { return value - 1; });
    const bool mapped_deque_ok = check_deque_equals(
        *this,
        TEXT("par_map(TDeque)"),
        mapped_deque,
        {4, 5, 6});

    TMap<FString, int32> input_map;
    input_map.Add(TEXT("a"), 1);
    input_map.Add(TEXT("bb"), 2);
    const TMap<int32, int32> mapped_map = ext::iter::parallel::par_map(
        input_map,
        [](const FString& key, const int32 value) {
            return TTuple<int32, int32>{value, key.Len()};
        });
    TestEqual(TEXT("par_map(TMap) output count"), mapped_map.Num(), 2);
    TestEqual(TEXT("par_map(TMap) value for key 1"), mapped_map[1], 1);
    TestEqual(TEXT("par_map(TMap) value for key 2"), mapped_map[2], 2);

    std::atomic<int32> array_sum{0};
    ext::iter::parallel::par_for_each(
        MakeArrayView(numbers),
        [&array_sum](const int32 value) {
            array_sum.fetch_add(value, std::memory_order_relaxed);
        });
    TestEqual(TEXT("par_for_each(TArray) visits every value"), array_sum.load(), 10);

    std::atomic<int32> set_sum{0};
    ext::iter::parallel::par_for_each(
        input_set,
        [&set_sum](const int32 value) {
            set_sum.fetch_add(value, std::memory_order_relaxed);
        });
    TestEqual(TEXT("par_for_each(TSet) visits every value"), set_sum.load(), 6);

    std::atomic<int32> deque_sum{0};
    ext::iter::parallel::par_for_each(
        input_deque,
        [&deque_sum](const int32 value) {
            deque_sum.fetch_add(value, std::memory_order_relaxed);
        });
    TestEqual(TEXT("par_for_each(TDeque) visits every value"), deque_sum.load(), 18);

    std::atomic<int32> map_sum{0};
    ext::iter::parallel::par_for_each(
        input_map,
        [&map_sum](const FString&, const int32 value) {
            map_sum.fetch_add(value, std::memory_order_relaxed);
        });
    TestEqual(TEXT("par_for_each(TMap) visits every entry"), map_sum.load(), 3);

    return mapped_deque_ok;
}

#endif
