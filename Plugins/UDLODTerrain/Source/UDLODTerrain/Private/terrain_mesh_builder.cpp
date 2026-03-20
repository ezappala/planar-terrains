// #include "terrain_mesh_builder.h"
//
// #include "PixelShaderUtils.h"
// #include "RHIStaticStates.h"
// #include "SystemTextures.h"
// #include "terrain_shaders.h"
// #include "terrain_vertex_factory.h"
// #include "TextureResource.h"
//
// // void FTerrainBuffers::FTerrainIndexBuffer::InitRHI(FRHICommandListBase& RHICmdList) {}
//
// // void FTerrainBuffers::FTerrainIndexBuffer::ReleaseRHI() { IndexBufferRHI.SafeRelease(); }
//
// bool FTerrainBuffers::IsValid() const {
//     return geometry_buffer.IsValid() &&
//         // index_buffer_rhi.IsValid() &&
//         position_srv.IsValid() &&
//         vertex_count > 0
//         //&& index_count > 0
//         ;
// }

// void FTerrainBuffers::Reset() {
//     // if (index_buffer.IsInitialized()) { index_buffer.ReleaseResource(); }
//
//     geometry_buffer.SafeRelease();
//     // index_buffer_rhi.SafeRelease();
//     position_srv.SafeRelease();
//     vertex_count = 0;
//     // index_count = 0;
//     resolution_x = 0;
//     resolution_y = 0;
// }
//
// void FTerrainBuffers::InitRHI(FRHICommandListBase& RHICmdList) {}
// void FTerrainBuffers::ReleaseRHI() { Reset(); }
// FTerrainMeshBuilder::FTerrainMeshBuilder() {}
// FTerrainMeshBuilder::~FTerrainMeshBuilder() {}

// void FTerrainMeshBuilder::exec_terrain_pipeline(
//     FRDGBuilder& gb,
//     const FTerrainSettings& settings,
//     const FMatrix& local_to_world,
//     const FVector& camera_position,
//     UTexture* heightmap,
//     FTerrainMeshData& out_mesh_data
// ) {
//     const FIntPoint resolution = calculate_resolution(settings.lod_count);
//     const int32 vertex_count = resolution.X * resolution.Y;
//     const int32 tile_count = settings.geometry_tile_count;
//
//     FRDGBufferSRVRef geometry_buffer = gb.CreateSRV(
//         gb.CreateBuffer(
//             FRDGBufferDesc::CreateStructuredDesc<GeometryTile>(vertex_count),
//             TEXT("UDLODTerrain.GeometryBuffer")
//         ));
//
//     FRDGBufferSRVRef tile_buffer = gb.CreateSRV(gb.CreateBuffer(
//         FRDGBufferDesc::CreateStructuredDesc<TileTreeEntry>(tile_count),
//         TEXT("UDLODTerrain.TileBuffer")
//     ));
//
//     dispatch_vertex_generation(gb, settings, resolution, local_to_world,
//         FVector::ZeroVector, heightmap, geometry_buffer, tile_buffer);
//     // dispatch_index_generation(gb, resolution, FIntVector4(1, 1, 1, 1), index_buffer);
//     extract_mesh_data(gb, resolution, geometry_buffer, /*index_buffer,*/
//         out_mesh_data);
// }

