#pragma once

#include "terrain.h"
#include "terrain_component.h"
#include "terrain_tile_atlas.h"
#include "UObject/ObjectMacros.h"

#include "terrain_actor.generated.h"

UCLASS()
class UDLODTERRAIN_API ATerrainActor : public AActor {
    GENERATED_BODY()

public:
    ATerrainActor();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD|Components")
    UTerrainComponent* terrain_component;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD|Components")
    UTerrain* terrain;
};