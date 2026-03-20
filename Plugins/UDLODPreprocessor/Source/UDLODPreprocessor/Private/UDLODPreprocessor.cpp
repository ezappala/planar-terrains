// Copyright Epic Games, Inc. All Rights Reserved.

#include "UDLODPreprocessor.h"

#include <Interfaces/IPluginManager.h>
#include <Misc/Paths.h>

#include "UnrealGDAL.h"

#define LOCTEXT_NAMESPACE "FUDLODPreprocessorModule"

void FUDLODPreprocessorModule::StartupModule() {
    const auto gdal_plugin = IPluginManager::Get().FindPlugin("UnrealGDAL");
    check(gdal_plugin.IsValid());

    auto* UnrealGDAL = FModuleManager::Get().LoadModulePtr<FUnrealGDALModule>("UnrealGDAL");
    UnrealGDAL->InitGDAL();
    // This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FUDLODPreprocessorModule::ShutdownModule() {
    // This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
    // we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUDLODPreprocessorModule, UDLODPreprocessor)
