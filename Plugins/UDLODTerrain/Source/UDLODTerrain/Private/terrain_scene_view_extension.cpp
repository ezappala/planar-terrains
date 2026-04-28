#include "terrain_scene_view_extension.h"

#include "PixelShaderUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "SystemTextures.h"
#include "terrain_funcs.h"
#include "terrain_picking.h"
#include "terrain_render_state.h"
#include "terrain_runtime_planar.h"
#include "terrain_shaders.h"
#include "terrain_tile_loader.h"
#include "terrain_world_subsystem.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "HAL/IConsoleManager.h"
#include "Logging/StructuredLog.h"
#include "Runtime/Renderer/Private/SceneRendering.h"

namespace {
TAutoConsoleVariable CVarUDLODUseCpuMeshBuilder(
    TEXT("r.UDLOD.UseCpuMeshBuilder"),
    0,
    TEXT("When non-zero, build and render the CPU mesh fallback instead of the GPU UDLOD path."),
    ECVF_RenderThreadSafe
);

TAutoConsoleVariable CVarUDLODGpuProbe(
    TEXT("r.UDLOD.GpuProbe"),
    1,
    TEXT("When non-zero, enqueue a GPU readback probe for UDLOD indirect args and prepass state."),
    ECVF_RenderThreadSafe
);

bool should_use_cpu_mesh_builder() {
    return CVarUDLODUseCpuMeshBuilder.GetValueOnRenderThread() != 0;
}

bool should_probe_gpu_path() { return CVarUDLODGpuProbe.GetValueOnRenderThread() != 0; }

void apply_runtime_debug_overrides(
    const ATerrainParentActor& root_actor,
    FTileTree& tile_tree,
    FTileAtlas& tile_atlas
) {
    const FTerrainDebugSettings& debug_settings = root_actor.debug_settings;
    tile_tree.grid_size = static_cast<uint32>(FMath::Max(2, debug_settings.grid_size));
    tile_tree.blend_distance = FMath::Max(1.0, debug_settings.blend_distance);
    tile_tree.load_distance = FMath::Max(1.0, debug_settings.load_distance);
    tile_tree.morph_distance = FMath::Max(1.0, debug_settings.morph_distance);
    tile_tree.subdivision_distance = FMath::Max(1.0, debug_settings.subdivision_distance);
    tile_atlas.height_scale = FMath::Max(0.0f, debug_settings.height_scale);
}

FRDGTextureRef add_depth_copy_pass(
    FRDGBuilder& gb,
    const FViewInfo& view,
    const FRDGTextureRef source_depth_texture
) {
    if (source_depth_texture == nullptr) { return nullptr; }

    const FRDGTextureDesc depth_copy_desc = FRDGTextureDesc::Create2D(
        source_depth_texture->Desc.Extent,
        PF_DepthStencil,
        FClearValueBinding::DepthFar,
        TexCreate_DepthStencilTargetable | TexCreate_ShaderResource
    );
    const auto coppied_depth_tex = gb.CreateTexture(depth_copy_desc, TEXT("UDLOD.TerrainDepthCopy"));

    FTerrainDepthCopyPixelShader::FParameters* pass_parameters =
        gb.AllocParameters<FTerrainDepthCopyPixelShader::FParameters>();
    if (source_depth_texture->Desc.NumSamples > 1) {
        pass_parameters->depth_texture_ms = source_depth_texture;
    } else { pass_parameters->depth_texture = source_depth_texture; }
    pass_parameters->RenderTargets.DepthStencil = FDepthStencilBinding(
        coppied_depth_tex,
        ERenderTargetLoadAction::EClear,
        ERenderTargetLoadAction::EClear,
        FExclusiveDepthStencil::DepthWrite_StencilWrite
    );

    FTerrainDepthCopyPixelShader::FPermutationDomain permutation_vector;
    permutation_vector.Set<FTerrainDepthCopyPixelShader::FMSAASampleCount>(
        source_depth_texture->Desc.NumSamples
    );
    const TShaderMapRef<FTerrainDepthCopyPixelShader> pixel_shader(
        view.ShaderMap,
        permutation_vector
    );

    FRHIDepthStencilState* depth_stencil_state = TStaticDepthStencilState<
        true,
        CF_Always,
        true,
        CF_Always,
        SO_Zero,
        SO_Zero,
        SO_Zero,
        true,
        CF_Always,
        SO_Zero,
        SO_Zero,
        SO_Zero
    >::GetRHI();

    FPixelShaderUtils::AddFullscreenPass(
        gb,
        view.ShaderMap,
        RDG_EVENT_NAME("UDLOD.DepthCopy"),
        pixel_shader,
        pass_parameters,
        view.ViewRect,
        nullptr,
        nullptr,
        depth_stencil_state,
        0u
    );

    return coppied_depth_tex;
}

struct FPrepassStateProbe {
    uint32 tile_count = 0;
    int32 counter = 0;
    int32 child_index = 0;
    int32 final_index = 0;
};

static_assert(sizeof(FPrepassStateProbe) == sizeof(uint32) * 4u);

struct FGpuSampleProbeReadback {
    FUintVector4 ids0 = FUintVector4(UINT32_MAX, 0u, 0u, 0u);
    FUintVector4 coords0 = FUintVector4(0u, 0u, 0u, 0u);
    FVector4f scalars = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
    FVector4f albedo = FVector4f(-1.0f, -1.0f, -1.0f, -1.0f);
    FVector4f base_world = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
    FVector4f displaced_world = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
    FVector4f extra = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
};

static_assert(sizeof(FGpuSampleProbeReadback) == 112u);

constexpr uint32 GpuProbeTileSampleLimit = 4u;

FVector3d compute_planar_world_position(
    const FTerrainConfig& config,
    const FTransform& terrain_transform,
    const FVector2d& face_uv
) {
    const FVector local_position{
        ((face_uv.X - 0.5) * config.side_length),
        ((face_uv.Y - 0.5) * config.side_length),
        0.0
    };
    return FVector3d(terrain_transform.TransformPosition(local_position));
}

void log_gpu_probe_tile_details(
    const uint64 submission_id,
    const uint32 view_key,
    const uint32 final_index,
    const GeometryTile* final_tiles,
    const uint32 sampled_tile_count,
    const FTileTree& tile_tree,
    const FTerrainConfig& config,
    const FTransform& terrain_tf
) {
    const uint32 tile_count = FMath::Min(sampled_tile_count, final_index);
    for (uint32 tile_index = 0; tile_index < tile_count; ++tile_index) {
        const GeometryTile& tile = final_tiles[tile_index];
        const FVector2d lod_tile_count = tile_tree.get_tile_count(tile.lod);
        const FVector2d uv_min{
            static_cast<double>(tile.xy.X) / lod_tile_count.X,
            static_cast<double>(tile.xy.Y) / lod_tile_count.Y
        };
        const FVector2d uv_max{
            static_cast<double>(tile.xy.X + 1u) / lod_tile_count.X,
            static_cast<double>(tile.xy.Y + 1u) / lod_tile_count.Y
        };
        const FVector3d world_min = compute_planar_world_position(config, terrain_tf, uv_min);
        const FVector3d world_max = compute_planar_world_position(config, terrain_tf, uv_max);

        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "[UDLOD.GpuProbeTile] submission=%llu view=%u tile[%u]={face=%u, "
                "lod=%u, xy=(%u,%u), uv_min=(%.6f,%.6f), uv_max=(%.6f,%.6f), "
                "world_min=(%.3f,%.3f,%.3f), world_max=(%.3f,%.3f,%.3f), "
                "view_distances=(%.3f,%.3f,%.3f,%.3f)}"
            ),
            submission_id,
            view_key,
            tile_index,
            tile.face,
            tile.lod,
            tile.xy.X,
            tile.xy.Y,
            uv_min.X,
            uv_min.Y,
            uv_max.X,
            uv_max.Y,
            world_min.X,
            world_min.Y,
            world_min.Z,
            world_max.X,
            world_max.Y,
            world_max.Z,
            tile.view_distances.X,
            tile.view_distances.Y,
            tile.view_distances.Z,
            tile.view_distances.W
        );
    }
}

