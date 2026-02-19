#pragma once
#include <MeshPassProcessor.h>

#include "shaders.h"

/// @brief Generates actual draw calls for various kinds of 3D primitives.
class FTerrainMeshProcessor final : public FMeshPassProcessor {
public:
    FMeshPassProcessorRenderState PassDrawState;

    FTerrainMeshProcessor(
        const FScene* scene, const FSceneView* view,
        const ERHIFeatureLevel::Type featureLevel,
        FMeshPassDrawListContext* commandsOutput,
        const FBlendStateRHIRef& blendState
    );

    void AddMeshBatch(
        const FMeshBatch& batch,
        const uint64 batchElementMask,
        const FPrimitiveSceneProxy* proxy,
        const int32 staticMeshID,
        FRHITexture* prevState, FRHISamplerState* prevStateSampler
    );

    //The usual 'AddMeshBatch()' will not be used; instead we will use an alternative with more parameters.
    virtual void AddMeshBatch(
        const FMeshBatch& batch, uint64 batchElementMask,
        const FPrimitiveSceneProxy* proxy, int32 staticMeshID
    ) override;
};
