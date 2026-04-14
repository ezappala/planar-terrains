#pragma once

#include "CoreMinimal.h"

#include "terrain_debug_settings.generated.h"

UENUM(BlueprintType)
enum class ETerrainPlanarGradientMode : uint8 {
    Dataset = 0 UMETA(DisplayName = "Dataset Gradient"),
    Earth = 1 UMETA(DisplayName = "Earth Albedo"),
    Albedo = 2 UMETA(DisplayName = "Albedo"),
    AlbedoWithFallback = 3 UMETA(DisplayName = "Albedo With Fallback"),
};

enum class ETerrainRuntimeDebugFlags : uint32 {
    None = 0u,
    Wireframe = 1u << 0,
    ShowDataLOD = 1u << 1,
    ShowGeometryLOD = 1u << 2,
    ShowTileTree = 1u << 3,
    ShowPixels = 1u << 4,
    ShowUV = 1u << 5,
    ShowNormals = 1u << 6,
    Morph = 1u << 7,
    Blend = 1u << 8,
    TileTreeLOD = 1u << 9,
    Lighting = 1u << 10,
    SampleGrad = 1u << 11,
    HighPrecision = 1u << 12,
    Freeze = 1u << 13,
    Test1 = 1u << 14,
    Test2 = 1u << 15,
    Test3 = 1u << 16,
};

FORCEINLINE constexpr ETerrainRuntimeDebugFlags operator|(
    const ETerrainRuntimeDebugFlags lhs,
    const ETerrainRuntimeDebugFlags rhs
) {
    return static_cast<ETerrainRuntimeDebugFlags>(
        static_cast<uint32>(lhs) | static_cast<uint32>(rhs)
    );
}

FORCEINLINE constexpr uint32 terrain_runtime_debug_flag(
    const ETerrainRuntimeDebugFlags flag
) {
    return static_cast<uint32>(flag);
}

USTRUCT(BlueprintType)
struct UDLODTERRAIN_API FTerrainDebugSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bWireframe = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bShowDataLOD = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bShowGeometryLOD = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bShowTileTree = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bShowPixels = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bShowUV = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bShowNormals = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bEnableMorph = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bEnableBlend = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bUseTileTreeLOD = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bEnableLighting = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bSampleGradients = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bUseHighPrecision = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bFreezeView = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bTest1 = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bTest2 = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Flags")
    bool bTest3 = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Transient, Category = "UDLOD|Debug|Display")
    ETerrainPlanarGradientMode planar_gradient_mode = ETerrainPlanarGradientMode::Earth;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Transient,
        Category = "UDLOD|Debug|Runtime",
        meta = (ClampMin = "0.0")
    )
    // WARN: Keep this aligned with FTileAtlas's runtime default so the render-thread debug override
    // cannot accidentally flatten terrain before the actor seeds its transient controls.
    float height_scale = 128000.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Transient,
        Category = "UDLOD|Debug|Runtime",
        meta = (ClampMin = "1.0")
    )
    double blend_distance = 1.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Transient,
        Category = "UDLOD|Debug|Runtime",
        meta = (ClampMin = "1.0")
    )
    double load_distance = 1.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Transient,
        Category = "UDLOD|Debug|Runtime",
        meta = (ClampMin = "1.0")
    )
    double morph_distance = 1.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Transient,
        Category = "UDLOD|Debug|Runtime",
        meta = (ClampMin = "1.0")
    )
    double subdivision_distance = 1.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Transient,
        Category = "UDLOD|Debug|Runtime",
        meta = (ClampMin = "2")
    )
    int32 grid_size = 2;
};

FORCEINLINE uint32 pack_terrain_runtime_debug_flags(
    const FTerrainDebugSettings& debug_settings
) {
    uint32 flags = terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::None);

    if (debug_settings.bWireframe) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::Wireframe);
    }
    if (debug_settings.bShowDataLOD) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::ShowDataLOD);
    }
    if (debug_settings.bShowGeometryLOD) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::ShowGeometryLOD);
    }
    if (debug_settings.bShowTileTree) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::ShowTileTree);
    }
    if (debug_settings.bShowPixels) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::ShowPixels);
    }
    if (debug_settings.bShowUV) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::ShowUV);
    }
    if (debug_settings.bShowNormals) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::ShowNormals);
    }
    if (debug_settings.bEnableMorph) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::Morph);
    }
    if (debug_settings.bEnableBlend) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::Blend);
    }
    if (debug_settings.bUseTileTreeLOD) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::TileTreeLOD);
    }
    if (debug_settings.bEnableLighting) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::Lighting);
    }
    if (debug_settings.bSampleGradients) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::SampleGrad);
    }
    if (debug_settings.bUseHighPrecision) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::HighPrecision);
    }
    if (debug_settings.bFreezeView) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::Freeze);
    }
    if (debug_settings.bTest1) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::Test1);
    }
    if (debug_settings.bTest2) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::Test2);
    }
    if (debug_settings.bTest3) {
        flags |= terrain_runtime_debug_flag(ETerrainRuntimeDebugFlags::Test3);
    }

    return flags;
}
