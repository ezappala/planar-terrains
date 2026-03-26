#pragma once

#include "CoreMinimal.h"
#include "terrain_mesh_vertex_factory.h"
#include "terrain_render_state.h"
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
    virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& views,
        const FSceneViewFamily& view_family,
        uint32 visibility_map,
        FMeshElementCollector& collector
    ) const override;
    virtual SIZE_T GetTypeHash() const override;
    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* view) const override;
    virtual uint32 GetMemoryFootprint() const override;
#pragma endregion

private:
    const FMaterialRenderProxy* material_proxy = nullptr;
    FMaterialRelevance material_relevance;
    TSharedPtr<FTerrainRenderResources, ESPMode::ThreadSafe> render_resources;
    FTerrainMeshVertexFactory vertex_factory;

    friend class UTerrain;
};
