#pragma once
#include "SceneViewExtension.h"

class UDLODTERRAIN_API FPreprocessSceneView final : public FSceneViewExtensionBase {
public:
    FPreprocessSceneView(const FAutoRegister& auto_register);

    virtual void SetupViewFamily(FSceneViewFamily& in_view_family) override;
    virtual void SetupView(FSceneViewFamily& in_view_family, FSceneView& in_view) override;
    virtual void BeginRenderViewFamily(FSceneViewFamily& in_view_family) override;

    virtual void PrePostProcessPass_RenderThread(FRDGBuilder& graph_builder, const FSceneView& in_view, const FPostProcessingInputs& inputs) override;
};
