// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <Modules/ModuleManager.h>
// ReSharper disable once CppWrongIncludesOrder, reason: required for DECLARE_LOG_CATEGORY_EXTERN to funcion
#include <CoreMinimal.h>

#define RDG_EVENT_SCOPE_CONDITIONAL_NAME(GraphBuilder, Condition, EventName ) \
    RDG_EVENT_SCOPE_IMPL(\
        GraphBuilder, \
        Condition, \
        ERDGScopeFlags::None, \
        RHI_GPU_STAT_ARGS_NONE, \
        TEXT("RDGEvent"), \
        RDG_SCOPE_ARGS("%s", EventName)\
    )

DECLARE_LOG_CATEGORY_EXTERN(LogUDLODTerrain, Log, All);

class FUDLODTerrainModule final : public IModuleInterface {
public:
    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

};
