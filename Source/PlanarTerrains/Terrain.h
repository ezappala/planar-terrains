#pragma once
#include "terrain_component.h"
#include "GameFramework/Actor.h"

// #include "UDLODTerrainComponent.h"
#include "Terrain.generated.h"

UCLASS()
class PLANARTERRAINS_API ATerrain : public AActor {
    GENERATED_BODY()

public:
    ATerrain();

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    USceneComponent* root;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    UTerrainComponent* terrain;
};
