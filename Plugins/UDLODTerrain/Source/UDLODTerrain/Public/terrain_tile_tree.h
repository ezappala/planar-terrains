#pragma once
#include "ext_array4.h"
#include "ext_math.h"
#include "RHIGPUReadback.h"
#include "terrain_config.h"
#include "terrain_shaders.h"
#include "terrain_view_config.h"
#include "preprocess_tile_coordinate.h"
#include "Async/Async.h"

class UTerrain;

enum class ERequestState : uint8 {
    Requested,
    Released
};

struct FTileState {
    FTileCoordinate coordinate;
    ERequestState request_state;

    friend bool operator ==(const FTileState& a, const FTileState& b) {
        return a.coordinate == b.coordinate
            && a.request_state == b.request_state;
    }
};

FORCEINLINE uint32 GetTypeHash(const FTileState& tile_state) {
    return HashCombine(
        GetTypeHash(tile_state.coordinate),
        GetTypeHash(tile_state.request_state)
    );
}

struct FTileTree {
    FTileTree(
        const FTerrainConfig& config,
        const FTerrainViewConfig& view_config
    ) : tree_size{view_config.tree_size},
        lod_count{config.lod_count},
        side_length{config.side_length},
        geometry_tile_count{view_config.geometry_tile_count},
        refinement_count{view_config.refinement_count},
        grid_size{view_config.grid_size},
        morph_distance{view_config.morph_distance * config.face_size},
        blend_distance{view_config.blend_distance * config.face_size},
        load_distance{
            view_config.blend_distance * config.face_size * (1. + view_config.load_tolerance)
        },
        subdivision_distance{
            view_config.morph_distance * config.face_size * (1. + view_config.subdivision_tolerance)
        },
        morph_range{view_config.morph_range},
        blend_range{view_config.blend_range},
        precision_distance{view_config.precision_distance * config.scale_scalar},
        view_face{0},
        view_lod{view_config.view_lod},
        data{
            static_cast<ext::types::usize>(config.face_count),
            static_cast<ext::types::usize>(config.lod_count),
            static_cast<ext::types::usize>(view_config.tree_size),
            static_cast<ext::types::usize>(view_config.tree_size)
        },
        tiles{
            static_cast<ext::types::usize>(config.face_count),
            static_cast<ext::types::usize>(config.lod_count),
            static_cast<ext::types::usize>(view_config.tree_size),
            static_cast<ext::types::usize>(view_config.tree_size)
        },
        approximate_height{0.},
        order{view_config.order},
        tile_tree_buffer{nullptr},
        terrain_view_buffer{nullptr},
        // terrain_view_buffer{nullptr},
        approximate_height_buffer{nullptr} {}

    uint32 tree_size;
    uint32 lod_count;
    double side_length;
    uint32 geometry_tile_count;
    uint32 refinement_count;
    uint32 grid_size;
    double morph_distance;
    double blend_distance;
    double load_distance;
    double subdivision_distance;
    float morph_range;
    float blend_range;
    double precision_distance;
    uint32 view_face;
    uint32 view_lod;
    FVector3d view_local_position;
    FVector3f view_world_position;

    TArray4D<TileTreeEntry> data;
    TArray4D<FTileState> tiles;
    TArray<FTileCoordinate> requested_tiles;
    TArray<FTileCoordinate> released_tiles;

    TStaticArray<FCoordinate, 6> view_coordinates;
    TStaticArray<FVector4f, 6> half_spaces;
    float approximate_height;
    uint32 order;

    FRDGBufferRef tile_tree_buffer;
    FRDGBufferRef terrain_view_buffer;
    // FRDGUniformBufferRef terrain_view_buffer;
    FRDGBufferRef approximate_height_buffer;

