#pragma once
#include "CoreMinimal.h"
#include "terrain_parent_actor.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FTerrainDebugWindowRequested, ATerrainParentActor*);

inline static FTerrainDebugWindowRequested GTerrainDebugWindowRequested;
