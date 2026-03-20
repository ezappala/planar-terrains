// Copyright Epic Games, Inc. All Rights Reserved.

#include "UDLODTerrain.h"

#include <ShaderCore.h>
#include <Interfaces/IPluginManager.h>
#include <Misc/Paths.h>

#include "UnrealGDAL.h"

#define LOCTEXT_NAMESPACE "FUDLODTerrainModule"
DEFINE_LOG_CATEGORY(LogUDLODTerrain);

void FUDLODTerrainModule::StartupModule() {
    const auto gdal_plugin = IPluginManager::Get().FindPlugin("UnrealGDAL");
    check(gdal_plugin.IsValid());

    auto* UnrealGDAL = FModuleManager::Get().LoadModulePtr<FUnrealGDALModule>("UnrealGDAL");
    UnrealGDAL->InitGDAL();

    const auto plugin = IPluginManager::Get().FindPlugin("UDLODTerrain");
    check(plugin.IsValid());

    auto plugin_dir = plugin->GetBaseDir();
    const FString shader_dir = FPaths::Combine(plugin_dir, TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/Plugins/UDLODTerrain"), shader_dir);
}

void FUDLODTerrainModule::ShutdownModule() {
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUDLODTerrainModule, UDLODTerrain)