    friend bool operator==(const FTileTree& a, const FTileTree& b) {
        return a.tree_size == b.tree_size
            && a.lod_count == b.lod_count
            && a.side_length == b.side_length
            && a.geometry_tile_count == b.geometry_tile_count
            && a.refinement_count == b.refinement_count
            && a.grid_size == b.grid_size
            && a.morph_distance == b.morph_distance
            && a.blend_distance == b.blend_distance
            && a.load_distance == b.load_distance
            && a.subdivision_distance == b.subdivision_distance
            && a.morph_range == b.morph_range
            && a.blend_range == b.blend_range
            && a.precision_distance == b.precision_distance
            && a.view_face == b.view_face
            && a.view_lod == b.view_lod
            && a.view_local_position == b.view_local_position
            && a.view_world_position == b.view_world_position
            && a.approximate_height == b.approximate_height
            && a.order == b.order
            && a.data == b.data
            && a.tiles == b.tiles;
    }

    static void approximate_height_readback(
        FRDGBuilder& gb,
        TMap<UTerrain*, FTileTree> tile_trees) {
        for (auto& [terrain, tile_tree] : tile_trees) {
            const FRDGBufferRef approximate_height_buffer = tile_tree.approximate_height_buffer;
            if (approximate_height_buffer == nullptr) { continue; }
            FRHIGPUBufferReadback* buffer_readback =
                new FRHIGPUBufferReadback(TEXT("UDLOD.ApproximateHeightReadback"));
            AddEnqueueCopyPass(gb, buffer_readback, approximate_height_buffer, 0u);
            auto async_callback = [&tile_tree](const float out_value) {
                tile_tree.approximate_height = out_value;
            };

            auto runner = [buffer_readback, async_callback](auto&& fn) -> void {
                if (buffer_readback->IsReady()) {
                    const float* data = static_cast<float*>(buffer_readback->Lock(1));
                    float out_value = data[0];
                    buffer_readback->Unlock();
                    AsyncTask(
                        ENamedThreads::GameThread,
                        [async_callback, out_value] {
                            async_callback(out_value);
                        });
                    delete buffer_readback;
                } else {
                    AsyncTask(ENamedThreads::ActualRenderingThread, [fn] { fn(fn); });
                }
            };

            AsyncTask(ENamedThreads::ActualRenderingThread, [runner] { runner(runner); });
        }
    }

    static FVector2d compute_tree_xy(const FCoordinate& coordinate, const double tile_count) {
        return FVector2d::Min(
            coordinate.uv * tile_count,
            FVector2d{tile_count - 1e-6, tile_count - 1e-6});
    }

    FIntVector2 compute_origin(const FCoordinate& view_coordinate, const uint32 lod) const {
        const auto tile_count = FMath::Exp2(static_cast<double>(lod));
        const auto tree_xy = compute_tree_xy(view_coordinate, tile_count);

        return {
            static_cast<int32>(FMath::Clamp(
                FMath::RoundHalfFromZero(tree_xy.X - 0.5 * static_cast<double>(tree_size)),
                0.,
                tile_count - static_cast<double>(tree_size)
            )),
            static_cast<int32>(FMath::Clamp(
                FMath::RoundHalfFromZero(tree_xy.Y - 0.5 * static_cast<double>(tree_size)),
                0.,
                tile_count - static_cast<double>(tree_size)
            )),
        };
    }

    double compute_tile_distance(
        const FTileCoordinate& tile,
        const FCoordinate& view_coordinate
    ) const {
        const auto tile_count = FMath::Exp2(static_cast<double>(tile.lod));
        const auto view_tile_xy = compute_tree_xy(view_coordinate, tile_count);
        const auto tile_offset = FIntVector2{
            tile.xy.X - static_cast<int32>(view_tile_xy.X),
            tile.xy.Y - static_cast<int32>(view_tile_xy.Y)
        };
        auto offset = FVector2d{
            FMath::Fmod(view_tile_xy.X, 1.),
            FMath::Fmod(view_tile_xy.Y, 1.)
        };

        if (tile_offset.X < 0) { offset.X = 0.; } else if (tile_offset.X > 0) { offset.X = 1.; }

        if (tile_offset.Y < 0) { offset.Y = 0.; } else if (tile_offset.Y > 0) { offset.Y = 1.; }

        const auto tile_position = FCoordinate{
            static_cast<uint32>(tile.face),
            FVector2d{
                tile.xy.X + offset.X,
                tile.xy.Y + offset.Y
            } / tile_count
        };
        const auto tile_local_position = tile_position.local_position(
            approximate_height,
            static_cast<FVector3d>(ext::math::scale(side_length)));

        return FVector3d::Distance(tile_local_position, view_local_position);
    }

