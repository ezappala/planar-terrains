#include "terrain_scene_view_extension.h"

#include "ImageUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "SystemTextures.h"
#include "terrain_funcs.h"
#include "terrain_parent_actor.h"
// #include "terrain_picking.h"
#include "Editor.h"
#include "terrain_shaders.h"
#include "terrain_tile_atlas.h"
#include "terrain_tile_loader.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Logging/StructuredLog.h"
#include "Materials/Material.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"
#include "Runtime/Renderer/Private/SceneRendering.h"

void FTerrainSceneViewExtension::PostRenderBasePassDeferred_RenderThread(
    FRDGBuilder& gb,
    FSceneView& in_view,
    const FRenderTargetBindingSlots& render_targets,
    const TRDGUniformBufferRef<FSceneTextureUniformParameters> scene_textures
) {
    (void)scene_textures;

    if (in_view.Family == nullptr || in_view.Family->Scene == nullptr) { return; }
    if (!in_view.bIsViewInfo) { return; }

    if (!stopped_error_spam && error_spam_buffer >= MAX_ERROR_SPAM_BUFFER) {
        UE_LOGFMT(
            LogTemp,
            Error,
            "[FTerrainSceneViewExtension::PostRenderBasePassDeferred_RenderThread] "
            "Reached max error spam buffer, stopping further warnings.");
        stopped_error_spam = true;
    }

    const FViewInfo& view_info = reinterpret_cast<FViewInfo&>(in_view);
    draw_terrain(gb, view_info, render_targets);
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

void FTerrainSceneViewExtension::draw_terrain(
    FRDGBuilder& gb,
    const FViewInfo& view,
    const FRenderTargetBindingSlots& render_targets
) const {
    RDG_EVENT_SCOPE(gb, "UDLOD.DrawTerrain");

    auto* tile_tree = root->view_component.GetPtrOrNull();
    auto* tile_atlas = root->tile_atlas.GetPtrOrNull();
    if (tile_tree == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::draw_terrain] "
                "Tile tree is null.");
        }
        return;
    }

    if (tile_atlas == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::draw_terrain] "
                "Tile atlas is null");
        }
        return;
    }

    tile_tree_compute_requests(tile_tree, root->spawned_terrain->GetComponentTransform(), view);
    tile_atlas_update(tile_tree, tile_atlas);
    terrain::tile_loader::drain_tile_loads(tile_atlas);
    tile_tree_adjust_to_tile_atlas(tile_tree, tile_atlas);
    tile_tree_update_terrain_view_buffer(gb, tile_tree);

    if (!root->spawned_terrain->IsValidLowLevel() || !root->spawned_terrain->IsRegistered()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::draw_terrain] "
                "No registered terrain actors found for terrain rendering, skipping");
        }
        return;
    }

    draw_tile_tree(gb, view, render_targets);
    // FTileTree::approximate_height_readback(gb, tile_tree);
    // picking_readback(gb, root->picking_data->buffer, root->picking_data);
}

