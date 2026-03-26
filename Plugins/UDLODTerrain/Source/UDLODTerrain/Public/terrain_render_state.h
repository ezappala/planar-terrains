#pragma once

#include "MeshBatch.h"
#include "RenderGraphResources.h"
#include "RHIResources.h"
#include "terrain_shaders.h"
#include "Containers/Map.h"
#include "Misc/ScopeRWLock.h"

struct FTerrainMeshViewState {
    TUniformBufferRef<Terrain> terrain_uniform_buffer;
    FShaderResourceViewRHIRef attachments_buffer_srv;
    FSamplerStateRHIRef terrain_sampler;
    FTextureRHIRef height_attachment_texture;
    FTextureRHIRef albedo_attachment_texture;
    FShaderResourceViewRHIRef terrain_view_buffer_srv;
    FShaderResourceViewRHIRef approximate_height_buffer_srv;
    FShaderResourceViewRHIRef tile_tree_buffer_srv;
    FShaderResourceViewRHIRef geometry_tiles_buffer_srv;
    TRefCountPtr<FRDGPooledBuffer> indirect_args_buffer;
    uint32 indirect_args_offset = 0;
    uint32 max_vertex_index = 0;

    bool IsReady() const {
        return terrain_uniform_buffer.IsValid() &&
            attachments_buffer_srv.IsValid() &&
            terrain_sampler.IsValid() &&
            height_attachment_texture.IsValid() &&
            albedo_attachment_texture.IsValid() &&
            terrain_view_buffer_srv.IsValid() &&
            approximate_height_buffer_srv.IsValid() &&
            tile_tree_buffer_srv.IsValid() &&
            geometry_tiles_buffer_srv.IsValid() &&
            indirect_args_buffer.IsValid();
    }
};

using FTerrainMeshBatchElementUserData = FTerrainMeshViewState;

class FTerrainRenderResources {
public:
    void ResetViewStates() {
        FWriteScopeLock lock(view_states_guard);
        view_states.Reset();
    }

    void UpdateViewState(const uint32 view_key, const FTerrainMeshViewState& view_state) {
        FWriteScopeLock lock(view_states_guard);
        view_states.FindOrAdd(view_key) = view_state;
    }

    bool TryGetViewState(const uint32 view_key, FTerrainMeshViewState& out_view_state) const {
        FReadScopeLock lock(view_states_guard);
        const FTerrainMeshViewState* view_state = view_states.Find(view_key);
        if (view_state == nullptr) {
            return false;
        }

        out_view_state = *view_state;
        return out_view_state.IsReady();
    }

private:
    mutable FRWLock view_states_guard;
    TMap<uint32, FTerrainMeshViewState> view_states;
};
