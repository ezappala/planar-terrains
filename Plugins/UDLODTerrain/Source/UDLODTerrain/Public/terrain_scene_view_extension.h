#pragma once
#include "AssetDefinition.h"
#include "SceneViewExtension.h"
#include "terrain.h"
#include "terrain_config.h"
#include "terrain_tile_atlas.h"
#include "terrain_typedefs.h"
#include "terrain_view_config.h"

class FTerrainSceneViewExtension final : public FSceneViewExtensionBase {
public:
    explicit FTerrainSceneViewExtension(const FAutoRegister& auto_register) : FSceneViewExtensionBase
        {auto_register},
        heightmap_texture{nullptr},
        albedo_texture{nullptr}
        // terrains_spawned{false}
    {}

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

    using FTerrains = TTuple<FTerrainConfig, FTerrainViewConfig, FMaterialInstancePtr, FView>;

    static void spawn_terrains(FRDGBuilder& gb, FSceneInterface& scene);

    void draw_terrain(FRDGBuilder& gb, const FScene& scene, const FViewInfo& view) const;
    void draw_tile_tree(FRDGBuilder& gb, const FScene& scene, const FViewInfo& view) const;

private:
    mutable FRDGTextureRef heightmap_texture;
    mutable FRDGTextureRef albedo_texture;
    // bool terrains_spawned;

public:
    static TSharedPtr<FTerrainSceneViewExtension> instance;
};
