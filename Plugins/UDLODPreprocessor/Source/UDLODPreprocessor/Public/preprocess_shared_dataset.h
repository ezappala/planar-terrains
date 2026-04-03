#pragma once

#include <atomic>

#include "Containers/Map.h"
#include "HAL/PlatformTLS.h"
#include "Misc/ScopeRWLock.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

inline uint64 NextSharedDatasetCacheId() {
    static std::atomic<uint64> next_cache_id{1};
    return next_cache_id.fetch_add(1, std::memory_order_relaxed);
}

struct FSharedDatasetCache {
    FSharedDatasetCache() : cache_id(NextSharedDatasetCacheId()) {}

    uint64 cache_id;
    mutable FRWLock datasets_guard;
    TMap<uint32, TUniquePtr<GDALDatasetRef>> datasets;
};

struct FThreadLocalSharedDatasetEntry {
    TWeakPtr<FSharedDatasetCache> cache;
    GDALDatasetRef* dataset = nullptr;
};

struct SharedDatasetRO {
    SharedDatasetRO() : cache(MakeShared<FSharedDatasetCache>()) {}

    explicit SharedDatasetRO(FString in_path) : path(MoveTemp(in_path)),
        cache(MakeShared<FSharedDatasetCache>()) {}

    friend bool operator==(const SharedDatasetRO& a, const SharedDatasetRO& b) {
        return a.path == b.path;
    }

    const GDALDatasetRef& get() const {
        static thread_local TMap<uint64, FThreadLocalSharedDatasetEntry> thread_local_datasets;
        const uint64 cache_id = cache->cache_id;
        if (const FThreadLocalSharedDatasetEntry* existing = thread_local_datasets.Find(cache_id)) {
            if (existing->cache.Pin() == cache && existing->dataset != nullptr) {
                return *existing->dataset;
            }

            thread_local_datasets.Remove(cache_id);
        }

        const uint32 thread_id = FPlatformTLS::GetCurrentThreadId();
        {
            FReadScopeLock read_scope_lock(cache->datasets_guard);
            if (const TUniquePtr<GDALDatasetRef>* existing = cache->datasets.Find(thread_id)) {
                thread_local_datasets.Add(cache_id, {cache, existing->Get()});
                return *existing->Get();
            }
        }

        GDALDataset* raw = GDALDataset::Open(
            TCHAR_TO_UTF8(*path),
            GDAL_OF_SHARED | GDAL_OF_READONLY | GDAL_OF_RASTER);
        if (raw == nullptr) {
            checkf(
                false,
                TEXT("Failed to open dataset at path %s with error (%d): %hs"),
                *path,
                CPLGetLastErrorNo(),
                CPLGetLastErrorMsg());
            static GDALDatasetRef null_dataset{};
            return null_dataset;
        }

        FWriteScopeLock write_scope_lock(cache->datasets_guard);
        if (const TUniquePtr<GDALDatasetRef>* existing = cache->datasets.Find(thread_id)) {
            GDALClose(raw);
            thread_local_datasets.Add(cache_id, {cache, existing->Get()});
            return *existing->Get();
        }

        const TUniquePtr<GDALDatasetRef>& stored = cache->datasets.Add(thread_id, MakeUnique<
            GDALDatasetRef>(raw));
        thread_local_datasets.Add(cache_id, {cache, stored.Get()});
        return *stored;
    }

    const FString& get_path() const { return path; }

private:
    FString path;
    TSharedPtr<FSharedDatasetCache> cache;
};

FORCEINLINE uint32 GetTypeHash(const SharedDatasetRO& dataset) {
    return GetTypeHash(dataset.get_path());
}
