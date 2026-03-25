#pragma once

#include "ext_math.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "terrain_config.h"
#include "terrain_settings.h"
#include "terrain_shader_helpers.h"
#include "terrain_tile_state.h"
#include "Logging/StructuredLog.h"

struct FTileAtlas {
    FTileAtlas() = delete;

    FTileAtlas(
        const FTerrainConfig& config,
        const FTerrainSettings& settings
    ) : attachments{
            ext::iter::map(
                config.attachments,
                [&config](
                FString name,
                const FAttachmentConfig& attachment_config)
                -> TTuple<FString, FAttachment> {
                    return {
                        name,
                        FAttachment{attachment_config, config.path}
                    };
                })
        },
        unused_indices{
            ext::iter::range<TDeque<uint32>, uint32>(0u, settings.atlas_size)
        },
        existing_tiles{config.tiles},
        lod_count{static_cast<uint32>(config.lod_count)},
        max_height{config.max_height},
        min_height{config.min_height},
        height_scale{128.0},
        side_length{config.side_length},
        terrain_buffer{nullptr} {}

    TMap<FString, FAttachment> attachments;
    TMap<FTileCoordinate, FTileLoadingState> tile_states;
    TDeque<uint32> unused_indices;
    TSet<FTileCoordinate> existing_tiles;
    TArray<FAttachmentTileWithData> uploading_tiles;
    TArray<FAttachmentTileWithData> downloading_tiles;
    TArray<FAttachmentTile> to_load;
    TMap<FAttachmentTile, FAttachmentTileData> loaded_tile_data;

    uint32 lod_count;
    float max_height;
    float min_height;
    float height_scale;
    double side_length;

    // TODO: where is this buffer instantiated?
    FRDGUniformBufferRef terrain_buffer;

    friend bool operator==(const FTileAtlas& a, const FTileAtlas& b) {
        if (
            a.lod_count != b.lod_count ||
            a.max_height != b.max_height ||
            a.min_height != b.min_height ||
            a.height_scale != b.height_scale ||
            a.side_length != b.side_length
        ) { return false; }

        const bool attachments_equal = a.attachments.Num() == b.attachments.Num();
        const bool tile_states_equal = a.tile_states.Num() == b.tile_states.Num();
        const bool existing_tiles_equal = a.existing_tiles.Num() == b.existing_tiles.Num();
        if (!attachments_equal || !tile_states_equal || !existing_tiles_equal) { return false; }

        if (existing_tiles_equal) {
            for (const auto& tile : a.existing_tiles) {
                if (!b.existing_tiles.Contains(tile)) { return false; }
            }
        }

        if (attachments_equal) {
            for (const auto& [key, value] : a.attachments) {
                const auto* other_value = b.attachments.Find(key);
                if (other_value == nullptr || *other_value != value) { return false; }
            }
        }

        if (tile_states_equal) {
            for (const auto& [key, value] : a.tile_states) {
                const auto* other_value = b.tile_states.Find(key);
                if (other_value == nullptr || *other_value != value) { return false; }
            }
        }

        return true;
    }

    friend bool operator!=(const FTileAtlas& a, const FTileAtlas& b) { return !(a == b); }

    TileTreeEntry get_best_tile(
        const FTileCoordinate& tile_coordinate
    ) {
        auto best_tile_coordinate = tile_coordinate;
        const auto default_tt = [] -> TileTreeEntry {
            TileTreeEntry tt;
            tt.atlas_index = MAX_uint32;
            tt.atlas_lod = MAX_uint32;
            return tt;
        }();

        while (true) {
            if (best_tile_coordinate == FTileCoordinate::INVALID()) { return default_tt; }

            if (!existing_tiles.Contains(best_tile_coordinate)) {
                best_tile_coordinate = best_tile_coordinate.parent().Get(
                    FTileCoordinate::INVALID());
                continue;
            }

            const auto tile = tile_states.Find(best_tile_coordinate);
            if (tile != nullptr && tile->state.is_loaded) {
                TileTreeEntry tt;
                tt.atlas_index = tile->atlas_index;
                tt.atlas_lod = best_tile_coordinate.lod;
                return tt;
            }

            best_tile_coordinate = best_tile_coordinate.parent().Get(FTileCoordinate::INVALID());
        }
    }

