#include "mat.h"

#include <MaterialCompiler.h>

#if WITH_EDITOR
int32 UMaterialExpressionTerrainInitOutputs::Compile(FMaterialCompiler* compiler, int32 pinIdx) {
    int32 codeID;
    if (pinIdx ==
        0) { codeID = InitialBinaryState.IsConnected() ? InitialBinaryState.Compile(compiler) : INDEX_NONE; } else if (
        pinIdx == 1) {
        codeID = InitialContinuousState.IsConnected() ? InitialContinuousState.Compile(compiler) : INDEX_NONE;
    } else { codeID = INDEX_NONE; }
    return compiler->CustomOutput(this, pinIdx, codeID);
}

int32 UMaterialExpressionTerrain1Outputs::Compile(FMaterialCompiler* compiler, int32 pinIdx) {
    int32 codeID;
    auto doPin = [&](int32 i, FExpressionInput& pin, float* fallback) {
        if (pinIdx != i) { return false; }

        if (pin.IsConnected()) { codeID = pin.Compile(compiler); } else if (fallback) {
            codeID = compiler->Constant(*fallback);
        } else { codeID = INDEX_NONE; }
        return true;
    };

    if (!doPin(0, ThresholdTooFew, &ThresholdTooFewConst) &&
        !doPin(1, ThresholdResurrect, &ThresholdResurrectConst) &&
        !doPin(2, ThresholdTooMany, &ThresholdTooManyConst)) { codeID = INDEX_NONE; }
    return compiler->CustomOutput(this, pinIdx, codeID);
}

int32 UMaterialExpressionTerrain2Outputs::Compile(FMaterialCompiler* compiler, int32 pinIdx) {
    int32 codeID;
    auto doPin = [&](int32 i, FExpressionInput& pin, float* fallback) {
        if (pinIdx != i) { return false; }

        if (pin.IsConnected()) { codeID = pin.Compile(compiler); } else if (fallback) {
            codeID = compiler->Constant(*fallback);
        } else { codeID = INDEX_NONE; }
        return true;
    };

    if (!doPin(0, ContinuousValue, nullptr)) { codeID = INDEX_NONE; }
    return compiler->CustomOutput(this, pinIdx, codeID);
}

int32 UMaterialExpressionTerrainMeshOutputs::Compile(FMaterialCompiler* compiler, int32 pinIdx) {
    int32 codeID;
    auto doPin = [&](int32 i, FExpressionInput& pin, float* fallback) {
        if (pinIdx != i) { return false; }

        if (pin.IsConnected()) { codeID = pin.Compile(compiler); } else if (fallback) {
            codeID = compiler->Constant(*fallback);
        } else { codeID = INDEX_NONE; }
        return true;
    };

    if (!doPin(0, DiscreteOutput, &DiscreteOutputConst) &&
        !doPin(1, ContinuousOutput, &ContinuousOutputConst) &&
        !doPin(2, OutputAlpha, &OutputAlphaConst)) { codeID = INDEX_NONE; }
    return compiler->CustomOutput(this, pinIdx, codeID);
}
#endif
