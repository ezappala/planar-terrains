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

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& owner_table_view) {
        item = InArgs._Item;
        SMultiColumnTableRow::Construct(
            FSuperRowType::FArguments().Padding(2.0f),
            owner_table_view
        );
    }

    virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& column_name) override {
        FString text;
        if (column_name == "Section") { text = item->section; } else if (column_name == "Field") {
            text = item->field;
        } else { text = item->value; }

        return SNew(STextBlock)
            .Text(FText::FromString(text));
    }

private:
    TSharedPtr<FTerrainDebugRow> item;
};

// ReSharper disable once CppParameterNeverUsed
void STerrainDebugTable::Construct(const FArguments& in_args) {
    ChildSlot
    [
        SNew(SBorder)
        .Padding(8.0f)
        [
            SAssignNew(list_view, SListView<FRowPtr>)
            .ListItemsSource(&rows)
            .OnGenerateRow(this, &STerrainDebugTable::on_generate_row)
            .SelectionMode(ESelectionMode::None)
            .HeaderRow(
                SNew(SHeaderRow)
                + SHeaderRow::Column("Section")
                .DefaultLabel(FText::FromString("Section"))
                .FillWidth(0.28f)
                + SHeaderRow::Column("Field")
                .DefaultLabel(FText::FromString("Field"))
                .FillWidth(0.28f)
                + SHeaderRow::Column("Value")
                .DefaultLabel(FText::FromString("Value"))
                .FillWidth(0.44f)
            )
        ]
    ];
}

void STerrainDebugTable::Refresh(ATerrainParentActor* in_actor) {
    actor = in_actor;
    Refresh();
}

void STerrainDebugTable::Refresh() {
    rows.Reset();

    if (!actor.IsValid()) {
        add(TEXT("System"), TEXT("Status"), TEXT("No actor"));
        list_view->RequestListRefresh();
        return;
    }

    auto* a = actor.Get();
    add(TEXT("Actor"), TEXT("Name"), a->GetName());

    add(TEXT("RuntimeSettings"), a->runtime_settings);
    add(TEXT("TileAtlas"), a->tile_atlas.GetPtrOrNull());
    add(TEXT("TileTree"), a->view_component.GetPtrOrNull());
    add(TEXT("GpuTileAtlas"), a->gpu_tile_atlas.GetPtrOrNull());
    add(TEXT("GpuTerrain"), a->gpu_terrain.GetPtrOrNull());
    add(TEXT("GpuTerrainView"), a->gpu_terrain_view.GetPtrOrNull());

    list_view->RequestListRefresh();
}

