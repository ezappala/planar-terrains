#pragma once
// #include "RenderGraphBuilder.h"
// #include "RenderResource.h"
// #include "terrain_component.h"
// #include "Engine/Texture.h"

// struct FTerrainMeshData {
//     TArray<FVector3f> vertices;
//     TArray<uint32> indices;
//
//     int32 resolution_x = 0;
//     int32 resolution_y = 0;
//
//     void Reset() {
//         vertices.Empty();
//         indices.Empty();
//         resolution_x = 0;
//         resolution_y = 0;
//     }
//
//     bool IsValid() const {
//         return vertices.Num() > 0 && indices.Num() > 0;
//     }
// };

// struct FTerrainBuffers final : FRenderResource {
//     FBufferRHIRef geometry_buffer;
//     FBufferRHIRef tile_buffer;
//
//     FShaderResourceViewRHIRef position_srv;
//     FShaderResourceViewRHIRef tile_srv;
//
//     int32 vertex_count = 0;
//     int32 geometry_count = 0;
//     int32 resolution_x = 0;
//     int32 resolution_y = 0;
//
//     bool IsValid() const;
//
//     void Reset();
//
//     virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
//     virtual void ReleaseRHI() override;
// };

// class UDLODTERRAIN_API FTerrainMeshBuilder {
// public:
//     FTerrainMeshBuilder();
//     ~FTerrainMeshBuilder();
//
//     void exec_terrain_pipeline(
//         FRDGBuilder& gb,
//         const FTerrainSettings& settings,
//         const FMatrix& local_to_world,
//         const FVector& camera_position,
//         UTexture* heightmap,
//         FTerrainMeshData& out_mesh_data
//     );
//
//     void exec_terrain_pipeline(
//         FRDGBuilder& gb,
//         const FTerrainSettings& settings,
//         const FMatrix& local_to_world,
//         const FVector& camera_position,
//         UTexture* heightmap,
//         FTerrainBuffers& out_buffers
//     );
//
//     static FIntPoint calculate_resolution(float lod_factor);
//
//     static void dispatch_vertex_generation(
//         FRDGBuilder& gb,
//         const FTerrainSettings& settings,
//         FIntPoint resolution,
//         const FMatrix& local_to_world,
//         const FVector& tile_offset,
//         UTexture* heightmap, FRDGBufferSRVRef& out_geometry_buffer, FRDGBufferSRVRef&
//         out_tile_buffer
//     );
//
//     // static void dispatch_index_generation(
//     //     FRDGBuilder& gb,
//     //     FIntPoint resolution,
//     //     const FIntVector4& edge_collapse_factors,
//     //     FRDGBufferRef& out_index_buffer
//     // );
//
//     static void extract_mesh_data(
//         FRDGBuilder& gb,
//         FIntPoint resolution,
//         FRDGBufferSRVRef vertex_buffer,
//         // FRDGBufferRef index_buffer,
//         FTerrainMeshData& out_mesh_data
//     );
//
//     static FRDGTextureRef create_rdg_texture_from_utexture(
//         FRDGBuilder& gb,
//         UTexture* tex,
//         const TCHAR* name
//     );
//
//     static FRDGTextureRef get_default_white_texture(FRDGBuilder& gb);
// };