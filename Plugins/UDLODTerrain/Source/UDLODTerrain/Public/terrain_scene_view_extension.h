#pragma once
#include "AssetDefinition.h"
#include "SceneViewExtension.h"
#include "terrain_parent_actor.h"
#include "terrain_world_subsystem.h"

class FTerrainSceneViewExtension final : public FWorldSceneViewExtension {
public:
    FTerrainSceneViewExtension(
        const FAutoRegister& auto_register,
        UWorld* in_world,
        ATerrainParentActor* in_root
    ) : FWorldSceneViewExtension{auto_register, in_world},
        // root{in_world->GetSubsystemChecked<UTerrainWorldSubsystem>()->get_terrain_root_checked()} {}
        root{in_root} {}

    TNotNull<ATerrainParentActor*> root;

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

    static FRDGTextureSRVRef CreateUTextureFromFilePath(
        FRDGBuilder& gb,
        const FString& path,
        const TCHAR* name
    );

    static FRDGTextureSRVRef GetDefaultWhiteTexture(FRDGBuilder& gb);

    void draw_terrain(
        FRDGBuilder& gb,
        const FViewInfo& view,
        const FRenderTargetBindingSlots& render_targets
    ) const;

    void draw_tile_tree(
        FRDGBuilder& gb,
        const FViewInfo& view,
        const FRenderTargetBindingSlots& render_targets
    ) const;

private:
    mutable uint32 error_spam_buffer = 0;
    mutable bool stopped_error_spam = false;
    uint32 MAX_ERROR_SPAM_BUFFER = 100;
};
