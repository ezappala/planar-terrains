#include "terrain_scene_view_extension.h"

#include "ImageUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphEvent.h"
#include "RenderGraphUtils.h"
#include "SystemTextures.h"
#include "terrain_funcs.h"
#include "terrain_render_state.h"
#include "terrain_shaders.h"
#include "terrain_tile_loader.h"
#include "terrain_world_subsystem.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Logging/StructuredLog.h"
#include "Runtime/Renderer/Private/SceneRendering.h"

namespace {
uint32 compute_vertices_per_tile(const FTileTree& tile_tree) {
    return 2u * tile_tree.grid_size * (tile_tree.grid_size + 2u);
}

FTerrainMeshViewState build_mesh_view_state(
    FRDGBuilder& gb,
    const uint32 view_key,
    const FTileTree& tile_tree,
    const FGpuTerrain& gpu_terrain,
    const FGpuTerrainView& gpu_terrain_view,
    const FGpuAttachment& height_attachment,
    const FGpuAttachment& albedo_attachment
) {
    FTerrainMeshViewState view_state;
    view_state.terrain_uniform_buffer = gpu_terrain.terrain_buffer;
    const TRefCountPtr<FRDGPooledBuffer>& attachments_buffer = ConvertToExternalAccessBuffer(
        gb,
        gpu_terrain.attachments_buffer,
        ERHIAccess::SRVMask
    );
    view_state.attachments_buffer_srv = attachments_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );
    view_state.terrain_sampler = gpu_terrain.atlas_sampler;
    view_state.height_attachment_texture = ConvertToExternalAccessTexture(
        gb,
        height_attachment.atlas_texture_resource,
        ERHIAccess::SRVMask
    )->GetRHI();
    view_state.albedo_attachment_texture = ConvertToExternalAccessTexture(
        gb,
        albedo_attachment.atlas_texture_resource,
        ERHIAccess::SRVMask
    )->GetRHI();

    const TRefCountPtr<FRDGPooledBuffer>& terrain_view_buffer = ConvertToExternalAccessBuffer(
        gb,
        tile_tree.terrain_view_buffer,
        ERHIAccess::SRVMask
    );
    const TRefCountPtr<FRDGPooledBuffer>& approximate_height_buffer = ConvertToExternalAccessBuffer(
        gb,
        tile_tree.approximate_height_buffer,
        ERHIAccess::SRVMask
    );
    const TRefCountPtr<FRDGPooledBuffer>& tile_tree_buffer = ConvertToExternalAccessBuffer(
        gb,
        tile_tree.tile_tree_buffer,
        ERHIAccess::SRVMask
    );
    const TRefCountPtr<FRDGPooledBuffer>& geometry_tiles_buffer = ConvertToExternalAccessBuffer(
        gb,
        gpu_terrain_view.final_tiles_buffer,
        ERHIAccess::SRVMask
    );

    view_state.terrain_view_buffer_srv = terrain_view_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );
    view_state.approximate_height_buffer_srv = approximate_height_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );
    view_state.tile_tree_buffer_srv = tile_tree_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );
    view_state.geometry_tiles_buffer_srv = geometry_tiles_buffer->GetOrCreateSRV(
        gb.RHICmdList,
        FRHIBufferSRVCreateInfo()
    );

    view_state.indirect_args_buffer = ConvertToExternalAccessBuffer(
        gb,
        gpu_terrain_view.indirect_args_buffer,
        ERHIAccess::IndirectArgs
    );
    view_state.indirect_args_offset = 0;
    view_state.max_vertex_index = FMath::Max(
        tile_tree.geometry_tile_count * compute_vertices_per_tile(tile_tree),
        1u
    ) - 1u;
    (void)view_key;
    return view_state;
}
}

void FTerrainSceneViewExtension::BeginRenderViewFamily(FSceneViewFamily& in_view_family) {
    if (UWorld* world = GetWorld()) {
        if (UTerrainWorldSubsystem* subsystem = world->GetSubsystem<UTerrainWorldSubsystem>()) {
            root = subsystem->resolve_terrain_root(true);
        }
    }
}

void FTerrainSceneViewExtension::PreRenderViewFamily_RenderThread(
    FRDGBuilder& gb,
    FSceneViewFamily& in_view_family
) {
    (void)gb;
    (void)in_view_family;

    if (const ATerrainParentActor* root_actor = root.Get()) {
        if (const UTerrain* terrain_component = root_actor->spawned_terrain) {
            if (terrain_component->render_resources.IsValid()) {
                terrain_component->render_resources->ResetViewStates();
            }
        }
    }
}

void FTerrainSceneViewExtension::PreRenderView_RenderThread(FRDGBuilder& gb, FSceneView& in_view) {
    if (in_view.Family == nullptr || in_view.Family->Scene == nullptr) {
        return;
    }
    if (!in_view.bIsViewInfo) {
        return;
    }

    if (!stopped_error_spam && error_spam_buffer >= MAX_ERROR_SPAM_BUFFER) {
        UE_LOGFMT(
            LogTemp,
            Error,
            "[FTerrainSceneViewExtension::PreRenderView_RenderThread] "
            "Reached max error spam buffer, stopping further warnings."
        );
        stopped_error_spam = true;
    }

    const FViewInfo& view_info = reinterpret_cast<FViewInfo&>(in_view);
    draw_terrain(gb, view_info);
}

