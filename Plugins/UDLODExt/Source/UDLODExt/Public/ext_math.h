#pragma once

#include "Math/Vector.h"

namespace ext::math {
inline FVector3f scale(const double side_length) {
    return {
        static_cast<float>(side_length),
        1.0f,
        static_cast<float>(side_length)
    };
}
}