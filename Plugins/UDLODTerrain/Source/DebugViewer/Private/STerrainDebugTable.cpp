#include "STerrainDebugTable.h"

#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"

class STerrainDebugTableRow final : public SMultiColumnTableRow<TSharedPtr<FTerrainDebugRow>> {
public:
    SLATE_BEGIN_ARGS(STerrainDebugTableRow) {}
        SLATE_ARGUMENT(TSharedPtr<FTerrainDebugRow>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTableView) {
        Item = InArgs._Item;
        SMultiColumnTableRow::Construct(
            FSuperRowType::FArguments().Padding(2.0f),
            OwnerTableView
        );
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override {
        FString Text;
        if (ColumnName == "Section") { Text = Item->Section; } else if (ColumnName == "Field") {
            Text = Item->Field;
        } else { Text = Item->Value; }

        return SNew(STextBlock)
            .Text(FText::FromString(Text));
    }

private:
    TSharedPtr<FTerrainDebugRow> Item;
};

// ReSharper disable once CppParameterNeverUsed
void STerrainDebugTable::Construct(const FArguments& InArgs) {
    ChildSlot
    [
        SNew(SBorder)
        .Padding(8.0f)
        [
            SAssignNew(ListView, SListView<FRowPtr>)
            .ListItemsSource(&Rows)
            .OnGenerateRow(this, &STerrainDebugTable::OnGenerateRow)
            .SelectionMode(ESelectionMode::None)
            .HeaderRow(
                SNew(SHeaderRow)
                + SHeaderRow::Column("Section")
                .DefaultLabel(FText::FromString("Section"))
                .FillWidth(0.28f)
                + SHeaderRow::Column("Field").DefaultLabel(FText::FromString("Field")).FillWidth(
                    0.28f)
                + SHeaderRow::Column("Value").DefaultLabel(FText::FromString("Value")).FillWidth(
                    0.44f)
            )
        ]
    ];
}

void STerrainDebugTable::RefreshFromActor(ATerrainParentActor* InActor) {
    Actor = InActor;
    RebuildRows();
}

void STerrainDebugTable::RebuildRows() {
    Rows.Reset();

    if (!Actor.IsValid()) {
        AddRow(TEXT("System"), TEXT("Status"), TEXT("No actor"));
        ListView->RequestListRefresh();
        return;
    }

    auto* A = Actor.Get();
    AddRow(TEXT("Actor"), TEXT("Name"), A->GetName());

    AddTileAtlas(TEXT("TileAtlas"), A->tile_atlas.GetPtrOrNull());
    AddTerrain(TEXT("TerrainConfig"), A->terrain.GetPtrOrNull());
    AddTileTree(TEXT("TileTree"), A->view_component.GetPtrOrNull());
    AddGpuTileAtlas(TEXT("GpuTileAtlas"), A->gpu_tile_atlas.GetPtrOrNull());
    AddGpuTerrain(TEXT("GpuTerrain"), A->gpu_terrain.GetPtrOrNull());
    AddGpuTerrainView(TEXT("GpuTerrainView"), A->gpu_terrain_view.GetPtrOrNull());

    ListView->RequestListRefresh();
}

void STerrainDebugTable::AddRow(
    const FString& Section,
    const FString& Field,
    const FString& Value) {
    const TSharedPtr<FTerrainDebugRow> Row = MakeShared<FTerrainDebugRow>();
    Row->Section = Section;
    Row->Field = Field;
    Row->Value = Value;
    Rows.Add(Row);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
TSharedRef<ITableRow> STerrainDebugTable::OnGenerateRow(
    // ReSharper disable once CppPassValueParameterByConstReference
    FRowPtr InItem,
    const TSharedRef<STableViewBase>& OwnerTable) {
    return SNew(STerrainDebugTableRow, OwnerTable).Item(InItem);
}

FString STerrainDebugTable::BoolToString(const bool b) { return b ? TEXT("true") : TEXT("false"); }

FString STerrainDebugTable::PtrToString(const void* Ptr) {
    return Ptr != nullptr ? FString::Printf(TEXT("%p"), Ptr) : TEXT("null");
}

FString STerrainDebugTable::IntPointToString(const FIntPoint& P) {
    return FString::Printf(TEXT("(%d, %d)"), P.X, P.Y);
}

FString STerrainDebugTable::Vec2dToString(const FVector2d& V) {
    return FString::Printf(TEXT("(%.6f, %.6f)"), V.X, V.Y);
}

FString STerrainDebugTable::Vec3dToString(const FVector3d& V) {
    return FString::Printf(TEXT("(%.6f, %.6f, %.6f)"), V.X, V.Y, V.Z);
}

FString STerrainDebugTable::Vec3fToString(const FVector3f& V) {
    return FString::Printf(TEXT("(%.6f, %.6f, %.6f)"), V.X, V.Y, V.Z);
}

FString STerrainDebugTable::Vec4fToString(const FVector4f& V) {
    return FString::Printf(TEXT("(%.6f, %.6f, %.6f, %.6f)"), V.X, V.Y, V.Z, V.W);
}

FString STerrainDebugTable::TileCoordinateToString(const FTileCoordinate& V) {
    return FString::Printf(
        TEXT("{ face=%d, lod=%d, xy=%s }"),
        V.face,
        V.lod,
        *IntPointToString(V.xy)
    );
}

FString STerrainDebugTable::CoordinateToString(const FCoordinate& V) {
    return FString::Printf(
        TEXT("{ face=%u, uv=%s }"),
        V.face,
        *Vec2dToString(V.uv)
    );
}

FString STerrainDebugTable::Matrix3x4ToString(const FMatrix3x4& M) {
    return FString::Printf(
        TEXT("[%.6f, %.6f, %.6f, %.6f]\n[%.6f, %.6f, %.6f, %.6f]\n[%.6f, %.6f, %.6f, %.6f]"),
        M.M[0][0],
        M.M[0][1],
        M.M[0][2],
        M.M[0][3],
        M.M[1][0],
        M.M[1][1],
        M.M[1][2],
        M.M[1][3],
        M.M[2][0],
        M.M[2][1],
        M.M[2][2],
        M.M[2][3]
    );
}

FString STerrainDebugTable::Matrix2x4ToString(const FMatrix2x4& M) {
    return FString::Printf(
        TEXT("[%.6f, %.6f, %.6f, %.6f]\n[%.6f, %.6f, %.6f, %.6f]"),
        M.M[0][0],
        M.M[0][1],
        M.M[0][2],
        M.M[0][3],
        M.M[1][0],
        M.M[1][1],
        M.M[1][2],
        M.M[1][3]
    );
}

FString STerrainDebugTable::DescribeArray4D(const TArray4D<TileTreeEntry>& V) {
    return FString::Printf(
        TEXT("TArray4D<TileTreeEntry> Size=[%llu, %llu, %llu, %llu]"),
        V.get_dim0(),
        V.get_dim1(),
        V.get_dim2(),
        V.get_dim3()
    );
}

FString STerrainDebugTable::DescribeArray4D(const TArray4D<FTileState>& V) {
    return FString::Printf(
        TEXT("TArray4D<FTileState> Size=[%llu, %llu, %llu, %llu]"),
        V.get_dim0(),
        V.get_dim1(),
        V.get_dim2(),
        V.get_dim3()
    );
}

FString STerrainDebugTable::DescribeAttachmentTileData(const FAttachmentTileData& Data) {
    if (Data.IsType<TArray<TStaticArray<uint8, 4>>>()) {
        const auto& V = Data.Get<TArray<TStaticArray<uint8, 4>>>();
        return FString::Printf(TEXT("TArray<TStaticArray<uint8,4>> Num=%d"), V.Num());
    }
    if (Data.IsType<TArray<uint16>>()) {
        const auto& V = Data.Get<TArray<uint16>>();
        return FString::Printf(TEXT("TArray<uint16> Num=%d"), V.Num());
    }
    if (Data.IsType<TArray<int16>>()) {
        const auto& V = Data.Get<TArray<int16>>();
        return FString::Printf(TEXT("TArray<int16> Num=%d"), V.Num());
    }
    if (Data.IsType<TArray<TStaticArray<uint16, 2>>>()) {
        const auto& V = Data.Get<TArray<TStaticArray<uint16, 2>>>();
        return FString::Printf(TEXT("TArray<TStaticArray<uint16,2>> Num=%d"), V.Num());
    }
    if (Data.IsType<TArray<float>>()) {
        const auto& V = Data.Get<TArray<float>>();
        return FString::Printf(TEXT("TArray<float> Num=%d"), V.Num());
    }

    return TEXT("<unknown variant>");
}

void STerrainDebugTable::AddTileAtlas(const FString& Section, const FTileAtlas* V) {
    if (V == nullptr) {
        AddRow(Section, TEXT("Status"), TEXT("No tile atlas"));
        return;
    }
    AddRow(Section, TEXT("lod_count"), LexToString(V->lod_count));
    AddRow(Section, TEXT("max_height"), LexToString(V->max_height));
    AddRow(Section, TEXT("min_height"), LexToString(V->min_height));
    AddRow(Section, TEXT("height_scale"), LexToString(V->height_scale));
    AddRow(Section, TEXT("side_length"), LexToString(V->side_length));
}

void STerrainDebugTable::AddAttachmentConfig(const FString& Section, const FAttachmentConfig& V) {
    AddRow(Section, TEXT("texture_size"), LexToString(V.texture_size));
    AddRow(Section, TEXT("border_size"), LexToString(V.border_size));
    AddRow(Section, TEXT("mip_level_count"), LexToString(V.mip_level_count));
    AddRow(Section, TEXT("mask"), BoolToString(V.mask));
    AddRow(Section, TEXT("format"), StaticEnum<EAttachmentFormat>()->GetValueAsString(V.format));
}

void STerrainDebugTable::AddTerrain(const FString& Section, const FTerrains* V) {
    if (V == nullptr) {
        AddRow(Section, TEXT("Status"), TEXT("No terrain"));
        return;
    }

    AddTerrainConfig(Section + TEXT(".terrain_config"), V->terrain_config);
    AddTerrainViewConfig(Section + TEXT(".terrain_view_config"), V->terrain_view_config);
}

void STerrainDebugTable::AddTerrainConfig(const FString& Section, const FTerrainConfig& V) {
    AddRow(Section, TEXT("path"), V.path);
    AddRow(Section, TEXT("lod_count"), LexToString(V.lod_count));
    AddRow(Section, TEXT("min_height"), LexToString(V.min_height));
    AddRow(Section, TEXT("max_height"), LexToString(V.max_height));
    AddRow(Section, TEXT("side_length"), LexToString(V.side_length));
    AddRow(Section, TEXT("face_count"), LexToString(V.face_count));
    AddRow(Section, TEXT("scale_scalar"), LexToString(V.scale_scalar));
    AddRow(Section, TEXT("face_size"), LexToString(V.face_size));

    for (const auto& Pair : V.attachments) {
        AddAttachmentConfig(
            FString::Printf(TEXT("%s.attachments[%s]"), *Section, *Pair.Key),
            Pair.Value
        );
    }
}

void STerrainDebugTable::AddTerrainViewConfig(const FString& Section, const FTerrainViewConfig& V) {
    AddRow(Section, TEXT("tree_size"), LexToString(V.tree_size));
    AddRow(Section, TEXT("geometry_tile_count"), LexToString(V.geometry_tile_count));
    AddRow(Section, TEXT("refinement_count"), LexToString(V.refinement_count));
    AddRow(Section, TEXT("grid_size"), LexToString(V.grid_size));
    AddRow(Section, TEXT("morph_distance"), LexToString(V.morph_distance));
    AddRow(Section, TEXT("blend_distance"), LexToString(V.blend_distance));
    AddRow(Section, TEXT("load_tolerance"), LexToString(V.load_tolerance));
    AddRow(Section, TEXT("subdivision_tolerance"), LexToString(V.subdivision_tolerance));
    AddRow(Section, TEXT("morph_range"), LexToString(V.morph_range));
    AddRow(Section, TEXT("blend_range"), LexToString(V.blend_range));
    AddRow(Section, TEXT("precision_distance"), LexToString(V.precision_distance));
    AddRow(Section, TEXT("view_lod"), LexToString(V.view_lod));
    AddRow(Section, TEXT("order"), LexToString(V.order));
}

void STerrainDebugTable::AddTileTree(const FString& Section, const FTileTree* V) {
    if (V == nullptr) {
        AddRow(Section, TEXT("Status"), TEXT("No tile tree"));
        return;
    }
    AddRow(Section, TEXT("tree_size"), LexToString(V->tree_size));
    AddRow(Section, TEXT("lod_count"), LexToString(V->lod_count));
    AddRow(Section, TEXT("side_length"), LexToString(V->side_length));
    AddRow(Section, TEXT("geometry_tile_count"), LexToString(V->geometry_tile_count));
    AddRow(Section, TEXT("refinement_count"), LexToString(V->refinement_count));
    AddRow(Section, TEXT("grid_size"), LexToString(V->grid_size));
    AddRow(Section, TEXT("morph_distance"), LexToString(V->morph_distance));
    AddRow(Section, TEXT("blend_distance"), LexToString(V->blend_distance));
    AddRow(Section, TEXT("load_distance"), LexToString(V->load_distance));
    AddRow(Section, TEXT("subdivision_distance"), LexToString(V->subdivision_distance));
    AddRow(Section, TEXT("morph_range"), LexToString(V->morph_range));
    AddRow(Section, TEXT("blend_range"), LexToString(V->blend_range));
    AddRow(Section, TEXT("precision_distance"), LexToString(V->precision_distance));
    AddRow(Section, TEXT("view_face"), LexToString(V->view_face));
    AddRow(Section, TEXT("view_lod"), LexToString(V->view_lod));
    AddRow(Section, TEXT("view_local_position"), Vec3dToString(V->view_local_position));
    AddRow(Section, TEXT("view_world_position"), Vec3fToString(V->view_world_position));
    AddRow(Section, TEXT("approximate_height"), LexToString(V->approximate_height));
    AddRow(Section, TEXT("order"), LexToString(V->order));

    for (int32 i = 0; i < V->lod_tile_counts.Num(); ++i) {
        AddRow(
            Section,
            FString::Printf(TEXT("lod_tile_counts[%d]"), i),
            IntPointToString(V->lod_tile_counts[i])
        );
    }

    for (int32 i = 0; i < V->requested_tiles.Num(); ++i) {
        AddRow(
            Section,
            FString::Printf(TEXT("requested_tiles[%d]"), i),
            TileCoordinateToString(V->requested_tiles[i])
        );
    }

    for (int32 i = 0; i < V->released_tiles.Num(); ++i) {
        AddRow(
            Section,
            FString::Printf(TEXT("released_tiles[%d]"), i),
            TileCoordinateToString(V->released_tiles[i])
        );
    }

    for (int32 i = 0; i < V->view_coordinates.Num(); ++i) {
        AddRow(
            Section,
            FString::Printf(TEXT("view_coordinates[%d]"), i),
            CoordinateToString(V->view_coordinates[i])
        );
    }

    for (int32 i = 0; i < V->half_spaces.Num(); ++i) {
        AddRow(
            Section,
            FString::Printf(TEXT("half_spaces[%d]"), i),
            Vec4fToString(V->half_spaces[i])
        );
    }

    // Your TArray4D API wasn’t provided, so this is intentionally a stub.
    AddRow(Section, TEXT("data"), DescribeArray4D(V->data));
    AddRow(Section, TEXT("tiles"), DescribeArray4D(V->tiles));
}

void STerrainDebugTable::AddTileState(const FString& Section, const FTileState* V) {
    if (V == nullptr) {
        AddRow(Section, TEXT("Status"), TEXT("No tile state"));
        return;
    }
    AddRow(Section, TEXT("coordinate"), TileCoordinateToString(V->coordinate));
    AddRow(
        Section,
        TEXT("request_state"),
        StaticEnum<ERequestState>()->GetValueAsString(V->request_state));
}

void STerrainDebugTable::AddTileTreeEntry(const FString& Section, const TileTreeEntry* V) {
    if (V == nullptr) {
        AddRow(Section, TEXT("Status"), TEXT("No tile tree entry"));
        return;
    }
    AddRow(Section, TEXT("atlas_index"), LexToString(V->atlas_index));
    AddRow(Section, TEXT("atlas_lod"), LexToString(V->atlas_lod));
}

void STerrainDebugTable::AddAtlasBufferInfo(const FString& Section, const FAtlasBufferInfo& V) {
    AddRow(Section, TEXT("mask"), BoolToString(V.mask));
    AddRow(Section, TEXT("lod_count"), LexToString(V.lod_count));
    AddRow(Section, TEXT("format"), StaticEnum<EAttachmentFormat>()->GetValueAsString(V.format));
    AddRow(Section, TEXT("texture_size"), LexToString(V.texture_size));
    AddRow(Section, TEXT("border_size"), LexToString(V.border_size));
    AddRow(Section, TEXT("center_size"), LexToString(V.center_size));
    AddRow(Section, TEXT("mip_level_count"), LexToString(V.mip_level_count));
    AddRow(Section, TEXT("pixels_per_entry"), LexToString(V.pixels_per_entry));
    AddRow(Section, TEXT("actual_side_size"), LexToString(V.actual_side_size));
    AddRow(Section, TEXT("aligned_side_size"), LexToString(V.aligned_side_size));
    AddRow(Section, TEXT("actual_tile_size"), LexToString(V.actual_tile_size));
    AddRow(Section, TEXT("aligned_tile_size"), LexToString(V.aligned_tile_size));
    AddRow(Section, TEXT("entries_per_side"), LexToString(V.entries_per_side));
    AddRow(Section, TEXT("entries_per_tile"), LexToString(V.entries_per_tile));
}

void STerrainDebugTable::AddGpuAttachment(const FString& Section, const FGpuAttachment& V) {
    AddRow(Section, TEXT("index"), LexToString(V.index));
    AddAtlasBufferInfo(Section + TEXT(".buffer_info"), V.buffer_info);
}

void STerrainDebugTable::AddAttachmentTileWithData(
    const FString& Section,
    const FAttachmentTileWithData& V) {
    AddRow(Section, TEXT("atlas_index"), LexToString(V.atlas_index));
    AddRow(Section, TEXT("label"), V.label);
    AddRow(Section, TEXT("data"), DescribeAttachmentTileData(V.data));
}

void STerrainDebugTable::AddGpuTileAtlas(const FString& Section, const FGpuTileAtlas* V) {
    if (V == nullptr) {
        AddRow(Section, TEXT("Status"), TEXT("No GPU tile atlas"));
        return;
    }
    for (const auto& Pair : V->attachments) {
        AddGpuAttachment(
            FString::Printf(TEXT("%s.attachments[%s]"), *Section, *Pair.Key),
            Pair.Value
        );
    }

    for (int32 i = 0; i < V->upload_tiles.Num(); ++i) {
        AddAttachmentTileWithData(
            FString::Printf(TEXT("%s.upload_tiles[%d]"), *Section, i),
            V->upload_tiles[i]
        );
    }

    for (int32 i = 0; i < V->download_tiles.Num(); ++i) {
        AddAttachmentTileWithData(
            FString::Printf(TEXT("%s.download_tiles[%d]"), *Section, i),
            V->download_tiles[i]
        );
    }
}

void STerrainDebugTable::AddGpuTerrainAttachmentConfig(
    const FString& Section,
    const AttachmentConfig& V) {
    AddRow(Section, TEXT("texture_size"), LexToString(V.texture_size));
    AddRow(Section, TEXT("center_size"), LexToString(V.center_size));
    AddRow(Section, TEXT("scale"), LexToString(V.scale));
    AddRow(Section, TEXT("offset"), LexToString(V.offset));
    AddRow(Section, TEXT("mask"), LexToString(V.mask));
    AddRow(Section, TEXT("paddinga"), LexToString(V.paddinga));
    AddRow(Section, TEXT("paddingb"), LexToString(V.paddingb));
    AddRow(Section, TEXT("paddingc"), LexToString(V.paddingc));
}

void STerrainDebugTable::AddGpuTerrainBlock(const FString& Section, const Terrain* V) {
    if (V == nullptr) {
        AddRow(Section, TEXT("Status"), TEXT("No terrain"));
        return;
    }
    AddRow(Section, TEXT("lod_count"), LexToString(V->lod_count));
    AddRow(Section, TEXT("scale"), Vec3fToString(V->scale));
    AddRow(Section, TEXT("min_height"), LexToString(V->min_height));
    AddRow(Section, TEXT("max_height"), LexToString(V->max_height));
    AddRow(Section, TEXT("height_scale"), LexToString(V->height_scale));
    AddRow(Section, TEXT("world_from_unit"), Matrix3x4ToString(V->world_from_unit));
    AddRow(
        Section,
        TEXT("unit_from_world_transpose_a"),
        Matrix2x4ToString(V->unit_from_world_transpose_a));
    AddRow(
        Section,
        TEXT("unit_from_world_transpose_b"),
        LexToString(V->unit_from_world_transpose_b));
}

void STerrainDebugTable::AddGpuAttachmentsBlock(const FString& Section, const Attachments& V) {
    for (int32 i = 0; i < V.attachment_configs.Num(); ++i) {
        AddGpuTerrainAttachmentConfig(
            FString::Printf(TEXT("%s.attachment_configs[%d]"), *Section, i),
            V.attachment_configs[i]
        );
    }
}

void STerrainDebugTable::AddGpuTerrain(const FString& Section, const FGpuTerrain* V) {
    if (V == nullptr) {
        AddRow(Section, TEXT("Status"), TEXT("No GPU terrain"));
        return;
    }
    for (int32 i = 0; i < V->attachment_configs.Num(); ++i) {
        AddGpuTerrainAttachmentConfig(
            FString::Printf(TEXT("%s.attachment_configs[%d]"), *Section, i),
            V->attachment_configs[i]
        );
    }
}

void STerrainDebugTable::AddPrepass(const FString& Section, const Prepass* V) {
    AddGpuTerrainBlock(
        Section + TEXT(".terrain"),
        reinterpret_cast<TRDGUniformBuffer<Terrain>*>(
            V->terrain.GetUniformBuffer())->GetContents()
    );
    AddGpuAttachmentsBlock(Section + TEXT(".attachments"), V->attachments);
}

void STerrainDebugTable::AddRefineTiles(const FString& Section, const RefineTiles* V) {
    AddGpuTerrainBlock(
        Section + TEXT(".terrain"),
        reinterpret_cast<TRDGUniformBuffer<Terrain>*>(
            V->terrain.GetUniformBuffer())->GetContents()
    );
    AddGpuAttachmentsBlock(Section + TEXT(".attachments"), V->attachments);
}

void STerrainDebugTable::AddGpuTerrainView(const FString& Section, const FGpuTerrainView* V) {
    AddRow(Section, TEXT("order"), LexToString(V->order));
    AddRow(Section, TEXT("refinement_count"), LexToString(V->refinement_count));
    AddRow(Section, TEXT("prepass_parameters"), PtrToString(V->prepass_parameters));
    AddRow(Section, TEXT("refine_tiles_parameters"), PtrToString(V->refine_tiles_parameters));

    if (V->prepass_parameters != nullptr) {
        AddPrepass(Section + TEXT(".prepass"), V->prepass_parameters);
    }

    if (V->refine_tiles_parameters != nullptr) {
        AddRefineTiles(Section + TEXT(".refine_tiles"), V->refine_tiles_parameters);
    }
}
