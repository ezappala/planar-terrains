#pragma once
#include "SmartPointers.h"
#include "HAL/ThreadSingleton.h"

struct SharedDatasetTLS final : TThreadSingleton<SharedDatasetTLS> {
    TMap<FString, GDALDatasetRef> Datasets;
};

struct SharedDatasetRO {
    SharedDatasetRO() = default;

    explicit SharedDatasetRO(FString InPath) : Path(MoveTemp(InPath)) {}

    friend bool operator==(const SharedDatasetRO& A, const SharedDatasetRO& B) {
        return A.Path == B.Path;
    }

    GDALDatasetRef get() const {
        SharedDatasetTLS& Tls = SharedDatasetTLS::Get();

        if (GDALDatasetRef* Existing = Tls.Datasets.Find(Path)) { return MoveTemp(*Existing); }

        GDALDataset* Raw = GDALDataset::Open(TCHAR_TO_UTF8(*Path));
        if (Raw == nullptr) {
            checkf(
                false,
                TEXT("Failed to open dataset at path %s with error (%d): %hs"),
                *Path,
                CPLGetLastErrorNo(),
                CPLGetLastErrorMsg());
            return nullptr;
        }

        return MoveTemp(Tls.Datasets.Add(Path, GDALDatasetRef{Raw}));
    }

    const FString& get_path() const { return Path; }

private:
    FString Path;
};

FORCEINLINE uint32 GetTypeHash(const SharedDatasetRO& Dataset) {
    return GetTypeHash(Dataset.get_path());
}
