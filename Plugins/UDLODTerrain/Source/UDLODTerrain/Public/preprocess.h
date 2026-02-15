///
/// @brief Defines functions to add heightmap preprocessing passes to the render graph for UDLOD terrain.
/// @par [Implementation Steps] {
///     These functions reimplement the behavior of gdal2tiles in compute shaders to preprocess heightmaps for
///     use in the UDLODTerrain renderer.
///
///     The order of operations is as follows:
///         1) Split the input heightmap at LOD_MAX into tiles:
///             a) Compute the pixel size for LOD_MAX
///             b) Snap the dataset extent of the heightmap to the grid defined by the pixel size. (crucial for
///                 determinism)
///             c) For each tile index (tx, ty):
///                 i) Allocate a t-by-t tile
///                 ii) fill only the core region [BORDER_SIZE..BORDER_SIZE + EFFECTIVE_TILE_SIZE) x
///                     [BORDER_SIZE..BORDER_SIZE + EFFECTIVE_TILE_SIZE) by sampling and copying the corresponding
///                     window from the heightmap.
///                 iii) Leave borders uninitialized.
///                 iv) track per-pixel validity as the pixels are written
///         2) Stitch tiles at LOD_MAX. For each tile, where available, fill its borders by copying pixels from
///             neighboring tiles' core.
///         3) Mask and fill the heightmap tiles at LOD_MAX:
///             a) Build a bitmask. (1 = valid data, 0 = no data)
///             b) Flood fill via jump flooding to propagate valid data into no-data regions.
///             c) Store the mask into the height channel's least significant bits.
///         For each LOD for i = LOD_MAX - 1 down to 0:
///         4) Downsample (children -> parent)
///             a) For each parent tile at LOD i:
///                 i) Gather the 2x2 child tiles at LOD i+1 that cover it
///                 ii) Downsample their cores into the parent core using a bilinear filter.
///                 iii) Leave borders uninitialized, they should *always* come from stitching.
///         5) Stitch tiles at LOD i. For each tile, where available, fill bordres from neighbouring parents' cores.
///         6) Mask and fill the heightmap at LOD i:
///             a) Recompute mask for the parent
///                 i) this may be derived from the children's masks & some heuristic, or recomputed from validity
///                     after downsampling.
///             b) Flood fill via jump flooding again so the parent level is also safe under filtering and sampling.
/// }
///
/// @par [Notes] {
///     - Stitching should happen before mask & fill at each LOD, because borders are part of what gets sampled. If
///       filling happens befor stitching, it's possible to reintroduce no-data pixels at the borders.
///     - We don't downsample using already-stitched borders. Downsample cores, then stitch borders at that LOD.
/// }
#pragma once

#include "preprocess_shaders.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Shader.h"
#include "ext/math.h"

/// @brief Maximum level of detail (LOD) for the terrain.
/// @remark This define must match the LOD_MAX define in @file[common.ush] shaders.
constexpr uint8 LOD_MAX = 8;
/// @brief Size of a single tile including border pixels.
/// @details Each tile is TILE_SIZE x TILE_SIZE pixels, with a BORDER_SIZE pixel border on each side.
/// @remark This define must match the TILE_SIZE define in @file[common.ush] shaders.
constexpr uint16 TILE_SIZE = 256;
/// @brief Size of the border around each tile.
/// @details Each tile has a BORDER_SIZE pixel border on each side.
/// @remark This define must match the BORDER_SIZE define in @file[common.ush] shaders.
constexpr uint8 BORDER_SIZE = 4;
/// @brief Size of the effective area of a tile, excluding border pixels.
/// @details Each tile has an effective area of EFFECTIVE_TILE_SIZE x EFFECTIVE_TILE_SIZE pixels minus the border pixels.
constexpr uint32 EFFECTIVE_TILE_SIZE = TILE_SIZE - 2 * BORDER_SIZE;

inline FIntPoint tile_size_px() {
    return FIntPoint(TILE_SIZE, TILE_SIZE);
}

constexpr uint32 total_subdivisions_of_tiles_over_lod() {
    constexpr uint8 base = 4;
    return base * (1 - ext::pow<uint32>(base, LOD_MAX)) / (1 - base) + 1;
}

