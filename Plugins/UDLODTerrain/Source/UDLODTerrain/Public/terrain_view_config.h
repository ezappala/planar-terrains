#pragma once
#include "CoreTypes.h"

#include "terrain_view_config.generated.h"

USTRUCT(BlueprintType)
struct FTerrainViewConfig {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
    int32 tree_size = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
    int32 geometry_tile_count = 1000000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
    int32 refinement_count = 30;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1"))
    int32 grid_size = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float morph_range = 0.2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float blend_range = 0.2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double morph_distance = 40.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double blend_distance = 5.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double subdivision_tolerance = 0.1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double load_tolerance = 0.2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double precision_distance = 0.001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
    int32 view_lod = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0"))
    int32 order = 0;

    FString to_string() const {
        return FString::Printf(
            TEXT(
                "tree_size=%d, geometry_tile_count=%d, refinement_count=%d, grid_size=%d, "
                "morph_range=%f, blend_range=%f, morph_distance=%f, blend_distance=%f, "
                "subdivision_tolerance=%f, load_tolerance=%f, precision_distance=%f, "
                "view_lod=%d, order=%d"),
            tree_size,
            geometry_tile_count,
            refinement_count,
            grid_size,
            morph_range,
            blend_range,
            morph_distance,
            blend_distance,
            subdivision_tolerance,
            load_tolerance,
            precision_distance,
            view_lod,
            order
        );
    }
};
