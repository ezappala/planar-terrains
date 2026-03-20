// #pragma once
// #include "MeshDrawShaderBindings.h"
// #include "VertexFactory.h"
//
// class FTerrainVertexFactory final : public FVertexFactory {
//     DECLARE_VERTEX_FACTORY_TYPE(FTerrainVertexFactory);
//
//     FTerrainVertexFactory(ERHIFeatureLevel::Type fl);
//
//     void SetBuffers(
//         const FShaderResourceViewRHIRef& in_position_srv
//         // const FShaderResourceViewRHIRef&
//         // normal_srv, const FShaderResourceViewRHIRef& uv_srv
//     );
//
//     virtual void InitRHI(FRHICommandListBase& cmd) override;
//     virtual void ReleaseRHI() override;
//
//     static bool ShouldCompilePermutation(
//         const FVertexFactoryShaderPermutationParameters& params
//     );
//
//     static void ModifyCompilationEnvironment(
//         const FVertexFactoryShaderPermutationParameters& params,
//         FShaderCompilerEnvironment& out_environment
//     );
//
//     static void ValidateCompiledResult(
//         const FVertexFactoryType* type,
//         EShaderPlatform platform,
//         const FShaderParameterMap& pm,
//         TArray<FString>& out_errors
//     );
//
//     FShaderResourceViewRHIRef position_srv;
//     // FShaderResourceViewRHIRef normal_srv;
//     // FShaderResourceViewRHIRef uv_srv;
// };
//
// class FTerrainVertexFactoryShaderParameters
//     : public FVertexFactoryShaderParameters {
//     DECLARE_TYPE_LAYOUT(FTerrainVertexFactoryShaderParameters, NonVirtual);
//
//     void Bind(const FShaderParameterMap& pm) {
//         PositionBufferParameter.Bind(pm, TEXT("PositionBuffer"));
//     }
//
//     void GetElementShaderBindings(
//         const FSceneInterface* scene,
//         const FSceneView* view,
//         const FMeshMaterialShader* shader,
//         const EVertexInputStreamType input_stream_type,
//         ERHIFeatureLevel::Type feature_level,
//         const FVertexFactory* vertex_factory,
//         const FMeshBatchElement& batch_element,
//         FMeshDrawSingleShaderBindings& shader_bindings,
//         FVertexInputStreamArray& vertex_streams
//     ) const {
//         const FTerrainVertexFactory* tvf = static_cast<const
//             FTerrainVertexFactory*>(vertex_factory);
//
//         if (PositionBufferParameter.IsBound() && tvf->position_srv.IsValid()) {
//             shader_bindings.Add(PositionBufferParameter, tvf->position_srv);
//         }
//     }
//
// private:
//     LAYOUT_FIELD(FShaderResourceParameter, PositionBufferParameter);
// };