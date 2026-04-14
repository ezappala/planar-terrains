#include "terrain_scene_proxy.h"

#include "DynamicMeshBuilder.h"
#include "MaterialDomain.h"
#include "MeshBatch.h"
#include "MeshElementCollector.h"
#include "PrimitiveSceneInfo.h"
#include "PrimitiveUniformShaderParametersBuilder.h"
#include "SceneManagement.h"
#include "SceneRendererInterface.h"
#include "terrain.h"
#include "HAL/IConsoleManager.h"
#include "Materials/Material.h"

namespace terrain::scene_proxy::detail {
TAutoConsoleVariable CVarUDLODMeshProbe(
    TEXT("r.UDLOD.MeshProbe"),
    1,
    TEXT("When non-zero, log one-time GPU mesh submission probes for UDLOD terrain."),
    ECVF_RenderThreadSafe
);

bool should_use_cpu_mesh_builder() {
    static const TConsoleVariableData<int32>* cvar = IConsoleManager::Get().
        FindTConsoleVariableDataInt(
            TEXT("r.UDLOD.UseCpuMeshBuilder")
        );
    return cvar != nullptr && cvar->GetValueOnRenderThread() != 0;
}

bool should_probe_gpu_mesh() { return CVarUDLODMeshProbe.GetValueOnRenderThread() != 0; }
}

FTerrainSceneProxy::FTerrainSceneProxy(const UTerrain* component) : FPrimitiveSceneProxy(component),
    render_resources(component->render_resources),
    bDebugWireframe(component->bDebugWireframe),
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

FTerrainSceneProxy::~FTerrainSceneProxy() = default;

void FTerrainSceneProxy::CreateRenderThreadResources(FRHICommandListBase& RHICmdList) {
    FPrimitiveSceneProxy::CreateRenderThreadResources(RHICmdList);
    vertex_factory.InitResource(RHICmdList);
}

void FTerrainSceneProxy::DestroyRenderThreadResources() {
    vertex_factory.ReleaseResource();
    FPrimitiveSceneProxy::DestroyRenderThreadResources();
}

// float GTerrainIndirectArgsPoolBlockSizeFactor = 2.0;
// int32 GTerrainIndirectArgsPoolMinSize = 256;
// const ERHIAccess FTerrainSceneProxy::INDIRECT_ARGS_DEFAULT_STATE = ERHIAccess::IndirectArgs |
//     ERHIAccess::SRVMask;

// FTerrainSceneProxy::FIndirectArgSlot FTerrainSceneProxy::add_draw_indirect(
//     FRHICommandListBase& cmd,
//     const uint32 instance_count_buffer_offset,
//     const uint32 num_indices_per_instance,
//     const uint32 start_index_location,
//     const bool culled,
//     const ETerrainGpuComputeTickStage::Type ready_tick_stage) {
//     UE::TScopeLock _(add_draw_indirect_guard);
//
//     auto info = FTerrainDrawIndirectArgGenTaskInfo{
//         instance_count_buffer_offset,
//         num_indices_per_instance,
//         start_index_location};
//
//     auto* slot_info = draw_indirect_args_map.Find(info);
//     if (slot_info == nullptr) {
//         auto* pool_entry = draw_indirect_pool.Num() > 0 ? draw_indirect_pool.Last().Get() : nullptr;
//         if (pool_entry == nullptr || pool_entry->used_entries_total >= pool_entry->
//             allocated_entries) {
//             auto new_entry = MakeUnique<FIndirectArgsPoolEntry>();
//             new_entry->allocated_entries = pool_entry != nullptr
//                 ? static_cast<uint32>(pool_entry->allocated_entries *
//                     GTerrainIndirectArgsPoolBlockSizeFactor)
//                 : static_cast<uint32>(GTerrainIndirectArgsPoolMinSize);
//
//             TResourceArray<uint32> init_data;
//             init_data.AddZeroed(new_entry->allocated_entries * TERRAIN_DRAW_INDIRECT_ARGS_SIZE);
//             new_entry->buffer.Initialize(
//                 cmd,
//                 TEXT("TerrainGgpuDrawIndirectArgs"),
//                 sizeof(uint32),
//                 new_entry->allocated_entries * TERRAIN_DRAW_INDIRECT_ARGS_SIZE,
//                 PF_R32_UINT,
//                 INDIRECT_ARGS_DEFAULT_STATE,
//                 BUF_Static |
//                 BUF_DrawIndirect,
//                 &init_data);
//
//             pool_entry = new_entry.Get();
//             draw_indirect_pool.Emplace(MoveTemp(new_entry));
//         }
//
//         info.indirect_args_buffer_offset = pool_entry->used_entries_total *
//             TERRAIN_DRAW_INDIRECT_ARGS_SIZE;
//         pool_entry->used_entries_total += 1;
//         slot_info = &draw_indirect_args_map.Add(info);
//         slot_info->pool_idx = draw_indirect_pool.Num() - 1;
//         slot_info->buffer_offset = info.indirect_args_buffer_offset * sizeof(uint32);
//
//         const ETerrainGpuCountUpdatePhase::Type count_phase = ready_tick_stage ==
//             ETerrainGpuComputeTickStage::PostOpaqueRender
//             ? ETerrainGpuCountUpdatePhase::PostOpaque
//             : ETerrainGpuCountUpdatePhase::PreOpaque;
//         draw_indirect_arg_gen_tasks[count_phase].Add(info);
//         pool_entry->used_entries[count_phase] += 1;
//     }
//
//     return {
//         draw_indirect_pool[slot_info->pool_idx]->buffer.Buffer,
//         draw_indirect_pool[slot_info->pool_idx]->buffer.SRV,
//         slot_info->buffer_offset
//     };
// }

