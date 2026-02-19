#include "terrain_view.h"

#include <RHIResources.h>

#include "mesh_pass.h"
#include "shaders.h"

FRHITextureCreateDesc FTerrainView::SimStateDesc(const FInt32Point& viewportSize) {
    //The sim looks pretty nice running at half-resolution;
    //    doing this also cuts the performance cost by 75%.
    const auto texSize = viewportSize / 2;

    auto d = FRHITextureCreateDesc::Create2D(
        TEXT("GoL_State"),
        texSize,
        PF_R8G8 //Two-channel unorm, 8 bits per channel
    );
    d.AddFlags(TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);

    return d;
}

FTerrainView::FTerrainView(
    FRDGBuilder& graph, const FViewInfo& view, const FIntRect& viewportSubset,
    const UMaterialInterface* initShaderMaterial,
    const FSceneTextureShaderParameters& sceneTextures
)
    : F_EGP_ViewPersistentData(graph, view, viewportSubset) {
    const auto desc = SimStateDesc(viewportSubset.Size());
    SimState = RHICreateTexture(desc);
    SimBuffer = RHICreateTexture(desc);

    const FRDGTextureRef simStateRDG = RegisterExternalTexture(graph, SimState, TEXT("GoL_InitialState"));
    InitTerrainState(graph, simStateRDG, view, sceneTextures, initShaderMaterial);
}

void FTerrainView::Resample(
    FRDGBuilder& graph, const FViewInfo& view,
    const FInt32Point& oldResolution, const FInt32Point& newResolution,
    const FInt32Point& offsetDelta
) {
    //The sim state texture doesn't share viewport space like viewport render-targets do,
    //    so we don't care about position changes -- only resolution changes.
    if (oldResolution == newResolution) { return; }

    const auto oldState = SimState;
    const auto newDesc = SimStateDesc({newResolution.X, newResolution.Y});
    SimState = RHICreateTexture(newDesc);
    SimBuffer = RHICreateTexture(newDesc);

    const FRDGTextureRef oldStateRDG = RegisterExternalTexture(graph, oldState, TEXT("Previous_GoL_State"));
    auto* newStateRDG = RegisterExternalTexture(graph, SimState, TEXT("Next_GoL_State"));

    auto* params = graph.AllocParameters<FTerrainResamplePS::FParameters>();
    params->InputTex = graph.CreateSRV(FRDGTextureSRVDesc{oldStateRDG});
    //If the resolution change is less than 0.5x we will lose data;
    //    fortunately that's not a big deal for demo purposes.
    params->InputSampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
    params->RenderTargets[0] = {newStateRDG, ERenderTargetLoadAction::ENoAction};

    //Unreal's "draw screen pass" will by default cover the whole screen using opaque blending
    //    and use their own trivial Vertex Shader.
    //This is perfect for our use-case.
    AddDrawScreenPass(
        graph, RDG_EVENT_NAME("GoL_Resample"), view,
        FScreenPassTextureViewport{newStateRDG},
        FScreenPassTextureViewport{oldStateRDG},
        TShaderMapRef<FTerrainResamplePS>{view.ShaderMap},
        params
    );
}
