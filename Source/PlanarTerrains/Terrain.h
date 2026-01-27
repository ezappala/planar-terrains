#pragma once

#include "CoreMinimal.h"
#include "UDLODTerrainComponent.h"
#include "Terrain.generated.h"

UCLASS()
class PLANARTERRAINS_API ATerrain : public AActor {
    GENERATED_BODY()

public:
    ATerrain();

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    USceneComponent* root;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    UUDLODTerrainComponent* terrain;
};
