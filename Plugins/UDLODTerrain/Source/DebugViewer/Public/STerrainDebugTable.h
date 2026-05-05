#pragma once

#include "CoreMinimal.h"
#include "gpu_terrain.h"
#include "terrain_parent_actor.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FTerrainDebugRow {
    FString section;
    FString field;
    FString value;
};

class STerrainDebugTable final : public SCompoundWidget {
public:
    SLATE_BEGIN_ARGS(STerrainDebugTable) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& in_args);
    void Refresh(ATerrainParentActor* in_actor);

private:
    using FRowPtr = TSharedPtr<FTerrainDebugRow>;

    TWeakObjectPtr<ATerrainParentActor> actor;
    TArray<FRowPtr> rows;
    TSharedPtr<SListView<FRowPtr>> list_view;

    void Refresh();
    void add(const FString& section, const FString& field, const FString& value);

    TSharedRef<ITableRow> on_generate_row(
        FRowPtr in_item,
        const TSharedRef<STableViewBase>& owner_table);

    // formatting helpers
    static FString to_string(bool v);
    static FString to_string(const void* v);
    static FString to_string(const FIntPoint& v);
    static FString to_string(const FVector2d& v);
    static FString to_string(const FVector3d& v);
    static FString to_string(const FVector3f& v);
    static FString to_string(const FVector4f& v);
    static FString to_string(const FTileCoordinate& v);
    static FString to_string(const FCoordinate& v);
    static FString to_string(const FMatrix3x4& v);
    static FString to_string(const FMatrix2x4& v);
    static FString to_string(const TArray4D<TileTreeEntry>& v);
    static FString to_string(const TArray4D<FTileState>& v);
    static FString to_string(const FAttachmentTileData& v);

    // flatteners
    void add(const FString& section, const FTileAtlas* v);
    void add(const FString& section, const FAttachmentConfig& v);
    void add(const FString& section, const FTerrains* v);
    void add(const FString& section, const FTerrainActorSettings& v);
    void add(const FString& section, const FTerrainSettings& v);
    void add(const FString& section, const FTerrainConfig& v);
    void add(const FString& section, const FTerrainViewConfig& v);
    void add(const FString& section, const FTileTree* v);
    void add(const FString& section, const FTileState* v);
    void add(const FString& section, const TileTreeEntry* v);
    void add(const FString& section, const FAtlasBufferInfo& v);
    void add(const FString& section, const FGpuAttachment& v);
    void add(const FString& section, const FAttachmentTileWithData& v);
    void add(const FString& section, const FGpuTileAtlas* v);
    void add(const FString& section, const AttachmentConfig& v);
    void add(const FString& section, const Terrain* v);
    void add(const FString& section, const Attachments& v);
    void add(const FString& section, const FGpuTerrain* v);
    void add(const FString& section, const Prepass* v);
    void add(const FString& section, const RefineTiles* v);
    void add(const FString& section, const FGpuTerrainView* v);
};
