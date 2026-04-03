#include "terrain_mesh_vertex_factory.h"

#include "MaterialDomain.h"
#include "MeshBatch.h"
#include "MeshDrawShaderBindings.h"
#include "MeshMaterialShader.h"
#include "ShaderParameterUtils.h"
#include "terrain_render_state.h"
#include "HAL/IConsoleManager.h"
#include "Logging/StructuredLog.h"

namespace terrain::vertex_factory::detail {
bool should_probe_gpu_mesh() {
    static const TConsoleVariableData<int32>* cvar = IConsoleManager::Get().
        FindTConsoleVariableDataInt(TEXT("r.UDLOD.MeshProbe"));
    return cvar != nullptr && cvar->GetValueOnRenderThread() != 0;
}
}

class FTerrainMeshVertexFactoryShaderParameters final : public FVertexFactoryShaderParameters {
    DECLARE_TYPE_LAYOUT(FTerrainMeshVertexFactoryShaderParameters, NonVirtual);

    void Bind(const FShaderParameterMap& parameter_map) {
        attachments_buffer_parameter.Bind(parameter_map, TEXT("attachment_configs"));
        terrain_sampler_parameter.Bind(parameter_map, TEXT("terrain_sampler"));
        height_attachment_parameter.Bind(parameter_map, TEXT("height_attachment"));
        albedo_attachment_parameter.Bind(parameter_map, TEXT("albedo_attachment"));
        terrain_view_buffer_parameter.Bind(parameter_map, TEXT("terrain_view"));
        approximate_height_buffer_parameter.Bind(parameter_map, TEXT("approximate_height"));
        tile_tree_buffer_parameter.Bind(parameter_map, TEXT("tile_tree"));
        geometry_tiles_buffer_parameter.Bind(parameter_map, TEXT("geometry_tiles"));
    }

    void GetElementShaderBindings(
        const FSceneInterface* scene,
        const FSceneView* view,
        const FMeshMaterialShader* shader,
        const EVertexInputStreamType input_stream_type,
        const ERHIFeatureLevel::Type feature_level,
        const FVertexFactory* vertex_factory,
        const FMeshBatchElement& batch_element,
        FMeshDrawSingleShaderBindings& shader_bindings,
        const FVertexInputStreamArray& vertex_streams
    ) const {
        const auto* user_data = static_cast<const FTerrainMeshBatchElementUserData*>(
            batch_element.VertexFactoryUserData
        );
        if (user_data == nullptr || !user_data->IsReady()) { return; }

        const uint32 view_key = view != nullptr ? view->GetViewKey() : 0u;

        if (attachments_buffer_parameter.IsBound() && !user_data->attachments_buffer_srv.
            IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Attachments buffer parameter is bound but the buffer SRV is invalid for view key {vk}",
                view_key);
            return;
        }

        if (terrain_sampler_parameter.IsBound() && !user_data->terrain_sampler.IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Terrain sampler parameter is bound but the sampler state is invalid for view key {vk}",
                view_key);
            return;
        }

        if (height_attachment_parameter.IsBound() && !user_data->height_attachment_texture.
            IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Height attachment parameter is bound but the texture is invalid for view key {vk}",
                view_key);
            return;
        }

        if (albedo_attachment_parameter.IsBound() && !user_data->albedo_attachment_texture.
            IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Albedo attachment parameter is bound but the texture is invalid for view key {vk}",
                view_key);
            return;
        }

        if (terrain_view_buffer_parameter.IsBound() && !user_data->terrain_view_buffer_srv.
            IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Terrain view buffer parameter is bound but the buffer SRV is invalid for view key {vk}",
                view_key);
            return;
        }

        if (approximate_height_buffer_parameter.IsBound() &&
            !user_data->approximate_height_buffer_srv.IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Approximate height buffer parameter is bound but the buffer SRV is invalid for view key {vk}",
                view_key);
            return;
        }

        if (tile_tree_buffer_parameter.IsBound() && !user_data->tile_tree_buffer_srv.IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Tile tree buffer parameter is bound but the buffer SRV is invalid for view key {vk}",
                view_key);
            return;
        }

        if (geometry_tiles_buffer_parameter.IsBound() &&
            !user_data->geometry_tiles_buffer_srv.IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Geometry tiles buffer parameter is bound but the buffer SRV is invalid for view key {vk}",
                view_key);
            return;
        }

        const auto terrain_uniform_buffer_parameter = shader->GetUniformBufferParameter<Terrain>();
        if (terrain_uniform_buffer_parameter.IsBound() && !user_data->terrain_uniform_buffer.
            IsValid()) {
            UE_LOGFMT(
                LogTemp,
                Error,
                "Terrain uniform buffer parameter is bound but the uniform buffer is invalid for view key {vk}",
                view_key);
            return;
        }

        shader_bindings.Add(
            terrain_uniform_buffer_parameter,
            user_data->terrain_uniform_buffer
        );
        shader_bindings.Add(attachments_buffer_parameter, user_data->attachments_buffer_srv);
        shader_bindings.Add(terrain_sampler_parameter, user_data->terrain_sampler);
        shader_bindings.Add(height_attachment_parameter, user_data->height_attachment_texture);
        shader_bindings.Add(albedo_attachment_parameter, user_data->albedo_attachment_texture);
        shader_bindings.Add(terrain_view_buffer_parameter, user_data->terrain_view_buffer_srv);
        shader_bindings.Add(
            approximate_height_buffer_parameter,
            user_data->approximate_height_buffer_srv
        );
        shader_bindings.Add(tile_tree_buffer_parameter, user_data->tile_tree_buffer_srv);
        shader_bindings.Add(geometry_tiles_buffer_parameter, user_data->geometry_tiles_buffer_srv);

        if (terrain::vertex_factory::detail::should_probe_gpu_mesh()) {
            static bool logged_successful_gpu_bind = false;
            if (!logged_successful_gpu_bind) {
                logged_successful_gpu_bind = true;
                UE_LOGFMT(
                    LogTemp,
                    Display,
                    "[UDLOD.MeshProbe] Bound terrain VF shader parameters for view={0}.",
                    view_key
                );
            }
        }
    }

