#include "terrain_scene_view_extension.h"

#include "ImageUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "SystemTextures.h"
#include "terrain_actor.h"
#include "terrain_funcs.h"
#include "terrain_parent_actor.h"
#include "terrain_shaders.h"
#include "terrain_world_subsystem.h"
#include "TextureResource.h"
#include "terrain_tile_atlas.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Logging/StructuredLog.h"
#include "Materials/Material.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"
#include "Runtime/Renderer/Private/SceneRendering.h"

#include "Runtime/Renderer/Private/SpriteIndexBuffer.h"

TGlobalResource<FSpriteIndexBuffer<64>> GSpriteIndexBuffer;

void FTerrainSceneViewExtension::PreRenderView_RenderThread(
    FRDGBuilder& gb,
    FSceneView& in_view
) {
    if (in_view.Family == nullptr || in_view.Family->Scene == nullptr) { return; }
    if (!in_view.bIsViewInfo) { return; }

    const FViewInfo& view_info = reinterpret_cast<FViewInfo&>(in_view);
    if (!terrains_spawned) {
        spawn_terrains(gb, *in_view.Family->Scene);
        terrains_spawned = true;
    }

    draw_terrain(gb, *in_view.Family->Scene->GetRenderScene(), view_info);
}

FRDGTextureSRVRef FTerrainSceneViewExtension::CreateRDGTextureFromUTexture(
    FRDGBuilder& gb,
    UTexture* texture,
    const TCHAR* name
) {
    if (texture == nullptr || texture->GetResource() == nullptr) {
        return GetDefaultWhiteTexture(gb);
    }

    const FTextureResource* TextureResource = texture->GetResource();
    FRHITexture* RHITexture = TextureResource->TextureRHI;

    return gb.CreateSRV(gb.RegisterExternalTexture(CreateRenderTarget(RHITexture, name)));
}

FRDGTextureSRVRef FTerrainSceneViewExtension::CreateUTextureFromFilePath(
    FRDGBuilder& gb,
    const FString& path,
    const TCHAR* name
) {
    auto* tex = FImageUtils::ImportFileAsTexture2D(path);
    if (tex == nullptr) { return GetDefaultWhiteTexture(gb); }
    return CreateRDGTextureFromUTexture(gb, tex, name);
}

FRDGTextureSRVRef FTerrainSceneViewExtension::GetDefaultWhiteTexture(
    FRDGBuilder& gb
) {
    // Use system white texture
    return gb.CreateSRV(GSystemTextures.GetWhiteDummy(gb));
}

void FTerrainSceneViewExtension::spawn_terrains(
    FRDGBuilder& gb,
    FSceneInterface& scene
) {
    RDG_EVENT_SCOPE(gb, "UDLOD.SpawnTerrains");
    auto* world = scene.GetWorld();
    auto subsys = world->GetSubsystemChecked<UTerrainWorldSubsystem>();
    auto root = subsys->get_terrain_root_checked();
    auto* terrains = root->terrains.Get();
    auto* settings = root->settings.Get();
    auto* materials = root->materials.Get();
    auto* tile_trees = root->view_components.Get();

    for (const auto& terrain : *terrains) {
        const auto& [config, view_config, material, view] = terrain;
        FActorSpawnParameters params{};
        params.Owner = root;
        const auto* terrain_actor = world->SpawnActor<ATerrainActor>(params);
        // Spawn terrain actors based on the provided configurations
        terrain_actor->GetRootComponent()
            // TODO: set transform from scale?
            ->SetWorldTransform(FTransform::Identity);

        terrain_actor->terrain->set_object_data(config, *settings, material);
        // TODO: Do we need to track this separately
        materials->Add(material);

        auto tile_tree = FTileTree{config, view_config};
        auto* approximate_height = gb.AllocParameters<ApproximateHeight>();
        approximate_height->value = tile_tree.approximate_height;
        tile_tree.approximate_height_buffer = CreateStructuredBuffer(
            gb,
            TEXT("ApproximateHeightBuffer"),
            sizeof(ApproximateHeight),
            1,
            approximate_height,
            sizeof(ApproximateHeight)
        );

        tile_tree.terrain_view_buffer = terrain_view_from_tile_tree(gb, tile_tree);
        tile_tree.tile_tree_buffer = tile_tree_from_cpu_tree(gb, tile_tree);

        tile_trees->Emplace(terrain_actor->terrain, tile_tree);
    }
}

