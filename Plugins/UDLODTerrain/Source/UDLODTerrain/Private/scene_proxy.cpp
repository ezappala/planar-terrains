#include "scene_proxy.h"

#include <Misc/HashBuilder.h>

FTerrainSceneProxy::FTerrainSceneProxy(
    const UPrimitiveComponent* const from_prim_comp, const FName& name
) : FPrimitiveSceneProxy{from_prim_comp, name} {}

FTerrainSceneProxy::FTerrainSceneProxy(
    const FPrimitiveSceneProxyDesc& from_prim_scene_proxy_desc, const FName& name
) : FPrimitiveSceneProxy{from_prim_scene_proxy_desc, name} {}

FTerrainSceneProxy::FTerrainSceneProxy(const FPrimitiveSceneProxy& from_prim_scene_proxy) : FPrimitiveSceneProxy{
    from_prim_scene_proxy
} {}

SIZE_T FTerrainSceneProxy::GetTypeHash() const {
    return typeid(FTerrainSceneProxy).hash_code();
}

uint32 FTerrainSceneProxy::GetMemoryFootprint() const {
    return sizeof(*this) + GetAllocatedSize();
}

void FTerrainSceneProxy::GetDynamicMeshElements(
    const TArray<const FSceneView*>& views, const FSceneViewFamily& vf, uint32 visibility_map, FMeshElementCollector& c
) const {
    FPrimitiveSceneProxy::GetDynamicMeshElements(views, vf, visibility_map, c);
}

void FTerrainSceneProxy::DrawStaticElements(FStaticPrimitiveDrawInterface* pdi) {
    FPrimitiveSceneProxy::DrawStaticElements(pdi);
}
