#include "mesh_processor.h"

#include <MeshPassProcessor.inl>

FTerrainMeshProcessor::FTerrainMeshProcessor(
    const FScene* scene, const FSceneView* view, const ERHIFeatureLevel::Type featureLevel,
    FMeshPassDrawListContext* commandsOutput, const FBlendStateRHIRef& blendState
) : FMeshPassProcessor(TEXT("TerrainRenderer"), scene, featureLevel, view, commandsOutput) {
    PassDrawState.SetBlendState(blendState);
    PassDrawState.SetDepthStencilState(TStaticDepthStencilState<false>::GetRHI());
}

void FTerrainMeshProcessor::AddMeshBatch(
    const FMeshBatch& batch, const uint64 batchElementMask, const FPrimitiveSceneProxy* proxy, const int32 staticMeshID,
    FRHITexture* prevState, FRHISamplerState* prevStateSampler
) {
    // Get the first usable Material in the chain,
    //     starting at the batch's desired Material and ending at the Default Material.
    const FMaterialRenderProxy* fallbackMat = nullptr;
    const auto& resource = batch.MaterialRenderProxy->GetMaterialWithFallback(
        FeatureLevel, fallbackMat
    );
    const auto& materialProxy = fallbackMat != nullptr ? *fallbackMat : *batch.MaterialRenderProxy;

    //Compile our shaders against the batch's Material and Vertex-Factory.
    TMeshProcessorShaders<FTerrainMeshVS, FTerrainMeshPS> shaderRefs;
    {
        FMaterialShaderTypes shaderTypes;
        shaderTypes.AddShaderType<FTerrainMeshVS>();
        shaderTypes.AddShaderType<FTerrainMeshPS>();

        FMaterialShaders materialShaders;
        verify(resource.TryGetShaders(shaderTypes, batch.VertexFactory->GetType(), materialShaders));

        materialShaders.TryGetVertexShader(shaderRefs.VertexShader);
        materialShaders.TryGetPixelShader(shaderRefs.PixelShader);
    }

    //Configure per-element settings.
    FTerrainMeshShaderElementData elementData;
    elementData.InitializeMeshMaterialData(ViewIfDynamicMeshCommand, proxy, batch, staticMeshID, false);
    //Set per-element shader parameters.
    elementData.PreviousStateTex = prevState;
    elementData.PreviousStateSampler = prevStateSampler;

    //Generate the draw calls for this batch.
    const FMeshDrawingPolicyOverrideSettings overrides = ComputeMeshOverrideSettings(batch);
    BuildMeshDrawCommands(
        batch,
        batchElementMask,
        proxy,
        materialProxy,
        resource,
        PassDrawState,
        MoveTemp(shaderRefs),
        ComputeMeshFillMode(resource, overrides),
        ComputeMeshCullMode(resource, overrides),
        FMeshDrawCommandSortKey::Default,
        EMeshPassFeatures::Default,
        elementData
    );
}

void FTerrainMeshProcessor::AddMeshBatch(
    const FMeshBatch& batch, uint64 batchElementMask, const FPrimitiveSceneProxy* proxy, int32 staticMeshID
) { check(false); }
