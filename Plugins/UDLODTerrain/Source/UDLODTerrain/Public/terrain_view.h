#pragma once
#include <RHIResources.h>

#include "EGP_CustomRenderPasses.h"

//An instance of the Game of Life sim, running in one particular viewport.
struct UDLODTERRAIN_API FTerrainView final : F_EGP_ViewPersistentData {
    static FRHITextureCreateDesc SimStateDesc(const FInt32Point& viewportSize);
    TRefCountPtr<FRHITexture> SimState, SimBuffer;

    float NextTickTime = 0;
    bool ReinitializeViews = false;

    FTerrainView(
        FRDGBuilder& graph, const FViewInfo& view, const FIntRect& viewportSubset,
        const UMaterialInterface* initShaderMaterial,
        const FSceneTextureShaderParameters& sceneTextures
    );
    //Moves and destructor are handled automatically thanks to the ref-counted pointer.

    virtual void Resample(
        FRDGBuilder& graph, const FViewInfo& view,
        const FInt32Point& oldResolution, const FInt32Point& newResolution,
        const FInt32Point& offsetDelta
    ) override;
};
