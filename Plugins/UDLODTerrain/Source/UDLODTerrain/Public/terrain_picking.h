#pragma once
#include "RHIGPUReadback.h"
#include "SceneView.h"
#include "terrain_shaders.h"
#include "Async/Async.h"
#include "Logging/StructuredLog.h"
#include "Runtime/Renderer/Internal/SceneTextures.h"

inline Picking* build_picking_parameters(
    FRDGBuilder& gb,
    const FSceneView& view,
    const FRDGBufferRef picking_data_buffer,
    const FRDGTextureSRVRef depth_texture,
    const FRDGTextureSRVRef stencil_texture,
    const FIntPoint source_extent
) {
    if (depth_texture == nullptr || stencil_texture == nullptr || picking_data_buffer == nullptr) {
        return nullptr;
    }

    if (view.CursorPos.X < 0 || view.CursorPos.Y < 0) { return nullptr; }

    if (source_extent.X <= 0 || source_extent.Y <= 0) { return nullptr; }

    const FIntPoint absolute_cursor = view.UnscaledViewRect.Min + view.CursorPos;
    const FVector2f cursor_coords{
        FMath::Clamp(
            static_cast<float>(absolute_cursor.X) / static_cast<float>(source_extent.X),
            0.0f,
            1.0f
        ),
        FMath::Clamp(
            1.0f -
            static_cast<float>(absolute_cursor.Y) / static_cast<float>(source_extent.Y),
            0.0f,
            1.0f
        )
    };

    FPickingDataUpload* data = gb.AllocPODArray<FPickingDataUpload>(1);
    data[0].cursor_coords = cursor_coords;
    data[0].depth = 0.0f;
    data[0].stencil = 255u;
    data[0].world_from_clip = FMatrix44f(view.ViewMatrices.GetInvViewProjectionMatrix());
    data[0].cell = FIntVector::ZeroValue;
    data[0].cell_padding = 0;

    gb.QueueBufferUpload(
        picking_data_buffer,
        data,
        sizeof(FPickingDataUpload)
    );

    auto* params = gb.AllocParameters<Picking>();
    params->picking_data = gb.CreateUAV(picking_data_buffer);
    params->depth_texture = depth_texture;
    params->stencil_texture = stencil_texture;

    return params;
}

struct FPickingData {
    FVector2f cursor_coords = FVector2f::ZeroVector;
    FIntVector cell = FIntVector::ZeroValue;
    TOptional<FVector3f> translation;
    FMatrix44f world_from_clip = FMatrix44f::Identity;
};

inline void picking_readback(
    FRDGBuilder& gb,
    const FRDGBufferRef buffer,
    TSharedPtr<FPickingData>& inout_picking_data
) {
    if (inout_picking_data == nullptr) { inout_picking_data = MakeShared<FPickingData>(); }

    if (buffer == nullptr) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[picking_readback] Buffer is nullptr, skipping picking readback");
        return;
    }

    FRHIGPUBufferReadback* buffer_readback =
        new FRHIGPUBufferReadback(TEXT("UDLOD.PickingDataReadback"));
    AddEnqueueCopyPass(gb, buffer_readback, buffer, 0u);

    auto callback = [&inout_picking_data](const FPickingDataUpload& data) {
        const FVector3f ndc_coords = FVector3f(2.0f * data.cursor_coords - 1.0f, data.depth);
        inout_picking_data->cursor_coords = data.cursor_coords;
        inout_picking_data->cell = data.cell;
        if (data.depth > 0.0) {
            const FVector4f world_position = data.world_from_clip.TransformFVector4(
                FVector4f(ndc_coords, 1.0f)
            );
            if (!FMath::IsNearlyZero(world_position.W)) {
                inout_picking_data->translation = FVector3f(world_position) / world_position.W;
            } else { inout_picking_data->translation = NullOpt; }
        } else { inout_picking_data->translation = NullOpt; }
        inout_picking_data->world_from_clip = data.world_from_clip;
    };

    auto runner = [buffer_readback, callback](auto&& fn) -> void {
        if (buffer_readback->IsReady()) {
            const FPickingDataUpload* data = static_cast<const FPickingDataUpload*>(
                buffer_readback->Lock(sizeof(FPickingDataUpload))
            );
            FPickingDataUpload out_data = *data;
            buffer_readback->Unlock();
            AsyncTask(
                ENamedThreads::GameThread,
                [callback, out_data] { callback(out_data); });
            delete buffer_readback;
        } else { AsyncTask(ENamedThreads::ActualRenderingThread, [fn] { fn(fn); }); }
    };
    AsyncTask(ENamedThreads::ActualRenderingThread, [runner] { runner(runner); });
}
