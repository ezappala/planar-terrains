#pragma once
#include <compare>

#include "ext_iter.h"
#include "Math/IntPoint.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "Misc/Paths.h"
#include "UObject/ObjectMacros.h"

#include "preprocess_tile_coordinate.generated.h"

constexpr int32 BLOCK_SIZE = 8;

inline FVector3d position_unit_to_local(
    const FVector3d& unit_position,
    const double height,
    const FVector3d& scale
) {
    const auto local_position = scale * unit_position;
    auto local_normal = scale * FVector3d::UnitY();
    local_normal.Normalize();
    return local_position + height * local_normal;
}

inline FVector3d position_local_to_unit(
    const FVector3d& local_position,
    const FVector3d& scale
) {
    auto unit_position = local_position / scale;
    unit_position.Normalize();
    return unit_position;
}

struct FCoordinate {
    uint32 face;
    FVector2d uv;

    FVector3d unit_position() const { return {uv.X - 0.5, 0.0, uv.Y - 0.5}; }

    FVector3d local_position(const float height, const FVector3d& scale) const {
        const auto unit_pos = unit_position();
        return position_unit_to_local(unit_pos, static_cast<double>(height), scale);
    }

    static FCoordinate from_unit_position(const FVector3d& unit_position) {
        const auto uv = FVector2d{
            FMath::Clamp(unit_position.X + 0.5, 0, 1),
            FMath::Clamp(unit_position.Z + 0.5, 0, 1)
        };
        return FCoordinate{0, uv};
    }

    static FCoordinate from_local_position(
        const FVector3d& world_position,
        const FVector3d& scale
    ) {
        const auto unit_position = position_local_to_unit(world_position, scale);
        return from_unit_position(unit_position);
    }

    friend bool operator ==(const FCoordinate& a, const FCoordinate& b) {
        return a.face == b.face && a.uv == b.uv;
    }
};

FORCEINLINE uint32 GetTypeHash(const FCoordinate& coordinate) {
    return HashCombine(GetTypeHash(coordinate.face), GetTypeHash(coordinate.uv));
}

// struct FViewCoordinate {
//     FIntPoint xy;
//     FVector2f uv;
//
//     FViewCoordinate(const FCoordinate& coordinate, const uint32 lod) {
//         const auto count = FMath::Exp2(static_cast<double>(lod));
//         xy = FIntPoint{
//             static_cast<int32>(coordinate.uv.X * count),
//             static_cast<int32>(coordinate.uv.Y * count)
//         };
//
//         uv = FVector2f{
//             static_cast<float>(FMath::Frac(coordinate.uv.X)),
//             static_cast<float>(FMath::Frac(coordinate.uv.Y))
//         };
//     }
// };

enum class FaceRotation : uint8 {
    Identical = 0,
    ShiftU = 1,
    RotateCCW = 2,
    Backside = 3,
    RotateCW = 4,
    ShiftV = 5,
};

const FIntPoint NEIGHBOR_OFFSETS[8] = {
    {0, -1},
    {1, 0},
    {0, 1},
    {-1, 0},
    {-1, -1},
    {1, -1},
    {1, 1},
    {-1, 1}
};

USTRUCT(BlueprintType)
struct FTileCoordinate {
    GENERATED_BODY()
    FTileCoordinate() : face(0),
        lod(0),
        xy(FIntPoint::ZeroValue) {}

    FTileCoordinate(
        const uint32 in_face,
        const uint32 in_lod,
        const FIntPoint in_xy
    ) : face{static_cast<int32>(in_face)},
        lod{static_cast<int32>(in_lod)},
        xy{in_xy} {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 face;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 lod;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FIntPoint xy;

    FString path(FString in_path) const {
        const auto tile_block = xy / BLOCK_SIZE;
        return FPaths::Combine(
            in_path,
            FString::FromInt(lod),
            FString::Printf(TEXT("%d_%d"), tile_block.X, tile_block.Y),
            FString::Printf(TEXT("%s.tif"), *to_string()));
    }

    FString to_string() const {
        return FString::Printf(TEXT("%d_%d_%d_%d"), face, lod, xy.X, xy.Y);
    }

    TArray<FTileCoordinate> children() const {
        return ext::iter::map_range<int32, FTileCoordinate>(
            0,
            4,
            [&](
            const int32 index
        ) -> FTileCoordinate {
                return FTileCoordinate{
                    static_cast<uint32>(face),
                    static_cast<uint32>(lod) + 1,
                    {(xy.X << 1) + index % 2, (xy.Y << 1) + index / 2}
                };
            });
    }

    TOptional<FTileCoordinate> parent() const {
        if (lod == 0) { return NullOpt; }
        return FTileCoordinate{
            static_cast<uint32>(face),
            static_cast<uint32>(lod) - 1,
            {xy.X >> 1, xy.Y >> 1}
        };
    }

    static FTileCoordinate INVALID() {
        return {
            MAX_uint32,
            MAX_uint32,
            FIntPoint{MAX_int32, MAX_int32}
        };
    }

    TArray<TTuple<FTileCoordinate, FaceRotation>> neighbors() const {
        TArray<TTuple<FTileCoordinate, FaceRotation>> result{};

        for (FIntPoint offset : NEIGHBOR_OFFSETS) {
            const auto edge_position = xy + offset;
            const auto tile_count = 1 << lod;

            if (edge_position.X < 0 || edge_position.Y < 0 || edge_position.X
                >= tile_count || edge_position.Y >= tile_count) {
                result.Emplace(INVALID(), FaceRotation::Identical);
            } else {
                result.Emplace(
                    FTileCoordinate{
                        static_cast<uint32>(face),
                        static_cast<uint32>(lod),
                        edge_position
                    },
                    FaceRotation::Identical
                );
            }
        }
        return result;
    }

    friend std::strong_ordering operator <=>(
        const FTileCoordinate& a,
        const FTileCoordinate& b
    ) {
        if (const auto cmp = a.face <=> b.face; cmp != 0) { return cmp; }
        if (const auto cmp = a.lod <=> b.lod; cmp != 0) { return cmp; }
        return a.xy.X == b.xy.X
            ? a.xy.Y <=> b.xy.Y
            : a.xy.X <=> b.xy.X;
    }

    friend bool operator ==(const FTileCoordinate& a, const FTileCoordinate& b) {
        return a.face == b.face && a.lod == b.lod && a.xy == b.xy;
    }

    friend bool operator !=(const FTileCoordinate& a, const FTileCoordinate& b) {
        return !(a == b);
    }
};

FORCEINLINE uint32 GetTypeHash(const FTileCoordinate& tile_coordinate) {
    return HashCombine(
        HashCombine(GetTypeHash(tile_coordinate.face), GetTypeHash(tile_coordinate.lod)),
        GetTypeHash(tile_coordinate.xy)
    );
}