FRDGTextureSRVRef FTerrainSceneViewExtension::CreateRDGTextureFromUTexture(
    FRDGBuilder& gb,
    UTexture* texture,
    const TCHAR* name
) {
    if (texture == nullptr || texture->GetResource() == nullptr) {
        return GetDefaultWhiteTexture(gb);
    }

    const FTextureResource* texture_resource = texture->GetResource();
    FRHITexture* rhi_texture = texture_resource->TextureRHI;
    return gb.CreateSRV(gb.RegisterExternalTexture(CreateRenderTarget(rhi_texture, name)));
}

FRDGTextureSRVRef FTerrainSceneViewExtension::CreateUTextureFromFilePath(
    FRDGBuilder& gb,
    const FString& path,
    const TCHAR* name
) {
    UTexture2D* tex = FImageUtils::ImportFileAsTexture2D(path);
    if (tex == nullptr) {
        return GetDefaultWhiteTexture(gb);
    }
    return CreateRDGTextureFromUTexture(gb, tex, name);
}

FRDGTextureSRVRef FTerrainSceneViewExtension::GetDefaultWhiteTexture(FRDGBuilder& gb) {
    return gb.CreateSRV(GSystemTextures.GetWhiteDummy(gb));
}

void FTerrainSceneViewExtension::draw_terrain(
    FRDGBuilder& gb,
    const FViewInfo& view
) const {
    RDG_EVENT_SCOPE(gb, "UDLOD.DrawTerrain");

    ATerrainParentActor* root_actor = root.Get();
    if (!IsValid(root_actor)) {
        return;
    }
    if (!IsValid(root_actor->spawned_terrain)) {
        return;
    }

    auto* tile_tree = root_actor->view_component.GetPtrOrNull();
    auto* tile_atlas = root_actor->tile_atlas.GetPtrOrNull();
    if (tile_tree == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(LogTemp, Warning, "[FTerrainSceneViewExtension::draw_terrain] Tile tree is null.");
        }
        return;
    }

    if (tile_atlas == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(LogTemp, Warning, "[FTerrainSceneViewExtension::draw_terrain] Tile atlas is null");
        }
        return;
    }

    tile_tree_compute_requests(tile_tree, root_actor->spawned_terrain->GetComponentTransform(), view);
    tile_atlas_update(tile_tree, tile_atlas);
    terrain::tile_loader::drain_tile_loads(tile_atlas);
    tile_tree_adjust_to_tile_atlas(tile_tree, tile_atlas);
    tile_tree_update_terrain_view_buffer(gb, tile_tree);

    if (!root_actor->spawned_terrain->IsValidLowLevel() || !root_actor->spawned_terrain->IsRegistered()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::draw_terrain] "
                "No registered terrain actors found for terrain rendering, skipping"
            );
        }
        return;
    }

    draw_tile_tree(gb, view);
}

