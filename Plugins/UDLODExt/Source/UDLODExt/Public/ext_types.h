#pragma once
#include "CoreTypes.h"
#include "Math/IntPoint.h"

namespace ext::types {
using isize = SSIZE_T;
using usize = SIZE_T;

namespace internal {
template <typename T>
    requires std::is_arithmetic_v<T>
struct tuple_base : TTuple<T, T> {
    using Super = TTuple<T, T>;

    tuple_base(T a, T b) : Super(a, b) {}

    tuple_base(const FIntPoint& point) : Super(static_cast<T>(point.X), static_cast<T>(point.Y)) {}

    template <typename Rhs>
        requires std::is_arithmetic_v<Rhs>
    friend tuple_base operator-(const tuple_base& lhs, Rhs rhs) {
        return tuple_base{
            static_cast<T>(lhs.template Get<0>() - rhs),
            static_cast<T>(lhs.template Get<1>() - rhs)
        };
    }

    friend tuple_base operator-(const tuple_base& lhs, const tuple_base& rhs) {
        return tuple_base{
            lhs.template Get<0>() - rhs.template Get<0>(),
            lhs.template Get<1>() - rhs.template Get<1>()
        };
    }

    template <typename Rhs>
        requires std::is_arithmetic_v<Rhs>
    friend tuple_base operator+(const tuple_base& lhs, Rhs rhs) {
        return tuple_base{
            static_cast<T>(lhs.template Get<0>() + rhs),
            static_cast<T>(lhs.template Get<1>() + rhs)
        };
    }

    friend tuple_base operator+(const tuple_base& lhs, const tuple_base& rhs) {
        return tuple_base{
            lhs.template Get<0>() + rhs.template Get<0>(),
            lhs.template Get<1>() + rhs.template Get<1>()
        };
    }

    template <typename Rhs>
        requires std::is_arithmetic_v<Rhs>
    friend tuple_base operator*(const tuple_base& lhs, Rhs rhs) {
        return tuple_base{
            static_cast<T>(lhs.template Get<0>() * rhs),
            static_cast<T>(lhs.template Get<1>() * rhs)
        };
    }

    friend tuple_base operator*(const tuple_base& lhs, const tuple_base& rhs) {
        return tuple_base{
            lhs.template Get<0>() * rhs.template Get<0>(),
            lhs.template Get<1>() * rhs.template Get<1>()
        };
    }

    template <typename Rhs>
        requires std::is_arithmetic_v<Rhs>
    friend tuple_base operator/(const tuple_base& lhs, Rhs rhs) {
        return tuple_base{
            static_cast<T>(lhs.template Get<0>() / rhs),
            static_cast<T>(lhs.template Get<1>() / rhs)
        };
    }

    friend tuple_base operator/(const tuple_base& lhs, const tuple_base& rhs) {
        return tuple_base{
            lhs.template Get<0>() / rhs.template Get<0>(),
            lhs.template Get<1>() / rhs.template Get<1>()
        };
    }

    operator FIntPoint() const {
        return FIntPoint{
            static_cast<int32>(this->template Get<0>()),
            static_cast<int32>(this->template Get<1>())
        };
    }

    tuple_base transpose() const {
        return tuple_base{
            this->template Get<1>(),
            this->template Get<0>()
        };
    }
};

template <std::size_t Index, typename T>
auto&& tuple_base_get_helper(T&& p) {
    static_assert(
        Index < 2,
        "Index out of bounds for preprocess::internal::tuple_base<T>");
    if constexpr (Index == 0) { return std::forward<T>(p).template Get<0>(); }
    if constexpr (Index == 1) { return std::forward<T>(p).template Get<1>(); }
}

template <std::size_t Index, typename T>
auto&& get(tuple_base<T>& p) { return tuple_base_get_helper<Index>(p); }

template <std::size_t Index, typename T>
auto&& get(const tuple_base<T>& p) { return tuple_base_get_helper<Index>(p); }

template <std::size_t Index, typename T>
auto&& get(tuple_base<T>&& p) { return tuple_base_get_helper<Index>(std::move(p)); }

template <std::size_t Index, typename T>
auto&& get(const tuple_base<T>&& p) { return tuple_base_get_helper<Index>(std::move(p)); }
}
}

namespace std {
template <typename T>
struct tuple_size<ext::types::internal::tuple_base<T>>
    : integral_constant<size_t, 2> {};

template <size_t I, typename T>
struct tuple_element<I, ext::types::internal::tuple_base<T>>
    : conditional<I == 0, T, T> {
    static_assert(I < 2, "Index out of bounds for tuple_base<T>");
};
} // namespace std

namespace ext::types {
using isize_c = internal::tuple_base<isize>;
using usize_c = internal::tuple_base<usize>;
}