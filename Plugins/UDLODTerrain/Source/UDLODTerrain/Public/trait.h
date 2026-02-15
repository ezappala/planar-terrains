#pragma once
#include "GlobalShader.h"

namespace UDLODTerrain::traits {
template <class T>
concept HasFParameters = requires {
    typename T::FParameters;
};

template <class T>
concept IsGlobalShaderWithParameters =
    TIsDerivedFrom<T, FGlobalShader>::Value && HasFParameters<T>;
}
