#include "UDLODTerrainShaders.h"

IMPLEMENT_GLOBAL_SHADER(FUDLOD_RootTileCS, "/Plugins/UDLODTerrain/UDLOD_Terrain.usf", "MainCS_Root", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FUDLOD_MakeDispatchArgsCS, "/Plugins/UDLODTerrain/UDLOD_Terrain.usf", "MainCS_Dispatch", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FUDLOD_RefineCS, "/Plugins/UDLODTerrain/UDLOD_Terrain.usf", "MainCS_Refine", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FUDLOD_DrawArgsCS, "/Plugins/UDLODTerrain/UDLOD_Terrain.usf", "MainCS_DrawArgs", SF_Compute);

IMPLEMENT_GLOBAL_SHADER(FUDLOD_TerrainVS, "/Plugins/UDLODTerrain/UDLOD_Terrain.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FUDLOD_TerrainPS, "/Plugins/UDLODTerrain/UDLOD_Terrain.usf", "MainPS", SF_Pixel);