private:
    LAYOUT_FIELD(FShaderResourceParameter, attachments_buffer_parameter);
    LAYOUT_FIELD(FShaderResourceParameter, terrain_sampler_parameter);
    LAYOUT_FIELD(FShaderResourceParameter, height_attachment_parameter);
    LAYOUT_FIELD(FShaderResourceParameter, albedo_attachment_parameter);
    LAYOUT_FIELD(FShaderResourceParameter, terrain_view_buffer_parameter);
    LAYOUT_FIELD(FShaderResourceParameter, approximate_height_buffer_parameter);
    LAYOUT_FIELD(FShaderResourceParameter, tile_tree_buffer_parameter);
    LAYOUT_FIELD(FShaderResourceParameter, geometry_tiles_buffer_parameter);
};

IMPLEMENT_TYPE_LAYOUT(FTerrainMeshVertexFactoryShaderParameters);
IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(
    FTerrainMeshVertexFactory,
    SF_Vertex,
    FTerrainMeshVertexFactoryShaderParameters
);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(
    FTerrainMeshVertexFactory,
    SF_Pixel,
    FTerrainMeshVertexFactoryShaderParameters
);

IMPLEMENT_VERTEX_FACTORY_TYPE(
    FTerrainMeshVertexFactory,
    "/Plugins/UDLODTerrain/terrain_mesh_vertex_factory.ush",
    EVertexFactoryFlags::UsedWithMaterials |
    EVertexFactoryFlags::SupportsDynamicLighting
    // | EVertexFactoryFlags::SupportsPSOPrecaching
);

FTerrainMeshVertexFactory::FTerrainMeshVertexFactory(
    const ERHIFeatureLevel::Type in_feature_level) : FVertexFactory(in_feature_level) {}

void FTerrainMeshVertexFactory::InitRHI(FRHICommandListBase& RHICmdList) {
    // This draw path still needs a valid graphics vertex declaration, but it does not consume
    // any streamed vertex attributes because geometry is reconstructed from SV_VertexID.
    FVertexDeclarationElementList elements;
    InitDeclaration(elements);
}

void FTerrainMeshVertexFactory::ReleaseRHI() { FVertexFactory::ReleaseRHI(); }

bool FTerrainMeshVertexFactory::ShouldCompilePermutation(
    const FVertexFactoryShaderPermutationParameters& parameters
) {
    return IsFeatureLevelSupported(parameters.Platform, ERHIFeatureLevel::SM5) &&
    (parameters.MaterialParameters.MaterialDomain == MD_Surface ||
        parameters.MaterialParameters.bIsDefaultMaterial);
}

void FTerrainMeshVertexFactory::ModifyCompilationEnvironment(
    const FVertexFactoryShaderPermutationParameters& parameters,
    FShaderCompilerEnvironment& out_environment
) {
    FVertexFactory::ModifyCompilationEnvironment(parameters, out_environment);
    out_environment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), 1);
    out_environment.SetDefine(TEXT("PREPASS"), 0);
    out_environment.SetDefine(TEXT("SAMPLE_GRAD"), 1);
    out_environment.SetDefine(TEXT("TILE_TREE_LOD"), 1);
    out_environment.SetDefine(TEXT("BLEND"), 1);
    out_environment.SetDefine(TEXT("MORPH"), 1);

    // Mutually exclusive view modes
    out_environment.SetDefine(TEXT("LIGHTING"), 0);
    out_environment.SetDefine(TEXT("SHOW_DATA_LOD"), 0);
    out_environment.SetDefine(TEXT("SHOW_GEOMETRY_LOD"), 0);
    out_environment.SetDefine(TEXT("SHOW_TILE_TREE"), 0);
    out_environment.SetDefine(TEXT("SHOW_PIXELS"), 0);
    out_environment.SetDefine(TEXT("SHOW_UV"), 0);
    out_environment.SetDefine(TEXT("SHOW_NORMALS"), 0);

    out_environment.SetDefine(TEXT("VF_SUPPORTS_PRIMITIVE_SCENE_DATA"), 0);
    out_environment.SetDefine(TEXT("UDLOD_TERRAIN_MESH_VERTEX_FACTORY"), 1);
}