void FTerrainSceneViewExtension::draw_terrain(
    FRDGBuilder& gb,
    const FScene& scene,
    const FViewInfo& view
) const {
    RDG_EVENT_SCOPE(gb, "UDLOD.DrawTerrain");

    const auto* world = scene.GetWorld();
    if (world == nullptr) {
        UE_LOGFMT(
            LogTemp,
            Fatal,
            "[FTerrainSceneViewExtension::DrawTerrain] World is nullptr nullptr");
    }
    const auto subsys = world->GetSubsystemChecked<UTerrainWorldSubsystem>();
    const auto root = subsys->get_terrain_root_checked();
    TMap<UTerrain*, FTileTree>* tile_trees = root->view_components.Get();

    auto* tile_atlases = root->tile_atlases.Get();

    TSet<FTileAtlas> atlases_to_update;
    for (auto& [terrain_comp, atlas] : *tile_atlases) { atlases_to_update.Emplace(atlas); }

    TSet<FTileAtlas> atlases_seen;
    TArray<TPair<FTileAtlas, FTransform>> atlases_to_update_with_transform;
    for (auto& [terrain_comp, atlas] : *tile_atlases) {
        if (!atlases_seen.Contains(atlas)) {
            atlases_to_update_with_transform.Emplace(atlas, terrain_comp->GetComponentTransform());
            atlases_seen.Add(atlas);
        }
    }

    tile_tree_compute_requests(*tile_trees, view);
    tile_atlas_update(*tile_trees, atlases_to_update);
    tile_tree_adjust_to_tile_atlas(*tile_trees, *tile_atlases);
    tile_tree_update_terrain_view_buffer(gb, *tile_trees);
    FTileAtlas::update_terrain_buffer(gb, atlases_to_update_with_transform);

    for (const auto& [terrain, tile_tree] : *tile_trees) {
        if (!terrain->IsValidLowLevel() || !terrain->IsRegistered()) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTerrain] Skipping invalid terrain actor, {n}",
                terrain->GetName()
            );
            continue;
        }

        draw_tile_tree(gb, scene, view);
    }

    FTileTree::approximate_height_readback(gb, *tile_trees);
}

