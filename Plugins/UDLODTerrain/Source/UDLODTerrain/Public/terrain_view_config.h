#pragma once
#include "CoreTypes.h"

struct FTerrainViewConfig {
    uint32 tree_size = 16;
    uint32 geometry_tile_count = 1000000;
    uint32 refinement_count = 30;
    uint32 grid_size = 16;
    float morph_range = 0.2;
    float blend_range = 0.2;
    double morph_distance = 40.0;
    double blend_distance = 5.0;
    double subdivision_tolerance = 0.1;
    double load_tolerance = 0.2;
    double precision_distance = 0.001;
    uint32 view_lod = 10;
    uint32 order = 0;
};
