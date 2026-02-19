#pragma once
#include <RHIResources.h>

/// @brief Custom render-pass settings for each individual mesh batch within each component we want to render.
/// This is actually where we need to keep all shader parameters even if they're not per-batch.
struct FTerrainMeshShaderElementData : FMeshMaterialShaderElementData {
    FRHITexture* PreviousStateTex;
    FRHISamplerState* PreviousStateSampler;
};
