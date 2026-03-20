#pragma once

#include "CoreTypes.h"
#include "ext_types.h"
#include "Containers/Array.h"
#include "Misc/AssertionMacros.h"

/**
 * TArray4D<T>
 * - A lightweight, contiguous 4D array (owns storage).
 * - Layout: row-major, last axis fastest (index = ((i0*d1 + i1)*d2 + i2)*d3 + i3)
 * - Provides const/non-const operator() indexing, simple subview (non-owning),
 *   fill, map, reshape (if total size matches), and raw data access for GPU uploads.
 */
template <typename T>
class TArray4D {
public:
    // Constructors
    TArray4D() = default;
    using usize = ext::types::usize;

    TArray4D(const usize d0, const usize d1, const usize d2, const usize d3) : dim0(d0),
        dim1(d1),
        dim2(d2),
        dim3(d3) {
        recompute_strides();
        _data.Init(T(), size());
    }

    TArray4D(
        const usize d0,
        const usize d1,
        const usize d2,
        const usize d3,
        const T& initial) : dim0(d0),
        dim1(d1),
        dim2(d2),
        dim3(d3) {
        recompute_strides();
        _data.Init(initial, size());
    }

    // Basic queries
    FORCEINLINE usize get_dim0() const { return dim0; }
    FORCEINLINE usize get_dim1() const { return dim1; }
    FORCEINLINE usize get_dim2() const { return dim2; }
    FORCEINLINE usize get_dim3() const { return dim3; }
    FORCEINLINE usize size() const { return dim0 * dim1 * dim2 * dim3; }
    FORCEINLINE bool IsEmpty() const { return size() == 0; }

    // Indexing (with debug checks)
    FORCEINLINE T& operator()(const usize i0, const usize i1, const usize i2, const usize i3) {
        checkSlow(valid_indices(i0, i1, i2, i3));
        return _data[compute_linear_index(i0, i1, i2, i3)];
    }

    FORCEINLINE const T& operator()(
        const usize i0,
        const usize i1,
        const usize i2,
        const usize i3) const {
        checkSlow(valid_indices(i0, i1, i2, i3));
        return _data[compute_linear_index(i0, i1, i2, i3)];
    }

    // Raw data pointer (contiguous)
    FORCEINLINE T* data() { return _data.GetData(); }
    FORCEINLINE const T* data() const { return _data.GetData(); }
    FORCEINLINE TArray<T>& get_storage() { return _data; }
    FORCEINLINE const TArray<T>& get_storage() const { return _data; }

    // Iteration over contiguous storage
    FORCEINLINE T* begin() { return data(); }
    FORCEINLINE T* end() { return data() + size(); }
    FORCEINLINE const T* begin() const { return data(); }
    FORCEINLINE const T* end() const { return data() + size(); }

    // Fill entire array with value
    void fill(const T& Value) { for (usize i = 0; i < size(); ++i) { _data[i] = Value; } }

    // Apply a function to each element: func(T&) or func(const T&)
    template <typename Func>
    void map_in_place(Func&& F) { for (usize i = 0; i < size(); ++i) { F(_data[i]); } }

    // Reshape (only allowed when total size matches)
    bool reshape(const usize Newd0, const usize Newd1, const usize Newd2, const usize Newd3) {
        if (Newd0 * Newd1 * Newd2 * Newd3 != size()) { return false; }
        dim0 = Newd0;
        dim1 = Newd1;
        dim2 = Newd2;
        dim3 = Newd3;
        recompute_strides();
        return true;
    }

    // Convenience: shallow copy of underlying TArray (full copy semantics)
    FORCEINLINE TArray<T> release_storrage() { return MoveTemp(_data); }

    friend bool operator==(const TArray4D& lhs, const TArray4D& rhs) {
        return lhs.dim0 == rhs.dim0 && lhs.dim1 == rhs.dim1 && lhs.dim2 == rhs.dim2 &&
            lhs.dim3 == rhs.dim3 && lhs._data == rhs._data;
    }

    // --- Simple non-owning view (slice) type ---
    struct FView {
        FView() = default;