static void add_prepass(
    FRDGBuilder& graph_builder,
    const TShaderMapRef<FPreprocess> shader,
    const FRDGTextureRef heightmap
) {
    if (heightmap->Desc.Format == PF_R16F) {
        checkf(false, TEXT("TODO: support PF_R16F heightmaps in preprocess by converting to PF_R32_FLOAT"));
    }

    checkf(heightmap->Desc.Format == PF_R32_FLOAT, TEXT("Heightmap texture must be in PF_R32_FLOAT format"));
    auto* pass_params = graph_builder.AllocParameters<FPreprocess::FParameters>();
    pass_params->heightmap = heightmap;

    const FRDGBufferRef tile_infos_buf = graph_builder.CreateBuffer(
        FRDGBufferDesc::CreateStructuredDesc<TileInfo>(
            total_subdivisions_of_tiles_over_lod()
        ), TEXT("PreprocessTileInfosBuffer")
    );
    pass_params->tile_infos = graph_builder.CreateSRV(tile_infos_buf);

    const FRDGBufferRef neighbors_buf = graph_builder.CreateBuffer(
        FRDGBufferDesc::CreateStructuredDesc<TileNeighbors>(
            total_subdivisions_of_tiles_over_lod()
        ),
        TEXT("PreprocessTileNeighborsBuffer")
    );
    pass_params->neighbors = graph_builder.CreateSRV(neighbors_buf);

    const FRDGBufferRef parent_infos_buf = graph_builder.CreateBuffer(
        FRDGBufferDesc::CreateStructuredDesc<ParentInfo>(
            total_subdivisions_of_tiles_over_lod()
        ),
        TEXT("PreprocessParentInfosBuffer")
    );
    pass_params->parent_infos = graph_builder.CreateUAV(parent_infos_buf);

    pass_params->tile_buf = graph_builder.CreateUAV(
        graph_builder.CreateTexture(
            FRDGTextureDesc::Create2DArray(tile_size_px(), PF_R32_FLOAT, FClearValueBinding::Transparent,
                TexCreate_UAV | TexCreate_ShaderResource,
                total_subdivisions_of_tiles_over_lod()),
            TEXT("PreprocessTileBuffer")
        )
    );

    pass_params->seed_buf = graph_builder.CreateUAV(
        graph_builder.CreateTexture(
            FRDGTextureDesc::Create2DArray(tile_size_px(), PF_R8_SINT, FClearValueBinding::Transparent,
                TexCreate_UAV | TexCreate_ShaderResource,
                total_subdivisions_of_tiles_over_lod()),
            TEXT("PreprocessSeedBuffer")
        )
    );

    FComputeShaderUtils::AddPass(
        graph_builder, RDG_EVENT_NAME("UDLOD.Preprocess"),
        ERDGPassFlags::Compute,
        shader,
        pass_params,
        FIntVector(8, 8, 1)
    );
}

