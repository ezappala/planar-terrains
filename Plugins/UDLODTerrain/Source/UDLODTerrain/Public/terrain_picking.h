#pragma once
#include "SceneView.h"
#include "terrain_shaders.h"
#include "Logging/StructuredLog.h"

inline PickingData* picking(
    FRDGBuilder& gb,
    const FSceneView& view,
    const FRDGBufferRef picking_data_readback_buffer) {
    const auto cursor_coords = view.CursorPos;

    const FMatrix44d view_from_world(view.ViewMatrices.GetViewMatrix());
    const FMatrix44d clip_from_view(view.ViewMatrices.GetProjectionNoAAMatrix());
    const FMatrix44d clip_from_world = clip_from_view * view_from_world;
    const FMatrix44f world_from_clip = FMatrix44f(clip_from_world.Inverse());

    auto* data = gb.AllocParameters<PickingData>();
    data->cursor_cords = cursor_coords;
    data->depth = 0.0f;
    data->stencil = 255;
    data->world_from_clip = world_from_clip;

    gb.QueueBufferUpload(
        picking_data_readback_buffer,
        data,
        sizeof(PickingData)
    );

    return data;
}

struct FPickingData {
    FPickingData(FRDGBuilder& gb) : buffer{
        gb.CreateBuffer(
            FRDGBufferDesc::CreateStructuredDesc<PickingData>(1),
            TEXT("UDLOD .PickingDataReadbackBuffer")
        )
    } {}

    FVector2f cursor_coords;
    TOptional<FVector3f> translation;
    FMatrix44f world_from_clip;
    FRDGBufferRef buffer;
};

inline void picking_readback(
    FRDGBuilder& gb,
    const FRDGBufferRef buffer,
    TSharedPtr<FPickingData> inout_picking_data
) {
    if (inout_picking_data == nullptr) { inout_picking_data = MakeShared<FPickingData>(gb); }

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

    auto callback = [&inout_picking_data](const PickingData& data) {
        const auto ncd_coords = FVector3f(2. * data.cursor_cords - 1.0f, data.depth);
        inout_picking_data->cursor_coords = data.cursor_cords;
        if (data.depth > 0.0) {
            inout_picking_data->translation = FVector3f(
                data.world_from_clip.TransformPosition(ncd_coords));
        } else { inout_picking_data->translation = NullOpt; }
        inout_picking_data->world_from_clip = data.world_from_clip;
    };

    auto runner = [buffer_readback, callback](auto&& fn) -> void {
        if (buffer_readback->IsReady()) {
            const PickingData* data = static_cast<PickingData*>(buffer_readback->Lock(1));
            PickingData out_data = *data;
            buffer_readback->Unlock();
            AsyncTask(
                ENamedThreads::GameThread,
                [callback, out_data] { callback(out_data); });
            delete buffer_readback;
        } else { AsyncTask(ENamedThreads::ActualRenderingThread, [fn] { fn(fn); }); }
    };
    AsyncTask(ENamedThreads::ActualRenderingThread, [runner] { runner(runner); });
}
