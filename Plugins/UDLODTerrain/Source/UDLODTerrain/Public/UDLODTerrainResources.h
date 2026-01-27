#pragma once

#include "RenderGraphResources.h"
#include "RHIFwd.h"
#include "Math/MathFwd.h"
#include "Math/Vector.h"

enum : uint32 {
    UDLOD_Morph_West = 1u << 0,
    UDLOD_Morph_East = 1u << 1,
    UDLOD_Morph_South = 1u << 2,
    UDLOD_Morph_North = 1u << 3,
};

struct alignas(16) FUDLODTileGpu {
    uint32 lod;
    uint32 x;
    uint32 y;
    uint32 morph_mask;
    FVector3f center_tws;
    float radius;
};

static_assert(sizeof(FUDLODTileGpu) == 32);

struct FUDLODTerrainSettingsRT {
    FVector2f world_min_xy_ws = FVector2f(0., 0.);
    FVector2f world_size_xy_ws = FVector2f(10000., 10000.);

    float height_min_ws = 0.;
    float height_max_ws = 20000.;

    /// @brief vertices per side per tile
    uint32 grid_resolution = 65;
    uint32 max_lod = 10;
    /// @brief hard cap for buffer sizes
    uint32 max_tiles = 262144;

    /// @details τpx
    float error_pixels = 2.;
    /// @details e0(cm) -> e(L) = e0 * 2^L
    float height_error_0ws = 50.;
    /// @remarks starting morph at 1.5 * split distance
    float morph_start_ratio = 1.5;
};

template <typename T>
FBufferRHIRef create_buffer(FRHIComputeCommandList& cmd, const TArray<T>& data, const TCHAR* name) {
    auto desc = FRHIBufferCreateDesc::CreateVertex(name, data.GetTypeSize() * data.Num());
    desc.InitialState = ERHIAccess::WriteOnlyMask;
    auto obuf = cmd.CreateBuffer(desc);
    void* ptr = cmd.LockBuffer(obuf, 0, data.GetTypeSize() * data.Num(), RLM_WriteOnly);
    FMemory::Memcpy(ptr, data.GetData(), data.GetTypeSize() * data.Num());
    cmd.UnlockBuffer(obuf);
    return obuf;
}

struct FUDLODTerrainResources {
    struct FGridVertex {
        FVector2f uv;
    };

    uint32 grid_resolution = 0;
    uint32 index_count = 0;

    FBufferRHIRef grid_vb;
    FBufferRHIRef grid_ib;
    FVertexDeclarationRHIRef vertex_decl;

    // Persistent pooled RDG buffers; multi-frame
    TRefCountPtr<FRDGPooledBuffer> working_tiles_a;
    TRefCountPtr<FRDGPooledBuffer> working_tiles_b;
    TRefCountPtr<FRDGPooledBuffer> final_tiles;

    /// @remarks a RWByteAddressBuffer(4)
    TRefCountPtr<FRDGPooledBuffer> working_count_a;
    TRefCountPtr<FRDGPooledBuffer> working_count_b;
    TRefCountPtr<FRDGPooledBuffer> final_count;

    /// @remarks indirect args (5 uints)
    TRefCountPtr<FRDGPooledBuffer> draw_args;

    void init(uint32 in_grid_resolution, uint32 in_max_tiles, FRHIComputeCommandList& cmd);
    void release();

private:
    void build_grid_buffers(uint32 n, FRHIComputeCommandList& cmd);
    void allocate_tile_buffers(uint32 max_tiles);
};
