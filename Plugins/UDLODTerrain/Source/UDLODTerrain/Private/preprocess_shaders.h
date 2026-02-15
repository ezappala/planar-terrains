#pragma once
#include "GlobalShader.h"
#include "HLSLTypeAliases.h"
#include "ShaderParameterStruct.h"

BEGIN_SHADER_PARAMETER_STRUCT(TileInfo, UDLODTERRAIN_API)
    SHADER_PARAMETER(FIntPoint, src_origin_px)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(TileNeighbors, UDLODTERRAIN_API)
    SHADER_PARAMETER(int, W)
    SHADER_PARAMETER(int, E)
    SHADER_PARAMETER(int, N)
    SHADER_PARAMETER(int, S)
    SHADER_PARAMETER(int, NW)
    SHADER_PARAMETER(int, NE)
    SHADER_PARAMETER(int, SW)
    SHADER_PARAMETER(int, SE)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(ParentInfo, UDLODTERRAIN_API)
    SHADER_PARAMETER(int, child00)
    SHADER_PARAMETER(int, child01)
    SHADER_PARAMETER(int, child10)
    SHADER_PARAMETER(int, child11)
END_SHADER_PARAMETER_STRUCT()

class UDLODTERRAIN_API FPreprocess : public FGlobalShader {
public:
    DECLARE_GLOBAL_SHADER(FPreprocess);
    SHADER_USE_PARAMETER_STRUCT(FPreprocess, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, UDLODTERRAIN_API)
        SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, heightmap)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TileInfo>, tile_infos)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TileNeighbors>, neighbors)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<ParentInfo>, parent_infos)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float>, tile_buf)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<int2>, seed_buf)
    END_SHADER_PARAMETER_STRUCT()
};