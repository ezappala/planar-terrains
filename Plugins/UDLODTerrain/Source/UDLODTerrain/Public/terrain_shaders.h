#pragma once
#include "ext_matrix2x4.h"
#include "GlobalShader.h"
#include "HLSLTypeAliases.h"
#include "ShaderParameterStruct.h"
#include "Runtime/Engine/Public/Matrix3x4.h"

// BEGIN_SHADER_PARAMETER_STRUCT(Terrain, UDLODTERRAIN_API)
BEGIN_UNIFORM_BUFFER_STRUCT(Terrain, UDLODTERRAIN_API)
    SHADER_PARAMETER(uint32, lod_count)
    SHADER_PARAMETER(FVector3f, scale)
    SHADER_PARAMETER(float, min_height)
    SHADER_PARAMETER(float, max_height)
    SHADER_PARAMETER(float, height_scale)
    SHADER_PARAMETER(FMatrix3x4, world_from_unit)
    SHADER_PARAMETER(FMatrix2x4, unit_from_world_transpose_a)
    SHADER_PARAMETER(float, unit_from_world_transpose_b)
END_UNIFORM_BUFFER_STRUCT()
// END_SHADER_PARAMETER_STRUCT()

// BEGIN_UNIFORM_BUFFER_STRUCT(AttachmentConfig, UDLODTERRAIN_API)
BEGIN_SHADER_PARAMETER_STRUCT(AttachmentConfig, UDLODTERRAIN_API)
    SHADER_PARAMETER(float, texture_size)
    SHADER_PARAMETER(float, center_size)
    SHADER_PARAMETER(float, scale)
    SHADER_PARAMETER(float, offset)
    SHADER_PARAMETER(uint32, mask)
    SHADER_PARAMETER(uint32, paddinga)
    SHADER_PARAMETER(uint32, paddingb)
    SHADER_PARAMETER(uint32, paddingc)
END_SHADER_PARAMETER_STRUCT()
// END_UNIFORM_BUFFER_STRUCT()

FORCEINLINE bool operator ==(
    const AttachmentConfig& a,
    const AttachmentConfig& b
) {
    return a.texture_size == b.texture_size
        && a.center_size == b.center_size
        && a.scale == b.scale
        && a.offset == b.offset
        && a.mask == b.mask;
}

FORCEINLINE bool operator !=(
    const AttachmentConfig& a,
    const AttachmentConfig& b
) { return !(a == b); }

FORCEINLINE uint32 GetTypeHash(const AttachmentConfig& config) {
    return HashCombine(
        HashCombine(
            HashCombine(
                HashCombine(
                    GetTypeHash(config.texture_size),
                    GetTypeHash(config.center_size)
                ),
                GetTypeHash(config.scale)
            ),
            GetTypeHash(config.offset)
        ),
        GetTypeHash(config.mask)
    );
}

// BEGIN_UNIFORM_BUFFER_STRUCT(Attachments, UDLODTERRAIN_API)
//     SHADER_PARAMETER_STRUCT_ARRAY(AttachmentConfig, attachment_configs, [2])
// END_UNIFORM_BUFFER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(Attachments, UDLODTERRAIN_API)
    SHADER_PARAMETER_STRUCT_ARRAY(AttachmentConfig, attachment_configs, [2])
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(ViewCoordinate, UDLODTERRAIN_API)
    SHADER_PARAMETER(FUint32Vector2, xy)
    SHADER_PARAMETER(FVector2f, uv)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(TerrainView, UDLODTERRAIN_API)
    SHADER_PARAMETER(uint32, tree_size)
    SHADER_PARAMETER(uint32, geometry_tile_count)
    SHADER_PARAMETER(float, grid_size)
    SHADER_PARAMETER(uint32, vertices_per_row)
    SHADER_PARAMETER(uint32, vertices_per_tile)
    SHADER_PARAMETER(float, morph_distance)
    SHADER_PARAMETER(float, blend_distance)
    SHADER_PARAMETER(float, load_distance)
    SHADER_PARAMETER(float, subdivision_distance)
    SHADER_PARAMETER(float, morph_range)
    SHADER_PARAMETER(float, blend_range)
    SHADER_PARAMETER(float, precision_distance)
    SHADER_PARAMETER(uint32, face)
    SHADER_PARAMETER(uint32, lod)
