#pragma once
#include <PrimitiveSceneProxy.h>

class FTerrainSceneProxy final : FPrimitiveSceneProxy {
    [[nodiscard]] FTerrainSceneProxy(const UPrimitiveComponent* const from_prim_comp, const FName& name);
    [[nodiscard]] FTerrainSceneProxy(const FPrimitiveSceneProxyDesc& from_prim_scene_proxy_desc, const FName& name);
    [[nodiscard]] explicit FTerrainSceneProxy(const FPrimitiveSceneProxy& from_prim_scene_proxy);

public:
    virtual SIZE_T GetTypeHash() const override;
    virtual uint32 GetMemoryFootprint() const override;
    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& views, const FSceneViewFamily& vf, uint32 visibility_map,
        FMeshElementCollector& c
    ) const override;
    virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* pdi) override;
};