void FTerrainSceneViewExtension::draw_tile_tree(
    FRDGBuilder& gb,
    const FScene& scene,
    const FViewInfo& view
) const {
    RDG_EVENT_SCOPE(gb, "UDLOD.DrawTileTree");

    const UWorld* world = scene.GetWorld();
    const auto subsys = world->GetSubsystemChecked<UTerrainWorldSubsystem>();
    const auto root = subsys->get_terrain_root_checked();

    auto* tile_atlases = root->tile_atlases.Get();
    auto* tile_trees = root->view_components.Get();
    auto* gpu_tile_atlases = root->gpu_tile_atlases.Get();
    auto* gpu_terrains = root->gpu_terrains.Get();
    auto* gpu_terrain_views = root->gpu_terrain_views.Get();
    const auto* terrain_settings = root->settings.Get();

    // TODO: terrain_picking
    // TODO: extract_terrain_phases
    FGpuTileAtlas::initialize(gb, *gpu_tile_atlases, *tile_atlases, *terrain_settings);
    FGpuTileAtlas::extract(*tile_atlases, *gpu_tile_atlases);
    FGpuTerrain::initialize(
        gb,
        GetDefaultWhiteTexture(gb),
        *gpu_terrains,
        *gpu_tile_atlases,
        *tile_atlases);
    FGpuTerrainView::initialize(gb, *gpu_terrain_views, *tile_trees);

    FGpuTileAtlas::prepare(gb, *gpu_tile_atlases);
    // FGpuTerrain::prepare(gb, *gpu_terrains);
    FGpuTerrainView::prepare(gb, *gpu_terrain_views);
    // TODO: prepare_terrain_depth_textures

    const auto* gsm = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const auto prep_root_cs = gsm->GetShader<FTerrainPrepassPrepareRootComputeShader>();
    const auto prep_next_cs = gsm->GetShader<FTerrainPrepassPrepareNextComputeShader>();
    const auto prep_render_cs = gsm->GetShader<FTerrainPrepassPrepareRenderComputeShader>();
    const auto refine_tiles_cs = gsm->GetShader<FTerrainPrepassRefineTilesComputeShader>();

    using ext::iter::parallel::par_for_each;

    par_for_each<TMap, UTerrain*, FGpuTerrainView>(
        *gpu_terrain_views,
        [&gb, &prep_root_cs](UTerrain*, const FGpuTerrainView& gpu_terrain_view) {
            FComputeShaderUtils::AddPass(
                gb,
                RDG_EVENT_NAME("UDLOD.Prepass.PrepRoot"),
                ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
                prep_root_cs,
                gpu_terrain_view.prepass_parameters,
                FIntVector{1, 1, 1}
            );
        }
    );

    par_for_each<TMap, UTerrain*, FGpuTerrainView>(
        *gpu_terrain_views,
        [&gb, &prep_next_cs](UTerrain*, const FGpuTerrainView& gpu_terrain_view) {
            FComputeShaderUtils::AddPass(
                gb,
                RDG_EVENT_NAME("UDLOD.Prepass.PrepRoot"),
                ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
                prep_next_cs,
                gpu_terrain_view.prepass_parameters,
                FIntVector{1, 1, 1}
            );
        }
    );

    par_for_each<TMap, UTerrain*, FGpuTerrainView>(
        *gpu_terrain_views,
        [&gb, &prep_render_cs](UTerrain*, const FGpuTerrainView& gpu_terrain_view) {
            FComputeShaderUtils::AddPass(
                gb,
                RDG_EVENT_NAME("UDLOD.Prepass.PrepRoot"),
                ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
                prep_render_cs,
                gpu_terrain_view.prepass_parameters,
                FIntVector{1, 1, 1}
            );
        }
    );

    par_for_each<TMap, UTerrain*, FGpuTerrainView>(
        *gpu_terrain_views,
        [&gb, &refine_tiles_cs](UTerrain*, const FGpuTerrainView& gpu_terrain_view) {
            FComputeShaderUtils::AddPass(
                gb,
                RDG_EVENT_NAME("UDLOD.Prepass.PrepRoot"),
                ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
                refine_tiles_cs,
                gpu_terrain_view.refine_tiles_parameters,
                FIntVector{64, 1, 1}
            );
        }
    );

    for (const auto& [terrain, tile_tree] : *tile_trees) {
        if (!terrain->IsValidLowLevel() || !terrain->IsRegistered()) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Skipping invalid terrain actor, {n}",
                terrain->GetName()
            );
            continue;
        }

        const auto* gpu_terrain_view = gpu_terrain_views->Find(terrain);
        if (gpu_terrain_view == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU terrain view found for terrain {n}, skipping",
                terrain->GetName()
            );
            continue;
        }

        const auto* gpu_terrain = gpu_terrains->Find(terrain);
        if (gpu_terrain == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU terrain found for terrain {n}, skipping",
                terrain->GetName()
            );
            continue;
        }

        auto* gpu_tile_atlas = gpu_tile_atlases->Find(terrain);
        if (gpu_tile_atlas == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU tile atlas found for terrain {n}, skipping",
                terrain->GetName()
            );
            continue;
        }

        const auto* terrain_buffer = gpu_terrain->terrain_buffer;
        if (terrain_buffer == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Invalid GPU terrain buffer for terrain {n}, skipping",
                terrain->GetName()
            );
            continue;
        }

        // const auto* height_attachment_buffer = gpu_terrain->height_attachment_buffer;
        // if (height_attachment_buffer == nullptr) {
        //     UE_LOGFMT(
        //         LogTemp,
        //         Warning,
        //         "[FTerrainSceneViewExtension::DrawTileTree] Invalid GPU terrain height attachment buffer for terrain {n}, skipping",
        //         terrain->GetName()
        //     );
        //     continue;
        // }
        //
        // const auto* albedo_attachment_buffer = gpu_terrain->albedo_attachment_buffer;
        // if (albedo_attachment_buffer == nullptr) {
        //     UE_LOGFMT(
        //         LogTemp,
        //         Warning,
        //         "[FTerrainSceneViewExtension::DrawTileTree] Invalid GPU terrain albedo attachment buffer for terrain {n}, skipping",
        //         terrain->GetName()
        //     );
        //     continue;
        // }

        const auto attachment_configs = gpu_terrain->attachment_configs;
        const FGpuAttachment* height_attachment_ptr = gpu_tile_atlas->attachments.Find("Height");
        if (height_attachment_ptr == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No height attachment found in GPU tile atlas for terrain {n}, skipping",
                terrain->GetName()
            );
            continue;
        }

        FRDGTextureSRV* height_attachemnt = height_attachment_ptr->atlas_texture;
        if (height_attachemnt == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Invalid height attachment view in GPU tile atlas for terrain {n}, skipping",
                terrain->GetName()
            );
            continue;
        }

        const FGpuAttachment* albedo_attachment_ptr = gpu_tile_atlas->attachments.Find("Albedo");
        if (albedo_attachment_ptr == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No albedo attachment found in GPU tile atlas for terrain {n}, skipping",
                terrain->GetName()
            );
            continue;
        }
        FRDGTextureSRV* albedo_attachment = albedo_attachment_ptr->atlas_texture;
        if (albedo_attachment == nullptr) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Invalid albedo attachment view in GPU tile atlas for terrain {n}, skipping",
                terrain->GetName()
            );
            continue;
        }

        auto* params = gb.AllocParameters<DrawElementsIndirectParameters>();
        params->terrain_sampler = gpu_terrain->atlas_sampler;
        params->height_attachment = height_attachemnt;
        params->albedo_attachment = albedo_attachment;
        Attachments attachments;
        attachments.attachment_configs = attachment_configs;
        params->attachments = attachments;
        params->geometry_tiles = GetAsBufferSRV(gpu_terrain_view->final_tiles_buffer);
        params->IndirectArgs = GetAsBuffer(
            reinterpret_cast<FRDGViewableResource*>(gpu_terrain_view->indirect_buffer));

        auto vertex_shader = gsm->GetShader<FTerrainVertexShader>();
        auto fragment_shader = gsm->GetShader<FTerrainFragmentShader>();

        const FIntRect& view_rect = view.ViewRect;

        gb.AddPass(
            RDG_EVENT_NAME("UDLOD.DrawTileTree"),
            params,
            ERDGPassFlags::NeverCull | ERDGPassFlags::Raster,
            [params, vertex_shader, fragment_shader, &view_rect](FRHICommandList& cmd) {
                FGraphicsPipelineStateInitializer pso;
                cmd.ApplyCachedRenderTargets(pso);
                cmd.SetViewport(
                    view_rect.Min.X,
                    view_rect.Min.Y,
                    0.0f,
                    view_rect.Max.X,
                    view_rect.Max.Y,
                    1.0f
                );

                pso.BlendState = TStaticBlendState<>::GetRHI();
                pso.RasterizerState = TStaticRasterizerState<>::GetRHI();
                pso.DepthStencilState = TStaticDepthStencilState<>::GetRHI();
                pso.PrimitiveType = GRHISupportsRectTopology ? PT_RectList : PT_TriangleList;
                pso.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.
                    VertexDeclarationRHI;
                pso.BoundShaderState.VertexShaderRHI = vertex_shader.GetVertexShader();
                pso.BoundShaderState.PixelShaderRHI = fragment_shader.GetPixelShader();

                SetGraphicsPipelineState(cmd, pso, 0);
                SetShaderParameters(cmd, vertex_shader, vertex_shader.GetVertexShader(), *params);
                SetShaderParameters(
                    cmd,
                    fragment_shader,
                    fragment_shader.GetPixelShader(),
                    *params);
                cmd.SetStreamSource(0, nullptr, 0);

                if (GRHISupportsRectTopology) {
                    cmd.DrawPrimitiveIndirect(params->IndirectArgs->GetIndirectRHICallBuffer(), 0);
                } else {
                    cmd.DrawIndexedPrimitiveIndirect(
                        GSpriteIndexBuffer.IndexBufferRHI,
                        params->IndirectArgs->GetIndirectRHICallBuffer(),
                        0);
                }
            });
    }
}

TSharedPtr<FTerrainSceneViewExtension> FTerrainSceneViewExtension::instance =
    FSceneViewExtensions::NewExtension<FTerrainSceneViewExtension>();
