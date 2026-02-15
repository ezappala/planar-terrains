#pragma once
#include "CoreTypes.h"

namespace ext {
template <typename T, typename I>
    requires std::is_arithmetic_v<T> && std::is_arithmetic_v<I>
constexpr T pow(I base, uint8 exp) {
    if (exp == 0) {
        return 1;
    }

    T result = 1;
    while (exp > 1) {
        if (exp % 2 == 1) {
            result *= base;
            exp -= 1;
        }
        base *= base;
        exp /= 2;
    }
    return base * result;
}
}
