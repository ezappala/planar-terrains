#pragma once

#include "SmartPointers.h"
#include "HAL/ThreadSingleton.h"

struct SharedDatasetTLS final : TThreadSingleton<SharedDatasetTLS> {
    TMap<FString, GDALDatasetRef> datasets;
};

struct SharedDatasetRO {
    SharedDatasetRO() = default;

    explicit SharedDatasetRO(FString in_path) : path(MoveTemp(in_path)) {}

    friend bool operator==(const SharedDatasetRO& a, const SharedDatasetRO& b) {
        return a.path == b.path;
    }

    const GDALDatasetRef& get() const {
        SharedDatasetTLS& tls = SharedDatasetTLS::Get();

        if (GDALDatasetRef* existing = tls.datasets.Find(path)) { return *existing; }

        GDALDataset* raw = GDALDataset::Open(
            TCHAR_TO_UTF8(*path),
            GDAL_OF_READONLY | GDAL_OF_RASTER | GDAL_OF_SHARED);
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

        return tls.datasets.Add(path, GDALDatasetRef{raw});
    }

    const FString& get_path() const { return path; }

private:
    FString path;
};

FORCEINLINE uint32 GetTypeHash(const SharedDatasetRO& dataset) {
    return GetTypeHash(dataset.get_path());
}