        FView(
            T* in_data,
            const usize in_d0,
            const usize in_d1,
            const usize in_d2,
            const usize in_d3,
            const usize in_s0,
            const usize in_s1,
            const usize in_s2,
            const usize in_s3
        ) : data_ptr(in_data) {
            d0 = in_d0;
            d1 = in_d1;
            d2 = in_d2;
            d3 = in_d3;
            S0 = in_s0;
            S1 = in_s1;
            S2 = in_s2;
            S3 = in_s3;
        }

        FORCEINLINE bool valid_indices(
            const usize i0,
            const usize i1,
            const usize i2,
            const usize i3) const { return i0 < d0 && i1 < d1 && i2 < d2 && i3 < d3; }

        FORCEINLINE T& operator()(
            const usize i0,
            const usize i1,
            const usize i2,
            const usize i3) const {
            checkSlow(valid_indices(i0, i1, i2, i3));
            return *(data_ptr + i0 * S0 + i1 * S1 + i2 * S2 + i3 * S3);
        }

        T* data_ptr = nullptr;
        usize d0 = 0, d1 = 0, d2 = 0, d3 = 0;
        usize S0 = 0, S1 = 0, S2 = 0, S3 = 0; // strides (in elements)
    };

    // Create a full view (non-owning) to the whole array
    FView AsView() {
        return FView(data(), dim0, dim1, dim2, dim3, stride0, stride1, stride2, stride3);
    }

    // Slice along an axis: returns a view that references the original memory (no copy)
    // axis in [0..3], start >= 0, length > 0
    // Example: SliceAxis(0, 1, 2) picks indices i0 in [1..2] with dims adjusted.
    FView SliceAxis(const usize Axis, const usize Start, usize Length) {
        checkSlow(Axis < 4);
        checkSlow(Length > 0);
        switch (Axis) {
        case 0:
            checkSlow(Start + Length <= dim0);
            return FView(
                data() + Start * stride0,
                Length,
                dim1,
                dim2,
                dim3,
                stride0,
                stride1,
                stride2,
                stride3);
        case 1:
            checkSlow(Start + Length <= dim1);
            return FView(
                data() + Start * stride1,
                dim0,
                Length,
                dim2,
                dim3,
                stride0,
                stride1,
                stride2,
                stride3);
        case 2:
            checkSlow(Start + Length <= dim2);
            return FView(
                data() + Start * stride2,
                dim0,
                dim1,
                Length,
                dim3,
                stride0,
                stride1,
                stride2,
                stride3);
        default: // 3
            checkSlow(Start + Length <= dim3);
            return FView(
                data() + Start * stride3,
                dim0,
                dim1,
                dim2,
                Length,
                stride0,
                stride1,
                stride2,
                stride3);
        }
    }

private:
    // underlying storage
    TArray<T> _data;

    // dims & strides
    usize dim0 = 0, dim1 = 0, dim2 = 0, dim3 = 0;
    usize stride0 = 0, stride1 = 0, stride2 = 0, stride3 = 0;

    FORCEINLINE void recompute_strides() {
        stride3 = 1;
        stride2 = dim3 * stride3;
        stride1 = dim2 * stride2;
        stride0 = dim1 * stride1;
    }

    FORCEINLINE usize compute_linear_index(
        const usize i0,
        const usize i1,
        const usize i2,
        const usize i3) const {
        // last axis fastest
        return ((i0 * dim1 + i1) * dim2 + i2) * dim3 + i3;
    }

    FORCEINLINE bool valid_indices(
        const usize i0,
        const usize i1,
        const usize i2,
        const usize i3) const { return i0 < dim0 && i1 < dim1 && i2 < dim2 && i3 < dim3; }
};

template <typename T>
FORCEINLINE uint32 GetTypeHash(const TArray4D<T>& arr) {
    uint32 hash = 0;
    hash = HashCombine(hash, GetTypeHash(arr.get_dim0()));
    hash = HashCombine(hash, GetTypeHash(arr.get_dim1()));
    hash = HashCombine(hash, GetTypeHash(arr.get_dim2()));
    hash = HashCombine(hash, GetTypeHash(arr.get_dim3()));
    for (const auto& elem : arr) { hash = HashCombine(hash, GetTypeHash(elem)); }
    return hash;
}
