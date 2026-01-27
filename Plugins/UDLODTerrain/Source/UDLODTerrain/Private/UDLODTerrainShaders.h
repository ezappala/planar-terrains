#pragma once
#include "GlobalShader.h"
#include "SceneView.h"
#include "ShaderParameterStruct.h"

class FUDLOD_RootTileCS : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FUDLOD_RootTileCS);
    SHADER_USE_PARAMETER_STRUCT(FUDLOD_RootTileCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUDLODTileGpu>, working_tiles_uav)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, working_count_uav)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, final_count_uav)
    END_SHADER_PARAMETER_STRUCT()
};

class FUDLOD_MakeDispatchArgsCS : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FUDLOD_MakeDispatchArgsCS);
    SHADER_USE_PARAMETER_STRUCT(FUDLOD_MakeDispatchArgsCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, in_count_srv)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, out_args_uav)
        SHADER_PARAMETER(uint32, group_size)
    END_SHADER_PARAMETER_STRUCT()
};

class FUDLOD_RefineCS : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FUDLOD_RefineCS);
    SHADER_USE_PARAMETER_STRUCT(FUDLOD_RefineCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FUDLODTileGpu>, in_tiles_srv)
        SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, in_count_srv)

        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUDLODTileGpu>, out_tiles_uav)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, out_count_uav)

        SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<FUDLODTileGpu>, final_tiles_uav)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, final_count_uav)

        SHADER_PARAMETER_TEXTURE(Texture2D, height_texture)
        SHADER_PARAMETER_SAMPLER(SamplerState, height_sampler)

        SHADER_PARAMETER(FVector2f, world_min_xy_ws)
        SHADER_PARAMETER(FVector2f, world_size_xy_ws)
        SHADER_PARAMETER(FVector2f, height_minmax_ws)

        SHADER_PARAMETER(uint32, grid_resolution)
        SHADER_PARAMETER(uint32, max_lod)
        SHADER_PARAMETER(float, error_pixels)
        SHADER_PARAMETER(float, height_error_0ws)
        SHADER_PARAMETER(float, focal_len_px)
        SHADER_PARAMETER(float, morph_start_ratio)

        SHADER_PARAMETER(FVector3f, pre_view_translation)
        SHADER_PARAMETER_ARRAY(FVector4f, frustum_planes_tws, [6])
    END_SHADER_PARAMETER_STRUCT()
};

class FUDLOD_DrawArgsCS : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FUDLOD_DrawArgsCS);
    SHADER_USE_PARAMETER_STRUCT(FUDLOD_DrawArgsCS, FGlobalShader);

public:
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, final_count_srv)
        SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, draw_args_uav)
        SHADER_PARAMETER(uint32, index_count_per_instance)
    END_SHADER_PARAMETER_STRUCT()
};

BEGIN_SHADER_PARAMETER_STRUCT(FUDLOD_TerrainPassParameters,)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, view)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FUDLODTileGpu>, final_tiles)
    SHADER_PARAMETER_TEXTURE(Texture2D, height_texture)
    SHADER_PARAMETER_SAMPLER(SamplerState, height_sampler)

    SHADER_PARAMETER(FVector2f, world_min_xy_ws)
    SHADER_PARAMETER(FVector2f, world_size_xy_ws)
    SHADER_PARAMETER(FVector2f, height_minmax_ws)
    SHADER_PARAMETER(uint32, grid_resolution)

    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FUDLOD_TerrainVS : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FUDLOD_TerrainVS);
    SHADER_USE_PARAMETER_STRUCT(FUDLOD_TerrainVS, FGlobalShader);
    using FParameters = FUDLOD_TerrainPassParameters;
};

class FUDLOD_TerrainPS : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FUDLOD_TerrainPS);
    SHADER_USE_PARAMETER_STRUCT(FUDLOD_TerrainPS, FGlobalShader);
    using FParameters = FUDLOD_TerrainPassParameters;
};