// TODO: FUCK Unreal engine. StructArrays are not yet supported as uniform buffer structs
    SHADER_PARAMETER_STRUCT_ARRAY(ViewCoordinate, coordinates, [6])
    SHADER_PARAMETER(float, height_scale)
    SHADER_PARAMETER(FVector3f, world_position)
    SHADER_PARAMETER_ARRAY(FVector4f, half_spaces, [6])
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(ApproximateHeight, UDLODTERRAIN_API)
    SHADER_PARAMETER(float, value)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(TileTreeEntry, UDLODTERRAIN_API)
    SHADER_PARAMETER(uint32, atlas_index)
    SHADER_PARAMETER(uint32, atlas_lod)
END_SHADER_PARAMETER_STRUCT()

FORCEINLINE bool operator ==(
    const TileTreeEntry& a,
    const TileTreeEntry& b
) {
    return a.atlas_index == b.atlas_index
        && a.atlas_lod == b.atlas_lod;
}

FORCEINLINE bool operator !=(
    const TileTreeEntry& a,
    const TileTreeEntry& b
) { return !(a == b); }

FORCEINLINE uint32 GetTypeHash(const TileTreeEntry& entry) {
    return HashCombine(
        GetTypeHash(entry.atlas_index),
        GetTypeHash(entry.atlas_lod)
    );
}

// BEGIN_UNIFORM_BUFFER_STRUCT(TileTree, UDLODTERRAIN_API)
//     RDG_BUFFER_ACCESS(entries, ERHIAccess::CopyDest)
// END_UNIFORM_BUFFER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(GeometryTile, UDLODTERRAIN_API)
    SHADER_PARAMETER(uint32, face)
    SHADER_PARAMETER(uint32, lod)
    SHADER_PARAMETER(FUintVector2, xy)
    SHADER_PARAMETER(FVector4f, view_distances)
    SHADER_PARAMETER(FVector4f, morph_radios)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(DrawElementsIndirectParameters, UDLODTERRAIN_API)
    // General bindings: bindings.ush
    SHADER_PARAMETER_STRUCT_REF(Terrain, terrain)
    SHADER_PARAMETER_STRUCT(Attachments, attachments)
    SHADER_PARAMETER_SAMPLER(SamplerState, terrain_sampler)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, height_attachment)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, albedo_attachment)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TerrainView>, terrain_view)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<float>, approximate_height)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TileTreeEntry>, tile_tree)

    // TODO: should this be a buffer access.
    // RDG_BUFFER_ACCESS(geometry_tiles, ERHIAccess::ReadOnlyMask) ?? or ERHIAccess::CopyDest
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<GeometryTile>, geometry_tiles)

    // Vertex Input: vertex.usf
    // No additional parameters for vertex shader, as all necessary data is included in the
    // general bindings above.
    //
    // The vertex shader also takes a SV_VertexID system value semantic as input.
    // This should be provided automatically by the DrawIndirect call.

    // Fragment (pixel) shader input: fragment.usf
    // The Fragment shader also takes an SV_Position system value, which should also be provided
    // automatically by the rasterizer.
    SHADER_PARAMETER(FVector3f, tile_uv)
    SHADER_PARAMETER(uint32, tile_index)
    SHADER_PARAMETER(float, view_distance)
    SHADER_PARAMETER(float, height)

    // Indirect args buffer access:
    RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class UDLODTERRAIN_API FTerrainVertexShader : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FTerrainVertexShader);
    SHADER_USE_PARAMETER_STRUCT(FTerrainVertexShader, FGlobalShader);

    // BEGIN_SHADER_PARAMETER_STRUCT(VertexInput, UDLODTERRAIN_API)
    //     SHADER_PARAMETER_STRUCT(Terrain, terrain)
    //     SHADER_PARAMETER_STRUCT(AttachmentConfig, height_config)
    //     SHADER_PARAMETER_STRUCT(AttachmentConfig, albedo_config)
    //     SHADER_PARAMETER_SAMPLER(SamplerState, terrain_sampler)
    //     SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, height_attachment)
    //     SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, albedo_attachment)
    //     SHADER_PARAMETER_STRUCT(TerrainView, terrain_view)
    //     SHADER_PARAMETER(float, approximate_height)
    //     SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TileTreeEntry>, tile_tree)
    //     SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<GeometryTile>, geometry_tiles)
    // END_SHADER_PARAMETER_STRUCT()

    using FParameters = DrawElementsIndirectParameters;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& params) {
        const bool feature_level = IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
        return feature_level;
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& params,
        FShaderCompilerEnvironment& out_environment
    ) {
        FGlobalShader::ModifyCompilationEnvironment(params, out_environment);
        out_environment.SetDefine(TEXT("FRAGMENT"), 0);
        out_environment.SetDefine(TEXT("VERTEX"), 1);
        out_environment.SetDefine(TEXT("SAMPLE_GRAD"), 1);
        out_environment.SetDefine(TEXT("TILE_TREE_LOD"), 1);
        out_environment.SetDefine(TEXT("BLEND"), 1);
        out_environment.SetDefine(TEXT("MORPH"), 1);
        out_environment.SetDefine(TEXT("LIGHTING"), 1);
        out_environment.SetDefine(TEXT("PBR_LIGHTING"), 0);
        out_environment.SetDefine(TEXT("SHOW_DATA_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_GEOMETRY_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_TILE_TREE"), 0);
        out_environment.SetDefine(TEXT("SHOW_PIXELS"), 0);
        out_environment.SetDefine(TEXT("SHOW_UV"), 0);
        out_environment.SetDefine(TEXT("SHOW_NORMALS"), 0);
    }
};

