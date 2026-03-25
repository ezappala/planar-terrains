#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "terrain.h"

struct FTerrainDynamicData {
    FVector camera_position;
    FMatrix local_to_world;

    FTerrainDynamicData() : camera_position(FVector::ZeroVector),
        local_to_world(FMatrix::Identity) {}
};

class FTerrainSceneProxy final : public FPrimitiveSceneProxy {
public:
    FTerrainSceneProxy(const UTerrain* component);
    virtual ~FTerrainSceneProxy() override;

#pragma region FPrimitiveSceneProxy_interface
    virtual SIZE_T GetTypeHash() const override;
    virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;
    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* view) const override;
    virtual uint32 GetMemoryFootprint() const override;
    uint32 GetAllocatedSize() const;
#pragma endregion

private:
    friend class UTerrain;
};
