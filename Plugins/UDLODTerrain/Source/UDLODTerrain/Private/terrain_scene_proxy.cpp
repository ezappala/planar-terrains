#include "terrain_scene_proxy.h"

#include "MaterialDomain.h"
#include "MeshBatch.h"
#include "MeshElementCollector.h"
#include "SceneManagement.h"
#include "Materials/Material.h"
#include "Materials/MaterialRenderProxy.h"

FTerrainSceneProxy::FTerrainSceneProxy(const UTerrain* component) : FPrimitiveSceneProxy(component),
    render_resources(component->render_resources),
    vertex_factory(GMaxRHIFeatureLevel) {
    const UMaterialInterface* material_interface = component->material;
    if (material_interface == nullptr) {
        material_interface = UMaterial::GetDefaultMaterial(MD_Surface);
    }

    const EShaderPlatform platform = GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel];

    if (material_interface != nullptr) {
        material_proxy = material_interface->GetRenderProxy();
        material_relevance = material_interface->GetRelevance(platform);
    }

    bWillEverBeLit = true;
    bCastDynamicShadow = true;
    bCastStaticShadow = false;
    bAffectDynamicIndirectLighting = true;
    bAffectDistanceFieldLighting = true;
}

FTerrainSceneProxy::~FTerrainSceneProxy() { vertex_factory.ReleaseResource(); }

void FTerrainSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList) {
    FPrimitiveSceneProxy::CreateRenderThreadResources(RHICmdList);
    vertex_factory.InitResource(RHICmdList);
}

void FTerrainSceneProxy::GetDynamicMeshElements(
    const TArray<const FSceneView*>& views,
    const FSceneViewFamily& view_family,
    const uint32 visibility_map,
    FMeshElementCollector& collector
) const {
    (void)view_family;

    if (material_proxy == nullptr || !vertex_factory.IsInitialized() || !render_resources.
        IsValid()) { return; }

    for (int32 view_index = 0; view_index < views.Num(); ++view_index) {
        if ((visibility_map & 1u << view_index) == 0) { continue; }

        const FSceneView* view = views[view_index];
        if (view == nullptr) { continue; }

        FTerrainMeshViewState view_state;
        if (!render_resources->TryGetViewState(view->GetViewKey(), view_state)) { continue; }

        FMeshBatch& mesh = collector.AllocateMesh();
        FMeshBatchElement& batch_element = mesh.Elements[0];
        auto& user_data = collector.AllocateOneFrameResource<FTerrainMeshBatchElementUserData>();
        user_data = view_state;

        batch_element.VertexFactoryUserData = &user_data;
        batch_element.IndexBuffer = nullptr;
        batch_element.FirstIndex = 0;
        batch_element.NumPrimitives = 0;
        batch_element.NumInstances = 1;
        batch_element.BaseVertexIndex = 0;
        batch_element.MinVertexIndex = 0;
        batch_element.MaxVertexIndex = view_state.max_vertex_index;
        batch_element.IndirectArgsBuffer = view_state.indirect_args_buffer->GetRHI();
        batch_element.IndirectArgsOffset = view_state.indirect_args_offset;
        batch_element.PrimitiveUniformBuffer = GetUniformBuffer();
        batch_element.PrimitiveIdMode = PrimID_ForceZero;

        mesh.VertexFactory = &vertex_factory;
        mesh.MaterialRenderProxy = material_proxy;
        mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
        mesh.Type = PT_TriangleStrip;
        mesh.DepthPriorityGroup = SDPG_World;
        mesh.CastShadow = IsShadowCast(view);
        mesh.bCanApplyViewModeOverrides = true;
        mesh.bUseForMaterial = true;
        mesh.bUseForDepthPass = true;

        collector.AddMesh(view_index, mesh);
    }
}

SIZE_T FTerrainSceneProxy::GetTypeHash() const {
    static size_t unique_pointer;
    return reinterpret_cast<size_t>(&unique_pointer);
}

FPrimitiveViewRelevance FTerrainSceneProxy::GetViewRelevance(const FSceneView* view) const {
    FPrimitiveViewRelevance relevance;
    relevance.bDrawRelevance = IsShown(view);
    relevance.bShadowRelevance = IsShadowCast(view);
    relevance.bDynamicRelevance = true;
    relevance.bRenderInMainPass = ShouldRenderInMainPass();
    relevance.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
    relevance.bRenderCustomDepth = ShouldRenderCustomDepth();
    relevance.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;

    material_relevance.SetPrimitiveViewRelevance(relevance);
    return relevance;
}

uint32 FTerrainSceneProxy::GetMemoryFootprint() const { return sizeof(*this) + GetAllocatedSize(); }