class UDLODTERRAIN_API FTerrainFragmentShader : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FTerrainFragmentShader)
    SHADER_USE_PARAMETER_STRUCT(FTerrainFragmentShader, FGlobalShader);

    // BEGIN_SHADER_PARAMETER_STRUCT(FragmentInput, UDLODTERRAIN_API)
    //     SHADER_PARAMETER(FVector3f, tile_uv)
    //     SHADER_PARAMETER(uint32, tile_index)
    //     SHADER_PARAMETER(float, view_distance)
    //     SHADER_PARAMETER(float, height)
    // END_SHADER_PARAMETER_STRUCT()

    // using FParameters = FragmentInput;
    using FParameters = DrawElementsIndirectParameters;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& params) {
        const bool feature_level = IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
        return feature_level;
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& params,
        FShaderCompilerEnvironment& out_environment
    ) {
        FGlobalShader::ModifyCompilationEnvironment(params, out_environment);
        out_environment.SetDefine(TEXT("FRAGMENT"), 1);
        out_environment.SetDefine(TEXT("VERTEX"), 0);
        out_environment.SetDefine(TEXT("PREPASS"), 0);
        out_environment.SetDefine(TEXT("SAMPLE_GRAD"), 1);
        out_environment.SetDefine(TEXT("TILE_TREE_LOD"), 1);
        out_environment.SetDefine(TEXT("BLEND"), 1);
        out_environment.SetDefine(TEXT("MORPH"), 1);
        out_environment.SetDefine(TEXT("LIGHTING"), 1);
        out_environment.SetDefine(TEXT("PBR_LIGHTING"), 0);
        out_environment.SetDefine(TEXT("SHOW_DATA_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_GEOMETRY_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_TILE_TREE"), 0);
        out_environment.SetDefine(TEXT("SHOW_PIXELS"), 0);
        out_environment.SetDefine(TEXT("SHOW_UV"), 0);
        out_environment.SetDefine(TEXT("SHOW_NORMALS"), 0);
    }
};