void log_gpu_probe_atlas_state(
    const uint64 submission_id,
    const uint32 view_key,
    const FTileAtlas& tile_atlas,
    const FTileTree& tile_tree
) {
    const FTileCoordinate root_tile = tile_atlas.get_face_root_tile(tile_tree.view_face);
    if (root_tile == FTileCoordinate::INVALID()) {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("[UDLOD.AtlasProbe] submission=%llu view=%u root={missing=true, face=%u}"),
            submission_id,
            view_key,
            tile_tree.view_face
        );
        return;
    }

    const FTileLoadingState* tile_state = tile_atlas.find_tile_state(root_tile);
    const uint32 sx = root_tile.xy.X % tile_tree.tree_size;
    const uint32 sy = root_tile.xy.Y % tile_tree.tree_size;
    const TileTreeEntry root_entry = tile_tree.data(root_tile.face, root_tile.lod, sx, sy);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "[UDLOD.AtlasProbe] submission=%llu view=%u "
            "root={coord=%s, pinned=%s, present=%s, loaded=%s, loading=%u, "
            "requests=%u, atlas_index=%u} "
            "tile_tree_entry={atlas_index=%u, atlas_lod=%u}"
        ),
        submission_id,
        view_key,
        *root_tile.to_string(),
        tile_atlas.is_pinned_tile(root_tile) ? TEXT("true") : TEXT("false"),
        tile_state != nullptr ? TEXT("true") : TEXT("false"),
        tile_state != nullptr && tile_state->state.is_loaded ? TEXT("true") : TEXT("false"),
        tile_state != nullptr ? tile_state->state.loading : 0u,
        tile_state != nullptr ? tile_state->requests : 0u,
        tile_state != nullptr ? tile_state->atlas_index : MAX_uint32,
        root_entry.atlas_index,
        root_entry.atlas_lod
    );
}

void log_gpu_probe_cpu_tile_tree_entry(
    const uint64 submission_id,
    const uint32 view_key,
    const FTileTree& tile_tree,
    const FGpuSampleProbeReadback& sample_probe
) {
    const uint32 face = sample_probe.ids0.W;
    const uint32 lod = sample_probe.ids0.Y;
    if (face >= tile_tree.tiles.get_dim0() || lod >= tile_tree.tiles.get_dim1()) { return; }

    const uint32 slot_x = sample_probe.coords0.X % tile_tree.tree_size;
    const uint32 slot_y = sample_probe.coords0.Y % tile_tree.tree_size;
    const FTileState& tile_state = tile_tree.tiles(face, lod, slot_x, slot_y);
    const TileTreeEntry& tile_tree_entry = tile_tree.data(face, lod, slot_x, slot_y);

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "[UDLOD.GpuProbeCpuTileTree] submission=%llu view=%u "
            "geometry={face=%u,lod=%u,xy=(%u,%u)} slot={x=%u,y=%u} "
            "stored_tile={coord=%s, requested=%s} "
            "entry={atlas_index=%u, atlas_lod=%u}"
        ),
        submission_id,
        view_key,
        face,
        lod,
        sample_probe.coords0.X,
        sample_probe.coords0.Y,
        slot_x,
        slot_y,
        *tile_state.coordinate.to_string(),
        tile_state.request_state == ERequestState::Requested ? TEXT("true") : TEXT("false"),
        tile_tree_entry.atlas_index,
        tile_tree_entry.atlas_lod
    );
}

[[maybe_unused]] TOptional<FTileCoordinate> find_atlas_owner_coordinate(
    const FTileAtlas& tile_atlas,
    const uint32 atlas_index
) {
    if (atlas_index == UINT32_MAX) { return NullOpt; }
    return tile_atlas.find_owner_coordinate(atlas_index);
}

struct FCpuAttachmentLookup {
    FTileCoordinate sampled_coordinate = FTileCoordinate::INVALID();
    const FAttachment* attachment = nullptr;
    const FAttachmentTileData* data = nullptr;
};

uint32 compute_vertices_per_tile(const FTileTree& tile_tree) {
    return 2u * tile_tree.grid_size * (tile_tree.grid_size + 2u);
}

TOptional<FString> find_attachment_label(
    const FTileAtlas& tile_atlas,
    const TCHAR* preferred_label
) {
    for (const TPair<FString, FAttachment>& entry : tile_atlas.attachments) {
        if (entry.Key.Equals(preferred_label, ESearchCase::IgnoreCase)) { return entry.Key; }
    }

    return NullOpt;
}

TOptional<FCpuAttachmentLookup> find_best_loaded_attachment(
    const FTileAtlas& tile_atlas,
    const FString& label,
    const FTileCoordinate& render_coordinate
) {
    const FAttachment* attachment = tile_atlas.attachments.Find(label);
    if (attachment == nullptr) { return NullOpt; }

    for (FTileCoordinate sampled_coordinate = render_coordinate;
         sampled_coordinate != FTileCoordinate::INVALID();
         sampled_coordinate = sampled_coordinate.parent().Get(FTileCoordinate::INVALID())) {
        const FAttachmentTile key{sampled_coordinate, label};
        if (const FAttachmentTileData* data = tile_atlas.loaded_tile_data.Find(key)) {
            return FCpuAttachmentLookup{sampled_coordinate, attachment, data};
        }
    }

    return NullOpt;
}

bool try_build_height_samples(const FAttachmentTileData& data, TArray<float>& out_samples) {
    if (const auto* rgba_data = data.TryGet<TArray<TStaticArray<uint8, 4>>>()) {
        out_samples.SetNumUninitialized(rgba_data->Num());
        for (int32 index = 0; index < rgba_data->Num(); ++index) {
            out_samples[index] = static_cast<float>((*rgba_data)[index][0]);
        }
        return true;
    }

    if (const auto* r16u_data = data.TryGet<TArray<uint16>>()) {
        out_samples.SetNumUninitialized(r16u_data->Num());
        for (int32 index = 0; index < r16u_data->Num(); ++index) {
            out_samples[index] = static_cast<float>((*r16u_data)[index]);
        }
        return true;
    }

    if (const auto* r16i_data = data.TryGet<TArray<int16>>()) {
        out_samples.SetNumUninitialized(r16i_data->Num());
        for (int32 index = 0; index < r16i_data->Num(); ++index) {
            out_samples[index] = static_cast<float>((*r16i_data)[index]);
        }
        return true;
    }

    if (const auto* rg16u_data = data.TryGet<TArray<TStaticArray<uint16, 2>>>()) {
        out_samples.SetNumUninitialized(rg16u_data->Num());
        for (int32 index = 0; index < rg16u_data->Num(); ++index) {
            out_samples[index] = static_cast<float>((*rg16u_data)[index][0]);
        }
        return true;
    }

    if (const auto* r32f_data = data.TryGet<TArray<float>>()) {
        out_samples = *r32f_data;
        return true;
    }

    return false;
}

TOptional<FVector2f> try_compute_height_range(const FAttachmentTileData& data) {
    TArray<float> samples;
    if (!try_build_height_samples(data, samples) || samples.IsEmpty()) { return NullOpt; }

    float min_value = MAX_flt;
    float max_value = -MAX_flt;
    for (const float sample : samples) {
        min_value = FMath::Min(min_value, sample);
        max_value = FMath::Max(max_value, sample);
    }

    return FVector2f(min_value, max_value);
}

void log_gpu_probe_cpu_height_attachment(
    const uint64 submission_id,
    const uint32 view_key,
    const FTileAtlas& tile_atlas,
    const TOptional<FTileCoordinate>& atlas_owner_coordinate
) {
    if (!atlas_owner_coordinate.IsSet()) { return; }

    const TOptional<FString> height_label = find_attachment_label(tile_atlas, TEXT("height"));
    if (!height_label.IsSet()) { return; }

    const FAttachment* attachment = tile_atlas.attachments.Find(height_label.GetValue());
    if (attachment == nullptr) { return; }

    const FAttachmentTile key{atlas_owner_coordinate.GetValue(), height_label.GetValue()};
    const FAttachmentTileData* height_data = tile_atlas.loaded_tile_data.Find(key);
    if (height_data == nullptr) {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "[UDLOD.HeightProbeCpu] submission=%llu view=%u coord=%s loaded=false"
            ),
            submission_id,
            view_key,
            *atlas_owner_coordinate->to_string()
        );
        return;
    }

    const TOptional<FVector2f> range = try_compute_height_range(*height_data);
    if (!range.IsSet()) {
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "[UDLOD.HeightProbeCpu] submission=%llu view=%u coord=%s loaded=true range={unavailable=true}"
            ),
            submission_id,
            view_key,
            *atlas_owner_coordinate->to_string()
        );
        return;
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "[UDLOD.HeightProbeCpu] submission=%llu view=%u coord=%s attachment={texture_size=%u, center_size=%u, border_size=%u, mask=%s} range={min=%.6f, max=%.6f} runtime={height_scale=%.6f, min_height=%.6f, max_height=%.6f}"
        ),
        submission_id,
        view_key,
        *atlas_owner_coordinate->to_string(),
        attachment->texture_size,
        attachment->center_size,
        attachment->border_size,
        attachment->mask ? TEXT("true") : TEXT("false"),
        range->X,
        range->Y,
        tile_atlas.height_scale,
        tile_atlas.min_height,
        tile_atlas.max_height
    );
}

bool try_build_color_samples(const FAttachmentTileData& data, TArray<FColor>& out_samples) {
    if (const auto* rgba_data = data.TryGet<TArray<TStaticArray<uint8, 4>>>()) {
        out_samples.SetNumUninitialized(rgba_data->Num());
        for (int32 index = 0; index < rgba_data->Num(); ++index) {
            const TStaticArray<uint8, 4>& pixel = (*rgba_data)[index];
            out_samples[index] = FColor(pixel[0], pixel[1], pixel[2], pixel[3]);
        }
        return true;
    }

    return false;
}

