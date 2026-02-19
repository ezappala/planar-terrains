#include "scene_view.h"

#include <PixelShaderUtils.h>
#include <RenderGraphEvent.h>
#include <SimpleMeshDrawCommandPass.h>
#include <Components/PrimitiveComponent.h>
#include <Runtime/Renderer/Internal/PostProcess/PostProcessInputs.h>
#include <Runtime/Renderer/Private/SceneRendering.h>

#include "EGP_DownsampleDepthPass.h"
#include "EGP_GetMeshBatches.h"
#include "mesh_processor.h"
#include "render_pass.h"
#include "ext/color.h"

// DECLARE_GPU_DRAWCALL_STAT(ColorExtract);
//
// FTerrainSceneViewExtension::FTerrainSceneViewExtension(const FAutoRegister& auto_register)
//     : FSceneViewExtensionBase{auto_register} {}
//
// void FTerrainSceneViewExtension::PrePostProcessPass_RenderThread(
//     FRDGBuilder& gb, const FSceneView& view,
//     const FPostProcessingInputs& inputs
// ) {
//     FSceneViewExtensionBase::PrePostProcessPass_RenderThread(gb, view, inputs);
//
//     checkSlow(view.bIsViewInfo);
//     const FIntRect viewport = static_cast<const FViewInfo&>(view).ViewRect;
//     const FGlobalShaderMap* gsm = GetGlobalShaderMap(GMaxRHIFeatureLevel);
//
//     RDG_GPU_STAT_SCOPE(GraphBuilder, ColorExtract); // for stat gpu
//     // ReSharper disable CppDeclarationHidesLocal
//     RDG_EVENT_SCOPE(gb, "Color Extract"); // for RenderDoc
//     // ReSharper restore CppDeclarationHidesLocal
//
//     const FSceneTextureShaderParameters SceneTextures = CreateSceneTextureShaderParameters(gb, view,
//         ESceneTextureSetupMode::SceneColor | ESceneTextureSetupMode::GBuffers);
//
//     const FScreenPassTexture scene_color_texture{(*inputs.SceneTextures)->SceneColorTexture, viewport};
//
//     // Allocate shader parameters
//     FColorExtractPS::FParameters* params = gb.AllocParameters<FColorExtractPS::FParameters>();
//     params->scene_color_texture = scene_color_texture.Texture;
//     params->scene_textures = SceneTextures;
//     params->target_color = ext::color::rgb_to_lab(FVector3f{1.0f, 0.0f, 0.0f});
//     params->view = view.ViewUniformBuffer;
//     params->RenderTargets[0] = FRenderTargetBinding{
//         (*inputs.SceneTextures)->SceneColorTexture,
//         ERenderTargetLoadAction::ELoad
//     };
//
//     const TShaderMapRef<FColorExtractPS> ps{gsm};
//     FPixelShaderUtils::AddFullscreenPass(gb, gsm, RDG_EVENT_NAME("UDLOD.RenderPass"),
//         ps, params, viewport);
// }

FTerrainSceneViewExtension::FTerrainSceneViewExtension(
    const FAutoRegister& auto_register, UTerrainRenderPass* render_pass
) : T_EGP_RenderPassSceneViewExtension(auto_register, render_pass) {}