BEGIN_SHADER_PARAMETER_STRUCT(PrepassState, UDLODTERRAIN_API)
    SHADER_PARAMETER(uint32, tile_count)
    SHADER_PARAMETER(int, counter)
    SHADER_PARAMETER(int, child_index)
    SHADER_PARAMETER(int, final_index)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(TileCoordinate, UDLODTERRAIN_API)
    SHADER_PARAMETER(uint32, face)
    SHADER_PARAMETER(uint32, lod)
    SHADER_PARAMETER(FUintVector2, xy)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(Prepass, UDLODTERRAIN_API)
    SHADER_PARAMETER_STRUCT_REF(Terrain, terrain)
    SHADER_PARAMETER_STRUCT(Attachments, attachments)
    SHADER_PARAMETER_SAMPLER(SamplerState, terrain_sampler)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, height_attachment)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, albedo_attachment)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TerrainView>, terrain_view)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, approximate_height)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TileTreeEntry>, tile_tree)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<PrepassState>, state)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, indirect_buffer)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<TileCoordinate>, temporary_tiles)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(RefineTiles, UDLODTERRAIN_API)
    SHADER_PARAMETER_STRUCT_REF(Terrain, terrain)
    SHADER_PARAMETER_STRUCT(Attachments, attachments)
    SHADER_PARAMETER_SAMPLER(SamplerState, terrain_sampler)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, height_attachment)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<float>, albedo_attachment)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TerrainView>, terrain_view)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, approximate_height)
    SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<TileTreeEntry>, tile_tree)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<PrepassState>, state)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<TileCoordinate>, temporary_tiles)
    SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<GeometryTile>, final_tiles)
    RDG_BUFFER_ACCESS(IndirectArgs, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

// BEGIN_SHADER_PARAMETER_STRUCT(PickingData, UDLODTERRAIN_API)
//     SHADER_PARAMETER(FVector2f, cursor_cords)
//     SHADER_PARAMETER(float, depth)
//     SHADER_PARAMETER(uint32, stencil)
//     SHADER_PARAMETER(FMatrix44f, world_from_clip)
// END_SHADER_PARAMETER_STRUCT()

// BEGIN_SHADER_PARAMETER_STRUCT(Picking, UDLODTERRAIN_API)
//     SHADER_PARAMETER_RDG_BUFFER_UAV(PickingData, picking_data)
//     SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DMS<float>, depth_texture)
//     SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DMS<uint32>, stencil_texture)
// END_SHADER_PARAMETER_STRUCT()

class UDLODTERRAIN_API FTerrainPrepassPrepareRootComputeShader : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FTerrainPrepassPrepareRootComputeShader);
    SHADER_USE_PARAMETER_STRUCT(FTerrainPrepassPrepareRootComputeShader, FGlobalShader);
    using FParameters = Prepass;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& params) {
        const bool feature_level = IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
        return feature_level;
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& params,
        FShaderCompilerEnvironment& out_environment
    ) {
        FGlobalShader::ModifyCompilationEnvironment(params, out_environment);
        out_environment.SetDefine(TEXT("FRAGMENT"), 0);
        out_environment.SetDefine(TEXT("VERTEX"), 0);
        out_environment.SetDefine(TEXT("PREPASS"), 1);
        out_environment.SetDefine(TEXT("SAMPLE_GRAD"), 1);
        out_environment.SetDefine(TEXT("TILE_TREE_LOD"), 1);
        out_environment.SetDefine(TEXT("BLEND"), 1);
        out_environment.SetDefine(TEXT("MORPH"), 1);
        out_environment.SetDefine(TEXT("LIGHTING"), 1);
        out_environment.SetDefine(TEXT("PBR_LIGHTING"), 0);
        out_environment.SetDefine(TEXT("SHOW_DATA_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_GEOMETRY_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_TILE_TREE"), 0);
        out_environment.SetDefine(TEXT("SHOW_PIXELS"), 0);
        out_environment.SetDefine(TEXT("SHOW_UV"), 0);
        out_environment.SetDefine(TEXT("SHOW_NORMALS"), 0);
    }
};

class UDLODTERRAIN_API FTerrainPrepassPrepareNextComputeShader : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FTerrainPrepassPrepareNextComputeShader);
    SHADER_USE_PARAMETER_STRUCT(FTerrainPrepassPrepareNextComputeShader, FGlobalShader);
    using FParameters = Prepass;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& params) {
        const bool feature_level = IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
        return feature_level;
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& params,
        FShaderCompilerEnvironment& out_environment
    ) {
        FGlobalShader::ModifyCompilationEnvironment(params, out_environment);
        out_environment.SetDefine(TEXT("FRAGMENT"), 0);
        out_environment.SetDefine(TEXT("VERTEX"), 0);
        out_environment.SetDefine(TEXT("PREPASS"), 1);
        out_environment.SetDefine(TEXT("SAMPLE_GRAD"), 1);
        out_environment.SetDefine(TEXT("TILE_TREE_LOD"), 1);
        out_environment.SetDefine(TEXT("BLEND"), 1);
        out_environment.SetDefine(TEXT("MORPH"), 1);
        out_environment.SetDefine(TEXT("LIGHTING"), 1);
        out_environment.SetDefine(TEXT("PBR_LIGHTING"), 0);
        out_environment.SetDefine(TEXT("SHOW_DATA_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_GEOMETRY_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_TILE_TREE"), 0);
        out_environment.SetDefine(TEXT("SHOW_PIXELS"), 0);
        out_environment.SetDefine(TEXT("SHOW_UV"), 0);
        out_environment.SetDefine(TEXT("SHOW_NORMALS"), 0);
    }
};