float sample_scalar_tile(
    const TArray<float>& samples,
    const uint32 texture_size,
    const uint32 center_size,
    const uint32 border_size,
    const FTileTree& tile_tree,
    const FTileCoordinate& render_coordinate,
    const FTileCoordinate& sampled_coordinate,
    const FVector2f& render_tile_uv
) {
    if (samples.IsEmpty() || texture_size == 0u || center_size == 0u) { return 0.0f; }

    const FVector2d render_tile_count = tile_tree.get_tile_count(render_coordinate.lod);
    const FVector2d sampled_tile_count = tile_tree.get_tile_count(sampled_coordinate.lod);
    const FVector2d face_uv{
        (static_cast<double>(render_coordinate.xy.X) + render_tile_uv.X) / render_tile_count.X,
        (static_cast<double>(render_coordinate.xy.Y) + render_tile_uv.Y) / render_tile_count.Y
    };
    const FVector2d sampled_local_uv{
        face_uv.X * sampled_tile_count.X - static_cast<double>(sampled_coordinate.xy.X),
        face_uv.Y * sampled_tile_count.Y - static_cast<double>(sampled_coordinate.xy.Y)
    };
    const float clamped_u = FMath::Clamp(static_cast<float>(sampled_local_uv.X), 0.0f, 1.0f);
    const float clamped_v = FMath::Clamp(static_cast<float>(sampled_local_uv.Y), 0.0f, 1.0f);
    const float sample_x = static_cast<float>(border_size) + clamped_u * static_cast<float>(
        center_size);
    const float sample_y = static_cast<float>(border_size) +
        clamped_v *
        static_cast<float>(center_size);

    const int32 x0 = FMath::Clamp(
        FMath::FloorToInt(sample_x),
        0,
        static_cast<int32>(texture_size) - 1);
    const int32 y0 = FMath::Clamp(
        FMath::FloorToInt(sample_y),
        0,
        static_cast<int32>(texture_size) - 1);

    const int32 x1 = FMath::Clamp(x0 + 1, 0, static_cast<int32>(texture_size) - 1);
    const int32 y1 = FMath::Clamp(y0 + 1, 0, static_cast<int32>(texture_size) - 1);
    const float frac_x = FMath::Frac(sample_x);
    const float frac_y = FMath::Frac(sample_y);

    const auto sample_at = [&samples, texture_size](const int32 x, const int32 y) {
        return samples[y * static_cast<int32>(texture_size) + x];
    };

    const float v00 = sample_at(x0, y0);
    const float v10 = sample_at(x1, y0);
    const float v01 = sample_at(x0, y1);
    const float v11 = sample_at(x1, y1);
    return FMath::Lerp(
        FMath::Lerp(v00, v10, frac_x),
        FMath::Lerp(v01, v11, frac_x),
        frac_y
    );
}

FColor sample_color_tile(
    const TArray<FColor>& samples,
    const uint32 texture_size,
    const uint32 center_size,
    const uint32 border_size,
    const FTileTree& tile_tree,
    const FTileCoordinate& render_coordinate,
    const FTileCoordinate& sampled_coordinate,
    const FVector2f& render_tile_uv,
    const FColor fallback
) {
    if (samples.IsEmpty() || texture_size == 0u || center_size == 0u) { return fallback; }

    const FVector2d render_tile_count = tile_tree.get_tile_count(render_coordinate.lod);
    const FVector2d sampled_tile_count = tile_tree.get_tile_count(sampled_coordinate.lod);
    const FVector2d face_uv{
        (static_cast<double>(render_coordinate.xy.X) + render_tile_uv.X) / render_tile_count.X,
        (static_cast<double>(render_coordinate.xy.Y) + render_tile_uv.Y) / render_tile_count.Y
    };
    const FVector2d sampled_local_uv{
        face_uv.X * sampled_tile_count.X - static_cast<double>(sampled_coordinate.xy.X),
        face_uv.Y * sampled_tile_count.Y - static_cast<double>(sampled_coordinate.xy.Y)
    };
    const float clamped_u = FMath::Clamp(static_cast<float>(sampled_local_uv.X), 0.0f, 1.0f);
    const float clamped_v = FMath::Clamp(static_cast<float>(sampled_local_uv.Y), 0.0f, 1.0f);
    const int32 x = FMath::Clamp(
        FMath::RoundToInt(
            static_cast<float>(border_size) + clamped_u * static_cast<float>(center_size)),
        0,
        static_cast<int32>(texture_size) - 1
    );
    const int32 y = FMath::Clamp(
        FMath::RoundToInt(
            static_cast<float>(border_size) + clamped_v * static_cast<float>(center_size)),
        0,
        static_cast<int32>(texture_size) - 1
    );
    return samples[y * static_cast<int32>(texture_size) + x];
}

struct FTileFaceBounds {
    FVector2d min = FVector2d::ZeroVector;
    FVector2d max = FVector2d::ZeroVector;
};

FTileFaceBounds compute_tile_face_bounds(
    const FTileTree& tile_tree,
    const FTileCoordinate& tile_coordinate
) {
    const FVector2d tile_count = tile_tree.get_tile_count(tile_coordinate.lod);
    const FVector2d min{
        static_cast<double>(tile_coordinate.xy.X) / tile_count.X,
        static_cast<double>(tile_coordinate.xy.Y) / tile_count.Y
    };
    const FVector2d max{
        static_cast<double>(tile_coordinate.xy.X + 1) / tile_count.X,
        static_cast<double>(tile_coordinate.xy.Y + 1) / tile_count.Y
    };
    return FTileFaceBounds{min, max};
}

bool bounds_contains_point(const FTileFaceBounds& bounds, const FVector2d& point) {
    return point.X >= bounds.min.X &&
        point.X < bounds.max.X &&
        point.Y >= bounds.min.Y &&
        point.Y < bounds.max.Y;
}

bool bounds_contains_bounds(const FTileFaceBounds& parent, const FTileFaceBounds& child) {
    constexpr double epsilon = 1e-6;
    return child.min.X >= parent.min.X - epsilon &&
        child.min.Y >= parent.min.Y - epsilon &&
        child.max.X <= parent.max.X + epsilon &&
        child.max.Y <= parent.max.Y + epsilon;
}

