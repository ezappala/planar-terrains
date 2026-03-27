#pragma once

#include "CoreMinimal.h"
#include "gpu_terrain.h"
#include "terrain_parent_actor.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FTerrainDebugRow {
    FString Section;
    FString Field;
    FString Value;
};

class STerrainDebugTable final : public SCompoundWidget {
public:
    SLATE_BEGIN_ARGS(STerrainDebugTable) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void RefreshFromActor(ATerrainParentActor* InActor);

private:
    using FRowPtr = TSharedPtr<FTerrainDebugRow>;

    TWeakObjectPtr<ATerrainParentActor> Actor;
    TArray<FRowPtr> Rows;
    TSharedPtr<SListView<FRowPtr>> ListView;

    void RebuildRows();
    void AddRow(const FString& Section, const FString& Field, const FString& Value);

    TSharedRef<ITableRow> OnGenerateRow(
        FRowPtr InItem,
        const TSharedRef<STableViewBase>& OwnerTable);

    // formatting helpers
    static FString BoolToString(bool b);
    static FString PtrToString(const void* Ptr);
    static FString IntPointToString(const FIntPoint& P);
    static FString Vec2dToString(const FVector2d& V);
    static FString Vec3dToString(const FVector3d& V);
    static FString Vec3fToString(const FVector3f& V);
    static FString Vec4fToString(const FVector4f& V);
    static FString TileCoordinateToString(const FTileCoordinate& V);
    static FString CoordinateToString(const FCoordinate& V);
    static FString Matrix3x4ToString(const FMatrix3x4& M);
    static FString Matrix2x4ToString(const FMatrix2x4& M);
    static FString DescribeArray4D(const TArray4D<TileTreeEntry>& V);
    static FString DescribeArray4D(const TArray4D<FTileState>& V);
    static FString DescribeAttachmentTileData(const FAttachmentTileData& Data);

    // flatteners
    void AddTileAtlas(const FString& Section, const FTileAtlas* V);
    void AddAttachmentConfig(const FString& Section, const FAttachmentConfig& V);
    void AddTerrain(const FString& Section, const FTerrains* V);
    void AddTerrainConfig(const FString& Section, const FTerrainConfig& V);
    void AddTerrainViewConfig(const FString& Section, const FTerrainViewConfig& V);
    void AddTileTree(const FString& Section, const FTileTree* V);
    void AddTileState(const FString& Section, const FTileState* V);
    void AddTileTreeEntry(const FString& Section, const TileTreeEntry* V);
    void AddAtlasBufferInfo(const FString& Section, const FAtlasBufferInfo& V);
    void AddGpuAttachment(const FString& Section, const FGpuAttachment& V);
    void AddAttachmentTileWithData(const FString& Section, const FAttachmentTileWithData& V);
    void AddGpuTileAtlas(const FString& Section, const FGpuTileAtlas* V);
    void AddGpuTerrainAttachmentConfig(const FString& Section, const AttachmentConfig& V);
    void AddGpuTerrainBlock(const FString& Section, const Terrain* V);
    void AddGpuAttachmentsBlock(const FString& Section, const Attachments& V);
    void AddGpuTerrain(const FString& Section, const FGpuTerrain* V);
    void AddPrepass(const FString& Section, const Prepass* V);
    void AddRefineTiles(const FString& Section, const RefineTiles* V);
    void AddGpuTerrainView(const FString& Section, const FGpuTerrainView* V);
};
