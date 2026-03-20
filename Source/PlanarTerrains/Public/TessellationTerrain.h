#pragma once
#include <UObject/ObjectMacros.h>

#include "GPUTessellationComponent.h"

#include "TessellationTerrain.generated.h"

UCLASS()
class ATessellationTerrain : public AActor {
    GENERATED_BODY()

public:
    ATessellationTerrain() {
        TessComp = CreateDefaultSubobject<UGPUTessellationComponent>(TEXT("Terrain"));
        RootComponent = TessComp;

        // Terrain settings
        TessComp->TessellationSettings.TessellationFactor = 32;
        TessComp->TessellationSettings.PlaneSizeX = 10000.0f; // 100m × 100m
        TessComp->TessellationSettings.PlaneSizeY = 10000.0f;
        TessComp->TessellationSettings.DisplacementIntensity = 500.0f;
        TessComp->TessellationSettings.bUseSineWaveDisplacement = false;

        // Enable LOD
        TessComp->TessellationSettings.LODMode = EGPUTessellationLODMode::DistanceBased;
        TessComp->TessellationSettings.MaxTessellationFactor = 64;
        TessComp->TessellationSettings.MinTessellationFactor = 8;

        // Normal calculation
        TessComp->TessellationSettings.NormalCalculationMethod =
            EGPUTessellationNormalMethod::FiniteDifference;
    }

    UPROPERTY(VisibleAnywhere)
    UGPUTessellationComponent* TessComp;
};