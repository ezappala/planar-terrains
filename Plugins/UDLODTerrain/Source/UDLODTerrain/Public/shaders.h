#pragma once
#include <MaterialDomain.h>
#include <MeshMaterialShader.h>

#include "EGP_PostProcessMaterialShaders.h"
#include "mesh_element_data.h"
#include "terrain_config.h"

class FTerrainMeshVS : public FMeshMaterialShader {
public:
    DECLARE_SHADER_TYPE(FTerrainMeshVS, MeshMaterial);

    static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& params) {
        return params.MaterialParameters.MaterialDomain == MD_Surface &&
            FMeshMaterialShader::ShouldCompilePermutation(params);
    }

    FTerrainMeshVS() = default;

    FTerrainMeshVS(const ShaderMetaType::CompiledShaderInitializerType& initializer)
        : FMeshMaterialShader(initializer) {}
};

class FTerrainMeshPS : public FMeshMaterialShader {
public:
    DECLARE_SHADER_TYPE(FTerrainMeshPS, MeshMaterial);

    static bool ShouldCompilePermutation(const FMeshMaterialShaderPermutationParameters& params) {
        return params.MaterialParameters.MaterialDomain == MD_Surface &&
            FMeshMaterialShader::ShouldCompilePermutation(params);
    }

    FTerrainMeshPS() = default;

    FTerrainMeshPS(const ShaderMetaType::CompiledShaderInitializerType& initializer)
        : FMeshMaterialShader(initializer) {
        //The user's Material may or may not make use of the previous state texture,
        //    so the uniforms must be marked Optional.
        PreviousStateTex.Bind(
            initializer.ParameterMap,
            TEXT("PreviousStateTex"),
            SPF_Optional
        );
        PreviousStateSampler.Bind(
            initializer.ParameterMap,
            TEXT("PreviousStateSampler"),
            SPF_Optional
        );
    }

    //Binds parameters to the shader to render the given "element".
    void GetShaderBindings(
        const FScene* scene,
        const ERHIFeatureLevel::Type featureLevel,
        const FPrimitiveSceneProxy* primitiveSceneProxy,
        const FMaterialRenderProxy& materialRenderProxy,
        const FMaterial& material,
        const FTerrainMeshShaderElementData& element,
        FMeshDrawSingleShaderBindings& shaderBindings
    ) const {
        FMeshMaterialShader::GetShaderBindings(
            scene, featureLevel, primitiveSceneProxy, materialRenderProxy, material, element, shaderBindings
        );

        shaderBindings.AddTexture(
            PreviousStateTex, PreviousStateSampler, element.PreviousStateSampler, element.PreviousStateTex
        );
    }

private:
    LAYOUT_FIELD(FShaderResourceParameter, PreviousStateTex);
    LAYOUT_FIELD(FShaderResourceParameter, PreviousStateSampler);
};

struct FTerrainInitializePS : EGP::FScreenSpaceShader {
    DECLARE_EXPORTED_SHADER_TYPE(FTerrainInitializePS, Material,)
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        EGP_SCREEN_SPACE_PASS_MATERIAL_DATA()
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()
    SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FTerrainInitializePS, EGP::FScreenSpaceShader)
};

struct FTerrainSimulateCS : EGP::FSimulationShader {
    DECLARE_EXPORTED_SHADER_TYPE(FTerrainSimulateCS, Material,);

    static FIntVector3 GroupSize() {
        namespace ump = udlod_terrain::mesh_pass;
        return {ump::SIM_GROUP_SIZE_X, ump::SIM_GROUP_SIZE_Y, ump::SIM_GROUP_SIZE_Z};
    }

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER(float, DeltaSeconds)
        SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float2>, NextSimStateTex)
        EGP_SIMULATION_PASS_MATERIAL_DATA()
    END_SHADER_PARAMETER_STRUCT()
    SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FTerrainSimulateCS, EGP::FSimulationShader)

    //Feed the group size to the shader so that it's only defined in one place.
    static void ModifyCompilationEnvironment(
        const FMaterialShaderPermutationParameters& params,
        FShaderCompilerEnvironment& env
    ) {
        FSimulationShader::ModifyCompilationEnvironment(params, env);
        env.SetDefine(TEXT("SIM_GROUP_SIZE_X"), GroupSize().X);
        env.SetDefine(TEXT("SIM_GROUP_SIZE_Y"), GroupSize().Y);
        env.SetDefine(TEXT("SIM_GROUP_SIZE_Z"), GroupSize().Z);
    }
};

struct FTerrainResamplePS : FGlobalShader {
    DECLARE_GLOBAL_SHADER(FTerrainResamplePS);

    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float2>, InputTex)
        SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()
    SHADER_USE_PARAMETER_STRUCT(FTerrainResamplePS, FGlobalShader)
};

struct FTerrainDisplayPS : EGP::FScreenSpaceShader {
    DECLARE_EXPORTED_SHADER_TYPE(FTerrainDisplayPS, Material,)
    BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
        EGP_SCREEN_SPACE_PASS_MATERIAL_DATA()
        RENDER_TARGET_BINDING_SLOTS()
    END_SHADER_PARAMETER_STRUCT()
    SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FTerrainDisplayPS, EGP::FScreenSpaceShader)
};
