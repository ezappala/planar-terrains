#pragma once
#include "PrimitiveSceneProxy.h"
#include "terrain_mesh_vertex_factory.h"
#include "terrain_render_state.h"

// #define TERRAIN_DRAW_INDIRECT_ARGS_GEN_THREAD_COUNT 64
// #define TERRAIN_DRAW_INDIRECT_ARGS_SIZE 5
// #define TERRAIN_DRAW_INDIRECT_TASK_INFO_SIZE 5
// #define TERRAIN_INIT_INSTANCE_COUNT_TASK_INFO_SIZE 2

// struct FTerrainDrawIndirectArgGenTaskInfo {
//     explicit FTerrainDrawIndirectArgGenTaskInfo(
//         const uint32 o,
//         const uint32 n,
//         const uint32 i) : indirect_args_buffer_offset{INDEX_NONE},
//         instance_count_buffer_offset{o},
//         num_indices_per_instance{n},
//         start_index_location{i} {}
//
//     uint32 indirect_args_buffer_offset;
//     uint32 instance_count_buffer_offset;
//     uint32 num_indices_per_instance;
//     uint32 start_index_location;
//
//     bool operator==(const FTerrainDrawIndirectArgGenTaskInfo& rhs) const {
//         return instance_count_buffer_offset == rhs.instance_count_buffer_offset &&
//             num_indices_per_instance == rhs.num_indices_per_instance &&
//             start_index_location == rhs.start_index_location;
//     }
// };

class UTerrain;
// class FTerrainSystemInterface : public TSharedFromThis<FTerrainSystemInterface> {
//     struct FTerrainInterfaceDeleter {
//         void operator()(FTerrainSystemInterface* obj) const;
//     };
//
// public:
//     UDLODTERRAIN_API static FTerrainSystemInterface* Create(
//         ERHIFeatureLevel::Type in_feature_level,
//         FSceneInterface* scene);
// };
//
// class FTerrainGpuComputeDispatchInterface : public FTerrainSystemInterface {
// public:
//     DECLARE_EVENT_OneParam(FTerrainGpuComputeDispatchInterface, FOnPreInitViewsEvent, FRDGBuilder&);
//     DECLARE_EVENT_OneParam(FTerrainGpuComputeDispatchInterface, FPnPostRenderEvent, FRDGBuilder&)
//
//     static UDLODTERRAIN_API FTerrainGpuComputeDispatchInterface* Get(UWorld* world);
//     static UDLODTERRAIN_API FTerrainGpuComputeDispatchInterface* Get(FSceneInterface* scene);
//     static UDLODTERRAIN_API FTerrainGpuComputeDispatchInterface* Get(
//         FTerrainSystemInterface* terrain_scene_interface);
//     UDLODTERRAIN_API explicit FTerrainGpuComputeDispatchInterface(
//         EShaderPlatform in_shader_platform,
//         ERHIFeatureLevel::Type in_feature_level);
//     UDLODTERRAIN_API virtual ~FTerrainGpuComputeDispatchInterface();
//     EShaderPlatform get_shader_platform() const { return shader_platform; }
//     ERHIFeatureLevel::Type get_feature_level() const { return feature_level; }
// };
// namespace ETerrainGpuComputeTickStage = ENiagaraGpuComputeTickStage;
// namespace ETerrainGpuCountUpdatePhase = ENiagaraGPUCountUpdatePhase;
class FTerrainSceneProxy final : public FPrimitiveSceneProxy {
public:
    FTerrainSceneProxy(const UTerrain* component);
    virtual ~FTerrainSceneProxy() override;

    virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override;
    virtual void DestroyRenderThreadResources() override;

    // FTerrainGpuComputeDispatchInterface* get_compute_dispatch_interface() const {
    //     return _compute_dispatch_interface;
    // };

    // struct FIndirectArgSlot {
    //     FBufferRHIRef buffer;
    //     FShaderResourceViewRHIRef srv;
    //     uint32 offset = INDEX_NONE;
    //     FIndirectArgSlot() = default;
    //
    //     FIndirectArgSlot(
    //         const FBufferRHIRef& b,
    //         const FShaderResourceViewRHIRef& s,
    //         const uint32 o) : buffer(b),
    //         srv(s),
    //         offset(o) {}
    //
    //     bool is_valid() const { return offset != INDEX_NONE; }
    // };

    // static const ERHIAccess INDIRECT_ARGS_DEFAULT_STATE;

    // FIndirectArgSlot add_draw_indirect(
    //     FRHICommandListBase& cmd,
    //     uint32 instance_count_buffer_offset,
    //     uint32
    //     num_indices_per_instance,
    //     uint32 start_index_location,
    //     bool culled,
    //     ETerrainGpuComputeTickStage::Type ready_tick_stage);

    virtual void GetDynamicMeshElements(
        const TArray<const FSceneView*>& views,
        const FSceneViewFamily& view_family,
        uint32 visibility_map,
        FMeshElementCollector& collector
    ) const override;
    virtual SIZE_T GetTypeHash() const override;
    virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* view) const override;
    virtual uint32 GetMemoryFootprint() const override;

    // struct FTerrainDrawIndirectArgGenSlotInfo {
    //     uint32 pool_idx = INDEX_NONE;
    //     uint32 buffer_offset = INDEX_NONE;
    // };
    // TMap<FTerrainDrawIndirectArgGenTaskInfo, FTerrainDrawIndirectArgGenSlotInfo>
    // draw_indirect_args_map;
    // TArray<FTerrainDrawIndirectArgGenTaskInfo> draw_indirect_arg_gen_tasks[
    //     ETerrainGpuCountUpdatePhase::Max];

    // struct FIndirectArgsPoolEntry {
    //     FRWBuffer buffer;
    //     uint32 allocated_entries = 0;
    //     uint32 used_entries_total = 0;
    //     uint32 used_entries[ETerrainGpuCountUpdatePhase::Max] = {};
    // };
    // using FIndirectArgsPoolEntryPtr = TUniquePtr<FIndirectArgsPoolEntry>;
    // TArray<FIndirectArgsPoolEntryPtr> draw_indirect_pool;

private:
    const FMaterialRenderProxy* material_proxy = nullptr;
    FMaterialRelevance material_relevance;
    TSharedPtr<FTerrainRenderResources> render_resources;
    bool bDebugWireframe = false;
    FTerrainMeshVertexFactory vertex_factory;
    // UE::FMutex add_draw_indirect_guard;

    // FTerrainGpuComputeDispatchInterface* _compute_dispatch_interface = nullptr;

    friend class UTerrain;
};
