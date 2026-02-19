#include "shaders.h"

IMPLEMENT_MATERIAL_SHADER_TYPE(, FTerrainInitializePS, TEXT("/Plugins/UDLODTerrain/m_init.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FTerrainResamplePS, "/Plugins/UDLODTerrain/m_resample.usf", "Main", SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FTerrainDisplayPS, TEXT("/Plugins/UDLODTerrain/m_display.usf"), TEXT("Main"), SF_Pixel);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FTerrainSimulateCS, TEXT("/Plugins/UDLODTerrain/m_simulate.usf"), TEXT("Main"), SF_Compute);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FTerrainMeshVS, TEXT("/Plugins/UDLODTerrain/m_mesh.usf"), TEXT("MainVS"), SF_Vertex);
IMPLEMENT_MATERIAL_SHADER_TYPE(, FTerrainMeshPS, TEXT("/Plugins/UDLODTerrain/m_mesh.usf"), TEXT("MainPS"), SF_Pixel);
