#pragma once

#include "AssetDefinition.h"
#include "RHIGPUReadback.h"
#include "SceneViewExtension.h"
#include "terrain_parent_actor.h"

struct FGpuTerrainView;
struct FGpuTerrain;
struct FGpuAttachment;

class FTerrainSceneViewExtension final : public FWorldSceneViewExtension {
    struct FTerrainViewSnapshot {
        uint32 view_key = 0;
        FVector3d view_world_position = FVector3d::ZeroVector;
        FMatrix44d view_from_world = FMatrix44d::Identity;
        FMatrix44d clip_from_view = FMatrix44d::Identity;
        int32 view_area = 0;
    };

    struct FPendingGpuProbe {
        uint64 submission_id = 0;
        uint32 view_key = 0;
        TMap<uint32, FTileCoordinate> atlas_owner_snapshot;
        TUniquePtr<FRHIGPUBufferReadback> indirect_args_readback;
        TUniquePtr<FRHIGPUBufferReadback> prepass_state_readback;
        TUniquePtr<FRHIGPUBufferReadback> final_tiles_readback;
        TUniquePtr<FRHIGPUBufferReadback> sample_probe_readback;
        uint32 final_tiles_sample_count = 0;
    };

public:
    FTerrainSceneViewExtension(
        const FAutoRegister& auto_register,
        UWorld* in_world,
        ATerrainParentActor* in_root
    ) : FWorldSceneViewExtension{auto_register, in_world},
        root{in_root} {}

    virtual void BeginRenderViewFamily(FSceneViewFamily& in_view_family) override;
    virtual void PreInitViews_RenderThread(FRDGBuilder& gb) override;
    virtual void PreRenderViewFamily_RenderThread(
        FRDGBuilder& gb,
        FSceneViewFamily& in_view_family
    ) override;
    virtual void PreRenderView_RenderThread(FRDGBuilder& gb, FSceneView& in_view) override;
    virtual void PostRenderBasePassDeferred_RenderThread(
        FRDGBuilder& gb,
        FSceneView& in_view,
        const FRenderTargetBindingSlots& render_targets,
        TRDGUniformBufferRef<FSceneTextureUniformParameters> scene_textures
    ) override;

    static FRDGTextureSRVRef CreateRDGTextureFromUTexture(
        FRDGBuilder& gb,
        UTexture* texture,
        const TCHAR* name
    );

    // static FRDGTextureSRVRef CreateUTextureFromFilePath(
    //     FRDGBuilder& gb,
    //     const FString& path,
    //     const TCHAR* name
    // );

    static FRDGTextureSRVRef GetDefaultWhiteTexture(FRDGBuilder& gb);

    void draw_terrain(
        FRDGBuilder& gb,
        const FTerrainViewSnapshot& view
    ) const;

    void draw_tile_tree(
        FRDGBuilder& gb,
        const FTerrainViewSnapshot& view
    ) const;

private:
    FTileTree* find_or_add_tile_tree(
        const ATerrainParentActor& root_actor,
        uint32 view_key
    ) const;
    void prune_stale_render_views(
        const TArray<FTerrainViewSnapshot>& active_views,
        ATerrainParentActor& root_actor
    ) const;
    static void release_requested_tiles(FTileAtlas& tile_atlas, const FTileTree& tile_tree);
    void process_gpu_probe_results() const;
    void enqueue_gpu_probe(
        FRDGBuilder& gb,
        const FTerrainViewSnapshot& view,
        const FTileAtlas& tile_atlas,
        const FTileTree& tile_tree,
        const FGpuTerrain& gpu_terrain,
        const FGpuTerrainView& gpu_terrain_view,
        const FGpuAttachment& height_attachment,
        const FGpuAttachment& albedo_attachment,
        uint32 geometry_tile_count
    ) const;

    TWeakObjectPtr<ATerrainParentActor> root;
    mutable FRWLock cached_views_guard;
    TArray<FTerrainViewSnapshot> cached_views;
    mutable TMap<uint32, FTileTree> render_tile_trees;
    mutable TMap<uint32, TOptional<FGpuTerrainView>> render_gpu_terrain_views;
    mutable TArray<FPendingGpuProbe> pending_gpu_probes;
    mutable uint32 error_spam_buffer = 0;
    mutable bool stopped_error_spam = false;
    mutable bool logged_multiple_cached_views = false;
    mutable uint64 gpu_probe_submission_id = 0;
    mutable bool gpu_probe_has_last_sample = false;
    mutable uint32 gpu_probe_last_vertex_count = 0;
    mutable uint32 gpu_probe_last_instance_count = 0;
    mutable uint32 gpu_probe_last_start_vertex = 0;
    mutable uint32 gpu_probe_last_start_instance = 0;
    mutable uint32 gpu_probe_last_tile_count = 0;
    mutable int32 gpu_probe_last_counter = 0;
    mutable int32 gpu_probe_last_child_index = 0;
    mutable int32 gpu_probe_last_final_index = 0;
    mutable bool gpu_probe_logged_attachment_state = false;
    mutable uint32 cached_view_family_frame_number = MAX_uint32;
    mutable int32 cached_view_family_area = INDEX_NONE;
    mutable uint32 processed_view_family_frame_number = MAX_uint32;
    uint32 MAX_ERROR_SPAM_BUFFER = 100;
};
