#include "terrain_shaders.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(Terrain, "Terrain")
// IMPLEMENT_UNIFORM_BUFFER_STRUCT(Attachments, "Attachments")
// IMPLEMENT_UNIFORM_BUFFER_STRUCT(TerrainView, "TerrainView")
// IMPLEMENT_UNIFORM_BUFFER_STRUCT(TileTree, "TileTrees")
// IMPLEMENT_UNIFORM_BUFFER_STRUCT(ApproximateHeight, "ApproximateHeight")

IMPLEMENT_GLOBAL_SHADER(
    FTerrainPrepassPrepareRootComputeShader,
    "/Plugins/UDLODTerrain/prepare_prepass.usf",
    "prepare_root",
    SF_Compute)
IMPLEMENT_GLOBAL_SHADER(
    FTerrainPrepassPrepareNextComputeShader,
    "/Plugins/UDLODTerrain/prepare_prepass.usf",
    "prepare_next",
    SF_Compute)
IMPLEMENT_GLOBAL_SHADER(
    FTerrainPrepassPrepareRenderComputeShader,
    "/Plugins/UDLODTerrain/prepare_prepass.usf",
    "prepare_render",
    SF_Compute)
IMPLEMENT_GLOBAL_SHADER(
    FTerrainPrepassRefineTilesComputeShader,
    "/Plugins/UDLODTerrain/refine_tiles.usf",
    "refine_tiles",
    SF_Compute)
IMPLEMENT_GLOBAL_SHADER(
    FTerrainVertexShader,
    "/Plugins/UDLODTerrain/vertex.usf",
    "vertex",
    SF_Vertex)
IMPLEMENT_GLOBAL_SHADER(
    FTerrainFragmentShader,
    "/Plugins/UDLODTerrain/fragment.usf",
    "fragment",
    SF_Pixel)
