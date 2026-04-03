#pragma once

#include "gpu_terrain_attachment.h"
#include "gpu_terrain_tile_atlas.h"
#include "terrain_shaders.h"

inline AttachmentConfig new_attachment_config(const FGpuAttachment& attachment) {
    AttachmentConfig ac;
    ac.texture_size = static_cast<float>(attachment.buffer_info.texture_size);
    ac.center_size = static_cast<float>(attachment.buffer_info.center_size);
    ac.scale = static_cast<float>(attachment.buffer_info.center_size) / static_cast<float>(
        attachment.buffer_info.texture_size);
    ac.offset = static_cast<float>(attachment.buffer_info.border_size) / static_cast<float>(
        attachment.buffer_info.texture_size);
    ac.mask = attachment.buffer_info.mask ? 1u : 0u;
    ac.paddinga = 0u;
    ac.paddingb = 0u;
    ac.paddingc = 0u;

    return ac;
}

inline const FGpuAttachment* find_attachment_by_label(
    const TMap<FString, FGpuAttachment>& attachments,
    const FString& label
) {
    if (const FGpuAttachment* attachment = attachments.Find(label)) {
        return attachment;
    }

    for (const auto& [key, attachment] : attachments) {
        if (key.Equals(label, ESearchCase::IgnoreCase)) {
            return &attachment;
        }
    }

    return nullptr;
}

inline AttachmentConfig attachment_config_from_gpu_tile_atlas(
    TOptional<FGpuTileAtlas>& tile_atlas,
    const FString& label
) {
    if (!tile_atlas.IsSet()) {
        UE_LOGFMT(
            LogTemp,
            Error,
            "GPU tile atlas is not set; cannot find attachment with label {n}",
            label
        );
        return AttachmentConfig{};
    }

    const FGpuAttachment* attachment = find_attachment_by_label(tile_atlas->attachments, label);
    if (attachment == nullptr) {
        UE_LOGFMT(
            LogTemp,
            Error,
            "Attachment with label {n} not found in GPU tile atlas; using default config",
            label
        );
        return AttachmentConfig{};
    }

    return new_attachment_config(*attachment);
}
