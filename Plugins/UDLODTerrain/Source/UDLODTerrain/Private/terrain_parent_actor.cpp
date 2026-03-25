#include "terrain_parent_actor.h"

#include "ext_logging.h"
#include "GPUTessellationComponent.h"
#include "terrain_preprocess_runner.h"
#include "terrain_tile_loader.h"
#include "terrain_world_subsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"

namespace {
TOptional<FTerrains> load_default_terrain_descriptor(
    const FTerrainPreprocessSettings& terrain_preprocess_settings
) {
    const FString config_path = FPaths::Combine(
        terrain_preprocess_settings.terrain_path,
        TEXT("config.json")
    );
    const TOptional<FTerrainConfig> config = FTerrainConfig::from_file(config_path);
    if (!config.IsSet()) {
        return NullOpt;
    }

    return FTerrains{
        config.GetValue(),
        FTerrainViewConfig{},
        nullptr,
        FView{}
    };
}

TOptional<FString> find_attachment_label(
    const FTerrainConfig& config,
    const FString& preferred_label
) {
    for (const auto& [label, attachment_config] : config.attachments) {
        if (label.Equals(preferred_label, ESearchCase::IgnoreCase)) { return label; }
    }

    return NullOpt;
}

TOptional<FTileCoordinate> find_root_tile(const FTerrainConfig& config) {
    TOptional<FTileCoordinate> root_tile = NullOpt;
    for (const FTileCoordinate& tile : config.tiles) {
        if (!root_tile.IsSet() ||
            tile.lod < root_tile->lod ||
            (tile.lod == root_tile->lod && tile.face < root_tile->face) ||
            (tile.lod == root_tile->lod && tile.face == root_tile->face && tile.xy.X < root_tile->xy
                .X) ||
            (tile.lod == root_tile->lod && tile.face == root_tile->face && tile.xy.X == root_tile->
                xy.X && tile.xy.Y < root_tile->xy.Y)) { root_tile = tile; }
    }

    return root_tile;
}

template <typename T>
void normalize_height_samples(
    const TArray<T>& source,
    const FTerrainConfig& config,
    TArray<uint8>& normalized_pixels
) {
    const float min_height = config.min_height;
    const float max_height = config.max_height;
    const float height_range = FMath::Max(max_height - min_height, 1.0f);

    normalized_pixels.SetNumUninitialized(source.Num());
    for (int32 index = 0; index < source.Num(); ++index) {
        const float normalized_height = FMath::Clamp(
            (static_cast<float>(source[index]) - min_height) / height_range,
            0.0f,
            1.0f
        );
        normalized_pixels[index] = static_cast<uint8>(
            FMath::RoundToInt(normalized_height * 255.0f));
    }
}

void normalize_height_samples(
    const TArray<TStaticArray<uint8, 4>>& source,
    const FTerrainConfig& config,
    TArray<uint8>& normalized_pixels
) {
    TArray<float> scalar_values;
    scalar_values.SetNumUninitialized(source.Num());
    for (int32 index = 0; index < source.Num(); ++index) {
        scalar_values[index] = static_cast<float>(source[index][0]);
    }

    normalize_height_samples(scalar_values, config, normalized_pixels);
}

void normalize_height_samples(
    const TArray<TStaticArray<uint16, 2>>& source,
    const FTerrainConfig& config,
    TArray<uint8>& normalized_pixels
) {
    TArray<float> scalar_values;
    scalar_values.SetNumUninitialized(source.Num());
    for (int32 index = 0; index < source.Num(); ++index) {
        scalar_values[index] = static_cast<float>(source[index][0]);
    }

    normalize_height_samples(scalar_values, config, normalized_pixels);
}

UTexture2D* create_fallback_height_texture(
    UObject* outer,
    const FTerrainConfig& config,
    const TOptional<FTileAtlas>& atlas
) {
    const TOptional<FString> height_label = find_attachment_label(config, TEXT("height"));
    if (!height_label.IsSet()) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] Missing height attachment for fallback terrain");
        return nullptr;
    }

    if (!atlas.IsSet()) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] Missing tile atlas for fallback terrain");
        return nullptr;
    }

    const FAttachment* attachment = atlas->attachments.Find(height_label.GetValue());
    if (attachment == nullptr) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] Missing height attachment data for fallback terrain");
        return nullptr;
    }

    const TOptional<FTileCoordinate> root_tile = find_root_tile(config);
    if (!root_tile.IsSet()) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] Missing tiles for fallback terrain");
        return nullptr;
    }

    const FAttachmentTile tile{
        root_tile.GetValue(),
        height_label.GetValue()
    };
    const FAttachmentTileData data =
        terrain::tile_loader::detail::load_tile_data(*attachment, tile);

    TArray<uint8> normalized_pixels;
    if (const auto* rgba_data = data.TryGet<TArray<TStaticArray<uint8,
        4>>>()) { normalize_height_samples(*rgba_data, config, normalized_pixels); } else if (const
        auto* r16u_data = data.TryGet<TArray<
            uint16>>()) { normalize_height_samples(*r16u_data, config, normalized_pixels); } else if
    (const auto* r16i_data = data.TryGet<TArray<
        int16>>()) { normalize_height_samples(*r16i_data, config, normalized_pixels); } else if (
        const auto* rg16u_data = data.TryGet<TArray<TStaticArray<uint16, 2>>>()) {
        normalize_height_samples(*rg16u_data, config, normalized_pixels);
    } else if (const auto* r32f_data = data.TryGet<TArray<float>>()) {
        normalize_height_samples(*r32f_data, config, normalized_pixels);
    } else {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] Unsupported height data for fallback terrain {Index}");
        return nullptr;
    }

    const int32 texture_size = static_cast<int32>(attachment->texture_size);
    const FName texture_name = MakeUniqueObjectName(
        outer,
        UTexture2D::StaticClass(),
        TEXT("TerrainFallbackHeight")
    );
    UTexture2D* const height_texture = UTexture2D::CreateTransient(
        texture_size,
        texture_size,
        PF_R8,
        texture_name,
        TConstArrayView64<uint8>(normalized_pixels.GetData(), normalized_pixels.Num())
    );
    if (height_texture == nullptr) { return nullptr; }

    height_texture->CompressionSettings = TC_Grayscale;
    height_texture->Filter = TF_Bilinear;
    height_texture->SRGB = false;
    height_texture->AddressX = TA_Clamp;
    height_texture->AddressY = TA_Clamp;
    height_texture->NeverStream = true;
