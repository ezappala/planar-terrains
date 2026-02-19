// Copyright Epic Games, Inc. All Rights Reserved.

#include "UDLODTerrain.h"

#include <ShaderCore.h>
#include <Interfaces/IPluginManager.h>
#include <Misc/Paths.h>

#define LOCTEXT_NAMESPACE "FUDLODTerrainModule"
DEFINE_LOG_CATEGORY(LogUDLODTerrain);

void FUDLODTerrainModule::StartupModule() {
    // This code will execute after your module is loaded into memory;
    // the exact timing is specified in the .uplugin file per-module
    const TSharedPtr<IPlugin> plugin = IPluginManager::Get().FindPlugin("UDLODTerrain");
    check(plugin.IsValid());

    const FString shader_dir = FPaths::Combine(plugin->GetBaseDir(), TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/Plugins/UDLODTerrain"), shader_dir);
}

void FUDLODTerrainModule::ShutdownModule() {
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUDLODTerrainModule, UDLODTerrain)
