#pragma once
// #include "terrain_component.h"
#include "component.h"
#include "GameFramework/Actor.h"

// #include "UDLODTerrainComponent.h"
#include "Terrain.generated.h"

UCLASS()
class PLANARTERRAINS_API ATerrain : public AActor {
    GENERATED_BODY()

public:
    ATerrain();
    virtual void BeginPlay() override;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    USceneComponent* root;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    UStaticMeshComponent* terrain_visualizer;

    UPROPERTY(BlueprintReadOnly, EditDefaultsOnly)
    UTerrainComponent* terrain_component;
};
