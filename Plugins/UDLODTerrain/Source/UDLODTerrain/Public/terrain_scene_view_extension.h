#pragma once

#include "AssetDefinition.h"
#include "SceneViewExtension.h"
#include "terrain_parent_actor.h"

class FTerrainSceneViewExtension final : public FWorldSceneViewExtension {
    struct FTerrainViewSnapshot {
        uint32 view_key = 0;
        FVector3d view_world_position = FVector3d::ZeroVector;
        FMatrix44d view_from_world = FMatrix44d::Identity;
        FMatrix44d clip_from_view = FMatrix44d::Identity;
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

    static FRDGTextureSRVRef CreateRDGTextureFromUTexture(
        FRDGBuilder& gb,
        UTexture* texture,
        const TCHAR* name
    );

    static FRDGTextureSRVRef CreateUTextureFromFilePath(
        FRDGBuilder& gb,
        const FString& path,
        const TCHAR* name
    );

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
    TWeakObjectPtr<ATerrainParentActor> root;
    mutable FRWLock cached_views_guard;
    TArray<FTerrainViewSnapshot> cached_views;
    mutable uint32 error_spam_buffer = 0;
    mutable bool stopped_error_spam = false;
    uint32 MAX_ERROR_SPAM_BUFFER = 100;
};