FTerrainCpuMeshViewState build_cpu_mesh_view_state(
    const FTileTree& tile_tree,
    const FTileAtlas& tile_atlas,
    const FTerrainConfig& config
) {
    FTerrainCpuMeshViewState cpu_view_state;

    const TOptional<FString> height_label = find_attachment_label(tile_atlas, TEXT("height"));
    if (!height_label.IsSet()) { return cpu_view_state; }

    const TOptional<FString> albedo_label = find_attachment_label(tile_atlas, TEXT("albedo"));
    TSet<FTileCoordinate> visible_tiles;
    for (uint32 lod = 0; lod < tile_tree.lod_count; ++lod) {
        for (uint32 x = 0; x < tile_tree.tree_size; ++x) {
            for (uint32 y = 0; y < tile_tree.tree_size; ++y) {
                const FTileState& tile_state = tile_tree.tiles(tile_tree.view_face, lod, x, y);
                if (tile_state.request_state != ERequestState::Requested) { continue; }

                if (tile_state.coordinate == FTileCoordinate::INVALID()) { continue; }

                visible_tiles.Add(tile_state.coordinate);
            }
        }
    }

    if (visible_tiles.IsEmpty()) { return cpu_view_state; }

    const TArray<FTileCoordinate> visible_tiles_array = visible_tiles.Array();
    const float height_scale = tile_atlas.height_scale;
    const float scaled_min_height = config.min_height * height_scale;
    const float scaled_max_height = config.max_height * height_scale;
    const uint32 subdivisions = FMath::Max(tile_tree.grid_size, 1u);
    for (const FTileCoordinate& render_coordinate : visible_tiles) {
        const TOptional<FCpuAttachmentLookup> height_lookup = find_best_loaded_attachment(
            tile_atlas,
            height_label.GetValue(),
            render_coordinate
        );
        if (!height_lookup.IsSet()) { continue; }

        TArray<float> height_samples;
        if (!try_build_height_samples(*height_lookup->data, height_samples)) { continue; }

        TArray<FColor> albedo_samples;
        TOptional<FCpuAttachmentLookup> albedo_lookup = NullOpt;
        if (albedo_label.IsSet()) {
            albedo_lookup = find_best_loaded_attachment(
                tile_atlas,
                albedo_label.GetValue(),
                render_coordinate
            );
            if (albedo_lookup.IsSet()) {
                try_build_color_samples(*albedo_lookup->data, albedo_samples);
            }
        }

        const FTileFaceBounds render_bounds =
            compute_tile_face_bounds(tile_tree, render_coordinate);
        const FVector2d tile_min_uv = render_bounds.min;
        const FVector2d tile_size_uv = render_bounds.max - render_bounds.min;
        TArray<FTileFaceBounds> descendant_bounds;
        for (const FTileCoordinate& candidate_coordinate : visible_tiles_array) {
            if (candidate_coordinate.face != render_coordinate.face ||
                candidate_coordinate.lod <= render_coordinate.lod) { continue; }

            const FTileFaceBounds candidate_bounds = compute_tile_face_bounds(
                tile_tree,
                candidate_coordinate
            );
            if (bounds_contains_bounds(render_bounds, candidate_bounds)) {
                descendant_bounds.Add(candidate_bounds);
            }
        }

        const int32 patch_vertex_base = cpu_view_state.vertices.Num();
        TArray<FVector3f> patch_positions;
        patch_positions.SetNumUninitialized((subdivisions + 1u) * (subdivisions + 1u));

        for (uint32 y = 0; y <= subdivisions; ++y) {
            for (uint32 x = 0; x <= subdivisions; ++x) {
                const FVector2f render_tile_uv{
                    static_cast<float>(x) / static_cast<float>(subdivisions),
                    static_cast<float>(y) / static_cast<float>(subdivisions)
                };
                const float height = sample_scalar_tile(
                    height_samples,
                    height_lookup->attachment->texture_size,
                    height_lookup->attachment->center_size,
                    height_lookup->attachment->border_size,
                    tile_tree,
                    render_coordinate,
                    height_lookup->sampled_coordinate,
                    render_tile_uv
                ) * height_scale;
                const float normalized_height = FMath::Clamp(
                    (height - scaled_min_height) /
                    FMath::Max(scaled_max_height - scaled_min_height, 1.0f),
                    0.0f,
                    1.0f
                );
                const FColor vertex_color = albedo_lookup.IsSet()
                    ? sample_color_tile(
                        albedo_samples,
                        albedo_lookup->attachment->texture_size,
                        albedo_lookup->attachment->center_size,
                        albedo_lookup->attachment->border_size,
                        tile_tree,
                        render_coordinate,
                        albedo_lookup->sampled_coordinate,
                        render_tile_uv,
                        FLinearColor(
                            normalized_height,
                            normalized_height,
                            normalized_height).ToFColor(true)
                    )
                    : FLinearColor(
                        normalized_height,
                        normalized_height,
                        normalized_height).ToFColor(true);
                const FVector2d face_uv = tile_min_uv + FVector2d{
                    tile_size_uv.X * static_cast<double>(render_tile_uv.X),
                    tile_size_uv.Y * static_cast<double>(render_tile_uv.Y)
                };
                const FVector3f local_position{
                    static_cast<float>((face_uv.X - 0.5) * config.side_length),
                    static_cast<float>((face_uv.Y - 0.5) * config.side_length),
                    height
                };

                const int32 patch_index = static_cast<int32>(y * (subdivisions + 1u) + x);
                patch_positions[patch_index] = local_position;
                cpu_view_state.vertices.Add(
                    FTerrainCpuMeshVertex{
                        local_position,
                        FVector2f(
                            static_cast<float>(face_uv.X),
                            static_cast<float>(face_uv.Y)
                        ),
                        FVector3f(1.0f, 0.0f, 0.0f),
                        FVector3f(0.0f, 1.0f, 0.0f),
                        FVector3f(0.0f, 0.0f, 1.0f),
                        vertex_color
                    });
            }
        }

        for (uint32 y = 0; y <= subdivisions; ++y) {
            for (uint32 x = 0; x <= subdivisions; ++x) {
                const uint32 patch_index = y * (subdivisions + 1u) + x;
                const FVector3f center = patch_positions[patch_index];
                const FVector3f left = patch_positions[y * (subdivisions + 1u) + (x > 0u
                    ? x - 1u
                    : x)];
                const FVector3f right = patch_positions[y * (subdivisions + 1u) + (x < subdivisions
                    ? x + 1u
                    : x)];
                const FVector3f down = patch_positions[(y > 0u ? y - 1u : y) * (subdivisions + 1u) +
                    x];
                const FVector3f up = patch_positions[(y < subdivisions ? y + 1u : y) * (subdivisions
                    + 1u) + x];

                FVector3f tangent_x = right - left;
                FVector3f tangent_y = up - down;
                if (tangent_x.IsNearlyZero()) { tangent_x = FVector3f(1.0f, 0.0f, 0.0f); }
                if (tangent_y.IsNearlyZero()) { tangent_y = FVector3f(0.0f, 1.0f, 0.0f); }
                tangent_x = tangent_x.GetSafeNormal();
                tangent_y = tangent_y.GetSafeNormal();
                FVector3f tangent_z = FVector3f::CrossProduct(tangent_x, tangent_y).GetSafeNormal();
                if (tangent_z.IsNearlyZero()) { tangent_z = FVector3f(0.0f, 0.0f, 1.0f); }

                FTerrainCpuMeshVertex& vertex = cpu_view_state.vertices[patch_vertex_base +
                    static_cast<int32>(patch_index)];
                vertex.position = center;
                vertex.tangent_x = tangent_x;
                vertex.tangent_y = tangent_y;
                vertex.tangent_z = tangent_z;
            }
        }

        for (uint32 y = 0; y < subdivisions; ++y) {
            for (uint32 x = 0; x < subdivisions; ++x) {
                const FVector2d cell_center_uv = tile_min_uv + FVector2d{
                    tile_size_uv.X * (static_cast<double>(x) + 0.5) / static_cast<double>(
                        subdivisions),
                    tile_size_uv.Y * (static_cast<double>(y) + 0.5) / static_cast<double>(
                        subdivisions)
                };
                bool bCoveredByDescendant = false;
                for (const FTileFaceBounds& descendant_bounds_entry : descendant_bounds) {
                    if (bounds_contains_point(descendant_bounds_entry, cell_center_uv)) {
                        bCoveredByDescendant = true;
                        break;
                    }
                }
                if (bCoveredByDescendant) { continue; }

                const uint32 i0 = patch_vertex_base + static_cast<int32>(y * (subdivisions + 1u) +
                    x);
                const uint32 i1 = patch_vertex_base + static_cast<int32>(y * (subdivisions + 1u) + x
                    + 1u);
                const uint32 i2 = patch_vertex_base + static_cast<int32>((y + 1u) * (subdivisions +
                    1u) + x);
                const uint32 i3 = patch_vertex_base + static_cast<int32>((y + 1u) * (subdivisions +
                    1u) + x + 1u);
                cpu_view_state.indices.Add(i0);
                cpu_view_state.indices.Add(i2);
                cpu_view_state.indices.Add(i1);
                cpu_view_state.indices.Add(i1);
                cpu_view_state.indices.Add(i2);
                cpu_view_state.indices.Add(i3);
            }
        }
    }

    return cpu_view_state;
}

FTerrainMeshViewState build_mesh_view_state(
    FRDGBuilder& gb,
    const uint32 view_key,
    const FTileTree& tile_tree,
    const FGpuTerrain& gpu_terrain,
    const FGpuTerrainView& gpu_terrain_view,
    const FGpuAttachment& height_attachment,
    const FGpuAttachment& albedo_attachment
) {
    FTerrainMeshViewState view_state;
    view_state.terrain_uniform_buffer = gpu_terrain.terrain_buffer;
    const TRefCountPtr<FRDGPooledBuffer>& attachments_buffer = ConvertToExternalAccessBuffer(
        gb,
        gpu_terrain.attachments_buffer,
        ERHIAccess::SRVMask
    );

    view_state.attachments_buffer_srv = attachments_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );

    view_state.terrain_sampler = gpu_terrain.atlas_sampler;
    view_state.height_attachment_texture = ConvertToExternalAccessTexture(
        gb,
        height_attachment.atlas_texture_resource,
        ERHIAccess::SRVMask
    )->GetRHI();
    view_state.albedo_attachment_texture = ConvertToExternalAccessTexture(
        gb,
        albedo_attachment.atlas_texture_resource,
        ERHIAccess::SRVMask
    )->GetRHI();

    const TRefCountPtr<FRDGPooledBuffer>& terrain_view_buffer = ConvertToExternalAccessBuffer(
        gb,
        tile_tree.terrain_view_buffer,
        ERHIAccess::SRVMask
    );

    const TRefCountPtr<FRDGPooledBuffer>& approximate_height_buffer = ConvertToExternalAccessBuffer(
        gb,
        tile_tree.approximate_height_buffer,
        ERHIAccess::SRVMask
    );

    const TRefCountPtr<FRDGPooledBuffer>& tile_tree_buffer = ConvertToExternalAccessBuffer(
        gb,
        tile_tree.tile_tree_buffer,
        ERHIAccess::SRVMask
    );

    const TRefCountPtr<FRDGPooledBuffer>& geometry_tiles_buffer = ConvertToExternalAccessBuffer(
        gb,
        gpu_terrain_view.final_tiles_buffer,
        ERHIAccess::SRVMask
    );

    view_state.terrain_view_buffer_srv = terrain_view_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );
    view_state.approximate_height_buffer_srv = approximate_height_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );
    view_state.tile_tree_buffer_srv = tile_tree_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );
    view_state.geometry_tiles_buffer_srv = geometry_tiles_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );

    view_state.indirect_args_buffer = ConvertToExternalAccessBuffer(
        gb,
        gpu_terrain_view.indirect_args_buffer,
        ERHIAccess::IndirectArgs
    );
    view_state.indirect_args_offset = 0;
    view_state.max_vertex_index = FMath::Max(
        tile_tree.geometry_tile_count * compute_vertices_per_tile(tile_tree),
        1u
    ) - 1u;
    (void)view_key;
    return view_state;
}
}

void FTerrainSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& in_view_family) {
    if (const UWorld* world = GetWorld()) {
        if (UTerrainWorldSubsystem* subsystem = world->GetSubsystem<UTerrainWorldSubsystem>()) {
            root = subsystem->resolve_terrain_root(true);
        }
    }

    TArray<FTerrainViewSnapshot> new_cached_views;
    new_cached_views.Reserve(1);
    int32 largest_view_area = INDEX_NONE;
    int32 eligible_view_count = 0;

    for (const FSceneView* view : in_view_family.Views) {
        if (view == nullptr) { continue; }
        if (view->bIsSceneCapture || view->bIsReflectionCapture || view->bIsPlanarReflection) {
            continue;
        }

        const FIntRect view_rect = view->UnscaledViewRect;
        const int32 view_area = FMath::Max(view_rect.Width(), 0) *
            FMath::Max(view_rect.Height(), 0);
        if (view_area <= 0) { continue; }

        ++eligible_view_count;
        if (view_area <= largest_view_area) { continue; }

        largest_view_area = view_area;
        new_cached_views.Reset();

        new_cached_views.Emplace(
            view->GetViewKey(),
            FVector3d{view->ViewMatrices.GetViewOrigin()},
            FMatrix44d{view->ViewMatrices.GetViewMatrix()},
            FMatrix44d{view->ViewMatrices.GetProjectionNoAAMatrix()},
            view_area
        );
    }

    if (eligible_view_count > 1 && !logged_multiple_cached_views) {
        logged_multiple_cached_views = true;
        UE_LOG(
            LogTemp,
            Display,
            TEXT(
                "[UDLOD] Multiple eligible views found in a family; caching only the largest one "
                "to avoid editor helper passes clobbering terrain state."
            )
        );
    }

    if (new_cached_views.IsEmpty()) { return; }

    const uint32 frame_number = in_view_family.FrameNumber;
    FWriteScopeLock _(cached_views_guard);
    const bool is_new_frame = cached_view_family_frame_number != frame_number;
    if (is_new_frame) {
        cached_view_family_frame_number = frame_number;
        cached_view_family_area = INDEX_NONE;
    }

    if (largest_view_area > cached_view_family_area) {
        cached_views = MoveTemp(new_cached_views);
        cached_view_family_area = largest_view_area;
    }
}

void FTerrainSceneViewExtension::PreInitViews_RenderThread(FRDGBuilder& gb) {
    process_gpu_probe_results();

    TArray<FTerrainViewSnapshot> render_views;
    uint32 render_view_frame_number;
    {
        FReadScopeLock _{cached_views_guard};
        render_views = cached_views;
        render_view_frame_number = cached_view_family_frame_number;
    }

    if (render_views.IsEmpty()) { return; }
    if (render_view_frame_number == processed_view_family_frame_number) { return; }
    processed_view_family_frame_number = render_view_frame_number;

    if (const ATerrainParentActor* root_actor = root.Get(); root_actor != nullptr) {
        if (const UTerrain* terrain_component = root_actor->spawned_terrain; terrain_component !=
            nullptr) {
            if (terrain_component->render_resources.IsValid()) {
                terrain_component->render_resources->reset_cpu_view_states();
            }
        }
    }

    if (ATerrainParentActor* root_actor = root.Get(); root_actor != nullptr) {
        prune_stale_render_views(render_views, *root_actor);
    } else {
        render_tile_trees.Reset();
        render_gpu_terrain_views.Reset();
    }

    for (const FTerrainViewSnapshot& render_view : render_views) { draw_terrain(gb, render_view); }

    if (const ATerrainParentActor* root_actor = root.Get(); root_actor != nullptr) {
        if (const UTerrain* terrain_component = root_actor->spawned_terrain; terrain_component !=
            nullptr) {
            if (terrain_component->render_resources.IsValid()) {
                terrain_component->render_resources->advance_view_states();
            }
        }
    }
}

void FTerrainSceneViewExtension::PreRenderViewFamily_RenderThread(
    FRDGBuilder& gb,
    FSceneViewFamily& in_view_family
) {
    // noop
}

void FTerrainSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& gb, FSceneView& in_view) {
    // noop
}

void FTerrainSceneViewExtension::PostRenderBasePassDeferred_RenderThread(
    FRDGBuilder& gb,
    FSceneView& in_view,
    const FRenderTargetBindingSlots& render_targets,
    TRDGUniformBufferRef<FSceneTextureUniformParameters> scene_textures
) {
    const ATerrainParentActor* root_actor = root.Get();
    if (!IsValid(root_actor) || !IsValid(root_actor->spawned_terrain)) { return; }

    const uint32 view_key = in_view.GetViewKey();
    if (!render_tile_trees.Contains(view_key)) { return; }

    // for (auto& [key, t] : render_tile_trees) {
    //     FTileTree::approximate_height_readback(gb, &t);
    // }

    TOptional<FGpuTerrainView>* gpu_terrain_view_opt = render_gpu_terrain_views.Find(view_key);
    if (gpu_terrain_view_opt == nullptr || !gpu_terrain_view_opt->IsSet()) { return; }

    FGpuTerrainView& gpu_terrain_view = gpu_terrain_view_opt->GetValue();
    if (gpu_terrain_view.picking_data_buffer == nullptr) { return; }
    if (in_view.CursorPos.X < 0 || in_view.CursorPos.Y < 0) { return; }

    const FViewInfo& view_info = static_cast<const FViewInfo&>(in_view);
    const FSceneTextures* scene_textures_ptr = view_info.GetSceneTexturesChecked();
    if (scene_textures_ptr == nullptr) { return; }

    FRDGTextureRef picking_depth_tex = scene_textures_ptr->Depth.Resolve;
    if (picking_depth_tex != nullptr) {
        if (const auto coppied_depth_tex = add_depth_copy_pass(gb, view_info, picking_depth_tex)) {
            picking_depth_tex = coppied_depth_tex;
        }
    }

    if (picking_depth_tex == nullptr) { return; }

    const FRDGTextureSRVRef picking_depth_texture_srv = gb.CreateSRV(picking_depth_tex);

    gpu_terrain_view.picking_parameters = build_picking_parameters(
        gb,
        in_view,
        gpu_terrain_view.picking_data_buffer,
        picking_depth_texture_srv,
        scene_textures_ptr->Stencil,
        picking_depth_tex->Desc.Extent
    );
    if (gpu_terrain_view.picking_parameters == nullptr) { return; }

    const FGlobalShaderMap* global_shader_map = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const TShaderMapRef<FTerrainPickingComputeShader> picking_cs(global_shader_map);

    FComputeShaderUtils::AddPass(
        gb,
        RDG_EVENT_NAME("UDLOD.Picking"),
        ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
        picking_cs,
        gpu_terrain_view.picking_parameters,
        FIntVector(1, 1, 1)
    );
}

FRDGTextureSRVRef FTerrainSceneViewExtension::CreateRDGTextureFromUTexture(
    FRDGBuilder& gb,
    UTexture* texture,
    const TCHAR* name
) {
    if (texture == nullptr || texture->GetResource() == nullptr) {
        return GetDefaultWhiteTexture(gb);
    }

    const FTextureResource* texture_resource = texture->GetResource();
    FRHITexture* rhi_texture = texture_resource->TextureRHI;
    return gb.CreateSRV(gb.RegisterExternalTexture(CreateRenderTarget(rhi_texture, name)));
}

FRDGTextureSRVRef FTerrainSceneViewExtension::GetDefaultWhiteTexture(FRDGBuilder& gb) {
    return gb.CreateSRV(GSystemTextures.GetWhiteDummy(gb));
}

