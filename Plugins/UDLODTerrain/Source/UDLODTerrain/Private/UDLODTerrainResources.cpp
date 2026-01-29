#include "UDLODTerrainResources.h"

#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"

/// @brief triangle strip per row with 2 degenerate indices between rows.
/// Foreach of (n-1) rows: (2*n) indices + (2 degenerates) except last row.
uint32 builds_strip_index_count(const uint32 n) {
    if (n < 2) { return 0; }

    const uint32 row = 2 * n;
    constexpr uint32 degens = 2;
    return (n - 1) * row + (n - 2) * degens;
}

void FUDLODTerrainResources::init(
    const uint32 in_grid_resolution, const uint32 in_max_tiles, FRHIComputeCommandList& cmd
) {
    grid_resolution = in_grid_resolution;
    build_grid_buffers(in_grid_resolution, cmd);
    allocate_tile_buffers(in_max_tiles);
}

void FUDLODTerrainResources::release() {
    working_tiles_a.SafeRelease();
    working_tiles_b.SafeRelease();
    final_tiles.SafeRelease();
    working_count_a.SafeRelease();
    working_count_b.SafeRelease();
    final_count.SafeRelease();
    draw_args.SafeRelease();

    grid_vb.SafeRelease();
    grid_ib.SafeRelease();
    vertex_decl.SafeRelease();

    // Reset "initialized" markers
    grid_resolution = 0;
    index_count = 0;
}

void FUDLODTerrainResources::build_grid_buffers(const uint32 n, FRHIComputeCommandList& cmd) {
    TArray<FGridVertex> verts;
    verts.SetNumUninitialized(n * n);

    for (uint32 y = 0; y < n; ++y) {
        for (uint32 x = 0; x < n; ++x) {
            const float u = n > 1 ? static_cast<float>(x) / static_cast<float>(n - 1) : 0.;
            const float v = n > 1 ? static_cast<float>(y) / static_cast<float>(n - 1) : 0.;
            verts[y * n + x].uv = FVector2f(u, v);
        }
    }

    TArray<uint32> indices;
    index_count = builds_strip_index_count(n);
    indices.Reserve(index_count);

    for (uint32 y = 0; y < n - 1; ++y) {
        if (y > 0) {
            // degenerate: repeat first vertex of row
            indices.Add(y * n);
        }

        for (uint32 x = 0; x < n; ++x) {
            // NOTE: this +0 is left in as a marker for clarity
            indices.Add((y + 0) * n + x);
            indices.Add((y + 1) * n + x);
        }

        if (y < n - 2) {
            // degenerate: repeat last vertex of row
            indices.Add((y + 1) * n + (n - 1));
        }
    }

    // Create vertex buffer
    {
        auto name = TEXT("UDLOD_GridVB");
        grid_vb = [&name, &verts, &cmd] {
            auto desc = FRHIBufferCreateDesc::CreateVertex<FGridVertex>(name, verts.Num());
            desc.InitialState = ERHIAccess::WriteOnlyMask;
            auto obuf = cmd.CreateBuffer(desc);
            void* ptr = cmd.LockBuffer(obuf, 0, verts.GetTypeSize() * verts.Num(), RLM_WriteOnly);
            FMemory::Memcpy(ptr, verts.GetData(), verts.GetTypeSize() * verts.Num());
            cmd.UnlockBuffer(obuf);
            return obuf;
        }();
    }

    // Create index buffer
    {
        auto name = TEXT("UDLOD_GridIB");
        grid_ib = [&name, &indices, &cmd] {
            auto desc = FRHIBufferCreateDesc::CreateIndex<uint32>(name, indices.Num());
            desc.InitialState = ERHIAccess::WriteOnlyMask;
            auto obuf = cmd.CreateBuffer(desc);
            void* ptr = cmd.LockBuffer(obuf, 0, indices.GetTypeSize() * indices.Num(), RLM_WriteOnly);
            FMemory::Memcpy(ptr, indices.GetData(), indices.GetTypeSize() * indices.Num());
            cmd.UnlockBuffer(obuf);
            return obuf;
        }();
        create_buffer<uint32>(cmd, indices, TEXT("UDLOD_GridIB"));
    }

    FVertexDeclarationElementList elements{};
    constexpr uint8 stream_index = 0;
    constexpr uint8 offset = 0;
    constexpr EVertexElementType type = VET_Float2;
    constexpr uint8 attribute_index = 0;
    constexpr uint16 stride = sizeof(FGridVertex);
    elements.Add(FVertexElement{stream_index, offset, type, attribute_index, stride});

    vertex_decl = RHICreateVertexDeclaration(elements);
    check(vertex_decl.IsValid());
}

void FUDLODTerrainResources::allocate_tile_buffers(const uint32 max_tiles) {
    const FRDGBufferDesc tile_desc = FRDGBufferDesc::CreateStructuredDesc(sizeof(FUDLODTileGpu), max_tiles);
    const FRDGBufferDesc count_desc = FRDGBufferDesc::CreateByteAddressDesc(4);
    const FRDGBufferDesc args_desc = FRDGBufferDesc::CreateIndirectDesc(5);

    AllocatePooledBuffer(tile_desc, working_tiles_a, TEXT("UDLOD_WorkingTilesA"), ERDGPooledBufferAlignment::None);
    AllocatePooledBuffer(tile_desc, working_tiles_b, TEXT("UDLOD_WorkingTilesB"), ERDGPooledBufferAlignment::None);
    AllocatePooledBuffer(tile_desc, final_tiles, TEXT("UDLOD_FinalTiles"), ERDGPooledBufferAlignment::None);

    AllocatePooledBuffer(count_desc, working_count_a, TEXT("UDLOD_WorkingCountA"), ERDGPooledBufferAlignment::None);
    AllocatePooledBuffer(count_desc, working_count_b, TEXT("UDLOD_WorkingCountB"), ERDGPooledBufferAlignment::None);
    AllocatePooledBuffer(count_desc, final_count, TEXT("UDLOD_FinalCount"), ERDGPooledBufferAlignment::None);

    AllocatePooledBuffer(args_desc, draw_args, TEXT("UDLOD_DrawArgs"), ERDGPooledBufferAlignment::None);
}