    void tile_loaded(
        const FAttachmentTile& tile,
        const FAttachmentTileData& data
    ) {
        auto* tile_state = tile_states.Find(tile.coordinate);
        if (tile_state == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Ignoring loaded tile with no tracked state: {tc}",
                tile.coordinate.to_string());
            return;
        }

        if (tile_state->state.loading == 1) {
            tile_state->state = FLoadingState{true, 0};
            // UE_LOGFMT(
            //     LogTemp,
            //     Log,
            //     "Tile loaded: {tc}, atlas index: {ai}",
            //     tile.coordinate.to_string(),
            //     tile_state->atlas_index);
        } else if (
            const auto& n = tile_state->state.loading; n > 1) {
            tile_state->state.loading = n - 1;
            // UE_LOGFMT(
            //     LogTemp,
            //     Log,
            //     "Tile loaded: {tc}, atlas index: {ai}, remaining loading count: {n}",
            //     tile.coordinate.to_string(),
            //     tile_state->atlas_index,
            //     tile_state->state.loading);
        } else {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Ignoring duplicate tile load for {tc}",
                tile.coordinate.to_string());
            return;
        }

        loaded_tile_data.Add(tile, data);
        // UE_LOGFMT(
        //     LogTemp,
        //     Log,
        //     "Added loaded tile data to atlas for tile {tc}, label: {label}",
        //     tile.coordinate.to_string(),
        //     tile.label);
        uploading_tiles.Push(
            FAttachmentTileWithData{
                tile_state->atlas_index,
                tile.label,
                data
            }
        );
        // UE_LOGFMT(
        //     LogTemp,
        //     Log,
        //     "Scheduled tile for upload for tile {tc}, label: {label}, atlas index: {ai}",
        //     tile.coordinate.to_string(),
        //     tile.label,
        //     tile_state->atlas_index);
    }

    static void update_terrain_buffer(
        FRDGBuilder& gb,
        TArray<TPair<FTileAtlas, FTransform>>& tile_atlases
    ) {
        uint32 counter = 0;
        for (const TPair<FTileAtlas, FTransform>& tile_atlas_data : tile_atlases) {
            const FTileAtlas& tile_atlas = tile_atlas_data.Key;
            const FTransform& transform = tile_atlas_data.Value;
            gb.AddPass(
                RDG_EVENT_NAME("UDLOD.UpdateTerrainBuffer.%d", counter++),
                ERDGPassFlags::NeverCull | ERDGPassFlags::Raster,
                [tile_atlas, transform](FRHICommandList& cmd_list) {
                    const Terrain t = new_terrain(
                        tile_atlas.lod_count,
                        ext::math::scale(tile_atlas.side_length),
                        tile_atlas.max_height,
                        tile_atlas.min_height,
                        tile_atlas.height_scale,
                        transform
                    );
                    cmd_list.UpdateUniformBuffer(
                        reinterpret_cast<FRHIUniformBuffer*>(tile_atlas.terrain_buffer),
                        &t
                    );
                }
            );
        }
    }

    void request_tile(FTileCoordinate tile_coordinate) {
        // UE_LOGFMT(
        //     LogTemp,
        //     Log,
        //     "Requesting tile {tc}",
        //     tile_coordinate.to_string());

        if (!existing_tiles.Contains(tile_coordinate)) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Trying to request a tile that does not exist: {tc}",
                tile_coordinate.to_string());
            return;
        }

        if (FTileLoadingState* tile = tile_states.Find(tile_coordinate)) {
            // FString logstr = "";
            if (tile->requests == 0) {
                // logstr += " (was not previously requested)";
                ext::iter::retain<uint32>(
                    unused_indices,
                    [tile](const uint32 atlas_index) { return atlas_index != tile->atlas_index; });
            }

            tile->requests += 1;
            // logstr += FString::Printf(TEXT(", total requests: %d"), tile->requests);
            // UE_LOGFMT(
            //     LogTemp,
            //     Log,
            //     "Tile {tc} is already loading or loaded, incrementing request count{logstr}",
            //     tile_coordinate.to_string(),
            //     logstr);
        } else {
            uint32 atlas_index;
            if (!unused_indices.TryPopFirst(atlas_index)) {
                UE_LOGFMT(
                    LogTemp,
                    Fatal,
                    "No more space in the tile atlas!");
            }

            ext::iter::retain(
                tile_states,
                [&atlas_index](const FTileCoordinate&, const FTileLoadingState& t) {
                    return t.atlas_index != atlas_index;
                });

            tile_states.Emplace(
                tile_coordinate,
                FTileLoadingState{
                    .state = FLoadingState{
                        .is_loaded = false,
                        .loading = static_cast<uint32>(attachments.Num())
                    },
                    .atlas_index = atlas_index,
                    .requests = 1
                }
            );

            // UE_LOGFMT(
            //     LogTemp,
            //     Log,
            //     "Tile {tc} is not loaded, starting load with atlas index {ai}",
            //     tile_coordinate.to_string(),
            //     atlas_index);

            TArray<FString> keys;
            attachments.GetKeys(keys);
            // FString logstr;
            for (const auto& label : keys) {
                to_load.Emplace(tile_coordinate, label);
                // logstr += FString::Printf(TEXT("%s, "), *label);
            }

            // UE_LOGFMT(
            //     LogTemp,
            //     Log,
            //     "Requested attachments for tile {tc}: {labels}",
            //     tile_coordinate.to_string(),
            //     logstr);
        }
    }

    void release_tile(const FTileCoordinate& tile_coordinate) {
        if (!existing_tiles.Contains(tile_coordinate)) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Trying to release a tile that does not exist: {tc}",
                tile_coordinate.to_string());
            return;
        }

        auto* state = tile_states.Find(tile_coordinate);
        if (state == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Trying to release a tile that is not loaded or loading: {tc}",
                tile_coordinate.to_string());
            return;
        }

        if (state->requests == 0) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Trying to release a tile with no active requests: {tc}",
                tile_coordinate.to_string());
            return;
        }

        state->requests -= 1;
        // UE_LOGFMT(
        //     LogTemp,
        //     Log,
        //     "Releasing tile {tc}, remaining requests: {n}",
        //     tile_coordinate.to_string(),
        //     state->requests);
        if (state->requests == 0) { unused_indices.PushLast(state->atlas_index); }
    }
};

inline uint32 GetTypeHash(const FTileAtlas& tile_atlas) {
    uint32 hash = 0;
    for (const auto& [key, value] : tile_atlas.attachments) {
        hash = HashCombine(hash, GetTypeHash(key));
        hash = HashCombine(hash, GetTypeHash(value));
    }
    for (const auto& [key, value] : tile_atlas.tile_states) {
        hash = HashCombine(hash, GetTypeHash(key));
        hash = HashCombine(hash, GetTypeHash(value));
    }
    for (const auto& tile : tile_atlas.existing_tiles) {
        hash = HashCombine(hash, GetTypeHash(tile));
    }
    hash = HashCombine(hash, GetTypeHash(tile_atlas.lod_count));
    hash = HashCombine(hash, GetTypeHash(tile_atlas.max_height));
    hash = HashCombine(hash, GetTypeHash(tile_atlas.min_height));
    hash = HashCombine(hash, GetTypeHash(tile_atlas.height_scale));
    hash = HashCombine(hash, GetTypeHash(tile_atlas.side_length));
    return hash;
}
