#pragma once

#include <SceneViewExtension.h>

#include "component.h"
#include "EGP_CustomRenderPasses.h"
#include "mesh_pass.h"
#include "render_pass.h"

// class FTerrainSceneViewExtension final : public FSceneViewExtensionBase {
// public:
//     FTerrainSceneViewExtension(const FAutoRegister& auto_register);
//
//     virtual void SetupViewFamily(FSceneViewFamily& view_family) override {}
//     virtual void SetupView(FSceneViewFamily& view_family, FSceneView& view) override {}
//     virtual void BeginRenderViewFamily(FSceneViewFamily& view_family) override {}
//
//     virtual void PostRenderBasePassDeferred_RenderThread(
//         FRDGBuilder& gb,
//         FSceneView& view,
//         const FRenderTargetBindingSlots& render_targets,
//         TRDGUniformBufferRef<FSceneTextureUniformParameters> scene_textures
//     ) override {}
//
//     virtual void PreRenderViewFamily_RenderThread(FRDGBuilder& gb, FSceneViewFamily& view_family) override {}
//     virtual void PreRenderView_RenderThread(FRDGBuilder& gb, FSceneView& view) override {}
//     virtual void PostRenderView_RenderThread(FRDGBuilder& gb, FSceneView& view) override {}
//
//     virtual void PrePostProcessPass_RenderThread(
//         FRDGBuilder& gb, const FSceneView& view, const FPostProcessingInputs& inputs
//     ) override;
//
//     virtual void PostRenderViewFamily_RenderThread(FRDGBuilder& gb, FSceneViewFamily& view) override {}
//
// private:
//     FTextureRenderTargetBinding current_render_target;
// };

//This struct doesn't feed into any shader, but tells the RDG about our mesh pass.
BEGIN_SHADER_PARAMETER_STRUCT(FTerrainMeshPassParameters,)
    SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, PrevState)
    SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
    SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneUniformParameters, Scene)
    SHADER_PARAMETER_STRUCT_INCLUDE(FInstanceCullingDrawParams, InstanceCullingDrawParams)
    RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

struct UDLODTERRAIN_API FTerrainSceneViewExtension final
    : T_EGP_RenderPassSceneViewExtension<UTerrainRenderPass, UTerrainComponent, FTerrainPrimitiveRenderSettings> {
    //Re-use the parent constructor:
    FTerrainSceneViewExtension(const FAutoRegister& auto_register, UTerrainRenderPass* render_pass);

    virtual void PrePostProcessPass_RenderThread(
        FRDGBuilder& gb,
        const FSceneView& view,
        const FPostProcessingInputs& inputs
    ) override;
};
