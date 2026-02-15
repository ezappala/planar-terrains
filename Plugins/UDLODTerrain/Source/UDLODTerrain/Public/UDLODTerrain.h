// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
// ReSharper disable once CppWrongIncludesOrder, reason: needed for defines
#include "config.h"
// ReSharper disable once CppWrongIncludesOrder, reason: required for DECLARE_LOG_CATEGORY_EXTERN to funcion
#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogUDLODTerrain, Log, All);

class FUDLODTerrainModule final : public IModuleInterface {
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