class UDLODTERRAIN_API FTerrainPrepassPrepareRenderComputeShader : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FTerrainPrepassPrepareRenderComputeShader);
    SHADER_USE_PARAMETER_STRUCT(FTerrainPrepassPrepareRenderComputeShader, FGlobalShader);
    using FParameters = Prepass;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& params) {
        const bool feature_level = IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
        return feature_level;
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& params,
        FShaderCompilerEnvironment& out_environment
    ) {
        FGlobalShader::ModifyCompilationEnvironment(params, out_environment);
        out_environment.SetDefine(TEXT("FRAGMENT"), 0);
        out_environment.SetDefine(TEXT("VERTEX"), 0);
        out_environment.SetDefine(TEXT("PREPASS"), 1);
        out_environment.SetDefine(TEXT("SAMPLE_GRAD"), 1);
        out_environment.SetDefine(TEXT("TILE_TREE_LOD"), 1);
        out_environment.SetDefine(TEXT("BLEND"), 1);
        out_environment.SetDefine(TEXT("MORPH"), 1);
        out_environment.SetDefine(TEXT("LIGHTING"), 1);
        out_environment.SetDefine(TEXT("PBR_LIGHTING"), 0);
        out_environment.SetDefine(TEXT("SHOW_DATA_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_GEOMETRY_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_TILE_TREE"), 0);
        out_environment.SetDefine(TEXT("SHOW_PIXELS"), 0);
        out_environment.SetDefine(TEXT("SHOW_UV"), 0);
        out_environment.SetDefine(TEXT("SHOW_NORMALS"), 0);
    }
};

class UDLODTERRAIN_API FTerrainPrepassRefineTilesComputeShader : public FGlobalShader {
    DECLARE_GLOBAL_SHADER(FTerrainPrepassRefineTilesComputeShader);
    SHADER_USE_PARAMETER_STRUCT(FTerrainPrepassRefineTilesComputeShader, FGlobalShader);
    using FParameters = RefineTiles;

    static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& params) {
        const bool feature_level = IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
        return feature_level;
    }

    static void ModifyCompilationEnvironment(
        const FGlobalShaderPermutationParameters& params,
        FShaderCompilerEnvironment& out_environment
    ) {
        FGlobalShader::ModifyCompilationEnvironment(params, out_environment);
        out_environment.SetDefine(TEXT("FRAGMENT"), 0);
        out_environment.SetDefine(TEXT("VERTEX"), 0);
        out_environment.SetDefine(TEXT("PREPASS"), 1);
        out_environment.SetDefine(TEXT("SAMPLE_GRAD"), 1);
        out_environment.SetDefine(TEXT("TILE_TREE_LOD"), 1);
        out_environment.SetDefine(TEXT("BLEND"), 1);
        out_environment.SetDefine(TEXT("MORPH"), 1);
        out_environment.SetDefine(TEXT("LIGHTING"), 1);
        out_environment.SetDefine(TEXT("PBR_LIGHTING"), 0);
        out_environment.SetDefine(TEXT("SHOW_DATA_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_GEOMETRY_LOD"), 0);
        out_environment.SetDefine(TEXT("SHOW_TILE_TREE"), 0);
        out_environment.SetDefine(TEXT("SHOW_PIXELS"), 0);
        out_environment.SetDefine(TEXT("SHOW_UV"), 0);
        out_environment.SetDefine(TEXT("SHOW_NORMALS"), 0);
    }
};

// class UDLODTERRAIN_API FTerrainPickingComputeShader : public FGlobalShader {
//     DECLARE_GLOBAL_SHADER(FTerrainPickingComputeShader);
//     SHADER_USE_PARAMETER_STRUCT(FTerrainPickingComputeShader, FGlobalShader);
//     using FParameters = Picking;
//
//     static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& params) {
//         const bool feature_level = IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
//         return feature_level;
//     }
//
//     static void ModifyCompilationEnvironment(
//         const FGlobalShaderPermutationParameters& params,
//         FShaderCompilerEnvironment& out_environment
//     ) {
//         FGlobalShader::ModifyCompilationEnvironment(params, out_environment);
//         out_environment.SetDefine(TEXT("FRAGMENT"), 0);
//         out_environment.SetDefine(TEXT("VERTEX"), 0);
//         out_environment.SetDefine(TEXT("PREPASS"), 1);
//         out_environment.SetDefine(TEXT("SAMPLE_GRAD"), 0);
//         out_environment.SetDefine(TEXT("TILE_TREE_LOD"), 0);
//         out_environment.SetDefine(TEXT("BLEND"), 0);
//         out_environment.SetDefine(TEXT("MORPH"), 0);
//         out_environment.SetDefine(TEXT("LIGHTING"), 0);
//         out_environment.SetDefine(TEXT("PBR_LIGHTING"), 0);
//         out_environment.SetDefine(TEXT("SHOW_DATA_LOD"), 0);
//         out_environment.SetDefine(TEXT("SHOW_GEOMETRY_LOD"), 0);
//         out_environment.SetDefine(TEXT("SHOW_TILE_TREE"), 0);
//         out_environment.SetDefine(TEXT("SHOW_PIXELS"), 0);
//         out_environment.SetDefine(TEXT("SHOW_UV"), 0);
//         out_environment.SetDefine(TEXT("SHOW_NORMALS"), 0);
//     }
// };
