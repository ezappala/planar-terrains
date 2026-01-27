#include "UDLODTerrainComponent.h"

#include "UDLODTerrainSceneProxy.h"
#include "Engine/Texture2D.h"

UUDLODTerrainComponent::UUDLODTerrainComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    bUseAsOccluder = true;
    UUDLODTerrainComponent::SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

FPrimitiveSceneProxy* UUDLODTerrainComponent::CreateSceneProxy() {
    if (!heightmap || grid_resolution < 2 || max_lod < 0) {
        return nullptr;
    }

    return new FUDLODTerrainSceneProxy(this);
}

FBoxSphereBounds UUDLODTerrainComponent::CalcBounds(const FTransform& LocalToWorld) const {
    const FVector extent_ws{world_size_xy_ws.X * 0.5, world_size_xy_ws.Y * 0.5, (height_max_ws - height_min_ws) * 0.5};
    const FVector center_ws{extent_ws.X, extent_ws.Y, extent_ws.Z + height_min_ws};

    const FBox box{center_ws - extent_ws, center_ws + extent_ws};
    return FBoxSphereBounds(box).TransformBy(LocalToWorld);
}

void UUDLODTerrainComponent::OnRegister() {
    Super::OnRegister();
    heightmap->UpdateResource();
}