// void FTerrainMeshBuilder::exec_terrain_pipeline(
//     FRDGBuilder& gb,
//     const FTerrainSettings& settings,
//     const FMatrix& local_to_world,
//     const FVector& camera_position,
//     UTexture* heightmap,
//     FTerrainBuffers& out_buffers
// ) {
//     const FIntPoint resolution = calculate_resolution(settings.lod_count);
//     const int32 vertex_count = resolution.X * resolution.Y;
//     // const int32 index_count = (resolution.X - 1) * (resolution.Y - 1) * 6;
//
//     FRDGBufferSRVRef vertex_buffer = gb.CreateSRV(
//         gb.CreateBuffer(
//             FRDGBufferDesc::CreateStructuredDesc<GeometryTile>(vertex_count),
//             TEXT("UDLODTerrain.VertexBuffer")
//         )
//     );
//     // FRDGBufferRef index_buffer = nullptr;
//
//     FRDGBufferSRVRef tile_buffer = gb.CreateSRV(
//         gb.CreateBuffer(
//             FRDGBufferDesc::CreateStructuredDesc<FTileCoordinate>(settings
//                 .geometry_tile_count),
//             TEXT("UDLODTerrain.TileBuffer")
//         )
//     );
//
//     dispatch_vertex_generation(
//         gb,
//         settings,
//         resolution,
//         local_to_world,
//         FVector::ZeroVector,
//         heightmap,
//         vertex_buffer, tile_buffer
//     );
//     // dispatch_displacement(gb, settings, resolution, heightmap, vertex_buffer);
//     // dispatch_index_generation(gb, resolution, FIntVector4(1, 1, 1, 1), index_buffer);
//
//     out_buffers.vertex_count = vertex_count;
//     // out_buffers.index_count = index_count;
//     out_buffers.resolution_x = resolution.X;
//     out_buffers.resolution_y = resolution.Y;
//
//     TRefCountPtr<FRDGPooledBuffer> pooled_position_buffer = gb.
//         ConvertToExternalBuffer(reinterpret_cast<FRDGBufferRef>(vertex_buffer));
//     TRefCountPtr<FRDGPooledBuffer> pooled_tile_buffer = gb.
//         ConvertToExternalBuffer(reinterpret_cast<FRDGBufferRef>(tile_buffer));
//     // TRefCountPtr<FRDGPooledBuffer> pooled_index_buffer = gb.ConvertToExternalBuffer(index_buffer);
//
//     gb.AddPass(
//         RDG_EVENT_NAME("UDLOD.CreateGPUBufferSRVs"),
//         ERDGPassFlags::None,
//         [&out_buffers, pooled_position_buffer, &pooled_tile_buffer
//             /*, pooled_index_buffer*/
//         ](FRHICommandList& cmd) {
//             if (pooled_position_buffer.IsValid()) {
//                 out_buffers.geometry_buffer = pooled_position_buffer->GetRHI();
//                 out_buffers.position_srv = cmd.CreateShaderResourceView(
//                     out_buffers.geometry_buffer,
//                     FRHIViewDesc::CreateBufferSRV().SetType(
//                         FRHIViewDesc::EBufferType::Structured)
//                 );
//             }
//
//             if (pooled_tile_buffer.IsValid()) {
//                 out_buffers.tile_buffer = pooled_tile_buffer->GetRHI();
//                 out_buffers.tile_srv = cmd.CreateShaderResourceView(
//                     out_buffers.tile_buffer,
//                     FRHIViewDesc::CreateBufferSRV().SetType(
//                         FRHIViewDesc::EBufferType::Structured)
//                 );
//             }
//
//             // if (pooled_index_buffer.IsValid()) {
//             //     out_buffers.index_buffer_rhi = pooled_index_buffer->GetRHI();
//             //     out_buffers.index_buffer.IndexBufferRHI = out_buffers.index_buffer_rhi;
//             //     if (!out_buffers.index_buffer.IsInitialized()) {
//             //         out_buffers.index_buffer.InitResource(cmd);
//             //     }
//             // }
//         }
//     );
// }
//
// FIntPoint FTerrainMeshBuilder::calculate_resolution(const float lod_factor) {
//     constexpr int32 THREAD_GROUP_SIZE = 8;
//     constexpr int32 MAX_RESOLUTION = 1024;
//     constexpr int32 MAX_SEGMENTS = MAX_RESOLUTION - 1;
//
//     const int32 desired_segments_calc = FMath::Max(1,
//         FMath::RoundToInt(lod_factor) * 4);
//     const int32 desired_segments = FMath::Clamp(
//         desired_segments_calc,
//         THREAD_GROUP_SIZE,
//         MAX_SEGMENTS
//     );
//
//     const int32 segments_calc =
//         FMath::DivideAndRoundUp(desired_segments, THREAD_GROUP_SIZE) *
//         THREAD_GROUP_SIZE;
//
//     const int32 segments = FMath::Clamp(
//         segments_calc,
//         THREAD_GROUP_SIZE,
//         MAX_SEGMENTS
//     );
//
//     const int32 resolution = FMath::Min(segments + 1, MAX_RESOLUTION);
//     return {resolution, resolution};
// }
//
// void FTerrainMeshBuilder::dispatch_vertex_generation(
//     FRDGBuilder& gb,
//     const FTerrainSettings& settings,
//     FIntPoint resolution,
//     const FMatrix& local_to_world,
//     const FVector& tile_offset,
//     UTexture* heightmap,
//     FRDGBufferSRVRef& out_geometry_buffer,
//     FRDGBufferSRVRef& out_tile_buffer
// ) {
//     const FRDGTextureRef heightmap_texture_ref = heightmap != nullptr
//         ? create_rdg_texture_from_utexture(gb, heightmap,
//             TEXT("HeightmapTexture"))
//         : get_default_white_texture(gb);
//
//     FTerrainVertexShader::FParameters* pass_params = gb.AllocParameters<
//         FTerrainVertexShader::FParameters>();
//
//     Terrain terrain;
//     terrain.lod_count = settings.lod_count;
//     terrain.scale = settings.scale;
//     terrain.min_height = settings.min_height;
//     terrain.max_height = settings.max_height;
//     terrain.height_scale = settings.height_scale;
//     // TODO: calculate proper world_from_unit
//     terrain.world_from_unit = FMatrix3x4{};
//     terrain.unit_from_world_transpose_a = FMatrix2x4{};
//     terrain.unit_from_world_transpose_b = 0.0f;
//
//     AttachmentConfig height_config;
//     height_config.texture_size = settings.heightmap_texture_size;
//     height_config.center_size = settings.heightmap_center_size();
//     height_config.scale = settings.heightmap_scale;
//     height_config.offset = settings.heightmap_offset();
//     TAlignedTypedef<unsigned, 4>::Type heightmap_mask = settings.heightmap_mask;
//     height_config.mask = heightmap_mask;
//
//     AttachmentConfig albedo_config;
//     albedo_config.texture_size = settings.albedo_texture_size;
//     albedo_config.center_size = settings.albedo_center_size();
//     albedo_config.scale = settings.albedo_scale;
//     albedo_config.offset = settings.albedo_offset();
//     TAlignedTypedef<unsigned, 4>::Type albedo_mask = settings.albedo_mask;
//     albedo_config.mask = albedo_mask;
//
//     TerrainView terrain_view;
//     terrain_view.tree_size = settings.tree_size;
//     terrain_view.geometry_tile_count = settings.geometry_tile_count;
//     terrain_view.grid_size = settings.grid_size;
//     terrain_view.vertices_per_row = resolution.X;
//     terrain_view.vertices_per_tile = settings.tile_size + 1;
//     terrain_view.morph_distance = settings.morph_distance;
//     terrain_view.blend_distance = settings.blend_distance;
//     terrain_view.load_distance = settings.load_distance;
//     terrain_view.subdivision_distance = settings.subdivision_distance;
//     terrain_view.morph_range = settings.morph_range;
//     terrain_view.blend_range = settings.blend_range;
//     terrain_view.precision_distance = settings.precision_distance;
//     terrain_view.face = 0; // TODO: support other faces
//     terrain_view.lod = 0;
//     // TODO: calculate proper LOD based on camera position and other factors
//     for (int i = 0; i < 6; i++) {
//         terrain_view.coordinates[i].xy = FUintVector::ZeroValue;
//         terrain_view.coordinates[i].uv = FVector2f::ZeroVector;
//         terrain_view.half_spaces[i] = FVector4f::Zero();
//     }
//     terrain_view.height_scale = settings.height_scale;
//     terrain_view.world_position = FVector3f(local_to_world.GetOrigin());
//
//     pass_params->terrain = terrain;
//     pass_params->height_config = height_config;
//     pass_params->albedo_config = albedo_config;
//     pass_params->terrain_sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
//     pass_params->height_attachment = gb.CreateSRV(
//         FRDGTextureSRVDesc::Create(heightmap_texture_ref));
//     pass_params->albedo_attachment = gb.
//         CreateSRV(get_default_white_texture(gb));
//     pass_params->terrain_view = terrain_view;
//     pass_params->approximate_height = (settings.min_height + settings.
//         max_height) * 0.5f;
//     pass_params->tile_tree = out_tile_buffer;
//     pass_params->geometry_tiles = out_geometry_buffer;
//
//     FGlobalShaderMap* gsm = GetGlobalShaderMap(GMaxRHIShaderPlatform);
//     TShaderMapRef<FTerrainVertexShader> vertex_shader{gsm};
//     gb.AddPass<FTerrainVertexShader::FParameters, TFunction<void(FRHICommandList&)>>(
//         RDG_EVENT_NAME("UDLOD.Vertex"),
//         pass_params,
//         ERDGPassFlags::NeverCull,
//         [vertex_shader, pass_params, resolution](
//             FRHICommandList& cmd
//         ) {
//         });
//     FComputeShaderUtils::AddPass(
//         gb,
//         RDG_EVENT_NAME("UDLOD.Vertex"),
//         ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
//         vertex_shader,
//         pass_params,
//         FIntVector(
//             FMath::DivideAndRoundUp(resolution.X, 8),
//             FMath::DivideAndRoundUp(resolution.Y, 8),
//             1
//         ));
// }
//
// // void FTerrainMeshBuilder::dispatch_index_generation(
// //     FRDGBuilder& gb, FIntPoint resolution, const FIntVector4& edge_collapse_factors, FRDGBufferRef& out_index_buffer
// // ) {}
//
// void FTerrainMeshBuilder::extract_mesh_data(
//     FRDGBuilder& gb, FIntPoint resolution, FRDGBufferSRVRef vertex_buffer,
//     FTerrainMeshData& out_mesh_data
// ) {}
//
// FRDGTextureRef FTerrainMeshBuilder::create_rdg_texture_from_utexture(
//     FRDGBuilder& gb, UTexture* tex, const TCHAR* name
// ) {
//     if (tex == nullptr || tex->GetResource() == nullptr) {
//         return get_default_white_texture(gb);
//     }
//
//     const FTextureResource* texture_resource = tex->GetResource();
//     FRHITexture* rhi_tex = texture_resource->TextureRHI;
//     return gb.RegisterExternalTexture(CreateRenderTarget(rhi_tex, name));
// }
//
// FRDGTextureRef FTerrainMeshBuilder::get_default_white_texture(FRDGBuilder& gb) {
//     return GSystemTextures.GetWhiteDummy(gb);
// }
