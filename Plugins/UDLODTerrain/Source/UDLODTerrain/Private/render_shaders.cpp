#include "render_shaders.h"

IMPLEMENT_GLOBAL_SHADER(FInitCS, "/Plugins/UDLODTerrain/r_init.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FRefineCS, "/Plugins/UDLODTerrain/r_refine.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FDrawArgsCS, "/Plugins/UDLODTerrain/r_draw_args.usf", "MainCS", SF_Compute);
IMPLEMENT_GLOBAL_SHADER(FTerrainVS, "/Plugins/UDLODTerrain/r_draw.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FTerrainPS, "/Plugins/UDLODTerrain/r_draw.usf", "MainPS", SF_Pixel);

#if defined(BASIC_MODE) && BASIC_MODE
IMPLEMENT_GLOBAL_SHADER(FBasicPS, "/Plugins/UDLODTerrain/r_basic.usf", "MainPS", SF_Pixel);
#endif