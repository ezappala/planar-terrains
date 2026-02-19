// #pragma once
// #include "EGP_CustomRenderPasses.h"
//
// class UDLODTERRAIN_API FTerrainPass final : public U_EGP_RenderPass {
//
// };

#pragma once

#include "EGP_PostProcessMaterialShaders.h"

void RenderTerrainState(
    FRDGBuilder& graph, const FViewInfo& view,
    FRDGTextureRef simStateTex,
    FRHIBlendState* blending,
    const FRenderTargetBinding& output,
    const UMaterialInterface* material,
    const FSceneTextureShaderParameters& sceneTextures
);

void EnsureCompiledSimulateShader(ERHIFeatureLevel::Type featureLevel, const UMaterialInterface* uMaterial);
void InitTerrainState(
    FRDGBuilder& graph, FRDGTextureRef simStateTex,
    const FViewInfo& view, const FSceneTextureShaderParameters& sceneTextures,
    const UMaterialInterface* uMaterial
);

void UpdateTerrainState(
    FRDGBuilder& graph,
    const FViewInfo& view,
    FRDGTextureRef currentSimState,
    FRDGTextureRef nextSimState,
    float deltaSeconds,
    const UMaterialInterface* uMaterial
);
