// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "UDLODTerrainComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UDLODTERRAIN_API UUDLODTerrainComponent : public UPrimitiveComponent {
    GENERATED_BODY()

public:
    UUDLODTerrainComponent();

    /** @brief Texture to sample from. */
    UPROPERTY(EditAnywhere, Category="UDLOD|Heightmap", meta=(ToolTip="Texture to sample height from. Preferred format is RF32, other floating point types like RF16 may work"))
    TObjectPtr<UTexture2D> heightmap = nullptr;

    /** @brief World size x and y */
    UPROPERTY(EditAnywhere, Category="UDLOD|Domain", DisplayName="World Size XY (World Space)")
    FVector2D world_size_xy_ws = {100000., 100000.};

    UPROPERTY(EditAnywhere, Category="UDLOD|Domain")
    float height_min_ws = 0.;

    UPROPERTY(EditAnywhere, Category="UDLOD|Domain")
    float height_max_ws = 200000.;

    UPROPERTY(EditAnywhere, Category="UDLOD|UDLOD")
    int32 grid_resolution = 65;

    UPROPERTY(EditAnywhere, Category="UDLOD|UDLOD")
    int32 max_lod = 10;

    UPROPERTY(EditAnywhere, Category="UDLOD|UDLOD")
    int32 max_tiles = 262144;

    UPROPERTY(EditAnywhere, Category="UDLOD|UDLOD")
    float error_pixels = 2.;

    UPROPERTY(EditAnywhere, Category="UDLOD|UDLOD")
    float height_error_0ws = 50.;

    UPROPERTY(EditAnywhere, Category="UDLOD|UDLOD")
    float morph_start_ratio = 1.5;

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& local_to_world) const override;

    virtual void OnRegister() override;
};