void FTerrainSceneViewExtension::draw_terrain(
    FRDGBuilder& gb,
    const FTerrainViewSnapshot& view
) const {
    RDG_EVENT_SCOPE(gb, "UDLOD.DrawTerrain");

    ATerrainParentActor* root_actor = root.Get();
    if (!IsValid(root_actor)) { return; }
    if (!IsValid(root_actor->spawned_terrain)) { return; }

    FTileTree* tile_tree = find_or_add_tile_tree(*root_actor, view.view_key);
    auto* tile_atlas = root_actor->tile_atlas.GetPtrOrNull();
    if (tile_tree == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::draw_terrain] Tile tree is null.");
        }
        return;
    }

    if (tile_atlas == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::draw_terrain] Tile atlas is null");
        }
        return;
    }

    apply_runtime_debug_overrides(*root_actor, *tile_tree, *tile_atlas);

    if (!root_actor->debug_settings.bFreezeView) {
        tile_tree_compute_requests(
            tile_tree,
            root_actor->spawned_terrain->GetComponentTransform(),
            view.view_world_position,
            view.view_from_world,
            view.clip_from_view
        );
    }
    tile_atlas_update(tile_tree, tile_atlas);
    terrain::tile_loader::pump_tile_loads(tile_atlas);
    tile_tree_adjust_to_tile_atlas(tile_tree, tile_atlas);
    tile_tree_update_terrain_view_buffer(
        gb,
        tile_tree,
        pack_terrain_runtime_debug_flags(root_actor->debug_settings),
        static_cast<uint32>(root_actor->debug_settings.planar_gradient_mode)
    );

    if (!root_actor->spawned_terrain->IsValidLowLevel() || !root_actor->spawned_terrain->
        IsRegistered()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::draw_terrain] "
                "No registered terrain actors found for terrain rendering, skipping"
            );
        }
        return;
    }

    // Hot path:
    // 1. update requests
    // 2. kick/poll async CPU tile loads
    // 3. update tile-tree -> atlas lookup
    // 4. run the GPU UDLOD prepass and publish the indirect draw state
    // 5. optionally publish the CPU mesh-builder snapshot as a debug fallback
    draw_tile_tree(gb, view);

    if (should_use_cpu_mesh_builder() &&
        root_actor->spawned_terrain->render_resources.IsValid()) {
        root_actor->spawned_terrain->render_resources->update_cpu_view_state(
            view.view_key,
            build_cpu_mesh_view_state(*tile_tree, *tile_atlas, root_actor->spawned_terrain->config)
        );
    }
}

void FTerrainSceneViewExtension::draw_tile_tree(
    FRDGBuilder& gb,
    const FTerrainViewSnapshot& view
) const {
    RDG_EVENT_SCOPE(gb, "UDLOD.DrawTileTree");

    ATerrainParentActor* root_actor = root.Get();
    if (!IsValid(root_actor) || !IsValid(root_actor->spawned_terrain)) { return; }

    auto* tile_atlas = root_actor->tile_atlas.GetPtrOrNull();
    if (tile_atlas == nullptr) { return; }

    const FTileTree* tile_tree = render_tile_trees.Find(view.view_key);
    if (tile_tree == nullptr) { return; }

    auto* gpu_tile_atlas = &root_actor->gpu_tile_atlas;
    auto* gpu_terrain = &root_actor->gpu_terrain;
    TOptional<FGpuTerrainView>& gpu_terrain_view = render_gpu_terrain_views.
        FindOrAdd(view.view_key);
    const FTerrainSettings terrain_settings = root_actor->settings;

    FGpuTileAtlas::initialize(gb, *gpu_tile_atlas, *tile_atlas, terrain_settings);
    FGpuTileAtlas::extract(*tile_atlas, *gpu_tile_atlas);
    FGpuTerrain::initialize(gb, GetDefaultWhiteTexture(gb), *gpu_terrain, *gpu_tile_atlas);
    FGpuTerrainView::initialize(gb, gpu_terrain_view, tile_tree);

    FGpuTileAtlas::prepare(gb, *gpu_tile_atlas);
    FGpuTerrainView::prepare(gb, gpu_terrain_view, tile_tree);

    const auto* gsm = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const auto prep_root_cs = gsm->GetShader<FTerrainPrepassPrepareRootComputeShader>();
    const auto prep_next_cs = gsm->GetShader<FTerrainPrepassPrepareNextComputeShader>();
    const auto prep_render_cs = gsm->GetShader<FTerrainPrepassPrepareRenderComputeShader>();
    const auto refine_tiles_cs = gsm->GetShader<FTerrainPrepassRefineTilesComputeShader>();

    if (!gpu_terrain_view.IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU terrain view.");
        }
        return;
    }

    if (!gpu_terrain->IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU terrain.");
        }
        return;
    }

    if (!gpu_tile_atlas->IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU tile atlas.");
        }
        return;
    }

    const FGpuAttachment* height_attachment_ptr = nullptr;
    const FGpuAttachment* albedo_attachment_ptr = nullptr;
    for (const auto& [label, attachment] : (*gpu_tile_atlas)->attachments) {
        if (height_attachment_ptr == nullptr && label.Equals(
            TEXT("height"),
            ESearchCase::IgnoreCase)) { height_attachment_ptr = &attachment; }
        if (albedo_attachment_ptr == nullptr && label.Equals(
            TEXT("albedo"),
            ESearchCase::IgnoreCase)) { albedo_attachment_ptr = &attachment; }
    }

    if (height_attachment_ptr == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No height attachment found in GPU tile atlas"
            );
        }
        return;
    }

    if (height_attachment_ptr->atlas_texture == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Invalid height attachment view in GPU tile atlas"
            );
        }
        return;
    }

    if (albedo_attachment_ptr == nullptr || albedo_attachment_ptr->atlas_texture == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Missing albedo attachment; using height attachment fallback"
            );
        }
        albedo_attachment_ptr = height_attachment_ptr;
    }

    const Terrain terrain_uniform = terrain_comp(
        tile_atlas->lod_count,
        terrain::runtime::planar::scale(tile_atlas->side_length),
        tile_atlas->min_height,
        tile_atlas->max_height,
        tile_atlas->height_scale,
        root_actor->spawned_terrain->GetComponentTransform()
    );
    (*gpu_terrain)->terrain_buffer = TUniformBufferRef<Terrain>::CreateUniformBufferImmediate(
        terrain_uniform,
        UniformBuffer_MultiFrame
    );

    const FRDGBufferSRVRef tile_tree_buffer = gb.CreateSRV(tile_tree->tile_tree_buffer);
    const FRDGBufferSRVRef _ = gb.CreateSRV(tile_tree->approximate_height_buffer);
    const FRDGBufferUAVRef approximate_height_uav = gb.CreateUAV(
        tile_tree->approximate_height_buffer);

    gpu_terrain_view->prepass_parameters->terrain = (*gpu_terrain)->terrain_buffer;
    gpu_terrain_view->prepass_parameters->attachment_configs =
        (*gpu_terrain)->attachments_buffer_srv;
    gpu_terrain_view->prepass_parameters->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    gpu_terrain_view->prepass_parameters->height_attachment = height_attachment_ptr->
        atlas_texture;
    gpu_terrain_view->prepass_parameters->albedo_attachment = albedo_attachment_ptr->
        atlas_texture;
    gpu_terrain_view->prepass_parameters->approximate_height = approximate_height_uav;
    gpu_terrain_view->prepass_parameters->tile_tree = tile_tree_buffer;

    gpu_terrain_view->refine_tiles_parameters->terrain = (*gpu_terrain)->terrain_buffer;
    gpu_terrain_view->refine_tiles_parameters->attachment_configs =
        (*gpu_terrain)->attachments_buffer_srv;
    gpu_terrain_view->refine_tiles_parameters->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    gpu_terrain_view->refine_tiles_parameters->height_attachment = height_attachment_ptr->
        atlas_texture;
    gpu_terrain_view->refine_tiles_parameters->albedo_attachment = albedo_attachment_ptr->
        atlas_texture;
    gpu_terrain_view->refine_tiles_parameters->approximate_height = approximate_height_uav;
    gpu_terrain_view->refine_tiles_parameters->tile_tree = tile_tree_buffer;

    const FRDGBufferRef indirect_args_buffer = gpu_terrain_view->indirect_args_buffer;

    FComputeShaderUtils::AddPass(
        gb,
        RDG_EVENT_NAME("UDLOD.Prepass.PrepRoot"),
        ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
        prep_root_cs,
        gpu_terrain_view->prepass_parameters,
        FIntVector{1, 1, 1}
    );

    auto add_refine_tiles_pass = [&gb, refine_tiles_cs, indirect_args_buffer](
        RefineTiles* refine_tiles_parameters
    ) {
        FComputeShaderUtils::AddPass(
            gb,
            RDG_EVENT_NAME("UDLOD.Prepass.RefineTiles"),
            refine_tiles_cs,
            refine_tiles_parameters,
            indirect_args_buffer,
            0
        );
    };

    for (uint32 refinement_step = 0; refinement_step < gpu_terrain_view->refinement_count; ++
         refinement_step) {
        add_refine_tiles_pass(gpu_terrain_view->refine_tiles_parameters);

        FComputeShaderUtils::AddPass(
            gb,
            RDG_EVENT_NAME("UDLOD.Prepass.PrepNext"),
            ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
            prep_next_cs,
            gpu_terrain_view->prepass_parameters,
            FIntVector{1, 1, 1}
        );
    }

    add_refine_tiles_pass(gpu_terrain_view->refine_tiles_parameters);

    FComputeShaderUtils::AddPass(
        gb,
        RDG_EVENT_NAME("UDLOD.Prepass.PrepRender"),
        ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
        prep_render_cs,
        gpu_terrain_view->prepass_parameters,
        FIntVector{1, 1, 1}
    );

    enqueue_gpu_probe(
        gb,
        view,
        *tile_atlas,
        *tile_tree,
        gpu_terrain->GetValue(),
        gpu_terrain_view.GetValue(),
        *height_attachment_ptr,
        *albedo_attachment_ptr,
        tile_tree->geometry_tile_count
    );

    if (root_actor->spawned_terrain->render_resources.IsValid()) {
        root_actor->spawned_terrain->render_resources->stage_view_state(
            view.view_key,
            build_mesh_view_state(
                gb,
                view.view_key,
                *tile_tree,
                gpu_terrain->GetValue(),
                gpu_terrain_view.GetValue(),
                *height_attachment_ptr,
                *albedo_attachment_ptr
            )
        );
    }
}

