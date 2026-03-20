#pragma once

#include "CoreMinimal.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneProxy.h"
#include "PrimitiveViewRelevance.h"
#include "terrain_component.h"
#include "terrain_mesh_builder.h"

class FTerrainSceneProxy final : public FPrimitiveSceneProxy {
public:
    FTerrainSceneProxy(UTerrainComponent* component);
    virtual ~FTerrainSceneProxy() override;

    virtual SIZE_T GetTypeHash() const override;
    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& views,
        const FSceneViewFamily& view_family,
        uint32 visibility_map,
        FMeshElementCollector& collector
    ) const override;

    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* view) const override;
    virtual uint32 GetMemoryFootprint() const override;

private:
    TSharedPtr<FPrimaryTerrainSettings> settings;
    TSharedPtr<FTerrainPreprocessSettings> preproc_settings;

    mutable FVector last_camera_position;
    friend class UTerrainComponent;
};
