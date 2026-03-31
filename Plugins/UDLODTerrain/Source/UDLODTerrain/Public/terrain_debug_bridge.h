#pragma once

#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"

class ATerrainParentActor;
DECLARE_MULTICAST_DELEGATE_OneParam(FTerrainDebugWindowRequested, ATerrainParentActor*)

inline FTerrainDebugWindowRequested TERRAIN_DEBUG_WINDOW_REQUESTED;