void FTerrainSceneViewExtension::PrePostProcessPass_RenderThread(
    FRDGBuilder& gb, const FSceneView& view,
    const FPostProcessingInputs& inputs
) {
    check(view.bIsViewInfo);
    const auto& view_info = reinterpret_cast<const FViewInfo&>(view);
    if (!Pass->ViewFilter->ShouldRenderFor(view_info)) { return; }

    auto* passMaterial = Pass->GetEffectMaterial_RenderThread();

    // ReSharper disable CppDeclarationHidesLocal
    RDG_EVENT_SCOPE(gb, "Terrain, viewport %ix%i", view_info.ViewRect.Width(), view_info.ViewRect.Height());
    // ReSharper restore CppDeclarationHidesLocal

    //Get or create the per-view data.
    auto& viewData = Pass->PerViewData.DataForView(
        gb, view_info,
        //For new views, the view data constructor arguments:
        passMaterial, GetSceneTextureShaderParameters(inputs.SceneTextures)
    );
    FRDGTextureRef simStateRDG = RegisterExternalTexture(gb, viewData.SimState, TEXT("Terrain_State"));

    //If re-initialization was requested, do that first.
    if (viewData.ReinitializeViews) {
        // ReSharper disable CppDeclarationHidesLocal
        RDG_EVENT_SCOPE(gb, "Terrain: Reinitialize");
        // ReSharper restore CppDeclarationHidesLocal
        InitTerrainState(gb, simStateRDG, view_info,
            GetSceneTextureShaderParameters(inputs.SceneTextures),
            passMaterial);
        viewData.ReinitializeViews = false;
    }
    //If some time has passed on the game thread, tick this viewport's sim.
    if (viewData.NextTickTime > 0) {
        // ReSharper disable CppDeclarationHidesLocal
        RDG_EVENT_SCOPE(gb, "Terrain: Tick (%f seconds)", viewData.NextTickTime);
        // ReSharper restore CppDeclarationHidesLocal

        const FRDGTextureRef nextSimStateRDG =
            RegisterExternalTexture(gb, viewData.SimBuffer, TEXT("GoL_NextState"));
        UpdateTerrainState(
            gb, view_info,
            simStateRDG, nextSimStateRDG,
            viewData.NextTickTime, passMaterial
        );
        std::swap(viewData.SimBuffer, viewData.SimState);
        simStateRDG = nextSimStateRDG;
        viewData.NextTickTime = 0;
    }

    //Draw our mesh pass into the sim state.
    {
        // ReSharper disable CppDeclarationHidesLocal
        RDG_EVENT_SCOPE(gb, "Terrain: Mesh passes (%i primitives)", Pass->GetComponentData_RenderThread().Num());
        // ReSharper restore CppDeclarationHidesLocal

        FScene* renderScene = nullptr;
        if (view_info.Family != nullptr && view_info.Family->Scene != nullptr) {
            renderScene = view_info.Family->Scene->GetRenderScene();
        }

        //We want to use the scene's depth-texture for our mesh pass,
        //    however the Game of Life state texture exists on its own
        //    rather than being a subset of a larger render target,
        //    and also uses a lower-resolution than the viewport.
        //To fix this, we need to resample the depth buffer.
        FRDGTextureRef depthBuffer = inputs.SceneTextures->GetContents()->SceneDepthTexture;
        if (view_info.ViewRect.Min != FIntPoint::ZeroValue || depthBuffer->Desc.Extent != viewData.SimState->
            GetSizeXY()) {
            auto resampledDepthBufferDesc = depthBuffer->Desc;
            resampledDepthBufferDesc.Extent = viewData.SimState->GetSizeXY();
            const FRDGTextureRef resampledDepthBuffer = gb.CreateTexture(
                resampledDepthBufferDesc, TEXT("Terrain_ResampledSceneDepth")
            );

            EGP::AddDownsampleDepthPass(
                gb, view_info,
                FScreenPassTexture{depthBuffer, view_info.ViewRect},
                FScreenPassRenderTarget{resampledDepthBuffer, ERenderTargetLoadAction::ENoAction},
                EDownsampleDepthFilter::Checkerboard
            );
            depthBuffer = resampledDepthBuffer;
        }

        //Note that it doesn't matter if the texture has already been registered in this graph previously --
        //    in that case its previous RDG handle will be returned here.
        const FRDGTextureRef nextSimStateRDG = RegisterExternalTexture(
            gb, viewData.SimBuffer,
            TEXT("Terrain_NextState")
        );

        //Set up the RDG configuration of the pass.
        auto* passParams = gb.AllocParameters<FTerrainMeshPassParameters>();
        passParams->View = view_info.ViewUniformBuffer;
        passParams->Scene = GetSceneUniformBufferRef(gb, view_info);
        passParams->PrevState = gb.CreateSRV(FRDGTextureSRVDesc{simStateRDG});
        passParams->RenderTargets[0] = FRenderTargetBinding{nextSimStateRDG, ERenderTargetLoadAction::ELoad};
        passParams->RenderTargets.DepthStencil = {
            depthBuffer,
            ERenderTargetLoadAction::ELoad,
            FExclusiveDepthStencil::DepthRead_StencilNop
        };

        //Dispatch the draw calls.
        AddCopyTexturePass(gb, simStateRDG, nextSimStateRDG);
        AddSimpleMeshPass(gb, passParams, renderScene, view_info, nullptr,
            RDG_EVENT_NAME("GoLMeshes"),
            FIntRect{FIntPoint::ZeroValue, simStateRDG->Desc.Extent},
            [&](FDynamicPassMeshDrawListContext* output) {
                //Define one mesh processor for each blend mode.
                FTerrainMeshProcessor meshProcessorAlpha{
                    renderScene, &view_info, view_info.FeatureLevel, output,
                    TStaticBlendState<CW_RGBA, BO_Add, BF_SourceAlpha, BF_InverseSourceAlpha>::GetRHI()
                };
                FTerrainMeshProcessor meshProcessorAdditive{
                    renderScene, &view_info, view_info.FeatureLevel, output,
                    TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_One>::GetRHI()
                };
                FTerrainMeshProcessor meshProcessorMultiply{
                    renderScene, &view_info, view_info.FeatureLevel, output,
                    TStaticBlendState<CW_RGBA, BO_Add, BF_DestColor>::GetRHI()
                };

                //Get every scene primitive that's tagged with our custom component.
                ForEachComponent_RenderThread([&](
                    const UTerrainComponent& component,
                    const FTerrainPrimitiveRenderSettings& componentSettings,
                    const UPrimitiveComponent& primitive,
                    const FPrimitiveSceneProxy& primitiveProxy
                ) {
                        //Pick the blend mode.
                        FTerrainMeshProcessor* componentProcessor;
                        switch (componentSettings.blend_mode) {
                        case ETerrainMeshBlendModes::Alpha: componentProcessor = &meshProcessorAlpha;
                            break;
                        case ETerrainMeshBlendModes::Additive: componentProcessor = &meshProcessorAdditive;
                            break;
                        case ETerrainMeshBlendModes::Multiply: componentProcessor = &meshProcessorMultiply;
                            break;
                        default: check(false);
                            return;
                        }

                        //Draw every renderable mesh-batch in that component.
                        EGP::ForEachBatch(view_info, &primitiveProxy,
                            [&](const FMeshBatch& batch, uint64 mask, const auto* sceneProxy, int staticMeshID) {
                                componentProcessor->AddMeshBatch(batch, mask, sceneProxy, staticMeshID,
                                    viewData.SimState,
                                    TStaticSamplerState<SF_Bilinear>::GetRHI());
                            });
                    });
            });

        //Swap 'previous' and 'next' textures.
        std::swap(viewData.SimBuffer, viewData.SimState);
        simStateRDG = nextSimStateRDG;
    }

    //Finally, draw the sim state onto the scene color texture.
    RenderTerrainState(
        gb, view_info, simStateRDG,
        //Use multiplicative blending.
        TStaticBlendState<CW_RGBA, BO_Add, BF_DestColor>::GetRHI(),
        //Blend on top of the current scene color, so make sure its existing contents are Loaded when bound.
        {inputs.SceneTextures->GetContents()->SceneColorTexture, ERenderTargetLoadAction::ELoad},
        passMaterial,
        GetSceneTextureShaderParameters(inputs.SceneTextures)
    );
}
