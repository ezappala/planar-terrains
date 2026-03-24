#pragma once

#include "ext_math.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "terrain_config.h"
#include "terrain_settings.h"
#include "terrain_shader_helpers.h"
#include "terrain_tile_state.h"

#include "terrain_tile_atlas.generated.h"

USTRUCT()
struct FTileAtlas {
    GENERATED_BODY()

    FTileAtlas() = default;

    FTileAtlas(
        const FTerrainConfig& config,
        const FTerrainSettings& settings
    ) : attachments{
            ext::iter::map(
                config.attachments,
                [&config](
                FString name,
                const FAttachmentConfig attachment_config) -> TTuple<FString, FAttachment> {
                    return TTuple<FString, FAttachment>{
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

    UPROPERTY(VisibleAnywhere)
    TMap<FString, FAttachment> attachments;

    UPROPERTY(VisibleAnywhere)
    TMap<FTileCoordinate, FTileLoadingState> tile_states;

    TDeque<uint32> unused_indices;

    UPROPERTY(VisibleAnywhere)
    TSet<FTileCoordinate> existing_tiles;

    UPROPERTY(VisibleAnywhere)
    TArray<FAttachmentTileWithData> uploading_tiles;

    UPROPERTY(VisibleAnywhere)
    TArray<FAttachmentTileWithData> downloading_tiles;

    UPROPERTY(VisibleAnywhere)
    TArray<FAttachmentTile> to_load;

    UPROPERTY(VisibleAnywhere)
    uint32 lod_count;

    UPROPERTY(VisibleAnywhere)
    float max_height;

    UPROPERTY(VisibleAnywhere)
    float min_height;

    UPROPERTY(VisibleAnywhere)
    float height_scale;

    UPROPERTY(VisibleAnywhere)
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
        const bool tile_sates_equal = a.tile_states.Num() == b.tile_states.Num();
        const bool existing_tiles_equal = a.existing_tiles.Num() == b.existing_tiles
            .Num();
        if (!attachments_equal || !tile_sates_equal || !existing_tiles_equal) { return false; }

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

        if (tile_sates_equal) {
            for (const auto& [key, value] : a.tile_states) {
                const auto* other_value = b.tile_states.Find(key);
                if (other_value == nullptr || *other_value != value) { return false; }
            }
        }

        return true;
    }

    friend bool operator!=(const FTileAtlas& a, const FTileAtlas& b) { return !(a == b); }

    TileTreeEntry get_best_tile(
        const FTileCoordinate tile_coordinate
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

            const FTileLoadingState* tile = tile_states.Find(best_tile_coordinate);
            if (tile != nullptr && tile->state.is_loaded) {
                TileTreeEntry tt;
                tt.atlas_index = tile->atlas_index;
                tt.atlas_lod = best_tile_coordinate.lod;
                return tt;
            }

            best_tile_coordinate = best_tile_coordinate
                .parent()
                .Get(FTileCoordinate::INVALID());
        }
    }

    void tile_loaded(
        const FAttachmentTile& tile,
        const TVariant<TArray<TStaticArray<uint8, 4>>, TArray<uint16>, TArray<int16>
            , TArray<TStaticArray<uint16, 2>>, TArray<float>>& data
    ) {
        FTileLoadingState* tile_state = &tile_states[tile.coordinate];
        if (tile_state->state.loading == 1) {
            tile_state->state = FLoadingState{true, 0};
        } else if (
            const auto& n = tile_state->state.loading; n >
            1) { tile_state->state.loading = n - 1; } else { checkNoEntry(); }
        uploading_tiles.Push(
            FAttachmentTileWithData{
                tile_state->atlas_index,
                tile.label,
                data
            }
        );
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

    void request_tile(const FTileCoordinate& tile_coordinate) {
        if (!existing_tiles.Contains(tile_coordinate)) { return; }

        if (tile_states.Contains(tile_coordinate)) {
            auto* tile = &tile_states[tile_coordinate];
            if (tile->requests == 0) {
                ext::iter::retain<uint32>(
                    unused_indices,
                    [&tile](const uint32 atlas_index) { return atlas_index != tile->atlas_index; });
            }

            tile->requests += 1;
        } else {
            uint32 atlas_index;
            if (!unused_indices.TryPopFirst(atlas_index)) {
                UE_LOG(
                    LogTemp,
                    Fatal,
                    TEXT("No more space in the tile atlas!"));
            }
            ext::iter::retain<FTileCoordinate, FTileLoadingState>(
                tile_states,
                [&atlas_index](const FTileCoordinate&, const FTileLoadingState& tile) {
                    return tile.atlas_index != atlas_index;
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

            TArray<FString> keys;
            attachments.GetKeys(keys);
            for (const auto& label : keys) {
                to_load.Push(
                    FAttachmentTile{
                        .coordinate = tile_coordinate,
                        .label = label
                    }
                );
            }
        }
    }

    void release_tile(const FTileCoordinate& tile_coordinate) {
        if (!existing_tiles.Contains(tile_coordinate)) { return; }

        auto* tile = &tile_states[tile_coordinate];
        tile->requests -= 1;
        if (tile->requests == 0) { unused_indices.PushLast(tile->atlas_index); }
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