#if WITH_EDITORONLY_DATA
    height_texture->MipGenSettings = TMGS_NoMipmaps;
#endif
    height_texture->UpdateResource();
    return height_texture;
}

UGPUTessellationComponent* spawn_tessellation_fallback(
    ATerrainParentActor& owner,
    const FTerrainConfig& config,
    const FView& view,
    UMaterialInterface* material,
    UTexture2D* height_texture
) {
    if (height_texture == nullptr || owner.root == nullptr) { return nullptr; }

    const FName component_name = MakeUniqueObjectName(
        &owner,
        UGPUTessellationComponent::StaticClass(),
        TEXT("TerrainFallback")
    );
    auto* fallback_component = NewObject<UGPUTessellationComponent>(
        &owner,
        component_name,
        RF_Transient
    );
    if (!IsValid(fallback_component)) { return nullptr; }

    fallback_component->bAutoUpdate = false;
    fallback_component->TessellationSettings.TessellationFactor = FMath::Clamp(
        owner.tessellation_fallback_factor,
        1,
        128
    );
    fallback_component->TessellationSettings.PlaneSizeX = static_cast<float>(config.side_length);
    fallback_component->TessellationSettings.PlaneSizeY = static_cast<float>(config.side_length);
    fallback_component->TessellationSettings.DisplacementIntensity = FMath::Max(
        config.max_height - config.min_height,
        1.0f
    );
    fallback_component->TessellationSettings.DisplacementOffset = config.min_height;
    fallback_component->TessellationSettings.bUseSineWaveDisplacement = false;
    fallback_component->TessellationSettings.LODMode = EGPUTessellationLODMode::Disabled;
    fallback_component->DisplacementTexture = height_texture;
    fallback_component->Material = material;
    fallback_component->AttachToComponent(
        owner.root,
        FAttachmentTransformRules::KeepRelativeTransform);
    fallback_component->SetRelativeTransform(view.tile_world_transform);
    fallback_component->RegisterComponent();
    fallback_component->UpdateBounds();
    fallback_component->UpdateTessellatedMesh();
    return fallback_component;
}
}

ATerrainParentActor::ATerrainParentActor() {
    root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(root);
    check(IsValid(root));
}

void ATerrainParentActor::OnConstruction(const FTransform& tf) {
    Super::OnConstruction(tf);
    UE_LOGFMT(LogTemp, Log, "Constructing terrain parent actor: {n}", GetName());
    rebuild_terrains();
}

void ATerrainParentActor::BeginPlay() {
    Super::BeginPlay();
    rebuild_terrains();
}

void ATerrainParentActor::PreprocessTerrain() {
    using ext::logging::log_time;
    const FDateTime start_preprocessing = FDateTime::UtcNow();
    UE_LOGFMT(LogTemp, Log, "{t}: Starting terrain preprocessing.", start_preprocessing.ToString());

    const auto result = run_preprocess(
        terrain_preprocess_settings,
        preprocess::FPreprocessRunOptions{
            preprocess::EPreprocessProgressMode::EditorDialog,
            0.25f
        });
    if (!result.has_value()) {
        UE_LOGFMT(LogTemp, Error, "Terrain preprocessing failed: {error}", result.error().ToString());
        return;
    }

    log_time(start_preprocessing, "Finished terrain preprocessing.");

    terrain = FTerrains{
        FTerrainConfig::from_file(
            FPaths::Combine(terrain_preprocess_settings.terrain_path, TEXT("config.json"))).
        Get(FTerrainConfig{}),
        FTerrainViewConfig{},
        nullptr,
        FView{}
    };

    rebuild_terrains();
}

