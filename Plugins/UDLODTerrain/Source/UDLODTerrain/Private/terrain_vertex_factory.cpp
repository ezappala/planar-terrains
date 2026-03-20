// #include "terrain_vertex_factory.h"
//
// #include "MaterialDomain.h"
// #include "MeshMaterialShader.h"
//
// IMPLEMENT_TYPE_LAYOUT(FTerrainVertexFactoryShaderParameters);
// IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(
//     FTerrainVertexFactory, SF_Vertex, FTerrainVertexFactoryShaderParameters);
// IMPLEMENT_VERTEX_FACTORY_TYPE(
//     FTerrainVertexFactory,
//     "/Plugin/UDLODTerrain/vertex_factory.ush",
//     EVertexFactoryFlags::UsedWithMaterials |
//     EVertexFactoryFlags::SupportsDynamicLighting |
//     EVertexFactoryFlags::SupportsPositionOnly
// );
//
// FTerrainVertexFactory::FTerrainVertexFactory(const ERHIFeatureLevel::Type fl) : FVertexFactory{fl} {}
//
// void FTerrainVertexFactory::SetBuffers(const FShaderResourceViewRHIRef& in_position_srv) {
//     position_srv = in_position_srv;
// }
//
// void FTerrainVertexFactory::InitRHI(FRHICommandListBase& cmd) {
//     FVertexDeclarationElementList elems;
//     elems.Add(FVertexElement(0, 0, VET_Float3, 0, 0, false));
//     InitDeclaration(elems);
// }
//
// void FTerrainVertexFactory::ReleaseRHI() {
//     position_srv.SafeRelease();
//
//     FVertexFactory::ReleaseRHI();
// }
//
// bool FTerrainVertexFactory::ShouldCompilePermutation(
//     const FVertexFactoryShaderPermutationParameters& params
// ) {
//     const bool usage_level = params.MaterialParameters.bIsUsedWithStaticLighting ||
//         params.MaterialParameters.bIsUsedWithSkeletalMesh ||
//         static_cast<bool>(params.MaterialParameters.bIsDefaultMaterial) ||
//         params.MaterialParameters.MaterialDomain == MD_Surface;
//     const bool feature_level = IsFeatureLevelSupported(params.Platform, ERHIFeatureLevel::SM5);
//     return feature_level && usage_level;
// }
//
// void FTerrainVertexFactory::ModifyCompilationEnvironment(
//     const FVertexFactoryShaderPermutationParameters& params,
//     FShaderCompilerEnvironment& out_environment
// ) {
//     FVertexFactory::ModifyCompilationEnvironment(params, out_environment);
//     out_environment.SetDefine(TEXT("VERTEX"), 1);
//     // TODO: set other defines
// }
//
// void FTerrainVertexFactory::ValidateCompiledResult(
//     [[maybe_unused]] const FVertexFactoryType* type,
//     [[maybe_unused]] EShaderPlatform platform,
//     [[maybe_unused]] const FShaderParameterMap& pm,
//     [[maybe_unused]] TArray<FString>& out_errors
// ) {
//     unimplemented()
// }
