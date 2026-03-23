#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "terrain.h"

class FTerrainSceneProxy final : public FPrimitiveSceneProxy {
public:
    FTerrainSceneProxy(const UTerrain* component);
    virtual ~FTerrainSceneProxy() override;

    virtual SIZE_T GetTypeHash() const override;

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* view) const override;
    virtual uint32 GetMemoryFootprint() const override;

private:
    mutable FVector last_camera_position;
    friend class UTerrain;
};