FTileTree* FTerrainSceneViewExtension::find_or_add_tile_tree(
    const ATerrainParentActor& root_actor,
    const uint32 view_key
) const {
    if (FTileTree* existing_tile_tree = render_tile_trees.Find(view_key)) {
        return existing_tile_tree;
    }

    if (root_actor.view_component.IsSet()) {
        render_tile_trees.Add(view_key, root_actor.view_component.GetValue());
        return render_tile_trees.Find(view_key);
    }

    if (root_actor.terrain.IsSet()) {
        const auto& [terrain_config, terrain_view_config] = root_actor.terrain.GetValue();
        render_tile_trees.Add(view_key, FTileTree{terrain_config, terrain_view_config});
        return render_tile_trees.Find(view_key);
    }

    return nullptr;
}

void FTerrainSceneViewExtension::prune_stale_render_views(
    const TArray<FTerrainViewSnapshot>& active_views,
    ATerrainParentActor& root_actor
) const {
    TSet<uint32> active_view_keys;
    active_view_keys.Reserve(active_views.Num());
    for (const FTerrainViewSnapshot& active_view : active_views) {
        active_view_keys.Add(active_view.view_key);
    }

    if (root_actor.tile_atlas.IsSet()) {
        TArray<uint32> stale_tile_tree_keys;
        for (const TPair<uint32, FTileTree>& pair : render_tile_trees) {
            if (!active_view_keys.Contains(pair.Key)) { stale_tile_tree_keys.Add(pair.Key); }
        }

        for (const uint32 stale_view_key : stale_tile_tree_keys) {
            if (const FTileTree* stale_tile_tree = render_tile_trees.Find(stale_view_key)) {
                release_requested_tiles(root_actor.tile_atlas.GetValue(), *stale_tile_tree);
            }
            render_tile_trees.Remove(stale_view_key);
            render_gpu_terrain_views.Remove(stale_view_key);
        }
    } else {
        render_tile_trees.Reset();
        render_gpu_terrain_views.Reset();
    }
}

void FTerrainSceneViewExtension::release_requested_tiles(
    FTileAtlas& tile_atlas,
    const FTileTree& tile_tree
) {
    for (const FTileState& tile_state : tile_tree.tiles.get_storage()) {
        if (tile_state.request_state != ERequestState::Requested) { continue; }
        if (tile_state.coordinate == FTileCoordinate::INVALID()) { continue; }

        tile_atlas.release_tile(tile_state.coordinate);
    }
}

void FTerrainSceneViewExtension::process_gpu_probe_results() const {
    const bool should_log_probe_results = should_probe_gpu_path();

    for (int32 probe_index = pending_gpu_probes.Num() - 1; probe_index >= 0; --probe_index) {
        auto& [
                submission_id,
                view_key,
                atlas_owner_snapshot,
                indirect_args_readback,
                prepass_state_readback,
                final_tiles_readback,
                sample_probe_readback,
                final_tiles_sample_count
            ] =
            pending_gpu_probes[probe_index];
        if (!indirect_args_readback.IsValid() || !prepass_state_readback.IsValid() ||
            !sample_probe_readback.IsValid() ||
            (final_tiles_sample_count > 0u && !final_tiles_readback.IsValid())) {
            pending_gpu_probes.RemoveAtSwap(probe_index);
            continue;
        }

        if (!indirect_args_readback->IsReady() || !prepass_state_readback->IsReady() ||
            !sample_probe_readback->IsReady() ||
            (final_tiles_sample_count > 0u && !final_tiles_readback->IsReady())) { continue; }

        const uint32* indirect_args = static_cast<const uint32*>(
            indirect_args_readback->Lock(sizeof(uint32) * 4u)
        );
        const FPrepassStateProbe* prepass_state = static_cast<const FPrepassStateProbe*>(
            prepass_state_readback->Lock(sizeof(FPrepassStateProbe))
        );
        const GeometryTile* final_tiles = final_tiles_sample_count > 0u
            ? static_cast<const GeometryTile*>(
                final_tiles_readback->Lock(sizeof(GeometryTile) * final_tiles_sample_count))
            : nullptr;
        const FGpuSampleProbeReadback* sample_probe =
            static_cast<const FGpuSampleProbeReadback*>(
                sample_probe_readback->Lock(sizeof(FGpuSampleProbeReadback))
            );

        if (indirect_args != nullptr && prepass_state != nullptr && sample_probe != nullptr) {
            const bool sample_changed =
                !gpu_probe_has_last_sample ||
                gpu_probe_last_vertex_count != indirect_args[0] ||
                gpu_probe_last_instance_count != indirect_args[1] ||
                gpu_probe_last_start_vertex != indirect_args[2] ||
                gpu_probe_last_start_instance != indirect_args[3] ||
                gpu_probe_last_tile_count != prepass_state->tile_count ||
                gpu_probe_last_counter != prepass_state->counter ||
                gpu_probe_last_child_index != prepass_state->child_index ||
                gpu_probe_last_final_index != prepass_state->final_index;

            if (sample_changed && should_log_probe_results) {
                UE_LOGFMT(
                    LogTemp,
                    Display,
                    "[UDLOD.GpuProbe] submission={0} view={1} indirect={{vertex_count={2}, instance_count={3}, start_vertex={4}, start_instance={5}}} state={{tile_count={6}, counter={7}, child_index={8}, final_index={9}}}",
                    submission_id,
                    view_key,
                    indirect_args[0],
                    indirect_args[1],
                    indirect_args[2],
                    indirect_args[3],
                    prepass_state->tile_count,
                    prepass_state->counter,
                    prepass_state->child_index,
                    prepass_state->final_index
                );
                UE_LOG(
                    LogTemp,
                    Display,
                    TEXT(
                        "[UDLOD.GpuProbeSample] submission=%llu view=%u "
                        "sample={atlas_index=%u, geometry_face=%u, geometry_lod=%u, "
                        "geometry_xy=(%u,%u), sampled_lod=%u, sampled_xy=(%u,%u), "
                        "sampled_uv=(%.6f,%.6f), height=%.6f, approximate_height=%.6f, "
                        "normal=(%.6f,%.6f,%.6f), "
                        "base_world=(%.6f,%.6f,%.6f), displaced_world=(%.6f,%.6f,%.6f), "
                        "sample_height_range=(%.6f,%.6f), height_delta=%.6f, "
                        "albedo=(%.6f,%.6f,%.6f,%.6f)}"
                    ),
                    submission_id,
                    view_key,
                    sample_probe->ids0.X,
                    sample_probe->ids0.W,
                    sample_probe->ids0.Y,
                    sample_probe->coords0.X,
                    sample_probe->coords0.Y,
                    sample_probe->ids0.Z,
                    sample_probe->coords0.Z,
                    sample_probe->coords0.W,
                    sample_probe->scalars.X,
                    sample_probe->scalars.Y,
                    sample_probe->scalars.Z,
                    sample_probe->scalars.W,
                    sample_probe->extra.X,
                    sample_probe->extra.Y,
                    sample_probe->extra.Z,
                    sample_probe->base_world.X,
                    sample_probe->base_world.Y,
                    sample_probe->base_world.Z,
                    sample_probe->displaced_world.X,
                    sample_probe->displaced_world.Y,
                    sample_probe->displaced_world.Z,
                    sample_probe->base_world.W,
                    sample_probe->displaced_world.W,
                    sample_probe->extra.W,
                    sample_probe->albedo.X,
                    sample_probe->albedo.Y,
                    sample_probe->albedo.Z,
                    sample_probe->albedo.W
                );

                if (ATerrainParentActor* root_actor = root.Get(); IsValid(root_actor) &&
                    IsValid(root_actor->spawned_terrain) &&
                    root_actor->tile_atlas.IsSet()) {
                    if (const FTileTree* probe_tile_tree = render_tile_trees.Find(view_key)) {
                        log_gpu_probe_atlas_state(
                            submission_id,
                            view_key,
                            root_actor->tile_atlas.GetValue(),
                            *probe_tile_tree
                        );
                        log_gpu_probe_cpu_tile_tree_entry(
                            submission_id,
                            view_key,
                            *probe_tile_tree,
                            *sample_probe
                        );

                        const FTileCoordinate* atlas_owner_coordinate_ptr =
                            atlas_owner_snapshot.Find(sample_probe->ids0.X);
                        const TOptional<FTileCoordinate> atlas_owner_coordinate =
                            atlas_owner_coordinate_ptr != nullptr
                            ? TOptional{*atlas_owner_coordinate_ptr}
                            : NullOpt;
                        if (atlas_owner_coordinate.IsSet()) {
                            UE_LOGFMT(
                                LogTemp,
                                Display,
                                "[UDLOD.GpuProbeAtlasOwner] submission={0} view={1} atlas_index={2} owner={{coord={3}}}",
                                submission_id,
                                view_key,
                                sample_probe->ids0.X,
                                atlas_owner_coordinate->to_string()
                            );

                            log_gpu_probe_cpu_height_attachment(
                                submission_id,
                                view_key,
                                root_actor->tile_atlas.GetValue(),
                                atlas_owner_coordinate
                            );
                        } else {
                            UE_LOGFMT(
                                LogTemp,
                                Display,
                                "[UDLOD.GpuProbeAtlasOwner] submission={0} view={1} atlas_index={2} owner={{missing=true}}",
                                submission_id,
                                view_key,
                                sample_probe->ids0.X
                            );
                        }
                    }

                    if (!gpu_probe_logged_attachment_state) {
                        gpu_probe_logged_attachment_state = true;

                        const FTileAtlas& tile_atlas = root_actor->tile_atlas.GetValue();
                        const TOptional<FString> height_label = find_attachment_label(
                            tile_atlas,
                            TEXT("height")
                        );
                        const TOptional<FString> albedo_label = find_attachment_label(
                            tile_atlas,
                            TEXT("albedo")
                        );
                        const FAttachment* height_attachment = height_label.IsSet()
                            ? tile_atlas.attachments.Find(height_label.GetValue())
                            : nullptr;
                        const FAttachment* albedo_attachment = albedo_label.IsSet()
                            ? tile_atlas.attachments.Find(albedo_label.GetValue())
                            : nullptr;

                        UE_LOGFMT(
                            LogTemp,
                            Display,
                            "[UDLOD.GpuProbeConfig] height={{mask={0}, texture_size={1}, border_size={2}}} albedo={{present={3}, mask={4}, texture_size={5}, border_size={6}}}",
                            height_attachment != nullptr && height_attachment->mask,
                            height_attachment != nullptr ? height_attachment->texture_size : 0u,
                            height_attachment != nullptr ? height_attachment->border_size : 0u,
                            albedo_attachment != nullptr,
                            albedo_attachment != nullptr && albedo_attachment->mask,
                            albedo_attachment != nullptr ? albedo_attachment->texture_size : 0u,
                            albedo_attachment != nullptr ? albedo_attachment->border_size : 0u
                        );
                    }

                    if (
                        const FTileTree* probe_tile_tree =
                            render_tile_trees.Find(view_key); final_tiles != nullptr &&
                        prepass_state->final_index > 0 &&
                        probe_tile_tree != nullptr
                    ) {
                        log_gpu_probe_tile_details(
                            submission_id,
                            view_key,
                            static_cast<uint32>(prepass_state->final_index),
                            final_tiles,
                            final_tiles_sample_count,
                            *probe_tile_tree,
                            root_actor->spawned_terrain->config,
                            root_actor->spawned_terrain->GetComponentTransform()
                        );
                    }
                }

                gpu_probe_has_last_sample = true;
                gpu_probe_last_vertex_count = indirect_args[0];
                gpu_probe_last_instance_count = indirect_args[1];
                gpu_probe_last_start_vertex = indirect_args[2];
                gpu_probe_last_start_instance = indirect_args[3];
                gpu_probe_last_tile_count = prepass_state->tile_count;
                gpu_probe_last_counter = prepass_state->counter;
                gpu_probe_last_child_index = prepass_state->child_index;
                gpu_probe_last_final_index = prepass_state->final_index;
            } else if (!should_log_probe_results) { gpu_probe_has_last_sample = false; }
        }

        if (indirect_args != nullptr) { indirect_args_readback->Unlock(); }
        if (prepass_state != nullptr) { prepass_state_readback->Unlock(); }
        if (final_tiles != nullptr) { final_tiles_readback->Unlock(); }
        if (sample_probe != nullptr) { sample_probe_readback->Unlock(); }
        pending_gpu_probes.RemoveAtSwap(probe_index);
    }
}