void FTerrainSceneProxy::GetDynamicMeshElements(
    const TArray<const FSceneView*>& views,
    const FSceneViewFamily& view_family,
    const uint32 visibility_map,
    FMeshElementCollector& collector
) const {
    (void)view_family;

    if (material_proxy == nullptr || !render_resources.IsValid()) { return; }

    for (int32 view_index = 0; view_index < views.Num(); ++view_index) {
        if ((visibility_map & 1u << view_index) == 0) { continue; }

        const FSceneView* view = views[view_index];
        if (view == nullptr) { continue; }

        if (terrain::scene_proxy::detail::should_use_cpu_mesh_builder()) {
            FTerrainCpuMeshViewState cpu_view_state;
            if (render_resources->try_get_cpu_view_state(view->GetViewKey(), cpu_view_state)) {
                FDynamicMeshBuilder mesh_builder(collector.GetFeatureLevel());
                mesh_builder.ReserveVertices(cpu_view_state.vertices.Num());
                mesh_builder.ReserveTriangles(cpu_view_state.indices.Num() / 3);

                for (const auto& [position, uv, tangent_x, tangent_y, tangent_z, color] :
                     cpu_view_state.vertices) {
                    mesh_builder.AddVertex(
                        position,
                        uv,
                        tangent_x,
                        tangent_y,
                        tangent_z,
                        color
                    );
                }

                mesh_builder.AddTriangles(cpu_view_state.indices);
                FDynamicMeshBuilderSettings mesh_builder_settings;
                mesh_builder_settings.bWireframe = bDebugWireframe;
                mesh_builder_settings.bCanApplyViewModeOverrides = true;
                mesh_builder_settings.CastShadow = IsShadowCast(view);
                mesh_builder.GetMesh(
                    GetLocalToWorld(),
                    material_proxy,
                    SDPG_World,
                    mesh_builder_settings,
                    nullptr,
                    view_index,
                    collector
                );
                continue;
            }
        }

        if (!vertex_factory.IsInitialized()) { continue; }

        FTerrainMeshViewState view_state;
        if (!render_resources->try_get_view_state(view->GetViewKey(), view_state)) {
            if (terrain::scene_proxy::detail::should_probe_gpu_mesh()) {
                static bool logged_missing_gpu_view_state = false;
                if (!logged_missing_gpu_view_state) {
                    logged_missing_gpu_view_state = true;
                    UE_LOGFMT(
                        LogTemp,
                        Warning,
                        "[UDLOD.MeshProbe] No ready GPU view state for view={0}; skipping GPU terrain mesh submission.",
                        view->GetViewKey()
                    );
                }
            }
            continue;
        }

        FMeshBatch& mesh = collector.AllocateMesh();

        mesh.VertexFactory = &vertex_factory;
        mesh.LCI = nullptr;
        mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
        mesh.CastShadow = IsShadowCast(view);
        mesh.DepthPriorityGroup = GetDepthPriorityGroup(view);

        FMeshBatchElement& batch_element = mesh.Elements[0];

        auto& user_data = collector.AllocateOneFrameResource<FTerrainMeshBatchElementUserData>();
        user_data = view_state;

        auto& dynamic_primitive_uniform_buffer =
            collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
        FPrimitiveUniformShaderParametersBuilder builder;
        BuildUniformShaderParameters(builder);
        dynamic_primitive_uniform_buffer.Set(collector.GetRHICommandList(), builder);

        batch_element.PrimitiveUniformBufferResource = &dynamic_primitive_uniform_buffer
            .UniformBuffer;
        batch_element.FirstIndex = 0;
        batch_element.MinVertexIndex = 0;
        batch_element.MaxVertexIndex = view_state.max_vertex_index;
        batch_element.NumInstances = 1;

        batch_element.VertexFactoryUserData = &user_data;
        batch_element.IndexBuffer = nullptr;
        batch_element.NumPrimitives = 0;
        batch_element.BaseVertexIndex = 0;
        batch_element.PrimitiveUniformBuffer = nullptr;
        batch_element.PrimitiveIdMode = PrimID_ForceZero;

        // auto compute_dispatch_interface = get_compute_dispatch_interface();
        // check(compute_dispatch_interface);
        // auto& count_manager = compute_dispatch_interface->get_gpu_instance_counter_manager();
        // auto indirect_draw = count_manager.AddDrawIndirecct();

        // auto indirect_draw = add_draw_indirect(
        //     collector.GetRHICommandList(),
        //     INDEX_NONE,
        //     0,
        //     batch_element.FirstIndex,
        //     false,
        //     ETerrainGpuComputeTickStage::PreInitViews
        // );
        batch_element.IndirectArgsBuffer = view_state.indirect_args_buffer->GetRHI();
        batch_element.IndirectArgsOffset = view_state.indirect_args_offset;

        mesh.MaterialRenderProxy = material_proxy;
        mesh.Type = PT_TriangleStrip;
        mesh.bCanApplyViewModeOverrides = true;
        mesh.bUseForMaterial = true;
        mesh.bUseForDepthPass = true;
        mesh.bWireframe = bDebugWireframe;
        mesh.bUseWireframeSelectionColoring = IsSelected();

        if (terrain::scene_proxy::detail::should_probe_gpu_mesh()) {
            static bool logged_gpu_mesh_submission = false;
            if (!logged_gpu_mesh_submission) {
                logged_gpu_mesh_submission = true;
                UE_LOGFMT(
                    LogTemp,
                    Display,
                    "[UDLOD.MeshProbe] Submitted GPU terrain mesh for view={0} with max_vertex_index={1} indirect_args_buffer={2}.",
                    view->GetViewKey(),
                    view_state.max_vertex_index,
                    reinterpret_cast<uint64>(view_state.indirect_args_buffer->GetRHI())
                );
            }
        }

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