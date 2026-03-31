#include "terrain_tile_loader.h"

#include "Async/Async.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace terrain::tile_loader {
namespace {
struct FAsyncTileLoadKey {
    const FTileAtlas* atlas = nullptr;
    FAttachmentTile tile;

    friend bool operator==(const FAsyncTileLoadKey& a, const FAsyncTileLoadKey& b) {
        return a.atlas == b.atlas && a.tile == b.tile;
    }
};

FORCEINLINE uint32 GetTypeHash(const FAsyncTileLoadKey& key) {
    return HashCombine(GetTypeHash(key.atlas), GetTypeHash(key.tile));
}

struct FAsyncTileLoadResult {
    const FTileAtlas* atlas = nullptr;
    FAttachmentTile tile;
    FAttachmentTileData data;
};

class FAsyncTileLoaderState {
public:
    void enqueue(FTileAtlas* tile_atlas, const FAttachment& attachment, const FAttachmentTile& tile) {
        const FAsyncTileLoadKey key{tile_atlas, tile};

        FScopeLock _(&guard);
        if (inflight.Contains(key)) {
            return;
        }

        inflight.Emplace(
            key,
            Async(
                EAsyncExecution::ThreadPool,
                [atlas_key = static_cast<const FTileAtlas*>(tile_atlas), attachment, tile]() mutable {
                    return FAsyncTileLoadResult{
                        atlas_key,
                        tile,
                        detail::load_tile_data(attachment, tile)
                    };
                }
            )
        );
    }

    TArray<FAsyncTileLoadResult> collect_ready(const FTileAtlas* tile_atlas) {
        FScopeLock _(&guard);

        TArray<FAsyncTileLoadKey> completed_keys;
        for (auto& pair : inflight) {
            if (!pair.Value.IsReady()) {
                continue;
            }

            completed.FindOrAdd(pair.Key.atlas).Add(pair.Value.Get());
            completed_keys.Add(pair.Key);
        }

        for (const FAsyncTileLoadKey& key : completed_keys) {
            inflight.Remove(key);
        }

        TArray<FAsyncTileLoadResult> ready_results;
        completed.RemoveAndCopyValue(tile_atlas, ready_results);
        return ready_results;
    }

private:
    FCriticalSection guard;
    TMap<FAsyncTileLoadKey, TFuture<FAsyncTileLoadResult>> inflight;
    TMap<const FTileAtlas*, TArray<FAsyncTileLoadResult>> completed;
};

FAsyncTileLoaderState& async_tile_loader() {
    static FAsyncTileLoaderState loader;
    return loader;
}
}

void pump_tile_loads(FTileAtlas* tile_atlas) {
    if (tile_atlas == nullptr) {
        return;
    }

    TArray<FAttachmentTile> pending = MoveTemp(tile_atlas->to_load);
    tile_atlas->to_load.Reset();

    for (const FAttachmentTile& tile : pending) {
        const FAttachment* attachment = tile_atlas->attachments.Find(tile.label);
        if (attachment == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[UDLODTerrain] No attachment with label {Label} found for tile {Tile}; skipping load",
                tile.label,
                tile.coordinate.to_string()
            );
            continue;
        }

        async_tile_loader().enqueue(tile_atlas, *attachment, tile);
    }

    for (const FAsyncTileLoadResult& result : async_tile_loader().collect_ready(tile_atlas)) {
        tile_atlas->tile_loaded(result.tile, result.data);
    }
}
}