void FTerrainSceneViewExtension::enqueue_gpu_probe(
    FRDGBuilder& gb,
    const FTerrainViewSnapshot& view,
    const FTileAtlas& tile_atlas,
    const FTileTree& tile_tree,
    const FGpuTerrain& gpu_terrain,
    const FGpuTerrainView& gpu_terrain_view,
    const FGpuAttachment& height_attachment,
    const FGpuAttachment& albedo_attachment,
    const uint32 geometry_tile_count
) const {
    if (!should_probe_gpu_path()) { return; }

    if (gpu_terrain_view.prepass_state_storage == nullptr ||
        gpu_terrain_view.indirect_args_buffer == nullptr ||
        gpu_terrain_view.final_tiles_buffer == nullptr) { return; }

    constexpr int32 max_pending_gpu_probes = 8;
    if (pending_gpu_probes.Num() >= max_pending_gpu_probes) { return; }

    auto& [
            submission_id,
            view_key,
            atlas_owner_snapshot,
            indirect_args_readback,
            prepass_state_readback,
            final_tiles_readback,
            sample_probe_readback,
            final_tiles_sample_count
        ] =
        pending_gpu_probes.Emplace_GetRef();
    submission_id = ++gpu_probe_submission_id;
    view_key = view.view_key;
    atlas_owner_snapshot = tile_atlas.atlas_owners;
    indirect_args_readback = MakeUnique<FRHIGPUBufferReadback>(
        TEXT("UDLOD.GpuProbe.IndirectArgs")
    );
    prepass_state_readback = MakeUnique<FRHIGPUBufferReadback>(
        TEXT("UDLOD.GpuProbe.PrepassState")
    );
    sample_probe_readback = MakeUnique<FRHIGPUBufferReadback>(
        TEXT("UDLOD.GpuProbe.Sample")
    );
    final_tiles_sample_count = FMath::Min(geometry_tile_count, GpuProbeTileSampleLimit);
    if (final_tiles_sample_count > 0u) {
        final_tiles_readback = MakeUnique<FRHIGPUBufferReadback>(
            TEXT("UDLOD.GpuProbe.FinalTiles")
        );
    }

    const FRDGBufferRef sample_probe_buffer = gb.CreateBuffer(
        FRDGBufferDesc::CreateStructuredDesc(sizeof(GpuSampleProbe), 1u),
        TEXT("UDLOD.GpuProbe.SampleBuffer")
    );
    const FRDGBufferUAVRef sample_probe_uav = gb.CreateUAV(sample_probe_buffer);

    FTerrainSampleProbeComputeShader::FParameters* sample_probe_parameters =
        gb.AllocParameters<FTerrainSampleProbeComputeShader::FParameters>();
    sample_probe_parameters->terrain = gpu_terrain.terrain_buffer;
    sample_probe_parameters->attachment_configs = gpu_terrain.attachments_buffer_srv;
    sample_probe_parameters->terrain_sampler = gpu_terrain.atlas_sampler;
    sample_probe_parameters->height_attachment = height_attachment.atlas_texture;
    sample_probe_parameters->albedo_attachment = albedo_attachment.atlas_texture;
    sample_probe_parameters->terrain_view = gpu_terrain_view.terrain_view_buffer;
    sample_probe_parameters->approximate_height = gb.CreateSRV(tile_tree.approximate_height_buffer);
    sample_probe_parameters->tile_tree = gb.CreateSRV(tile_tree.tile_tree_buffer);
    sample_probe_parameters->geometry_tiles = gpu_terrain_view.final_tiles_srv;
    sample_probe_parameters->output = sample_probe_uav;

    const auto* global_shader_map = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const auto sample_probe_cs = global_shader_map->GetShader<FTerrainSampleProbeComputeShader>();
    FComputeShaderUtils::AddPass(
        gb,
        RDG_EVENT_NAME("UDLOD.Probe.Sample"),
        ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
        sample_probe_cs,
        sample_probe_parameters,
        FIntVector(1, 1, 1)
    );

    AddEnqueueCopyPass(
        gb,
        indirect_args_readback.Get(),
        gpu_terrain_view.indirect_args_buffer,
        sizeof(uint32) * 4u
    );
    AddEnqueueCopyPass(
        gb,
        prepass_state_readback.Get(),
        gpu_terrain_view.prepass_state_storage,
        sizeof(FPrepassStateProbe)
    );
    if (final_tiles_sample_count > 0u) {
        AddEnqueueCopyPass(
            gb,
            final_tiles_readback.Get(),
            gpu_terrain_view.final_tiles_buffer,
            sizeof(GeometryTile) * final_tiles_sample_count
        );
    }
    AddEnqueueCopyPass(
        gb,
        sample_probe_readback.Get(),
        sample_probe_buffer,
        sizeof(FGpuSampleProbeReadback)
    );
}
