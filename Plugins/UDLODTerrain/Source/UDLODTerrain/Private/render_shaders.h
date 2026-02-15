#pragma once
#include "GlobalShader.h"
#include "HLSLTypeAliases.h"
#include "ShaderParameterStruct.h"
#include "tile.h"
#include "config.h"

class UDLODTERRAIN_API FInitCS : public FGlobalShader {
public:
    DECLARE_GLOBAL_SHADER(FInitCS);
    SHADER_USE_PARAMETER_STRUCT(FInitCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, UDLODTERRAIN_API)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FTile>, tile_list)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCount>, count_current)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCount>, count_next)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCount>, count_final)
        SHADER_PARAMETER(UE::HLSL::float2, root_origin)
        SHADER_PARAMETER(float, root_size)
    END_SHADER_PARAMETER_STRUCT()
};

class UDLODTERRAIN_API FRefineCS : public FGlobalShader {
public:
    DECLARE_GLOBAL_SHADER(FRefineCS);
    SHADER_USE_PARAMETER_STRUCT(FRefineCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, UDLODTERRAIN_API)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTile>, tile_list_in)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FTile>, tile_list_out_next)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FTile>, tile_list_out_final)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCount>, count_current)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCount>, count_next)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCount>, count_final)
        SHADER_PARAMETER(UE::HLSL::float3, view_world_pos)
        SHADER_PARAMETER(float, lod_distance_scale)
        SHADER_PARAMETER(uint32, max_depth)
        SHADER_PARAMETER(uint32, max_tiles)
        SHADER_PARAMETER(float, view_radius)
        SHADER_PARAMETER(uint32, pad)
        SHADER_PARAMETER_ARRAY(UE::HLSL::float4, frustum_planes, [6])
    END_SHADER_PARAMETER_STRUCT()
};

class UDLODTERRAIN_API FDrawArgsCS : public FGlobalShader {
public:
    DECLARE_GLOBAL_SHADER(FDrawArgsCS);
    SHADER_USE_PARAMETER_STRUCT(FDrawArgsCS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, UDLODTERRAIN_API)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FCount>, count_final)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, draw_args_uav)
        SHADER_PARAMETER(uint32, vertex_count_per_instance)
    END_SHADER_PARAMETER_STRUCT()
};

BEGIN_SHADER_PARAMETER_STRUCT(VSOut, UDLODTERRAIN_API)
    SHADER_PARAMETER(UE::HLSL::float4, pos_cs)
    SHADER_PARAMETER(UE::HLSL::float3, pos_ws)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(RenderPassParams, UDLODTERRAIN_API)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FTile>, final_tiles)
    SHADER_PARAMETER(uint32, subdivisions)
    SHADER_PARAMETER(float, height_scale)
    SHADER_PARAMETER(UE::HLSL::float2, terrain_origin_ws)
    SHADER_PARAMETER(UE::HLSL::float2, terrain_scale_ws)
    SHADER_PARAMETER_RDG_TEXTURE(Texture2D, height_tex)
    SHADER_PARAMETER_SAMPLER(SamplerState, height_samp)
    SHADER_PARAMETER_STRUCT_INCLUDE(VSOut, vs_out)

    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class UDLODTERRAIN_API FTerrainVS : public FGlobalShader {
public:
    DECLARE_GLOBAL_SHADER(FTerrainVS);
    SHADER_USE_PARAMETER_STRUCT(FTerrainVS, FGlobalShader);

    using FParameters = RenderPassParams;
};

class UDLODTERRAIN_API FTerrainPS : public FGlobalShader {
public:
    DECLARE_GLOBAL_SHADER(FTerrainPS);
    SHADER_USE_PARAMETER_STRUCT(FTerrainPS, FGlobalShader);

    using FParameters = RenderPassParams;
};

#if defined(BASIC_MODE) && BASIC_MODE
class FBasicPS : public FGlobalShader {
public:
    DECLARE_GLOBAL_SHADER(FBasicPS);
    SHADER_USE_PARAMETER_STRUCT(FBasicPS, FGlobalShader);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters, UDLODTERRAIN_API)
    END_SHADER_PARAMETER_STRUCT()
};
#endif