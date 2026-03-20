#pragma once

struct FLoadingState {
    bool is_loaded;
    uint32 loading;

    friend bool operator==(const FLoadingState& a, const FLoadingState& b) {
        return a.is_loaded == b.is_loaded && a.loading == b.loading;
    }

    friend bool operator!=(const FLoadingState& a, const FLoadingState& b) {
        return !(a == b);
    }
};

FORCEINLINE uint32 GetTypeHash(const FLoadingState& loading_state) {
    uint32 hash = 0;
    hash = HashCombine(hash, GetTypeHash(loading_state.is_loaded));
    hash = HashCombine(hash, GetTypeHash(loading_state.loading));
    return hash;
}

struct FTileLoadingState {
    FLoadingState state;
    uint32 atlas_index;
    uint32 requests;

    friend bool operator==(const FTileLoadingState& a, const FTileLoadingState& b) {
        return a.state == b.state && a.atlas_index == b.atlas_index && a.requests == b.requests;
    }

    friend bool operator!=(const FTileLoadingState& a, const FTileLoadingState& b) {
        return !(a == b);
    }
};

FORCEINLINE uint32 GetTypeHash(const FTileLoadingState& tile_state) {
    uint32 hash = 0;
    hash = HashCombine(hash, GetTypeHash(tile_state.state));
    hash = HashCombine(hash, GetTypeHash(tile_state.atlas_index));
    hash = HashCombine(hash, GetTypeHash(tile_state.requests));
    return hash;
}


