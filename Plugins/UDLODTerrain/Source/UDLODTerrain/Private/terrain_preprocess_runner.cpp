#include "terrain_preprocess_runner.h"

#include "preprocess_exec.h"

#define LOCTEXT_NAMESPACE "UDLODTerrainPreprocessRunner"

namespace preprocess {
PreprocessResult<FPreprocessRunSummary> run_preprocess(
    const FTerrainPreprocessSettings& settings,
    const FPreprocessRunOptions& options
) {
    FTerrainPreprocessSettings normalized_settings = settings;
    if (!normalized_settings.heightmap_lod_count.IsSet()) {
        normalized_settings.heightmap_lod_count = normalized_settings.use_albedo
            ? normalized_settings.albedo_lod_count.Get(7)
            : 7;
    }
    if (normalized_settings.use_albedo && !normalized_settings.albedo_lod_count.IsSet()) {
        normalized_settings.albedo_lod_count = normalized_settings.heightmap_lod_count.Get(7);
    }

    const FDateTime started_at = FDateTime::UtcNow();
    const auto progress = create_progress(options.progress_mode);
    const bool bPreprocessAlbedo = normalized_settings.use_albedo;
    FScopedPreprocessProgress _(
        progress.Get(),
        bPreprocessAlbedo ? 3.0f : 2.0f,
        LOCTEXT("PreprocessTerrain", "Preprocessing terrain"));

    progress->make_dialog_delayed(options.dialog_delay_seconds);

    progress->enter_progress_frame(1.0f, LOCTEXT("Initialize", "Initializing preprocessing"));
    auto initialize_result = initialize(normalized_settings);
    if (!initialize_result.has_value()) {
        return std::unexpected{initialize_result.error()};
    }

    auto& initialize_value = initialize_result.value();
    auto& heightmap_init = initialize_value.Get<0>();
    auto& albedo_init = initialize_value.Get<1>();
    auto& heightmap_dataset = heightmap_init.Get<0>();
    auto& heightmap_context = heightmap_init.Get<1>();
    auto& albedo_dataset = albedo_init.Get<0>();
    auto& albedo_context = albedo_init.Get<1>();

    progress->enter_progress_frame(1.0f, LOCTEXT("Heightmap", "Preprocessing heightmap"));
    const auto heightmap_result = preprocess(
        MoveTemp(heightmap_dataset),
        heightmap_context,
        progress.Get());
    if (!heightmap_result.has_value()) {
        return std::unexpected{heightmap_result.error()};
    }

    if (bPreprocessAlbedo) {
        progress->enter_progress_frame(1.0f, LOCTEXT("Albedo", "Preprocessing albedo"));
        const auto albedo_result = preprocess(
            MoveTemp(albedo_dataset),
            albedo_context,
            progress.Get());
        if (!albedo_result.has_value()) { return std::unexpected{albedo_result.error()}; }
    }

    return FPreprocessRunSummary{
        started_at,
        FDateTime::UtcNow() - started_at
    };
}
} // namespace preprocess

#undef LOCTEXT_NAMESPACE
