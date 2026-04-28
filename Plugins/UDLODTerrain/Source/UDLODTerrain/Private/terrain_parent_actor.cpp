#include "terrain_parent_actor.h"

#include "ext_logging.h"
#include "SceneInterface.h"
#include "terrain_debug_bridge.h"
#include "terrain_preprocess_runner.h"
#include "terrain_tile_loader.h"
#include "terrain_world_subsystem.h"
#include "DebugViewer/Public/DebugViewer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"
#include "spdlog/fmt/bundled/core.h"
#include "Toolkits/AssetEditorToolkit.h"

namespace terrain {
FTerrainDebugSettings make_default_debug_settings() {
    return FTerrainDebugSettings{};
}

bool has_pending_load_flags(const UObject* object) {
    return IsValid(object) && object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad);
}

TOptional<FTerrainConfig> load_terrain_config(
    const FFilePath& terrain_config
) {
    const TOptional<FTerrainConfig> config = FTerrainConfig::from_file(terrain_config.FilePath);
    return config;
}

TOptional<FTerrainConfig> load_terrain_config(const FDirectoryPath& terrain_config_path) {
    const auto path = FFilePath{FPaths::Combine(terrain_config_path.Path, TEXT("config.json"))};
    return load_terrain_config(path);
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
            FMath::RoundToInt(normalized_height * static_cast<float>(UINT8_MAX)));
    }
}

template <>
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

template <>
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
        tile_loader::detail::load_tile_data(*attachment, tile);

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
}

ATerrainParentActor::ATerrainParentActor() {
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;

    root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(root);
    check(IsValid(root));
    bEnableAutoLODGeneration = false;
    SetActorEnableCollision(false);
}

void ATerrainParentActor::PostLoad() {
    Super::PostLoad();
    root = Cast<USceneComponent>(GetRootComponent());
    migrate_deprecated_runtime_settings();
    ensure_runtime_terrain_config_loaded();
}

void ATerrainParentActor::PostRegisterAllComponents() {
    Super::PostRegisterAllComponents();
    ensure_runtime_terrain_config_loaded();
    VerifyInitializationState(false);
}

void ATerrainParentActor::OnConstruction(const FTransform& tf) {
    Super::OnConstruction(tf);
    UE_LOGFMT(LogTemp, Log, "Constructing terrain parent actor: {n}", GetName());
    VerifyInitializationState(true);
}

void ATerrainParentActor::Tick(const float delta_time) {
    Super::Tick(delta_time);

    // const auto* world = GetWorld();
    // if (world == nullptr) {
    //     return;
    // }

    // const auto c = GetActorTransform().GetLocation();
    // DrawDebugSphere(world, c, view_component->load_distance, 256, FColor::Magenta);
    // DrawDebugSphere(world, c, view_component->blend_distance, 256, FColor::Green);
    // DrawDebugSphere(world, c, view_component->morph_distance, 256, FColor::Cyan);
}

void ATerrainParentActor::BeginPlay() {
    Super::BeginPlay();
    VerifyInitializationState(false);
}

#if WITH_EDITOR
void ATerrainParentActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) {
    Super::PostEditChangeProperty(PropertyChangedEvent);
    if (PropertyChangedEvent.Property == nullptr && PropertyChangedEvent.MemberProperty == nullptr) {
        return;
    }

    const FName property_name = PropertyChangedEvent.Property != nullptr
        ? PropertyChangedEvent.Property->GetFName()
        : NAME_None;
    const FName member_property_name = PropertyChangedEvent.MemberProperty != nullptr
        ? PropertyChangedEvent.MemberProperty->GetFName()
        : NAME_None;

    if (property_name == GET_MEMBER_NAME_CHECKED(ATerrainParentActor, runtime_settings) ||
        member_property_name == GET_MEMBER_NAME_CHECKED(ATerrainParentActor, runtime_settings)) {
        if (property_name == GET_MEMBER_NAME_CHECKED(ATerrainParentActor, runtime_settings) ||
            property_name == GET_MEMBER_NAME_CHECKED(FFilePath, FilePath) ||
            property_name == GET_MEMBER_NAME_CHECKED(FTerrainActorSettings, terrain_config_path)) {
            ensure_runtime_terrain_config_loaded(true);
        }
        invalidate_runtime_settings(true);
        VerifyInitializationState(true);
        return;
    }

    if (
        property_name == GET_MEMBER_NAME_CHECKED(ATerrainParentActor, debug_settings) ||
        member_property_name == GET_MEMBER_NAME_CHECKED(ATerrainParentActor, debug_settings)
    ) {
        notify_runtime_debug_controls_changed();
    }
}
#endif

