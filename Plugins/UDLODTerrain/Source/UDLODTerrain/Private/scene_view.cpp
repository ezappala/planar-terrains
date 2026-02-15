#include "scene_view.h"

#include "preprocess_shaders.h"
#include "ScreenPass.h"
#include "TextureResource.h"
#include "UDLODTerrain.h"
#include "Logging/StructuredLog.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "Runtime/Renderer/Internal/PostProcess/PostProcessInputs.h"
#include "Runtime/Renderer/Private/SceneRendering.h"

namespace {
TAutoConsoleVariable CVarPreprocesssShaderOn{
    TEXT("r.UDLODTerrain.PreprocesssShaderOn"),
    1,
    TEXT("Enable or disable the UDLODTerrain preprocess shader.\n")
    TEXT("0: Disable the preprocess shader.\n")
    TEXT("1: Enable the preprocess shader."),
    ECVF_RenderThreadSafe
};
}

FPreprocessSceneView::FPreprocessSceneView(const FAutoRegister& auto_register) : FSceneViewExtensionBase{auto_register} {
    UE_LOGFMT(LogUDLODTerrain, Log, "[FPreprocessSceneView] Created scene view extension.");
}

void FPreprocessSceneView::SetupViewFamily(FSceneViewFamily& in_view_family) {
    FSceneViewExtensionBase::SetupViewFamily(in_view_family);
}

void FPreprocessSceneView::SetupView(FSceneViewFamily& in_view_family, FSceneView& in_view) {
    FSceneViewExtensionBase::SetupView(in_view_family, in_view);
}

void FPreprocessSceneView::BeginRenderViewFamily(FSceneViewFamily& in_view_family) {
    FSceneViewExtensionBase::BeginRenderViewFamily(in_view_family);
}

void FPreprocessSceneView::PrePostProcessPass_RenderThread(
    FRDGBuilder& graph_builder, const FSceneView& in_view, const FPostProcessingInputs& inputs
) {
    if (!IsInParallelRenderingThread()) {
        UE_LOGFMT(LogTemp, Warning,
            "[FPreprocessSceneView] PrePostProcessPass_RenderThread called outside of parallel rendering thread.");
        return;
    }

    // const FSceneViewFamily& view_family = *in_view.Family;
    // // if (!in_view.bIsViewInfo) {
    // //     UE_LOGFMT(LogTemp, Log, "View is not a FViewInfo, skipping preprocess shader.");
    // //     return;
    // // }
    // // FViewInfo view_info = dynamic_cast<FViewInfo>(in_view);
    // FRDGTextureRef scene_color = inputs.SceneTextures->GetContents()->SceneColorTexture;
    // if (scene_color == nullptr || CVarPreprocesssShaderOn.GetValueOnRenderThread() == 0) { return; }
    //
    // const FScreenPassTextureViewport scene_color_viewport{scene_color};
    // // ReSharper disable CppDeclarationHidesLocal
    // // RDG_EVENT_SCOPE(graph_builder, "Prepass (FPreprocessSceneView)");
    // // ReSharper restore CppDeclarationHidesLocal
    // // {
    // //     const TShaderMapRef<FPreprocess> shader(GetGlobalShaderMap(view_family.GetFeatureLevel()));
    // //     // const FTextureResource* heightmap_resource =
    // // }
    // // ReSharper disable CppDeclarationHidesLocal
    // RDG_EVENT_SCOPE(graph_builder, "Main Pass");
    // // ReSharper restore CppDeclarationHidesLocal
    // {
    //
    // }
}