// static void add_seed_mask_and_fill_pass(
//     FRDGBuilder& graph_builder,
//     const TShaderMapRef<FSeedMaskAndFillCS> shader,
//     const FRDGTextureRef heightmap
// ) {
//     auto* pass_params = graph_builder.AllocParameters<FSeedMaskAndFillCS::FParameters>();
//     pass_params->in_height_mf = heightmap;
//     pass_params->seed_out_mf = graph_builder.CreateUAV(
//         graph_builder.CreateTexture(
//             FRDGTextureDesc::Create2DArray(
//                 heightmap->Desc.Extent,
//                 PF_R32_FLOAT,
//                 heightmap->Desc.ClearValue,
//                 TexCreate_UAV | TexCreate_ShaderResource,
//                 heightmap->Desc.ArraySize
//             ),
//             TEXT("SeedMaskTexture")
//         )
//     );
//
//     FComputeShaderUtils::AddPass(graph_builder, RDG_EVENT_NAME("UDLOD.SeedMaskAndFill"), ERDGPassFlags::Compute,
//         shader, pass_params, FIntVector(8, 8, 1));
// }
//
// struct ParentInfo {
//     int child00;
//     int child01;
//     int child10;
//     int child11;
// };
//
// static void add_downsample_pass(
//     FRDGBuilder& graph_builder,
//     const TShaderMapRef<FDownsampleCS> shader,
//     const FRDGTextureRef heightmap
// ) {
//     auto* pass_params = graph_builder.AllocParameters<FDownsampleCS::FParameters>();
//
//     const FRDGBufferRef parents_buf = graph_builder.CreateBuffer(
//         FRDGBufferDesc::CreateStructuredDesc<ParentInfo>(
//             heightmap->Desc.ArraySize / 4
//         ),
//         TEXT("DownsampleParentsBuffer")
//     );
//
//     pass_params->parents = graph_builder.CreateSRV(parents_buf);
//     pass_params->child_tiles = graph_builder.CreateTexture(
//         FRDGTextureDesc::Create2DArray(
//             heightmap->Desc.Extent,
//             PF_R32_FLOAT,
//             heightmap->Desc.ClearValue,
//             TexCreate_ShaderResource,
//             heightmap->Desc.ArraySize
//         ),
//         TEXT("ChildTilesTexture")
//     );
//
//     pass_params->parent_tiles = graph_builder.CreateUAV(
//         graph_builder.CreateTexture(
//             FRDGTextureDesc::Create2DArray(
//                 heightmap->Desc.Extent,
//                 PF_R32_FLOAT,
//                 FClearValueBinding::Transparent,
//                 TexCreate_UAV | TexCreate_ShaderResource,
//                 heightmap->Desc.ArraySize / 4
//             ),
//             TEXT("ParentTilesTexture")
//         )
//     );
//
//     FComputeShaderUtils::AddPass(graph_builder, RDG_EVENT_NAME("UDLOD.Downsample"), ERDGPassFlags::Compute,
//         shader, pass_params, FIntVector(8, 8, 1));
// }
//
// struct TileInfo {
//     FIntVector2 src_origin_px;
// };
//
// static void add_split_pass(
//     FRDGBuilder& graph_builder,
//     const TShaderMapRef<FSplitCS> shader,
//     const FRDGTextureRef heightmap
// ) {
//     auto* pass_params = graph_builder.AllocParameters<FSplitCS::FParameters>();
//
//     pass_params->src_height = graph_builder.CreateTexture(
//         FRDGTextureDesc::Create2D(
//             heightmap->Desc.Extent,
//             PF_R32_FLOAT,
//             heightmap->Desc.ClearValue,
//             TexCreate_ShaderResource
//         ),
//         TEXT("SplitSrcHeightTexture")
//     );
//
//     const FRDGBufferRef tile_infos_buf = graph_builder.CreateBuffer(
//         FRDGBufferDesc::CreateStructuredDesc<TileInfo>(
//             heightmap->Desc.ArraySize * 4
//         ),
//         TEXT("SplitTileInfosBuffer")
//     );
//
//     pass_params->tile_infos = graph_builder.CreateSRV(tile_infos_buf);
//     pass_params->out_tile_height_split = graph_builder.CreateUAV(
//         graph_builder.CreateTexture(
//             FRDGTextureDesc::Create2DArray(
//                 heightmap->Desc.Extent / 2,
//                 PF_R32_FLOAT,
//                 heightmap->Desc.ClearValue,
//                 TexCreate_UAV | TexCreate_ShaderResource,
//                 heightmap->Desc.ArraySize * 4
//             ),
//             TEXT("SplitOutTileHeightTexture")
//         )
//     );
//
//     FComputeShaderUtils::AddPass(graph_builder, RDG_EVENT_NAME("UDLOD.Split"), ERDGPassFlags::Compute,
//         shader, pass_params, FIntVector(8, 8, 1));
// }
//
// struct TileNeighbors {
//     int W, E, N, S;
//     int NW, NE, SW, SE;
// };
//
// static void add_stitch_pass(
//     FRDGBuilder& graph_builder,
//     const TShaderMapRef<FStitchCS> shader,
//     const FRDGTextureRef heightmap
// ) {
//     auto* pass_params = graph_builder.AllocParameters<FStitchCS::FParameters>();
//
//     const FRDGBufferRef neighbors_buf = graph_builder.CreateBuffer(
//         FRDGBufferDesc::CreateStructuredDesc<TileNeighbors>(
//             heightmap->Desc.ArraySize / 4
//         ),
//         TEXT("StitchTileNeighborsBuffer")
//     );
//     pass_params->neighbor_st = graph_builder.CreateSRV(neighbors_buf);
//     pass_params->in_tile_height_st = graph_builder.CreateTexture(
//         FRDGTextureDesc::Create2DArray(
//             heightmap->Desc.Extent / 2,
//             PF_R32_FLOAT,
//             heightmap->Desc.ClearValue,
//             TexCreate_ShaderResource,
//             heightmap->Desc.ArraySize / 4
//         ),
//         TEXT("StitchInTileHeightTexture")
//     );
//
//     pass_params->out_tile_height_st = graph_builder.CreateUAV(
//         graph_builder.CreateTexture(
//             FRDGTextureDesc::Create2DArray(
//                 heightmap->Desc.Extent,
//                 PF_R32_FLOAT,
//                 heightmap->Desc.ClearValue,
//                 TexCreate_UAV | TexCreate_ShaderResource,
//                 heightmap->Desc.ArraySize
//             ),
//             TEXT("StitchOutTileHeightTexture")
//         )
//     );
//
//     FComputeShaderUtils::AddPass(graph_builder, RDG_EVENT_NAME("UDLOD.Stitch"), ERDGPassFlags::Compute,
//         shader, pass_params, FIntVector(8, 8, 1));
// }
//
// static void add_fill_pack_pass(
//     FRDGBuilder& graph_builder,
//     const TShaderMapRef<FFillPackCS> shader,
//     const FRDGTextureRef heightmap
// ) {
//     auto* pass_params = graph_builder.AllocParameters<FFillPackCS::FParameters>();
//     pass_params->in_height_fp = heightmap;
//     pass_params->in_seed_fp = graph_builder.CreateTexture(
//         FRDGTextureDesc::Create2DArray(
//             heightmap->Desc.Extent,
//             PF_R32_FLOAT,
//             heightmap->Desc.ClearValue,
//             TexCreate_ShaderResource,
//             heightmap->Desc.ArraySize
//         ),
//         TEXT("FillPackInSeedTexture")
//     );
//
//     pass_params->out_height_fp = graph_builder.CreateUAV(
//         graph_builder.CreateTexture(
//             FRDGTextureDesc::Create2DArray(
//                 heightmap->Desc.Extent,
//                 PF_R32_FLOAT,
//                 heightmap->Desc.ClearValue,
//                 TexCreate_UAV | TexCreate_ShaderResource,
//                 heightmap->Desc.ArraySize
//             ),
//             TEXT("FillPackOutHeightTexture")
//         )
//     );
//
//     FComputeShaderUtils::AddPass(graph_builder, RDG_EVENT_NAME("UDLOD.FillPack"), ERDGPassFlags::Compute,
//         shader, pass_params, FIntVector(8, 8, 1));
// }
//
// static void add_jump_fill_pass(
//     FRDGBuilder& graph_builder,
//     const TShaderMapRef<FJumpFillCS> shader,
//     const FRDGTextureRef heightmap
// ) {
//     auto* pass_params = graph_builder.AllocParameters<FJumpFillCS::FParameters>();
//     pass_params->seed_in_jf = graph_builder.CreateTexture(
//         FRDGTextureDesc::Create2DArray(
//             heightmap->Desc.Extent,
//             PF_R32_FLOAT,
//             heightmap->Desc.ClearValue,
//             TexCreate_ShaderResource,
//             heightmap->Desc.ArraySize
//         ),
//         TEXT("JumpFillInSeedTexture")
//     );
//
//     pass_params->seed_out_jf = graph_builder.CreateUAV(
//         graph_builder.CreateTexture(
//             FRDGTextureDesc::Create2DArray(
//                 heightmap->Desc.Extent,
//                 PF_R32_FLOAT,
//                 heightmap->Desc.ClearValue,
//                 TexCreate_UAV | TexCreate_ShaderResource,
//                 heightmap->Desc.ArraySize
//             ),
//             TEXT("JumpFillOutSeedTexture")
//         )
//     );
//
//     FComputeShaderUtils::AddPass(graph_builder, RDG_EVENT_NAME("UDLOD.JumpFill"), ERDGPassFlags::Compute,
//         shader, pass_params, FIntVector(8, 8, 1));
// }

