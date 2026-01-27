#pragma once

#include <PrimitiveSceneProxy.h>

#include "RHIGPUReadback.h"
#include "UDLODTerrainComponent.h"
#include "UDLODTerrainResources.h"

struct FUDLODTerrainSceneProxy final : FPrimitiveSceneProxy {
    FUDLODTerrainSceneProxy(const UUDLODTerrainComponent* comp);
    virtual ~FUDLODTerrainSceneProxy() override;

    virtual void CreateRenderThreadResources(FRHICommandListBase& cmd) override;
    virtual void DestroyRenderThreadResources() override;

    virtual SIZE_T GetTypeHash() const override { return 0x0D10D; }
    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override;
    virtual uint32 GetMemoryFootprint() const override { return sizeof(*this) + GetAllocatedSize(); }
    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& Views,
        const FSceneViewFamily& ViewFamily,
        uint32 VisibilityMap,
        FMeshElementCollector& Collector
    ) const override;

    FUDLODTerrainSettingsRT settings() const { return settings_rt; }
    FUDLODTerrainResources resources() const { return resources_rt; }

    FTextureRHIRef height_texture_rhi() const { return height_texture; }
    FRHISamplerState* height_sampler_rhi() const { return height_sampler; }

    void render_udlod(
        FRDGBuilder& graph_builder,
        const FSceneView& view,
        FRDGTextureRef scene_color,
        FRDGTextureRef scene_depth
    ) const;

private:
    FUDLODTerrainSettingsRT settings_rt;
    FUDLODTerrainResources resources_rt;

    FTextureRHIRef height_texture = nullptr;
    FRHISamplerState* height_sampler = nullptr;
};
