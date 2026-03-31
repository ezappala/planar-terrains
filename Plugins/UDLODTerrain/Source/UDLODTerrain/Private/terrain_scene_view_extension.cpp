#include "terrain_scene_view_extension.h"

#include "ImageUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "SystemTextures.h"
#include "terrain_funcs.h"
#include "terrain_render_state.h"
#include "terrain_shaders.h"
#include "terrain_tile_loader.h"
#include "terrain_world_subsystem.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
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

struct FPrepassStateProbe {
    uint32 tile_count = 0;
    int32 counter = 0;
    int32 child_index = 0;
    int32 final_index = 0;
};

static_assert(sizeof(FPrepassStateProbe) == sizeof(uint32) * 4u);

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
            return FCpuAttachmentLookup{
                sampled_coordinate,
                attachment,
                data
            };
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
    const float sample_y = static_cast<float>(border_size) + clamped_v * static_cast<float>(
        center_size);

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
    new_cached_views.Reserve(in_view_family.Views.Num());
    for (const FSceneView* view : in_view_family.Views) {
        if (view == nullptr) {
            continue;
        }

        FTerrainViewSnapshot snapshot;
        snapshot.view_key = view->GetViewKey();
        snapshot.view_world_position = FVector3d(view->ViewMatrices.GetViewOrigin());
        snapshot.view_from_world = FMatrix44d(view->ViewMatrices.GetViewMatrix());
        snapshot.clip_from_view = FMatrix44d(view->ViewMatrices.GetProjectionNoAAMatrix());
        new_cached_views.Add(snapshot);
    }

    FWriteScopeLock _(cached_views_guard);
    cached_views = MoveTemp(new_cached_views);
}

