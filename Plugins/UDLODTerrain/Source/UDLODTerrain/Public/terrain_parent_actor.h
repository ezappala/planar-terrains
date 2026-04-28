#pragma once
#include "gpu_terrain.h"
#include "gpu_terrain_atlas_buffer_info.h"
#include "terrain_config.h"
#include "terrain_debug_settings.h"
// #include "terrain_picking.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "terrain_tile_tree.h"
#include "terrain_view_config.h"
#include "GameFramework/Actor.h"
#include "Templates/SubclassOf.h"

#include "terrain_parent_actor.generated.h"

struct FTerrains;
class UTexture2D;
class UMaterialInterface;

namespace terrain {
bool has_pending_load_flags(const UObject* object);

TOptional<FTerrainConfig> load_terrain_config(const FFilePath& terrain_config);

TOptional<FTerrainConfig> load_terrain_config(const FDirectoryPath& terrain_config_path);

TOptional<FString> find_attachment_label(
    const FTerrainConfig& config,
    const FString& preferred_label);

TOptional<FTileCoordinate> find_root_tile(const FTerrainConfig& config);

template <typename T>
void normalize_height_samples(
    const TArray<T>& source,
    const FTerrainConfig& config,
    TArray<uint8>& normalized_pixels);

template <>
void normalize_height_samples(
    const TArray<TStaticArray<uint8, 4>>& source,
    const FTerrainConfig& config,
    TArray<uint8>& normalized_pixels);

template <>
void normalize_height_samples(
    const TArray<TStaticArray<uint16, 2>>& source,
    const FTerrainConfig& config,
    TArray<uint8>& normalized_pixels);

UTexture2D* create_fallback_height_texture(
    UObject* outer,
    const FTerrainConfig& config,
    const TOptional<FTileAtlas>& atlas);
}

USTRUCT(BlueprintType)
struct FTerrains {
    GENERATED_BODY()

    FTerrainConfig terrain_config;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDLOD|Runtime")
    FTerrainViewConfig terrain_view_config;

    FString to_string() const {
        return FString::Printf(
            TEXT("terrain_config: {%s}, terrain_view_config: {%s}"),
            *terrain_config.to_string(),
            *terrain_view_config.to_string());
    }
};

USTRUCT(BlueprintType)
struct FTerrainActorSettings {
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "UDLOD|Runtime",
        DisplayName="Terrain Config Path",
        meta=(FilePathFilter = "json"))
    FFilePath terrain_config_path{
        FPaths::ProjectContentDir() / TEXT("terrains/earth/config.json")
    };

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "UDLOD|Runtime",
        DisplayName="Loaded Terrain Config")
    FTerrainConfig terrain_config;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "UDLOD|Runtime",
        DisplayName="View")
    FTerrainViewConfig terrain_view_config;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "UDLOD|Runtime",
        DisplayName="Render")
    FTerrainSettings render_settings;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "UDLOD|Runtime",
        DisplayName="Material")
    UMaterialInterface* material = nullptr;

    bool has_loaded_terrain_config() const {
        return !terrain_config.path.IsEmpty() ||
            !terrain_config.attachments.IsEmpty() ||
            !terrain_config.tiles.IsEmpty();
    }

    FString to_string() const {
        return FString::Printf(
            TEXT("config_path=%s, terrain_config={%s}, terrain_view_config={%s}, render_settings={%s}"),
            *terrain_config_path.FilePath,
            *terrain_config.to_string(),
            *terrain_view_config.to_string(),
            *render_settings.ToString());
    }
};

UCLASS(
    Blueprintable,
    ClassGroup = (Rendering),
    hidecategories = (Object, LOD, Physics, Collision))
class UDLODTERRAIN_API ATerrainParentActor : public AActor {
    GENERATED_BODY()

public:
    ATerrainParentActor();
    virtual void PostLoad() override;
    virtual void PostRegisterAllComponents() override;
    virtual void OnConstruction(const FTransform& tf) override;
    virtual void Tick(float delta_time) override;
    virtual void BeginPlay() override;

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    USceneComponent* root;

