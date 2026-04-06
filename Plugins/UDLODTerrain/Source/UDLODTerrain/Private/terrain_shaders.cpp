#include "terrain_shaders.h"

IMPLEMENT_UNIFORM_BUFFER_STRUCT(Terrain, "terrain")
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
    FTerrainSampleProbeComputeShader,
    "/Plugins/UDLODTerrain/probe_samples.usf",
    "probe_samples",
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
IMPLEMENT_GLOBAL_SHADER(
    FTerrainPickingComputeShader,
    "/Plugins/UDLODTerrain/picking.usf",
    "pick",
    SF_Compute);
IMPLEMENT_GLOBAL_SHADER(
    FTerrainDepthCopyPixelShader,
    "/Plugins/UDLODTerrain/depth_copy.usf",
    "depth_copy",
    SF_Pixel);