void FTerrainSceneViewExtension::draw_tile_tree(
    FRDGBuilder& gb,
    const FViewInfo& view,
    const FRenderTargetBindingSlots& render_targets
) const {
    RDG_EVENT_SCOPE(gb, "UDLOD.DrawTileTree");

    auto* tile_atlas = root->tile_atlas.GetPtrOrNull();
    if (tile_atlas == nullptr) { return; }

    const auto* tile_tree = root->view_component.GetPtrOrNull();
    if (tile_tree == nullptr) { return; }

    auto* gpu_tile_atlas = &root->gpu_tile_atlas;
    auto* gpu_terrain = &root->gpu_terrain;
    auto* gpu_terrain_view = &root->gpu_terrain_view;
    const auto terrain_settings = root->settings;

    // TODO: terrain_picking
    // TODO: extract_terrain_phases
    FGpuTileAtlas::initialize(gb, *gpu_tile_atlas, *tile_atlas, terrain_settings);
    FGpuTileAtlas::extract(*tile_atlas, *gpu_tile_atlas);
    FGpuTerrain::initialize(
        gb,
        GetDefaultWhiteTexture(gb),
        *gpu_terrain,
        *gpu_tile_atlas);
    FGpuTerrainView::initialize(gb, *gpu_terrain_view, tile_tree);

    FGpuTileAtlas::prepare(gb, *gpu_tile_atlas);
    // FGpuTerrain::prepare(gb, *gpu_terrains);
    FGpuTerrainView::prepare(gb, *gpu_terrain_view);
    // TODO: prepare_terrain_depth_textures

    // auto* picking_params = picking(
    //     gb,
    //     view,
    //     root->picking_data->buffer,
    //     view.GetSceneTexturesChecked()
    // );

    const auto* gsm = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const auto prep_root_cs = gsm->GetShader<FTerrainPrepassPrepareRootComputeShader>();
    const auto prep_next_cs = gsm->GetShader<FTerrainPrepassPrepareNextComputeShader>();
    const auto prep_render_cs = gsm->GetShader<FTerrainPrepassPrepareRenderComputeShader>();
    const auto refine_tiles_cs = gsm->GetShader<FTerrainPrepassRefineTilesComputeShader>();
    // const auto picking_cs = gsm->GetShader<FTerrainPickingComputeShader>();

    if (tile_atlas == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No tile atlas."
            );
        }
        return;
    }

    if (gpu_terrain_view == nullptr || !gpu_terrain_view->IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU terrain view."
            );
        }
        return;
    }

    if (gpu_terrain == nullptr || !gpu_terrain->IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU terrain.");
        }
        return;
    }

    if (gpu_tile_atlas == nullptr || !gpu_tile_atlas->IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No GPU tile atlas.");
        }
        return;
    }

    const auto attachment_configs = (*gpu_terrain)->attachment_configs;
    const FGpuAttachment* height_attachment_ptr = nullptr;
    const FGpuAttachment* albedo_attachment_ptr = nullptr;
    for (const auto& [label, attachment] : (*gpu_tile_atlas)->attachments) {
        if (height_attachment_ptr == nullptr && label.Equals(
            TEXT("height"),
            ESearchCase::IgnoreCase)) { height_attachment_ptr = &attachment; }
        if (albedo_attachment_ptr == nullptr && label.Equals(
            TEXT("albedo"),
            ESearchCase::IgnoreCase)) { albedo_attachment_ptr = &attachment; }
    }

    if (height_attachment_ptr == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No height attachment found in GPU tile atlas")
            ;
        }
        return;
    }

    FRDGTextureSRV* height_attachemnt = height_attachment_ptr->atlas_texture;
    if (height_attachemnt == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Invalid height attachment view in GPU tile atlas")
            ;
        }
        return;
    }

    FRDGTextureSRV* albedo_attachment = albedo_attachment_ptr != nullptr
        ? albedo_attachment_ptr->atlas_texture
        : nullptr;
    if (albedo_attachment == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Missing albedo attachment; using height attachment fallback")
            ;
        }
        albedo_attachment = height_attachemnt;
    }

    const Terrain terrain_uniform = new_terrain(
        tile_atlas->lod_count,
        ext::math::scale(tile_atlas->side_length),
        tile_atlas->min_height,
        tile_atlas->max_height,
        tile_atlas->height_scale,
        root->spawned_terrain->GetComponentTransform()
    );

    (*gpu_terrain)->terrain_buffer = TUniformBufferRef<Terrain>::CreateUniformBufferImmediate(
        terrain_uniform,
        UniformBuffer_SingleFrame
    );

    Attachments attachments;
    attachments.attachment_configs = attachment_configs;

    const FRDGBufferSRVRef tile_tree_buffer = gb.CreateSRV(tile_tree->tile_tree_buffer);
    const FRDGBufferSRVRef approximate_height_srv = gb.CreateSRV(
        tile_tree->approximate_height_buffer);
    const FRDGBufferUAVRef approximate_height_uav = gb.CreateUAV(
        tile_tree->approximate_height_buffer);

    (*gpu_terrain_view)->prepass_parameters->terrain = (*gpu_terrain)->terrain_buffer;
    (*gpu_terrain_view)->prepass_parameters->attachments = attachments;
    (*gpu_terrain_view)->prepass_parameters->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    (*gpu_terrain_view)->prepass_parameters->height_attachment = height_attachemnt;
    (*gpu_terrain_view)->prepass_parameters->albedo_attachment = albedo_attachment;
    (*gpu_terrain_view)->prepass_parameters->approximate_height = approximate_height_uav;
    (*gpu_terrain_view)->prepass_parameters->tile_tree = tile_tree_buffer;

    (*gpu_terrain_view)->refine_tiles_parameters->terrain = (*gpu_terrain)->terrain_buffer;
    (*gpu_terrain_view)->refine_tiles_parameters->attachments = attachments;
    (*gpu_terrain_view)->refine_tiles_parameters->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    (*gpu_terrain_view)->refine_tiles_parameters->height_attachment = height_attachemnt;
    (*gpu_terrain_view)->refine_tiles_parameters->albedo_attachment = albedo_attachment;
    (*gpu_terrain_view)->refine_tiles_parameters->approximate_height = approximate_height_uav;
    (*gpu_terrain_view)->refine_tiles_parameters->tile_tree = tile_tree_buffer;

    const FRDGBufferRef indirect_args_buffer = (*gpu_terrain_view)->indirect_args_buffer;

    FComputeShaderUtils::AddPass(
        gb,
        RDG_EVENT_NAME("UDLOD.Prepass.PrepRoot"),
        ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
        prep_root_cs,
        (*gpu_terrain_view)->prepass_parameters,
        FIntVector{1, 1, 1}
    );

    auto add_refine_tiles_pass = [&gb, refine_tiles_cs, indirect_args_buffer](
        RefineTiles* refine_tiles_parameters
    ) {
        FComputeShaderUtils::AddPass(
            gb,
            RDG_EVENT_NAME("UDLOD.Prepass.RefineTiles"),
            refine_tiles_cs,
            refine_tiles_parameters,
            indirect_args_buffer,
            0
        );
    };

    for (uint32 refinement_step = 0; refinement_step < (*gpu_terrain_view)->refinement_count; ++
         refinement_step) {
        add_refine_tiles_pass((*gpu_terrain_view)->refine_tiles_parameters);

        FComputeShaderUtils::AddPass(
            gb,
            RDG_EVENT_NAME("UDLOD.Prepass.PrepNext"),
            ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
            prep_next_cs,
            (*gpu_terrain_view)->prepass_parameters,
            FIntVector{1, 1, 1}
        );
    }

    add_refine_tiles_pass((*gpu_terrain_view)->refine_tiles_parameters);

    FComputeShaderUtils::AddPass(
        gb,
        RDG_EVENT_NAME("UDLOD.Prepass.PrepRender"),
        ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
        prep_render_cs,
        (*gpu_terrain_view)->prepass_parameters,
        FIntVector{1, 1, 1}
    );

    // FComputeShaderUtils::AddPass(
    //     gb,
    //     RDG_EVENT_NAME("UDLOD.Picking"),
    //     ERDGPassFlags::NeverCull | ERDGPassFlags::Compute,
    //     picking_cs,
    //     picking_params,
    //     FIntVector{1, 1, 1}
    // );

    auto* params = gb.AllocParameters<DrawElementsIndirectParameters>();
    params->View = view.ViewUniformBuffer;
    params->terrain = (*gpu_terrain)->terrain_buffer;
    params->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    params->height_attachment = height_attachemnt;
    params->albedo_attachment = albedo_attachment;
    params->attachments = attachments;
    params->terrain_view = (*gpu_terrain_view)->terrain_view_buffer;
    params->approximate_height = approximate_height_srv;
    params->tile_tree = tile_tree_buffer;
    params->geometry_tiles = (*gpu_terrain_view)->final_tiles_srv;
    // params->tile_uv = FVector3f::ZeroVector;
    // params->tile_index = 0u;
    // params->view_distance = 0.0f;
    // params->height = 0.0f;
    params->IndirectArgs = indirect_args_buffer;
    params->RenderTargets = render_targets;

    auto vertex_shader = gsm->GetShader<FTerrainVertexShader>();
    auto fragment_shader = gsm->GetShader<FTerrainFragmentShader>();

    const FIntRect& view_rect = view.ViewRect;
    const bool reverse_culling = view.bReverseCulling;

    gb.AddPass(
        RDG_EVENT_NAME("UDLOD.DrawTileTree"),
        params,
        ERDGPassFlags::NeverCull | ERDGPassFlags::Raster,
        [params, vertex_shader, fragment_shader, &view_rect, reverse_culling](
        FRHICommandList& cmd) {
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
            pso.RasterizerState = reverse_culling
                ? TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI()
                : TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI();
            pso.DepthStencilState = TStaticDepthStencilState<>::GetRHI();
            pso.PrimitiveType = PT_TriangleStrip;
            pso.BoundShaderState.VertexDeclarationRHI =
                GEmptyVertexDeclaration.VertexDeclarationRHI;
            pso.BoundShaderState.VertexShaderRHI = vertex_shader.GetVertexShader();
            pso.BoundShaderState.PixelShaderRHI = fragment_shader.GetPixelShader();

            SetGraphicsPipelineState(cmd, pso, 0);
            SetShaderParameters(
                cmd,
                vertex_shader,
                vertex_shader.GetVertexShader(),
                *params);

            SetShaderParameters(
                cmd,
                fragment_shader,
                fragment_shader.GetPixelShader(),
                *params);

            cmd.SetStreamSource(0, nullptr, 0);
            cmd.DrawPrimitiveIndirect(params->IndirectArgs->GetIndirectRHICallBuffer(), 0);
        }
    );
}
