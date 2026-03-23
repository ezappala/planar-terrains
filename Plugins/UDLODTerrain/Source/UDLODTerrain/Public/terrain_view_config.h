#pragma once
#include "CoreTypes.h"

#include "terrain_view_config.generated.h"

USTRUCT()
struct FTerrainViewConfig {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    uint32 tree_size = 16;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    uint32 geometry_tile_count = 1000000;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    uint32 refinement_count = 30;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    uint32 grid_size = 16;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    float morph_range = 0.2;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    float blend_range = 0.2;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    double morph_distance = 40.0;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    double blend_distance = 5.0;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    double subdivision_tolerance = 0.1;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    double load_tolerance = 0.2;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    double precision_distance = 0.001;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    uint32 view_lod = 10;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    uint32 order = 0;
};