void FTerrainSceneViewExtension::draw_tile_tree(
    FRDGBuilder& gb,
    const FViewInfo& view
) const {
    RDG_EVENT_SCOPE(gb, "UDLOD.DrawTileTree");

    ATerrainParentActor* root_actor = root.Get();
    if (!IsValid(root_actor) || !IsValid(root_actor->spawned_terrain)) {
        return;
    }

    auto* tile_atlas = root_actor->tile_atlas.GetPtrOrNull();
    if (tile_atlas == nullptr) {
        return;
    }

    const auto* tile_tree = root_actor->view_component.GetPtrOrNull();
    if (tile_tree == nullptr) {
        return;
    }

    auto* gpu_tile_atlas = &root_actor->gpu_tile_atlas;
    auto* gpu_terrain = &root_actor->gpu_terrain;
    auto* gpu_terrain_view = &root_actor->gpu_terrain_view;
    const FTerrainSettings terrain_settings = root_actor->settings;

    FGpuTileAtlas::initialize(gb, *gpu_tile_atlas, *tile_atlas, terrain_settings);
    FGpuTileAtlas::extract(*tile_atlas, *gpu_tile_atlas);
    FGpuTerrain::initialize(gb, GetDefaultWhiteTexture(gb), *gpu_terrain, *gpu_tile_atlas);
    FGpuTerrainView::initialize(gb, *gpu_terrain_view, tile_tree);

    FGpuTileAtlas::prepare(gb, *gpu_tile_atlas);
    FGpuTerrainView::prepare(gb, *gpu_terrain_view);

    const auto* gsm = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const auto prep_root_cs = gsm->GetShader<FTerrainPrepassPrepareRootComputeShader>();
    const auto prep_next_cs = gsm->GetShader<FTerrainPrepassPrepareNextComputeShader>();
    const auto prep_render_cs = gsm->GetShader<FTerrainPrepassPrepareRenderComputeShader>();
    const auto refine_tiles_cs = gsm->GetShader<FTerrainPrepassRefineTilesComputeShader>();

    if (!gpu_terrain_view->IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(LogTemp, Warning, "[FTerrainSceneViewExtension::DrawTileTree] No GPU terrain view.");
        }
        return;
    }

    if (!gpu_terrain->IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(LogTemp, Warning, "[FTerrainSceneViewExtension::DrawTileTree] No GPU terrain.");
        }
        return;
    }

    if (!gpu_tile_atlas->IsSet()) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(LogTemp, Warning, "[FTerrainSceneViewExtension::DrawTileTree] No GPU tile atlas.");
        }
        return;
    }

    const TStaticArray<AttachmentConfig, 2> attachment_configs = (*gpu_terrain)->attachment_configs;
    const FGpuAttachment* height_attachment_ptr = nullptr;
    const FGpuAttachment* albedo_attachment_ptr = nullptr;
    for (const auto& [label, attachment] : (*gpu_tile_atlas)->attachments) {
        if (height_attachment_ptr == nullptr && label.Equals(TEXT("height"), ESearchCase::IgnoreCase)) {
            height_attachment_ptr = &attachment;
        }
        if (albedo_attachment_ptr == nullptr && label.Equals(TEXT("albedo"), ESearchCase::IgnoreCase)) {
            albedo_attachment_ptr = &attachment;
        }
    }

    if (height_attachment_ptr == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] No height attachment found in GPU tile atlas"
            );
        }
        return;
    }

    if (height_attachment_ptr->atlas_texture == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Invalid height attachment view in GPU tile atlas"
            );
        }
        return;
    }

    if (albedo_attachment_ptr == nullptr || albedo_attachment_ptr->atlas_texture == nullptr) {
        error_spam_buffer += 1;
        if (error_spam_buffer < MAX_ERROR_SPAM_BUFFER) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FTerrainSceneViewExtension::DrawTileTree] Missing albedo attachment; using height attachment fallback"
            );
        }
        albedo_attachment_ptr = height_attachment_ptr;
    }

    const Terrain terrain_uniform = new_terrain(
        tile_atlas->lod_count,
        ext::math::scale(tile_atlas->side_length),
        tile_atlas->min_height,
        tile_atlas->max_height,
        tile_atlas->height_scale,
        root_actor->spawned_terrain->GetComponentTransform()
    );
    (*gpu_terrain)->terrain_buffer = TUniformBufferRef<Terrain>::CreateUniformBufferImmediate(
        terrain_uniform,
        UniformBuffer_MultiFrame
    );

    Attachments attachments;
    attachments.attachment_configs = attachment_configs;

    const FRDGBufferSRVRef tile_tree_buffer = gb.CreateSRV(tile_tree->tile_tree_buffer);
    const FRDGBufferSRVRef approximate_height_srv = gb.CreateSRV(tile_tree->approximate_height_buffer);
    const FRDGBufferUAVRef approximate_height_uav = gb.CreateUAV(tile_tree->approximate_height_buffer);

    (*gpu_terrain_view)->prepass_parameters->terrain = (*gpu_terrain)->terrain_buffer;
    (*gpu_terrain_view)->prepass_parameters->attachments = attachments;
    (*gpu_terrain_view)->prepass_parameters->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    (*gpu_terrain_view)->prepass_parameters->height_attachment = height_attachment_ptr->atlas_texture;
    (*gpu_terrain_view)->prepass_parameters->albedo_attachment = albedo_attachment_ptr->atlas_texture;
    (*gpu_terrain_view)->prepass_parameters->approximate_height = approximate_height_uav;
    (*gpu_terrain_view)->prepass_parameters->tile_tree = tile_tree_buffer;

    (*gpu_terrain_view)->refine_tiles_parameters->terrain = (*gpu_terrain)->terrain_buffer;
    (*gpu_terrain_view)->refine_tiles_parameters->attachments = attachments;
    (*gpu_terrain_view)->refine_tiles_parameters->terrain_sampler = (*gpu_terrain)->atlas_sampler;
    (*gpu_terrain_view)->refine_tiles_parameters->height_attachment = height_attachment_ptr->atlas_texture;
    (*gpu_terrain_view)->refine_tiles_parameters->albedo_attachment = albedo_attachment_ptr->atlas_texture;
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

    for (uint32 refinement_step = 0; refinement_step < (*gpu_terrain_view)->refinement_count; ++refinement_step) {
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

    if (root_actor->spawned_terrain->render_resources.IsValid()) {
        root_actor->spawned_terrain->render_resources->UpdateViewState(
            view.GetViewKey(),
            build_mesh_view_state(
                gb,
                view.GetViewKey(),
                *tile_tree,
                gpu_terrain->GetValue(),
                gpu_terrain_view->GetValue(),
                *height_attachment_ptr,
                *albedo_attachment_ptr
            )
        );
    }
}