void FTerrainSceneViewExtension::PreInitViews_RenderThread(FRDGBuilder& gb) {
    process_gpu_probe_results();

    TArray<FTerrainViewSnapshot> render_views;
    {
        FReadScopeLock _(cached_views_guard);
        render_views = cached_views;
    }

    if (const ATerrainParentActor* root_actor = root.Get()) {
        if (const UTerrain* terrain_component = root_actor->spawned_terrain) {
            if (terrain_component->render_resources.IsValid()) {
                terrain_component->render_resources->advance_view_states();
                terrain_component->render_resources->reset_cpu_view_states();
            }
        }
    }

    for (const FTerrainViewSnapshot& render_view : render_views) {
        draw_terrain(gb, render_view);
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

FRDGTextureSRVRef FTerrainSceneViewExtension::CreateUTextureFromFilePath(
    FRDGBuilder& gb,
    const FString& path,
    const TCHAR* name
) {
    UTexture2D* tex = FImageUtils::ImportFileAsTexture2D(path);
    if (tex == nullptr) {
        return GetDefaultWhiteTexture(gb);
    }
    return CreateRDGTextureFromUTexture(gb, tex, name);
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

    auto* tile_tree = root_actor->view_component.GetPtrOrNull();
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

    tile_tree_compute_requests(
        tile_tree,
        root_actor->spawned_terrain->GetComponentTransform(),
        view.view_world_position,
        view.view_from_world,
        view.clip_from_view
    );
    tile_atlas_update(tile_tree, tile_atlas);
    terrain::tile_loader::pump_tile_loads(tile_atlas);
    tile_tree_adjust_to_tile_atlas(tile_tree, tile_atlas);
    tile_tree_update_terrain_view_buffer(gb, tile_tree);

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
    if (!IsValid(root_actor) || !IsValid(root_actor->spawned_terrain)) {
        return;
    }

    auto* tile_atlas = root_actor->tile_atlas.GetPtrOrNull();
    if (tile_atlas == nullptr) {
        return;
    }

    const auto* tile_tree = root_actor->view_component.GetPtrOrNull();
    if (tile_tree == nullptr) { return; }

    auto* gpu_tile_atlas = &root_actor->gpu_tile_atlas;
    auto* gpu_terrain = &root_actor->gpu_terrain;
    auto* gpu_terrain_view = &root_actor->gpu_terrain_view;
    const FTerrainSettings terrain_settings = root_actor->settings;

    FGpuTileAtlas::initialize(gb, *gpu_tile_atlas, *tile_atlas, terrain_settings);
    FGpuTileAtlas::extract(*tile_atlas, *gpu_tile_atlas);
    FGpuTerrain::initialize(gb, GetDefaultWhiteTexture(gb), *gpu_terrain, *gpu_tile_atlas);
    FGpuTerrainView::initialize(gb, *gpu_terrain_view, tile_tree);

    FGpuTileAtlas::prepare(gb, *gpu_tile_atlas);
    FGpuTerrainView::prepare(gb, *gpu_terrain_view, tile_tree);

    const auto* gsm = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const auto prep_root_cs = gsm->GetShader<FTerrainPrepassPrepareRootComputeShader>();
    const auto prep_next_cs = gsm->GetShader<FTerrainPrepassPrepareNextComputeShader>();
    const auto prep_render_cs = gsm->GetShader<FTerrainPrepassPrepareRenderComputeShader>();
    const auto refine_tiles_cs = gsm->GetShader<FTerrainPrepassRefineTilesComputeShader>();

    if (!gpu_terrain_view->IsSet()) {
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

    const Terrain terrain_uniform = new_terrain(
        tile_atlas->lod_count,
        ext::math::scale(tile_atlas->side_length),
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

    (*gpu_terrain_view)->prepass_parameters->terrain = (*gpu_terrain)->terrain_buffer;
    (*gpu_terrain_view)->prepass_parameters->attachment_configs =
        (*gpu_terrain)->attachments_buffer_srv;
    (*gpu_terrain_view)->prepass_parameters->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    (*gpu_terrain_view)->prepass_parameters->height_attachment = height_attachment_ptr->
        atlas_texture;
    (*gpu_terrain_view)->prepass_parameters->albedo_attachment = albedo_attachment_ptr->
        atlas_texture;
    (*gpu_terrain_view)->prepass_parameters->approximate_height = approximate_height_uav;
    (*gpu_terrain_view)->prepass_parameters->tile_tree = tile_tree_buffer;

    (*gpu_terrain_view)->refine_tiles_parameters->terrain = (*gpu_terrain)->terrain_buffer;
    (*gpu_terrain_view)->refine_tiles_parameters->attachment_configs =
        (*gpu_terrain)->attachments_buffer_srv;
    (*gpu_terrain_view)->refine_tiles_parameters->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    (*gpu_terrain_view)->refine_tiles_parameters->height_attachment = height_attachment_ptr->
        atlas_texture;
    (*gpu_terrain_view)->refine_tiles_parameters->albedo_attachment = albedo_attachment_ptr->
        atlas_texture;
    (*gpu_terrain_view)->refine_tiles_parameters->approximate_height = approximate_height_uav;
    (*gpu_terrain_view)->refine_tiles_parameters->tile_tree = tile_tree_buffer;

    const FRDGBufferRef indirect_args_buffer = (*gpu_terrain_view)->indirect_args_buffer;

    FComputeShaderUtils::AddPass(
        gb,
        RDG_EVENT_NAME("UDLOD.Prepass.PrepRoot"),
        ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
        prep_root_cs,
        (*gpu_terrain_view)->prepass_parameters,
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

    for (uint32 refinement_step = 0; refinement_step < (*gpu_terrain_view)->refinement_count; ++
         refinement_step) {
        add_refine_tiles_pass((*gpu_terrain_view)->refine_tiles_parameters);

        FComputeShaderUtils::AddPass(
            gb,
            RDG_EVENT_NAME("UDLOD.Prepass.PrepNext"),
            ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
            prep_next_cs,
            (*gpu_terrain_view)->prepass_parameters,
            FIntVector{1, 1, 1}
        );
    }

    add_refine_tiles_pass((*gpu_terrain_view)->refine_tiles_parameters);

    FComputeShaderUtils::AddPass(
        gb,
        RDG_EVENT_NAME("UDLOD.Prepass.PrepRender"),
        ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
        prep_render_cs,
        (*gpu_terrain_view)->prepass_parameters,
        FIntVector{1, 1, 1}
    );

    enqueue_gpu_probe(gb, view, gpu_terrain_view->GetValue());

    if (root_actor->spawned_terrain->render_resources.IsValid()) {
        root_actor->spawned_terrain->render_resources->stage_view_state(
            view.view_key,
            build_mesh_view_state(
                gb,
                view.view_key,
                *tile_tree,
                gpu_terrain->GetValue(),
                gpu_terrain_view->GetValue(),
                *height_attachment_ptr,
                *albedo_attachment_ptr
            )
        );
    }
}

void FTerrainSceneViewExtension::process_gpu_probe_results() const {
    const bool should_log_probe_results = should_probe_gpu_path();

    for (int32 probe_index = pending_gpu_probes.Num() - 1; probe_index >= 0; --probe_index) {
        auto& [submission_id, view_key, indirect_args_readback, prepass_state_readback] =
            pending_gpu_probes[probe_index];
        if (!indirect_args_readback.IsValid() || !prepass_state_readback.IsValid()) {
            pending_gpu_probes.RemoveAtSwap(probe_index);
            continue;
        }

        if (!indirect_args_readback->IsReady() || !prepass_state_readback->IsReady()) { continue; }

        const uint32* indirect_args = static_cast<const uint32*>(
            indirect_args_readback->Lock(sizeof(uint32) * 4u)
        );
        const FPrepassStateProbe* prepass_state = static_cast<const FPrepassStateProbe*>(
            prepass_state_readback->Lock(sizeof(FPrepassStateProbe))
        );

        if (indirect_args != nullptr && prepass_state != nullptr) {
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
        pending_gpu_probes.RemoveAtSwap(probe_index);
    }
}

void FTerrainSceneViewExtension::enqueue_gpu_probe(
    FRDGBuilder& gb,
    const FTerrainViewSnapshot& view,
    const FGpuTerrainView& gpu_terrain_view
) const {
    if (!should_probe_gpu_path()) { return; }

    if (gpu_terrain_view.prepass_state_storage == nullptr ||
        gpu_terrain_view.indirect_args_buffer == nullptr) { return; }

    constexpr int32 max_pending_gpu_probes = 8;
    if (pending_gpu_probes.Num() >= max_pending_gpu_probes) { return; }

    auto& [submission_id, view_key, indirect_args_readback, prepass_state_readback] =
        pending_gpu_probes.Emplace_GetRef();
    submission_id = ++gpu_probe_submission_id;
    view_key = view.view_key;
    indirect_args_readback = MakeUnique<FRHIGPUBufferReadback>(
        TEXT("UDLOD.GpuProbe.IndirectArgs")
    );
    prepass_state_readback = MakeUnique<FRHIGPUBufferReadback>(
        TEXT("UDLOD.GpuProbe.PrepassState")
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
}