void ATerrainParentActor::ReloadTerrainConfig() {
    ensure_runtime_terrain_config_loaded(true);
    invalidate_runtime_settings(true);
    VerifyInitializationState(true);
}

// Opens a dialog to display debug information about all the members of this actor.
void ATerrainParentActor::ViewComponentDebugData() {
    TERRAIN_DEBUG_WINDOW_REQUESTED.Broadcast(this);
}

void ATerrainParentActor::ViewComponentDebugDraw() const {
    if (IsValid(spawned_terrain)) { spawned_terrain->bDrawDebug = !spawned_terrain->bDrawDebug; }
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
        UE_LOGFMT(
            LogTemp,
            Error,
            "Terrain preprocessing failed: {error}",
            result.error().ToString());
        return;
    }

    log_time(start_preprocessing, "Finished terrain preprocessing.");

    runtime_settings.terrain_config_path.FilePath = FPaths::Combine(
        terrain_preprocess_settings.terrain_path.Path,
        TEXT("config.json")
    );
    ensure_runtime_terrain_config_loaded(true);
    invalidate_runtime_settings(true);
    VerifyInitializationState(true);
}

void ATerrainParentActor::VerifyInitializationState(const bool bForceRebuild) {
    if (IsTemplate() || HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad)) { return; }

    const UWorld* world = GetWorld();
    if (world == nullptr) { return; }

    if (!IsValid(root)) { root = Cast<USceneComponent>(GetRootComponent()); }

    if (has_pending_runtime_load()) { return; }

    if (UTerrainWorldSubsystem* subsystem = world->GetSubsystem<UTerrainWorldSubsystem>()) {
        subsystem->set_terrain_root(this, auto_spawned_by_world_subsystem);
    }

    ensure_runtime_terrain_config_loaded();

    if (bForceRebuild || needs_runtime_rebuild()) {
        rebuild_terrains();
        return;
    }

    sync_runtime_state_from_spawned_terrain();
}

uint64 ATerrainParentActor::GetRuntimeSettingsRevision() const {
    return runtime_settings_revision;
}

