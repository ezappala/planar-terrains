#pragma once

#include "RenderGraphResources.h"
#include "terrain_shaders.h"
#include "Containers/Map.h"
#include "Misc/ScopeRWLock.h"

struct FTerrainCpuMeshVertex {
    FVector3f position = FVector3f::ZeroVector;
    FVector2f uv = FVector2f::ZeroVector;
    FVector3f tangent_x = FVector3f(1.0f, 0.0f, 0.0f);
    FVector3f tangent_y = FVector3f(0.0f, 1.0f, 0.0f);
    FVector3f tangent_z = FVector3f(0.0f, 0.0f, 1.0f);
    FColor color = FColor::White;
};

struct FTerrainCpuMeshViewState {
    TArray<FTerrainCpuMeshVertex> vertices;
    TArray<uint32> indices;

    bool IsReady() const { return vertices.Num() > 0 && indices.Num() >= 3; }
};

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
    void reset_view_states() {
        FWriteScopeLock _(view_states_guard);
        published_view_states.Reset();
        staged_view_states.Reset();
    }

    void advance_view_states() {
        FWriteScopeLock _(view_states_guard);
        published_view_states = MoveTemp(staged_view_states);
        staged_view_states.Reset();
    }

    void stage_view_state(const uint32 view_key, const FTerrainMeshViewState& view_state) {
        FWriteScopeLock _(view_states_guard);
        staged_view_states.FindOrAdd(view_key) = view_state;
    }

    bool try_get_view_state(const uint32 view_key, FTerrainMeshViewState& out_view_state) const {
        FReadScopeLock _(view_states_guard);
        const FTerrainMeshViewState* view_state = published_view_states.Find(view_key);
        if (view_state == nullptr) { return false; }

        out_view_state = *view_state;
        return out_view_state.IsReady();
    }

private:
    mutable FRWLock view_states_guard;
    TMap<uint32, FTerrainMeshViewState> published_view_states;
    TMap<uint32, FTerrainMeshViewState> staged_view_states;

public:
    void reset_cpu_view_states() {
        FWriteScopeLock _(cpu_view_states_guard);
        cpu_view_states.Reset();
    }

    void update_cpu_view_state(const uint32 view_key, const FTerrainCpuMeshViewState& view_state) {
        FWriteScopeLock _(cpu_view_states_guard);
        cpu_view_states.FindOrAdd(view_key) = view_state;
    }

    bool try_get_cpu_view_state(
        const uint32 view_key,
        FTerrainCpuMeshViewState& out_view_state) const {
        FReadScopeLock _(cpu_view_states_guard);
        const FTerrainCpuMeshViewState* view_state = cpu_view_states.Find(view_key);
        if (view_state != nullptr) {
            out_view_state = *view_state;
            if (out_view_state.IsReady()) { return true; }
        }

        for (const TPair<uint32, FTerrainCpuMeshViewState>& pair : cpu_view_states) {
            if (!pair.Value.IsReady()) { continue; }

            out_view_state = pair.Value;
            return true;
        }

        return false;
    }

private:
    mutable FRWLock cpu_view_states_guard;
    TMap<uint32, FTerrainCpuMeshViewState> cpu_view_states;
};