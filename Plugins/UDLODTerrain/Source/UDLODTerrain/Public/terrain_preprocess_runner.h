#pragma once

#include "preprocess_progress.h"
#include "preprocess_result.h"
#include "terrain_settings.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

namespace preprocess {
struct FPreprocessRunOptions {
    EPreprocessProgressMode progress_mode = EPreprocessProgressMode::None;
    float dialog_delay_seconds = 0.25f;
};

struct FPreprocessRunSummary {
    FDateTime started_at = FDateTime::MinValue();
    FTimespan duration{};
};

UDLODTERRAIN_API PreprocessResult<FPreprocessRunSummary> run_preprocess(
    const FTerrainPreprocessSettings& settings,
    const FPreprocessRunOptions& options = {}
);
} // namespace preprocess