void ATerrainParentActor::ResetRuntimeDebugControls() {
    debug_settings = terrain::make_default_debug_settings();
    runtime_debug_controls_initialized = false;
    seed_runtime_debug_controls(true);
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::CyclePlanarGradientMode() {
    const uint8 next_value = (static_cast<uint8>(debug_settings.planar_gradient_mode) + 1u) % 4u;
    debug_settings.planar_gradient_mode = static_cast<ETerrainPlanarGradientMode>(next_value);
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::IncreaseRuntimeHeightScale() {
    seed_runtime_debug_controls(false);
    debug_settings.height_scale += 0.1f;
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::DecreaseRuntimeHeightScale() {
    seed_runtime_debug_controls(false);
    debug_settings.height_scale = FMath::Max(0.0f, debug_settings.height_scale - 0.1f);
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::IncreaseRuntimeBlendLoadDistance() {
    seed_runtime_debug_controls(false);
    const double step = 0.25 * get_debug_face_size();
    debug_settings.blend_distance += step;
    debug_settings.load_distance += step;
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::DecreaseRuntimeBlendLoadDistance() {
    seed_runtime_debug_controls(false);
    const double step = 0.25 * get_debug_face_size();
    debug_settings.blend_distance = FMath::Max(step, debug_settings.blend_distance - step);
    debug_settings.load_distance = FMath::Max(step, debug_settings.load_distance - step);
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::IncreaseRuntimeMorphDistance() {
    seed_runtime_debug_controls(false);
    const double step = get_debug_face_size();
    debug_settings.morph_distance += step;
    debug_settings.subdivision_distance += step;
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::DecreaseRuntimeMorphDistance() {
    seed_runtime_debug_controls(false);
    const double step = get_debug_face_size();
    debug_settings.morph_distance = FMath::Max(step, debug_settings.morph_distance - step);
    debug_settings.subdivision_distance = FMath::Max(
        step,
        debug_settings.subdivision_distance - step
    );
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::IncreaseRuntimeGridSize() {
    seed_runtime_debug_controls(false);
    debug_settings.grid_size += 2;
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::DecreaseRuntimeGridSize() {
    seed_runtime_debug_controls(false);
    debug_settings.grid_size = FMath::Max(2, debug_settings.grid_size - 2);
    notify_runtime_debug_controls_changed();
}

void ATerrainParentActor::RespawnFromSelectedBlueprint() {
    if (replacement_actor_class == nullptr) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] No replacement actor class is selected for terrain parent actor: {n}",
            GetName()
        );
        return;
    }

    const UWorld* world = GetWorld();
    if (world == nullptr) { return; }

    if (UTerrainWorldSubsystem* subsystem = world->GetSubsystem<UTerrainWorldSubsystem>()) {
        subsystem->respawn_terrain_root(this, replacement_actor_class, false);
    }
}

void ATerrainParentActor::RespawnAsAutoSpawnedInstance() {
    const UWorld* world = GetWorld();
    if (world == nullptr) { return; }

    if (UTerrainWorldSubsystem* subsystem = world->GetSubsystem<UTerrainWorldSubsystem>()) {
        subsystem->respawn_terrain_root(this, StaticClass(), true);
    }
}

void ATerrainParentActor::set_auto_spawned_by_world_subsystem(const bool bInAutoSpawned) {
    auto_spawned_by_world_subsystem = bInAutoSpawned;
}

bool ATerrainParentActor::is_auto_spawned_by_world_subsystem() const {
    return auto_spawned_by_world_subsystem;
}

bool ATerrainParentActor::has_pending_runtime_load() const {
    if (terrain::has_pending_load_flags(this) || terrain::has_pending_load_flags(root)) {
        return true;
    }

    if (spawned_terrain != nullptr && terrain::has_pending_load_flags(spawned_terrain)) {
        return true;
    }

    if (spawned_fallback_component != nullptr &&
        terrain::has_pending_load_flags(spawned_fallback_component)) { return true; }

    return false;
}

bool ATerrainParentActor::ensure_runtime_terrain_config_loaded(const bool bForceReload) {
    if (!bForceReload && runtime_settings.has_loaded_terrain_config()) { return true; }

    const TOptional<FTerrainConfig> loaded_config = terrain::load_terrain_config(
        runtime_settings.terrain_config_path);
    if (!loaded_config.IsSet()) {
        if (bForceReload) { runtime_settings.terrain_config = FTerrainConfig{}; }
        return false;
    }

    runtime_settings.terrain_config = loaded_config.GetValue();
    return true;
}

void ATerrainParentActor::migrate_deprecated_runtime_settings() {
    const FTerrainActorSettings default_runtime_settings;

    if (!terrain_config_path_DEPRECATED.FilePath.IsEmpty() &&
        runtime_settings.terrain_config_path.FilePath ==
        default_runtime_settings.terrain_config_path.FilePath) {
        runtime_settings.terrain_config_path = terrain_config_path_DEPRECATED;
    }

    if (terrain_DEPRECATED.IsSet()) {
        if (!runtime_settings.has_loaded_terrain_config() &&
            (terrain_DEPRECATED->terrain_config.path.IsEmpty() == false ||
                !terrain_DEPRECATED->terrain_config.attachments.IsEmpty() ||
                !terrain_DEPRECATED->terrain_config.tiles.IsEmpty())) {
            runtime_settings.terrain_config = terrain_DEPRECATED->terrain_config;
        }
        if (runtime_settings.terrain_view_config ==
            default_runtime_settings.terrain_view_config) {
            runtime_settings.terrain_view_config = terrain_DEPRECATED->terrain_view_config;
        }
    }

    if (runtime_settings.render_settings == default_runtime_settings.render_settings &&
        settings_DEPRECATED != FTerrainSettings{}) {
        runtime_settings.render_settings = settings_DEPRECATED;
    }

    if (runtime_settings.material == nullptr && material_DEPRECATED != nullptr) {
        runtime_settings.material = material_DEPRECATED;
    }

    terrain_DEPRECATED.Reset();
    material_DEPRECATED = nullptr;
    settings_DEPRECATED = FTerrainSettings{};
    terrain_config_path_DEPRECATED = FFilePath{};
}

void ATerrainParentActor::invalidate_runtime_settings(const bool bResetDebugControls) {
    ++runtime_settings_revision;
    if (bResetDebugControls) { runtime_debug_controls_initialized = false; }

    gpu_tile_atlas.Reset();
    gpu_terrain.Reset();
    gpu_terrain_view.Reset();

    if (IsValid(spawned_terrain) && spawned_terrain->render_resources.IsValid()) {
        spawned_terrain->render_resources->reset_view_states();
        spawned_terrain->render_resources->reset_cpu_view_states();
    }
}

bool ATerrainParentActor::needs_runtime_rebuild() const {
    if (!runtime_settings.has_loaded_terrain_config()) { return false; }

    if (applied_runtime_settings_revision != runtime_settings_revision) { return true; }

    if (!IsValid(spawned_terrain)) { return true; }

    if (!config.IsSet() || !view_component.IsSet() || !tile_atlas.IsSet()) { return true; }

    if (!spawned_terrain->IsRegistered()) { return true; }

    return spawned_terrain->GetAttachParent() != root;
}

void ATerrainParentActor::sync_runtime_state_from_spawned_terrain() {
    if (!IsValid(spawned_terrain) || !runtime_settings.has_loaded_terrain_config()) { return; }

    config = runtime_settings.terrain_config;
    if (!tile_atlas.IsSet()) { tile_atlas = spawned_terrain->atlas; }
    if (!view_component.IsSet()) {
        view_component = FTileTree{
            runtime_settings.terrain_config,
            runtime_settings.terrain_view_config
        };
    }

    seed_runtime_debug_controls(false);
    apply_runtime_debug_controls();
    spawned_terrain->UpdateBounds();
    spawned_terrain->MarkRenderStateDirty();
}

void ATerrainParentActor::seed_runtime_debug_controls(const bool bForceReset) {
    if ((!view_component.IsSet() || !tile_atlas.IsSet()) &&
        !runtime_settings.has_loaded_terrain_config()) { return; }
    if (!bForceReset && runtime_debug_controls_initialized) { return; }

    const FTerrainViewConfig& base_view_config = runtime_settings.terrain_view_config;

    if (tile_atlas.IsSet()) {
        debug_settings.height_scale = tile_atlas->height_scale;
    } else if (spawned_terrain != nullptr && spawned_terrain->atlas.IsSet()) {
        debug_settings.height_scale = spawned_terrain->atlas->height_scale;
    }

    if (view_component.IsSet()) {
        debug_settings.blend_distance = view_component->blend_distance;
        debug_settings.load_distance = view_component->load_distance;
        debug_settings.morph_distance = view_component->morph_distance;
        debug_settings.subdivision_distance = view_component->subdivision_distance;
        debug_settings.grid_size = static_cast<int32>(view_component->grid_size);
    } else if (runtime_settings.has_loaded_terrain_config()) {
        const double face_size = runtime_settings.terrain_config.face_size;
        debug_settings.blend_distance = base_view_config.blend_distance * face_size;
        debug_settings.load_distance = base_view_config.blend_distance *
            face_size *
            (1.0 + base_view_config.load_tolerance);
        debug_settings.morph_distance = base_view_config.morph_distance * face_size;
        debug_settings.subdivision_distance = base_view_config.morph_distance *
            face_size *
            (1.0 + base_view_config.subdivision_tolerance);
        debug_settings.grid_size = base_view_config.grid_size;
    }

    runtime_debug_controls_initialized = true;
}

void ATerrainParentActor::apply_runtime_debug_controls() {
    seed_runtime_debug_controls(false);

    if (tile_atlas.IsSet()) {
        tile_atlas->height_scale = debug_settings.height_scale;
    }
    if (spawned_terrain != nullptr) {
        spawned_terrain->bDebugWireframe = debug_settings.bWireframe;
        if (spawned_terrain->atlas.IsSet()) {
            spawned_terrain->atlas->height_scale = debug_settings.height_scale;
        }
    }
    if (view_component.IsSet()) {
        view_component->blend_distance = FMath::Max(1.0, debug_settings.blend_distance);
        view_component->load_distance = FMath::Max(1.0, debug_settings.load_distance);
        view_component->morph_distance = FMath::Max(1.0, debug_settings.morph_distance);
        view_component->subdivision_distance = FMath::Max(
            1.0,
            debug_settings.subdivision_distance
        );
        view_component->grid_size = static_cast<uint32>(FMath::Max(2, debug_settings.grid_size));
    }
}

void ATerrainParentActor::notify_runtime_debug_controls_changed() {
    apply_runtime_debug_controls();

    if (!IsValid(spawned_terrain)) { return; }

    spawned_terrain->UpdateBounds();
    spawned_terrain->MarkRenderStateDirty();
}

double ATerrainParentActor::get_debug_face_size() const {
    if (config.IsSet()) { return config->face_size; }
    if (runtime_settings.has_loaded_terrain_config()) {
        return runtime_settings.terrain_config.face_size;
    }
    return 1.0;
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
        Subsystem->set_terrain_root(this, auto_spawned_by_world_subsystem);
    }

    clear_spawned_terrains();

    ensure_runtime_terrain_config_loaded();

    if (!runtime_settings.has_loaded_terrain_config()) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] No terrain descriptor could be resolved for terrain parent actor: {n}",
            GetName()
        );
        return;
    }

    UE_LOGFMT(LogTemp, Log, "Spawning terrain actors for terrain parent actor: {n}", GetName());
    const FTerrainConfig& terrain_config = runtime_settings.terrain_config;
    const FTerrainViewConfig& terrain_view_config = runtime_settings.terrain_view_config;
    // If the terrain comp already exists...
    auto* terrain_comp = GetComponentByClass<UTerrain>();
    if (terrain_comp == nullptr) {
        terrain_comp = NewObject<UTerrain>(this, TEXT("TerrainComp_0"));
        terrain_comp->RegisterComponent();
        if (!terrain_comp->AttachToComponent(root, FAttachmentTransformRules::KeepRelativeTransform)) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Failed to attach terrain component: {n} "
                "to root for terrain parent actor: {parent_n}",
                terrain_comp->GetName(),
                GetName());
            terrain_comp->DestroyComponent();
            return;
        }

        if (!IsValid(terrain_comp)) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Failed to create terrain component for terrain parent actor: {n}",
                GetName());
            return;
        }
    }

    terrain_comp->set_object_data(
        terrain_config,
        runtime_settings.render_settings,
        runtime_settings.material
    );
    terrain_comp->UpdateBounds();
    terrain_comp->MarkRenderStateDirty();

    spawned_terrain = terrain_comp;
    config = terrain_config;
    view_component = FTileTree{terrain_config, terrain_view_config};

    tile_atlas = terrain_comp->atlas;
    seed_runtime_debug_controls(false);
    apply_runtime_debug_controls();
    applied_runtime_settings_revision = runtime_settings_revision;
    if (enable_tessellation_fallback) {
        UTexture2D* const height_texture = terrain::create_fallback_height_texture(
            this,
            terrain_config,
            terrain_comp->atlas);
        if (height_texture != nullptr) {
            fallback_height_texture = height_texture;
        }
    }
    UE_LOGFMT(
        LogTemp,
        Log,
        "Added tile atlas for terrain component: {n} "
        "in terrain parent actor: {parent_n}",
        terrain_comp->GetName(),
        GetName()
    );
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
    config.Reset();
    view_component.Reset();
    tile_atlas.Reset();
    gpu_tile_atlas.Reset();
    gpu_terrain.Reset();
    gpu_terrain_view.Reset();
    applied_runtime_settings_revision = 0;
}