void STerrainDebugTable::add(
    const FString& section,
    const FString& field,
    const FString& value) {
    const TSharedPtr<FTerrainDebugRow> row = MakeShared<FTerrainDebugRow>();
    row->section = section;
    row->field = field;
    row->value = value;
    rows.Add(row);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
TSharedRef<ITableRow> STerrainDebugTable::on_generate_row(
    // ReSharper disable once CppPassValueParameterByConstReference
    FRowPtr in_item,
    const TSharedRef<STableViewBase>& owner_table) {
    return SNew(STerrainDebugTableRow, owner_table).Item(in_item);
}

FString STerrainDebugTable::to_string(const bool v) { return v ? TEXT("true") : TEXT("false"); }

FString STerrainDebugTable::to_string(const void* v) {
    return v != nullptr ? FString::Printf(TEXT("%p"), v) : TEXT("null");
}

FString STerrainDebugTable::to_string(const FIntPoint& v) {
    return FString::Printf(TEXT("(%d, %d)"), v.X, v.Y);
}

FString STerrainDebugTable::to_string(const FVector2d& v) {
    return FString::Printf(TEXT("(%.6f, %.6f)"), v.X, v.Y);
}

FString STerrainDebugTable::to_string(const FVector3d& v) {
    return FString::Printf(TEXT("(%.6f, %.6f, %.6f)"), v.X, v.Y, v.Z);
}

FString STerrainDebugTable::to_string(const FVector3f& v) {
    return FString::Printf(TEXT("(%.6f, %.6f, %.6f)"), v.X, v.Y, v.Z);
}

FString STerrainDebugTable::to_string(const FVector4f& v) {
    return FString::Printf(TEXT("(%.6f, %.6f, %.6f, %.6f)"), v.X, v.Y, v.Z, v.W);
}

FString STerrainDebugTable::to_string(const FTileCoordinate& v) {
    return FString::Printf(
        TEXT("{ face=%d, lod=%d, xy=%s }"),
        v.face,
        v.lod,
        *to_string(v.xy)
    );
}

FString STerrainDebugTable::to_string(const FCoordinate& v) {
    return FString::Printf(
        TEXT("{ face=%u, uv=%s }"),
        v.face,
        *to_string(v.uv)
    );
}

FString STerrainDebugTable::to_string(const FMatrix3x4& v) {
    return FString::Printf(
        TEXT("[%.6f, %.6f, %.6f, %.6f]\n[%.6f, %.6f, %.6f, %.6f]\n[%.6f, %.6f, %.6f, %.6f]"),
        v.M[0][0],
        v.M[0][1],
        v.M[0][2],
        v.M[0][3],
        v.M[1][0],
        v.M[1][1],
        v.M[1][2],
        v.M[1][3],
        v.M[2][0],
        v.M[2][1],
        v.M[2][2],
        v.M[2][3]
    );
}

FString STerrainDebugTable::to_string(const FMatrix2x4& v) {
    return FString::Printf(
        TEXT("[%.6f, %.6f, %.6f, %.6f]\n[%.6f, %.6f, %.6f, %.6f]"),
        v.M[0][0],
        v.M[0][1],
        v.M[0][2],
        v.M[0][3],
        v.M[1][0],
        v.M[1][1],
        v.M[1][2],
        v.M[1][3]
    );
}

FString STerrainDebugTable::to_string(const TArray4D<TileTreeEntry>& v) {
    return FString::Printf(
        TEXT("TArray4D<TileTreeEntry> Size=[%llu, %llu, %llu, %llu]"),
        v.get_dim0(),
        v.get_dim1(),
        v.get_dim2(),
        v.get_dim3()
    );
}

FString STerrainDebugTable::to_string(const TArray4D<FTileState>& v) {
    return FString::Printf(
        TEXT("TArray4D<FTileState> Size=[%llu, %llu, %llu, %llu]"),
        v.get_dim0(),
        v.get_dim1(),
        v.get_dim2(),
        v.get_dim3()
    );
}

FString STerrainDebugTable::to_string(const FAttachmentTileData& v) {
    if (v.IsType<TArray<TStaticArray<uint8, 4>>>()) {
        const auto& V = v.Get<TArray<TStaticArray<uint8, 4>>>();
        return FString::Printf(TEXT("TArray<TStaticArray<uint8,4>> Num=%d"), V.Num());
    }
    if (v.IsType<TArray<uint16>>()) {
        const auto& V = v.Get<TArray<uint16>>();
        return FString::Printf(TEXT("TArray<uint16> Num=%d"), V.Num());
    }
    if (v.IsType<TArray<int16>>()) {
        const auto& V = v.Get<TArray<int16>>();
        return FString::Printf(TEXT("TArray<int16> Num=%d"), V.Num());
    }
    if (v.IsType<TArray<TStaticArray<uint16, 2>>>()) {
        const auto& V = v.Get<TArray<TStaticArray<uint16, 2>>>();
        return FString::Printf(TEXT("TArray<TStaticArray<uint16,2>> Num=%d"), V.Num());
    }
    if (v.IsType<TArray<float>>()) {
        const auto& V = v.Get<TArray<float>>();
        return FString::Printf(TEXT("TArray<float> Num=%d"), V.Num());
    }

    return TEXT("<unknown variant>");
}

void STerrainDebugTable::add(const FString& section, const FTileAtlas* v) {
    if (v == nullptr) {
        add(section, TEXT("Status"), TEXT("No tile atlas"));
        return;
    }
    add(section, TEXT("lod_count"), LexToString(v->lod_count));
    add(section, TEXT("max_height"), LexToString(v->max_height));
    add(section, TEXT("min_height"), LexToString(v->min_height));
    add(section, TEXT("height_scale"), LexToString(v->height_scale));
    add(section, TEXT("side_length"), LexToString(v->side_length));
}

void STerrainDebugTable::add(const FString& section, const FAttachmentConfig& v) {
    add(section, TEXT("texture_size"), LexToString(v.texture_size));
    add(section, TEXT("border_size"), LexToString(v.border_size));
    add(section, TEXT("mip_level_count"), LexToString(v.mip_level_count));
    add(section, TEXT("mask"), to_string(v.mask));
    add(section, TEXT("format"), StaticEnum<EAttachmentFormat>()->GetValueAsString(v.format));
}

void STerrainDebugTable::add(const FString& section, const FTerrains* v) {
    if (v == nullptr) {
        add(section, TEXT("Status"), TEXT("No terrain"));
        return;
    }

    add(section + TEXT(".terrain_config"), v->terrain_config);
    add(section + TEXT(".terrain_view_config"), v->terrain_view_config);
}

void STerrainDebugTable::add(const FString& section, const FTerrainActorSettings& v) {
    add(section, TEXT("terrain_config_path"), v.terrain_config_path.FilePath);
    add(section, TEXT("material"), to_string(v.material));
    add(section + TEXT(".terrain_config"), v.terrain_config);
    add(section + TEXT(".terrain_view_config"), v.terrain_view_config);
    add(section + TEXT(".render_settings"), v.render_settings);
}

void STerrainDebugTable::add(const FString& section, const FTerrainSettings& v) {
    add(section, TEXT("attachments"), FString::Join(v.attachments, TEXT(", ")));
    add(section, TEXT("atlas_size"), LexToString(v.atlas_size));
}

void STerrainDebugTable::add(const FString& section, const FTerrainConfig& v) {
    add(section, TEXT("path"), v.path);
    add(section, TEXT("lod_count"), LexToString(v.lod_count));
    add(section, TEXT("min_height"), LexToString(v.min_height));
    add(section, TEXT("max_height"), LexToString(v.max_height));
    add(section, TEXT("side_length"), LexToString(v.side_length));
    add(section, TEXT("face_count"), LexToString(v.face_count));
    add(section, TEXT("scale_scalar"), LexToString(v.scale_scalar));
    add(section, TEXT("face_size"), LexToString(v.face_size));

    for (const auto& Pair : v.attachments) {
        add(
            FString::Printf(TEXT("%s.attachments[%s]"), *section, *Pair.Key),
            Pair.Value
        );
    }
}

void STerrainDebugTable::add(const FString& section, const FTerrainViewConfig& v) {
    add(section, TEXT("tree_size"), LexToString(v.tree_size));
    add(section, TEXT("geometry_tile_count"), LexToString(v.geometry_tile_count));
    add(section, TEXT("refinement_count"), LexToString(v.refinement_count));
    add(section, TEXT("grid_size"), LexToString(v.grid_size));
    add(section, TEXT("morph_distance"), LexToString(v.morph_distance));
    add(section, TEXT("blend_distance"), LexToString(v.blend_distance));
    add(section, TEXT("load_tolerance"), LexToString(v.load_tolerance));
    add(section, TEXT("subdivision_tolerance"), LexToString(v.subdivision_tolerance));
    add(section, TEXT("morph_range"), LexToString(v.morph_range));
    add(section, TEXT("blend_range"), LexToString(v.blend_range));
    add(section, TEXT("precision_distance"), LexToString(v.precision_distance));
    add(section, TEXT("view_lod"), LexToString(v.view_lod));
    add(section, TEXT("order"), LexToString(v.order));
}

void STerrainDebugTable::add(const FString& section, const FTileTree* v) {
    if (v == nullptr) {
        add(section, TEXT("Status"), TEXT("No tile tree"));
        return;
    }
    add(section, TEXT("tree_size"), LexToString(v->tree_size));
    add(section, TEXT("lod_count"), LexToString(v->lod_count));
    add(section, TEXT("side_length"), LexToString(v->side_length));
    add(section, TEXT("geometry_tile_count"), LexToString(v->geometry_tile_count));
    add(section, TEXT("refinement_count"), LexToString(v->refinement_count));
    add(section, TEXT("grid_size"), LexToString(v->grid_size));
    add(section, TEXT("morph_distance"), LexToString(v->morph_distance));
    add(section, TEXT("blend_distance"), LexToString(v->blend_distance));
    add(section, TEXT("load_distance"), LexToString(v->load_distance));
    add(section, TEXT("subdivision_distance"), LexToString(v->subdivision_distance));
    add(section, TEXT("morph_range"), LexToString(v->morph_range));
    add(section, TEXT("blend_range"), LexToString(v->blend_range));
    add(section, TEXT("precision_distance"), LexToString(v->precision_distance));
    add(section, TEXT("view_face"), LexToString(v->view_face));
    add(section, TEXT("view_lod"), LexToString(v->view_lod));
    add(section, TEXT("view_local_position"), to_string(v->view_local_position));
    add(section, TEXT("view_world_position"), to_string(v->view_world_position));
    add(section, TEXT("approximate_height"), LexToString(v->approximate_height));
    add(section, TEXT("order"), LexToString(v->order));

    for (int32 i = 0; i < v->lod_tile_counts.Num(); ++i) {
        add(
            section,
            FString::Printf(TEXT("lod_tile_counts[%d]"), i),
            to_string(v->lod_tile_counts[i])
        );
    }

    for (int32 i = 0; i < v->requested_tiles.Num(); ++i) {
        add(
            section,
            FString::Printf(TEXT("requested_tiles[%d]"), i),
            to_string(v->requested_tiles[i])
        );
    }

    for (int32 i = 0; i < v->released_tiles.Num(); ++i) {
        add(
            section,
            FString::Printf(TEXT("released_tiles[%d]"), i),
            to_string(v->released_tiles[i])
        );
    }

    for (int32 i = 0; i < v->view_coordinates.Num(); ++i) {
        add(
            section,
            FString::Printf(TEXT("view_coordinates[%d]"), i),
            to_string(v->view_coordinates[i])
        );
    }

    for (int32 i = 0; i < v->half_spaces.Num(); ++i) {
        add(
            section,
            FString::Printf(TEXT("half_spaces[%d]"), i),
            to_string(v->half_spaces[i])
        );
    }

    // Your TArray4D API wasn’t provided, so this is intentionally a stub.
    add(section, TEXT("data"), to_string(v->data));
    add(section, TEXT("tiles"), to_string(v->tiles));
}

void STerrainDebugTable::add(const FString& section, const FTileState* v) {
    if (v == nullptr) {
        add(section, TEXT("Status"), TEXT("No tile state"));
        return;
    }
    add(section, TEXT("coordinate"), to_string(v->coordinate));
    add(
        section,
        TEXT("request_state"),
        StaticEnum<ERequestState>()->GetValueAsString(v->request_state));
}

void STerrainDebugTable::add(const FString& section, const TileTreeEntry* v) {
    if (v == nullptr) {
        add(section, TEXT("Status"), TEXT("No tile tree entry"));
        return;
    }
    add(section, TEXT("atlas_index"), LexToString(v->atlas_index));
    add(section, TEXT("atlas_lod"), LexToString(v->atlas_lod));
}

void STerrainDebugTable::add(const FString& section, const FAtlasBufferInfo& v) {
    add(section, TEXT("mask"), to_string(v.mask));
    add(section, TEXT("lod_count"), LexToString(v.lod_count));
    add(section, TEXT("format"), StaticEnum<EAttachmentFormat>()->GetValueAsString(v.format));
    add(section, TEXT("texture_size"), LexToString(v.texture_size));
    add(section, TEXT("border_size"), LexToString(v.border_size));
    add(section, TEXT("center_size"), LexToString(v.center_size));
    add(section, TEXT("mip_level_count"), LexToString(v.mip_level_count));
    add(section, TEXT("pixels_per_entry"), LexToString(v.pixels_per_entry));
    add(section, TEXT("actual_side_size"), LexToString(v.actual_side_size));
    add(section, TEXT("aligned_side_size"), LexToString(v.aligned_side_size));
    add(section, TEXT("actual_tile_size"), LexToString(v.actual_tile_size));
    add(section, TEXT("aligned_tile_size"), LexToString(v.aligned_tile_size));
    add(section, TEXT("entries_per_side"), LexToString(v.entries_per_side));
    add(section, TEXT("entries_per_tile"), LexToString(v.entries_per_tile));
}

void STerrainDebugTable::add(const FString& section, const FGpuAttachment& v) {
    add(section, TEXT("index"), LexToString(v.index));
    add(section + TEXT(".buffer_info"), v.buffer_info);
}

void STerrainDebugTable::add(
    const FString& section,
    const FAttachmentTileWithData& v) {
    add(section, TEXT("atlas_index"), LexToString(v.atlas_index));
    add(section, TEXT("label"), v.label);
    add(section, TEXT("data"), to_string(v.data));
}

void STerrainDebugTable::add(const FString& section, const FGpuTileAtlas* v) {
    if (v == nullptr) {
        add(section, TEXT("Status"), TEXT("No GPU tile atlas"));
        return;
    }
    for (const auto& Pair : v->attachments) {
        add(
            FString::Printf(TEXT("%s.attachments[%s]"), *section, *Pair.Key),
            Pair.Value
        );
    }

    for (int32 i = 0; i < v->upload_tiles.Num(); ++i) {
        add(
            FString::Printf(TEXT("%s.upload_tiles[%d]"), *section, i),
            v->upload_tiles[i]
        );
    }

    for (int32 i = 0; i < v->download_tiles.Num(); ++i) {
        add(
            FString::Printf(TEXT("%s.download_tiles[%d]"), *section, i),
            v->download_tiles[i]
        );
    }
}

void STerrainDebugTable::add(
    const FString& section,
    const AttachmentConfig& v) {
    add(section, TEXT("texture_size"), LexToString(v.texture_size));
    add(section, TEXT("center_size"), LexToString(v.center_size));
    add(section, TEXT("scale"), LexToString(v.scale));
    add(section, TEXT("offset"), LexToString(v.offset));
    add(section, TEXT("mask"), LexToString(v.mask));
    add(section, TEXT("paddinga"), LexToString(v.paddinga));
    add(section, TEXT("paddingb"), LexToString(v.paddingb));
    add(section, TEXT("paddingc"), LexToString(v.paddingc));
}

void STerrainDebugTable::add(const FString& section, const Terrain* v) {
    if (v == nullptr) {
        add(section, TEXT("Status"), TEXT("No terrain"));
        return;
    }
    add(section, TEXT("lod_count"), LexToString(v->lod_count));
    add(section, TEXT("scale"), to_string(v->scale));
    add(section, TEXT("min_height"), LexToString(v->min_height));
    add(section, TEXT("max_height"), LexToString(v->max_height));
    add(section, TEXT("height_scale"), LexToString(v->height_scale));
    add(section, TEXT("world_from_unit"), to_string(v->world_from_unit));
    add(
        section,
        TEXT("unit_from_world_transpose_a"),
        to_string(v->unit_from_world_transpose_a));
    add(
        section,
        TEXT("unit_from_world_transpose_b"),
        LexToString(v->unit_from_world_transpose_b));
}

void STerrainDebugTable::add(const FString& section, const Attachments& v) {
    for (int32 i = 0; i < v.attachment_configs.Num(); ++i) {
        add(
            FString::Printf(TEXT("%s.attachment_configs[%d]"), *section, i),
            v.attachment_configs[i]
        );
    }
}

void STerrainDebugTable::add(const FString& section, const FGpuTerrain* v) {
    if (v == nullptr) {
        add(section, TEXT("Status"), TEXT("No GPU terrain"));
        return;
    }
    for (int32 i = 0; i < v->attachment_configs.Num(); ++i) {
        add(
            FString::Printf(TEXT("%s.attachment_configs[%d]"), *section, i),
            v->attachment_configs[i]
        );
    }
}

void STerrainDebugTable::add(const FString& section, const Prepass* v) {
    add(
        section + TEXT(".terrain"),
        reinterpret_cast<TRDGUniformBuffer<Terrain>*>(
            v->terrain.GetUniformBuffer())->GetContents()
    );
    add(
        section,
        TEXT("attachment_configs"),
        v->attachment_configs != nullptr ? TEXT("bound") : TEXT("null")
    );
}

void STerrainDebugTable::add(const FString& section, const RefineTiles* v) {
    add(
        section + TEXT(".terrain"),
        reinterpret_cast<TRDGUniformBuffer<Terrain>*>(
            v->terrain.GetUniformBuffer())->GetContents()
    );
    add(
        section,
        TEXT("attachment_configs"),
        v->attachment_configs != nullptr ? TEXT("bound") : TEXT("null")
    );
}

void STerrainDebugTable::add(const FString& section, const FGpuTerrainView* v) {
    add(section, TEXT("order"), LexToString(v->order));
    add(section, TEXT("refinement_count"), LexToString(v->refinement_count));
    add(section, TEXT("prepass_parameters"), to_string(v->prepass_parameters));
    add(section, TEXT("refine_tiles_parameters"), to_string(v->refine_tiles_parameters));

    if (v->prepass_parameters != nullptr) {
        add(section + TEXT(".prepass"), v->prepass_parameters);
    }

    if (v->refine_tiles_parameters != nullptr) {
        add(section + TEXT(".refine_tiles"), v->refine_tiles_parameters);
    }
}
