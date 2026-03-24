#pragma once

#include "cpl_error.h"
#include "ext_traits.h"
#include "ext_types.h"

namespace ext {
/**
 * A buffer of data read from or to be written to a GDAL raster band, along
 * with its shape.
 */
template <typename T>
    requires traits::GdalType<T> && traits::Copy<T>
struct Buffer {
    Buffer(TArray<T> data, const types::usize_c& shape) : _shape{shape},
        _data{MoveTemp(data)} {
        checkf(
            _shape.Get<0>() * _shape.Get<1>() == _data.Num(),
            TEXT("Shape %d*%d=%d does not match data length %d"),
            _shape.Get<0>(),
            _shape.Get<1>(),
            _shape.Get<0>() * _shape.Get<1>(),
            _data.Num()
        );
    }

    TTuple<types::isize_c, TArray<T>> into_shape_and_vec() && {
        return {
            _shape,
            MoveTemp(_data)
        };
    }

    types::usize width() const { return _shape.Get<0>(); }
    types::usize height() const { return _shape.Get<1>(); }
    types::usize_c shape() const { return _shape; }
    TArray<T>::RangedForIteratorType begin() { return _data.begin(); }
    TArray<T>::RangedForIteratorType end() { return _data.end(); }
    TArray<T>::RangedForConstIteratorType begin() const { return _data.begin(); }
    TArray<T>::RangedForConstIteratorType end() const { return _data.end(); }
    TArray<T>& data() { return _data; }
    const TArray<T>& data() const { return _data; }
    types::usize len() const { return _data.Num(); }
    bool is_empty() const { return _data.IsEmpty(); }

    types::usize vec_index_for(const types::isize_c& coord) {
        if (
            static_cast<types::usize>(coord.Get<0>()) >= _shape.Get<1>() ||
            static_cast<types::usize>(coord.Get<1>()) >= _shape.Get<0>()) {
            checkf(
                false,
                TEXT("Coordinate (%d, %d) is out of bounds for shape (%d, %d)"),
                coord.Get<0>(),
                coord.Get<1>(),
                _shape.Get<0>(),
                _shape.Get<1>());
        }
        return coord.Get<0>() * _shape.Get<0>() + coord.Get<1>();
    }

    T& operator[](const types::isize_c& coord) { return _data[vec_index_for(coord)]; }

    const T& operator[](const types::isize_c& coord) const {
        return _data[vec_index_for(coord)];
    }

private:
    types::usize_c _shape;
    TArray<T> _data;
};

template <typename T>
using BufferResult = std::expected<Buffer<T>, CPLErrorNum>;
}
