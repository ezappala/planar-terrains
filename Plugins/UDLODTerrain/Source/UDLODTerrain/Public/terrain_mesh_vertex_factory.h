#pragma once

#include "CoreMinimal.h"
#include "VertexFactory.h"

class FTerrainMeshVertexFactory final : public FVertexFactory {
    DECLARE_VERTEX_FACTORY_TYPE(FTerrainMeshVertexFactory);

    explicit FTerrainMeshVertexFactory(ERHIFeatureLevel::Type in_feature_level);

    virtual void InitRHI(FRHICommandListBase& RHICmdList) override;
    virtual void ReleaseRHI() override;

    static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& parameters);
    static void ModifyCompilationEnvironment(
        const FVertexFactoryShaderPermutationParameters& parameters,
        FShaderCompilerEnvironment& out_environment
    );
};
