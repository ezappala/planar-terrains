#include "terrain_component.h"

#include "preprocess.h"
#include "TextureResource.h"
#include "UDLODTerrain.h"
#include "Logging/StructuredLog.h"

UTerrainComponent::UTerrainComponent() {
    PrimaryComponentTick.bCanEverTick = false;
    bUseAsOccluder = true;
    UTerrainComponent::SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// void UTerrainComponent::OnComponentCreated() {
//     Super::OnComponentCreated();
//     if (dispatcher == nullptr) {
//         dispatcher = MakeUnique<FCSDispatcher>();
//     }
//
//     dispatcher->heightmap = heightmap;
//     dispatcher->begin_pass();
// }
//
// void UTerrainComponent::OnComponentDestroyed(const bool bDestroyingHierarchy) {
//     if (dispatcher != nullptr) {
//         dispatcher->end_pass();
//     }
//
//     Super::OnComponentDestroyed(bDestroyingHierarchy);
// }

void UTerrainComponent::BeginPlay() {
    Super::BeginPlay();

    if (dispatcher == nullptr) {
        UE_LOGFMT(LogUDLODTerrain, Log, "[UTerrainComponent] Creating FCSDispatcher instance in BeginPlay.");
        dispatcher = MakeUnique<FCSDispatcher>();
    }

    dispatcher->heightmap = heightmap;
    dispatcher->begin_pass();
}

void UTerrainComponent::EndPlay(const EEndPlayReason::Type EndPlayReason) { Super::EndPlay(EndPlayReason); }

// ReSharper disable once CppMemberFunctionMayBeConst, UFUNCITONs must be non-const
// ReSharper disable once CppMemberFunctionMayBeStatic, UFUNCITONs must be non-static
void UTerrainComponent::PreprocessHeightmap() {
    UE_LOGFMT(LogUDLODTerrain, Log, "Begin heightmap preprocessing.");
}