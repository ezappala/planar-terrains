// Copyright Epic Games, Inc. All Rights Reserved.

#include "UDLODExt.h"

#include <Interfaces/IPluginManager.h>
#include <Misc/Paths.h>

#include "UnrealGDAL.h"

#define LOCTEXT_NAMESPACE "FUDLODExtModule"

void FUDLODExtModule::StartupModule() {
    const auto gdal_plugin = IPluginManager::Get().FindPlugin("UnrealGDAL");
    check(gdal_plugin.IsValid());

    auto* UnrealGDAL = FModuleManager::Get().LoadModulePtr<FUnrealGDALModule>("UnrealGDAL");
    UnrealGDAL->InitGDAL();

    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FUDLODExtModule::ShutdownModule() {
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUDLODExtModule, UDLODExt)