void ATerrainParentActor::rebuild_terrains() {
    UE_LOGFMT(LogTemp, Log, "Rebuilding terrains for terrain parent actor: {n}", GetName());
    if (IsTemplate()) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "Skipping terrain rebuild for terrain parent actor template: {n}",
            GetName()
        );
        return;
    }

    const UWorld* world = GetWorld();
    if (world == nullptr) {
        UE_LOGFMT(LogTemp, Warning, "Failed to get world for terrain parent actor: {n}", GetName());
        return;
    }

    if (UTerrainWorldSubsystem* Subsystem = world->GetSubsystem<UTerrainWorldSubsystem>()) {
        Subsystem->set_terrain_root(this);
    }

    clear_spawned_terrains();

    if (!terrain.IsSet()) {
        terrain = load_default_terrain_descriptor(terrain_preprocess_settings);
    }

    UE_LOGFMT(LogTemp, Log, "Spawning terrain actors for terrain parent actor: {n}", GetName());
    const auto& [terrain_config, terrain_view_config, material_interface, view] = terrain.
        GetValue();
    UTerrain* new_terrain = NewObject<UTerrain>(this, TEXT("TerrainComp_0"));
    new_terrain->RegisterComponent();
    if (!new_terrain->AttachToComponent(root, FAttachmentTransformRules::KeepRelativeTransform)) {
        UE_LOGFMT(
            LogTemp,
            Error,
            "Failed to attach terrain component: {n} "
            "to root for terrain parent actor: {parent_n}",
            new_terrain->GetName(),
            GetName());
        new_terrain->DestroyComponent();
        return;
    }

    if (!IsValid(new_terrain)) {
        UE_LOGFMT(
            LogTemp,
            Error,
            "Failed to create terrain component for terrain parent actor: {n}",
            GetName());
        return;
    }

    new_terrain->set_object_data(terrain_config, settings, material_interface);
    new_terrain->UpdateBounds();
    new_terrain->MarkRenderStateDirty();

    spawned_terrain = new_terrain;
    material = material_interface;
    config = terrain_config;
    view_component = FTileTree{terrain_config, terrain_view_config};

    tile_atlas = new_terrain->atlas;
    if (bEnableTessellationFallback) {
        UTexture2D* const height_texture = create_fallback_height_texture(
            this,
            terrain_config,
            new_terrain->atlas);
        if (height_texture != nullptr) {
            fallback_height_texture = height_texture;

            if (UGPUTessellationComponent* const fallback_component =
                spawn_tessellation_fallback(
                    *this,
                    terrain_config,
                    view,
                    material,
                    height_texture
                )) { spawned_fallback_component = fallback_component; }
        }
    }
    UE_LOGFMT(
        LogTemp,
        Log,
        "Added tile atlas for terrain component: {n} "
        "in terrain parent actor: {parent_n}",
        new_terrain->GetName(),
        GetName());
    // }
}

void ATerrainParentActor::clear_spawned_terrains() {
    UE_LOGFMT(LogTemp, Log, "Clearing spawned terrains for terrain parent actor: {n}", GetName());
    if (IsValid(spawned_fallback_component)) {
        UE_LOGFMT(
            LogTemp,
            Log,
            "Destroying fallback component: {n} "
            "in terrain parent actor: {parent_n}",
            spawned_fallback_component->GetName(),
            GetName());
        spawned_fallback_component->DestroyComponent();
    } else if (spawned_fallback_component != nullptr) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "Invalid fallback component found while clearing spawned terrains for terrain parent actor: {n}",
            GetName());
    }

    if (spawned_terrain != nullptr) {
        if (IsValid(spawned_terrain)) {
            UE_LOGFMT(LogTemp, Log, "Destroying terrain actor: {n}", spawned_terrain->GetName());
            spawned_terrain->DestroyComponent();
        } else {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Invalid terrain actor found while clearing spawned terrains "
                "for terrain parent actor: {n}",
                GetName());
        }
    }

    // spawned_terrain_actors.Reset();
    spawned_terrain = nullptr;
    spawned_fallback_component = nullptr;
    fallback_height_texture = nullptr;
    material = nullptr;
    config.Reset();
    view_component.Reset();
    tile_atlas.Reset();
    gpu_tile_atlas.Reset();
    gpu_terrain.Reset();
    gpu_terrain_view.Reset();
}