    UFUNCTION(
        CallInEditor,
        Category="UDLOD|Editor",
        DisplayName="Manually Reload Terrain Config",
        meta=(ToolTip=
            "Manually reloads the terrain configuration from the specified path. This can be used to refresh the terrain data after making changes to the config file without restarting the editor."
        ))
    void ReloadTerrainConfig();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Editor",
        DisplayName="View Component Debug Data",
        meta=(ToolTip=
            "Opens a dialog to display debug information about all the members of this actor."
        )
    )
    void ViewComponentDebugData();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Editor",
        DisplayName="Toggle Component Debug Draw",
        meta=(ToolTip=
            "Toggles debug drawing for the spawned terrain component, if it exists. This can be used to visualize the terrain tiles and LOD levels in the editor."
        ))
    void ViewComponentDebugDraw() const;

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Editor",
        DisplayName="Preprocess Terrain",
        meta=(ToolTip=
            "Preprocesses the terrain data using the specified preprocess settings. This will generate the necessary heightmap and albedo attachments, as well as the terrain configuration file, based on the provided source data paths and settings."
        ))
    void PreprocessTerrain();

    UFUNCTION(BlueprintCallable, Category = "UDLOD")
    void VerifyInitializationState(bool bForceRebuild = false);

    uint64 GetRuntimeSettingsRevision() const;

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Reset Runtime Debug Controls"
    )
    void ResetRuntimeDebugControls();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Cycle Planar Gradient Mode"
    )
    void CyclePlanarGradientMode();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Increase Height Scale"
    )
    void IncreaseRuntimeHeightScale();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Decrease Height Scale"
    )
    void DecreaseRuntimeHeightScale();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Increase Blend/Load Distance"
    )
    void IncreaseRuntimeBlendLoadDistance();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Decrease Blend/Load Distance"
    )
    void DecreaseRuntimeBlendLoadDistance();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Increase Morph/Subdivision Distance"
    )
    void IncreaseRuntimeMorphDistance();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Decrease Morph/Subdivision Distance"
    )
    void DecreaseRuntimeMorphDistance();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Increase Grid Size"
    )
    void IncreaseRuntimeGridSize();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Debug",
        DisplayName="Decrease Grid Size"
    )
    void DecreaseRuntimeGridSize();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Editor",
        DisplayName="Respawn From Selected Blueprint")
    void RespawnFromSelectedBlueprint();

    UFUNCTION(
        CallInEditor,
        Category = "UDLOD|Editor",
        DisplayName="Respawn Native Auto Terrain Parent")
    void RespawnAsAutoSpawnedInstance();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDLOD")
    FTerrainPreprocessSettings terrain_preprocess_settings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDLOD")
    FTerrainActorSettings runtime_settings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDLOD|Editor")
    TSubclassOf<ATerrainParentActor> replacement_actor_class;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug")
    FTerrainDebugSettings debug_settings;

    UPROPERTY()
    TOptional<FTerrains> terrain_DEPRECATED;

    UPROPERTY()
    UMaterialInterface* material_DEPRECATED = nullptr;

    UPROPERTY()
    FTerrainSettings settings_DEPRECATED;

    UPROPERTY()
    FFilePath terrain_config_path_DEPRECATED;

    TOptional<FTerrainConfig> config = NullOpt;
    TOptional<FTileTree> view_component = NullOpt;
    TOptional<FTileAtlas> tile_atlas = NullOpt;
    TOptional<FGpuTileAtlas> gpu_tile_atlas = NullOpt;
    TOptional<FGpuTerrain> gpu_terrain = NullOpt;
    TOptional<FGpuTerrainView> gpu_terrain_view = NullOpt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD|Fallback")
    bool enable_tessellation_fallback = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="UDLOD|Fallback",
        meta=(ClampMin="1", ClampMax="128", EditCondition="enable_tessellation_fallback",
            EditConditionHides))
    int32 tessellation_fallback_factor = 32;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    UTerrain* spawned_terrain;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    USceneComponent* spawned_fallback_component;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    UTexture2D* fallback_height_texture;

private:
    UPROPERTY(VisibleInstanceOnly, Transient, Category = "UDLOD|Editor")
    bool auto_spawned_by_world_subsystem = false;

    UPROPERTY(Transient)
    bool runtime_debug_controls_initialized = false;

    uint64 runtime_settings_revision = 1;
    uint64 applied_runtime_settings_revision = 0;

public:
    void set_auto_spawned_by_world_subsystem(bool bInAutoSpawned);
    bool is_auto_spawned_by_world_subsystem() const;

private:
    bool has_pending_runtime_load() const;
    bool needs_runtime_rebuild() const;
    bool ensure_runtime_terrain_config_loaded(bool bForceReload = false);
    void migrate_deprecated_runtime_settings();
    void invalidate_runtime_settings(bool bResetDebugControls = true);
    void sync_runtime_state_from_spawned_terrain();
    void seed_runtime_debug_controls(bool bForceReset = false);
    void apply_runtime_debug_controls();
    void notify_runtime_debug_controls_changed();
    double get_debug_face_size() const;
    void rebuild_terrains();
    void clear_spawned_terrains();
};
