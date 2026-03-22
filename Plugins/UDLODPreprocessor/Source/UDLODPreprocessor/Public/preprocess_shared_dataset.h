#pragma once

#include "SmartPointers.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformTLS.h"
#include "Misc/ScopeLock.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

struct FSharedDatasetCache {
    FCriticalSection mutex;
    TMap<uint32, TUniquePtr<GDALDatasetRef>> datasets;
};

struct SharedDatasetRO {
    SharedDatasetRO() : cache(MakeShared<FSharedDatasetCache>()) {}

    explicit SharedDatasetRO(FString in_path) : path(MoveTemp(in_path)),
        cache(MakeShared<FSharedDatasetCache>()) {}

    friend bool operator==(const SharedDatasetRO& a, const SharedDatasetRO& b) {
        return a.path == b.path;
    }

    const GDALDatasetRef& get() const {
        const uint32 thread_id = FPlatformTLS::GetCurrentThreadId();
        FScopeLock _(&cache->mutex);

        if (const TUniquePtr<GDALDatasetRef>* existing = cache->datasets.Find(thread_id)) {
            return *existing->Get();
        }

        GDALDataset* raw = GDALDataset::Open(
            TCHAR_TO_UTF8(*path),
            GDAL_OF_READONLY | GDAL_OF_RASTER);
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

        const TUniquePtr<GDALDatasetRef>& stored = cache->datasets.Add(
            thread_id,
            MakeUnique<GDALDatasetRef>(raw));
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
