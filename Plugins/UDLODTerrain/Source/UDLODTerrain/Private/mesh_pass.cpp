#include "mesh_pass.h"

#include <Landscape.h>
#include <RenderGraphUtils.h>
#include <SimpleMeshDrawCommandPass.h>
#include <Runtime/Renderer/Private/PostProcess/PostProcessing.h>

#include "EGP_DownsampleDepthPass.h"
#include "EGP_GetMeshBatches.h"
#include "scene_view.h"
#include "shaders.h"

void RenderTerrainState(
    FRDGBuilder& graph, const FViewInfo& view, FRDGTextureRef simStateTex, FRHIBlendState* blending,
    const FRenderTargetBinding& output, const UMaterialInterface* material,
    const FSceneTextureShaderParameters& sceneTextures
) {
    auto* params = graph.AllocParameters<FTerrainDisplayPS::FParameters>();
    params->RenderTargets[0] = output;

    //Configure the standard post-process Material inputs for this pass:
    EGP::FScreenSpacePassMaterialInputs inputs;
    inputs.Textures[0] = GetScreenPassTextureInput(
        FScreenPassTexture{simStateTex},
        TStaticSamplerState<SF_Bilinear>::GetRHI()
    );
    inputs.SceneTextures = sceneTextures;
    inputs.TargetView = &view;
    inputs.InputViewportData = FScreenPassTextureViewport{inputs.Textures[0].Texture};
    inputs.OutputViewportData = FScreenPassTextureViewport{output.GetTexture(), view.ViewRect};

    EGP::AddScreenSpaceRenderPass<EGP::FScreenSpaceRenderVS, FTerrainDisplayPS>(
        graph, RDG_EVENT_NAME("GoL_Display"), inputs,
        EGP::FScreenSpacePassRenderState{blending},
        params, material,
        //Extract the specific param structs for each shader:
        &params->ScreenSpacePassData, params
    );
}

void EnsureCompiledSimulateShader(ERHIFeatureLevel::Type featureLevel, const UMaterialInterface* uMaterial) {
    FMaterialShaderTypes types;
    types.AddShaderType<EGP::FScreenSpaceRenderVS>();
    types.AddShaderType<FTerrainInitializePS>();
    auto foundShaders = EGP::FindMaterialShaders_RenderThread(
        uMaterial, types,
        {MD_PostProcess, featureLevel}
    );
    check(foundShaders);

    //Extract the shader and material proxy.
    auto* materialProxy = foundShaders->MaterialProxy;
    auto* materialF = foundShaders->Material;
    TShaderRef<EGP::FScreenSpaceRenderVS> shaderV;
    TShaderRef<FTerrainInitializePS> shaderP;
    ensure(foundShaders->Shaders.TryGetVertexShader(shaderV));
    ensure(foundShaders->Shaders.TryGetPixelShader(shaderP));
}

void InitTerrainState(
    FRDGBuilder& graph, FRDGTextureRef simStateTex, const FViewInfo& view,
    const FSceneTextureShaderParameters& sceneTextures, const UMaterialInterface* uMaterial
) {
    auto* initShaderParams = graph.AllocParameters<FTerrainInitializePS::FParameters>();
    initShaderParams->RenderTargets[0] = {simStateTex, ERenderTargetLoadAction::ENoAction};

    EGP::FScreenSpacePassMaterialInputs postProcessMaterialInputs;
    postProcessMaterialInputs.SceneTextures = sceneTextures;
    postProcessMaterialInputs.TargetView = &view;
    postProcessMaterialInputs.OutputViewportData = FScreenPassTextureViewport{simStateTex};
    postProcessMaterialInputs.InputViewportData = FScreenPassTextureViewport{view.ViewRect};

    EGP::AddScreenSpaceRenderPass<EGP::FScreenSpaceRenderVS, FTerrainInitializePS>(
        graph, RDG_EVENT_NAME("Terrain_Initialize"),
        postProcessMaterialInputs,
        EGP::FScreenSpacePassRenderState{}, //Default to opaque blending and no depth/stencil usage
        initShaderParams, uMaterial,
        //Extract the specific param structs for each shader:
        &initShaderParams->ScreenSpacePassData, initShaderParams
    );
}

void UpdateTerrainState(
    FRDGBuilder& graph, const FViewInfo& view, FRDGTextureRef currentSimState, FRDGTextureRef nextSimState,
    float deltaSeconds, const UMaterialInterface* uMaterial
) {
    check(currentSimState->Desc.Extent == nextSimState->Desc.Extent);

    //Provide the previous state to the material graph as Post-Process Texture 0.
    EGP::FSimulationPassMaterialInputs inputs;
    inputs.Textures[0] = GetScreenPassTextureInput(
        FScreenPassTexture{currentSimState},
        TStaticSamplerState<SF_Bilinear>::GetRHI()
    );

    //Set up the other, non-Material shader params.
    auto* params = graph.AllocParameters<FTerrainSimulateCS::FParameters>();
    params->DeltaSeconds = deltaSeconds;
    params->NextSimStateTex = graph.CreateUAV(nextSimState);

    //Compute the group count for this dispatch.
    EGP::FSimulationPassState state;
    state.GroupCount.Set<FIntVector3>(FComputeShaderUtils::GetGroupCount(
        FIntVector3{currentSimState->Desc.Extent.X, currentSimState->Desc.Extent.Y, 1},
        FTerrainSimulateCS::GroupSize()
    ));

    EGP::AddSimulationMaterialPass<FTerrainSimulateCS>(graph, RDG_EVENT_NAME("GoL_Tick"),
        inputs, state, view,
        params, uMaterial);
}
