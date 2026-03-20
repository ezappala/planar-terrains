#pragma once
#include "AssetDefinition.h"
#include "SceneViewExtension.h"
#include "terrain.h"
#include "terrain_config.h"
#include "terrain_view_config.h"
#include "terrain_tile_atlas.h"
#include "terrain_typedefs.h"

class FTerrainSceneViewExtension final : public FSceneViewExtensionBase {
public:
    explicit FTerrainSceneViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase
        {AutoRegister},
        heightmap_texture{nullptr},
        albedo_texture{nullptr},
        terrains_spawned{false} {}

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

    static void Init(
        const FString& in_heightmap_src,
        const FString& in_albedo_src,
        const TSharedPtr<FPrimaryTerrainSettings>& in_terrain_settings
    ) {
        if (instance == nullptr) {
            instance = FSceneViewExtensions::NewExtension<FTerrainSceneViewExtension>();
        }
        instance->settings = in_terrain_settings;
        instance->heightmap_src = in_heightmap_src;
        instance->albedo_src = in_albedo_src;
    }

private:
    TSharedPtr<FPrimaryTerrainSettings> settings;
    FString heightmap_src;
    FString albedo_src;

    mutable FRDGTextureRef heightmap_texture;
    mutable FRDGTextureRef albedo_texture;
    bool terrains_spawned;

public:
    static TSharedPtr<FTerrainSceneViewExtension> instance;
};