    void update() {
        const auto view_coordinate = FCoordinate::from_local_position(
            view_local_position,
            static_cast<FVector3d>(ext::math::scale(side_length))
        );

        view_face = view_coordinate.face;
        // WARN: Assert face count is 0;
        view_coordinates[0] = view_coordinate;
        for (const auto& lod : ext::iter::range<TArray<int32>, int32>(0, lod_count)) {
            const auto origin = compute_origin(view_coordinate, lod);
            for (const auto& [x, y] : ext::iter::product(
                     ext::iter::range<TArray<int32>, int32>(0, tree_size),
                     ext::iter::range<TArray<int32>, int32>(0, tree_size))
            ) {
                const auto tile_coordinate = FTileCoordinate{
                    0u,
                    static_cast<uint32>(lod),
                    FIntPoint{origin.X + x + origin.Y + y}
                };
                const auto tile_dist = compute_tile_distance(tile_coordinate, view_coordinate);
                const auto load_dist =
                    this->load_distance / FMath::Exp2(static_cast<double>(tile_coordinate.lod));
                const auto state = [&lod, &tile_dist, &load_dist] {
                    if (lod == 0 || tile_dist < load_dist) { return ERequestState::Requested; }
                    return ERequestState::Released;
                }();

                auto* tile = &tiles(
                    0,
                    lod,
                    tile_coordinate.xy.X % tree_size,
                    tile_coordinate.xy.Y % tree_size
                );

                if (tile_coordinate != tile->coordinate) {
                    if (tile->request_state == ERequestState::Requested) {
                        tile->request_state = ERequestState::Released;
                        released_tiles.Add(tile->coordinate);
                    }
                    tile->coordinate = tile_coordinate;
                }

                if (tile->request_state == ERequestState::Released && state ==
                    ERequestState::Requested) {
                    tile->request_state = ERequestState::Requested;
                    requested_tiles.Add(tile->coordinate);
                } else if (tile->request_state == ERequestState::Requested && state ==
                    ERequestState::Released) {
                    tile->request_state = ERequestState::Released;
                    released_tiles.Add(tile->coordinate);
                }
            }
        }
    }
};

FORCEINLINE uint32 GetTypeHash(const FTileTree& tile_tree) {
    return HashCombine(
        HashCombine(
            HashCombine(
                HashCombine(
                    HashCombine(
                        HashCombine(
                            HashCombine(
                                GetTypeHash(tile_tree.tree_size),
                                GetTypeHash(tile_tree.lod_count)
                            ),
                            GetTypeHash(tile_tree.side_length)
                        ),
                        GetTypeHash(tile_tree.geometry_tile_count)
                    ),
                    GetTypeHash(tile_tree.refinement_count)
                ),
                GetTypeHash(tile_tree.grid_size)
            ),
            GetTypeHash(tile_tree.morph_distance)
        ),
        HashCombine(
            HashCombine(
                HashCombine(
                    HashCombine(
                        GetTypeHash(tile_tree.blend_distance),
                        GetTypeHash(tile_tree.load_distance)
                    ),
                    GetTypeHash(tile_tree.subdivision_distance)
                ),
                GetTypeHash(tile_tree.morph_range)
            ),
            HashCombine(
                GetTypeHash(tile_tree.blend_range),
                HashCombine(
                    GetTypeHash(tile_tree.precision_distance),
                    HashCombine(
                        GetTypeHash(tile_tree.view_face),
                        GetTypeHash(tile_tree.view_lod)
                    )
                )
            )
        )
    );
}
