#pragma once

#include "AssetDefinition.h"
#include "SceneViewExtension.h"
#include "terrain_parent_actor.h"

class FTerrainSceneViewExtension final : public FWorldSceneViewExtension {
public:
    FTerrainSceneViewExtension(
        const FAutoRegister& auto_register,
        UWorld* in_world,
        ATerrainParentActor* in_root
    ) : FWorldSceneViewExtension{auto_register, in_world},
        root{in_root} {}

    virtual void BeginRenderViewFamily(FSceneViewFamily& in_view_family) override;
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
        const FViewInfo& view
    ) const;

    void draw_tile_tree(
        FRDGBuilder& gb,
        const FViewInfo& view
    ) const;

private:
    TWeakObjectPtr<ATerrainParentActor> root;
    mutable uint32 error_spam_buffer = 0;
    mutable bool stopped_error_spam = false;
    uint32 MAX_ERROR_SPAM_BUFFER = 100;
};
