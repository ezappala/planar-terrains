#pragma once

#include <GlobalShader.h>
#include <SceneView.h>
// #include <SceneTexturesConfig.h>
// #include <ShaderParameterStruct.h>

#include "EGP_CustomRenderPasses.h"
#include "terrain_view.h"

#include "render_pass.generated.h"

// BEGIN_SHADER_PARAMETER_STRUCT(FColorExtractParams,)
//     SHADER_PARAMETER(FVector3f, target_color)
//
//     SHADER_PARAMETER_RDG_TEXTURE(Texture2D, scene_color_texture)
//
//     SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, view)
//
//     // Reference to uniform buffer's textures (for SceneColor)
//     SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureShaderParameters, scene_textures)
//
//     // Needed if outputting to a render target
//     RENDER_TARGET_BINDING_SLOTS()
// END_SHADER_PARAMETER_STRUCT()
//
// class UDLODTERRAIN_API FColorExtractPS : public FGlobalShader {
//     DECLARE_GLOBAL_SHADER(FColorExtractPS)
//     // Generates a constructor for the class
//     SHADER_USE_PARAMETER_STRUCT(FColorExtractPS, FGlobalShader);
//
//     using FParameters = FColorExtractParams;
//
//     static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& params) {
//         return IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
//     }
//
//     static void ModifyCompilationEnvironment(
//         const FGlobalShaderPermutationParameters& params, FShaderCompilerEnvironment& out_env
//     ) {
//         FGlobalShader::ModifyCompilationEnvironment(params, out_env);
//
//         SET_SHADER_DEFINE(out_env, USE_UNLIT_SCENE_COLOR, 0);
//     }
// };

UCLASS(BlueprintType)
class UDLODTERRAIN_API UTerrainRenderPass : public U_EGP_RenderPass {
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    UMaterialInterface* EffectMaterial = nullptr;

    UMaterialInterface* GetEffectMaterial_RenderThread() const {
        check(IsInRenderingThread());
        return effectMaterial_RenderThread;
    }

    T_EGP_PerViewData<FTerrainView> PerViewData;

    UFUNCTION(BlueprintCallable)
    void ReInitializeAllViews();

protected:
    virtual TSharedRef<F_EGP_RenderPassSceneViewExtension> InitThisPass_GameThread(UWorld& thisWorld) override;
    virtual void Tick_GameThread(UWorld& thisWorld, float deltaSeconds) override;
    virtual void Tick_RenderThread(const FSceneInterface& thisScene, float gameThreadDeltaSeconds) override;

private:
    // ReSharper disable once CppUE4ProbableMemoryIssuesWithUObject
    UMaterialInterface* effectMaterial_RenderThread = nullptr;
};
