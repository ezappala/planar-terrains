#include "terrain_utils.h"

#include <LandscapeComponent.h>
#include <UObject/UObjectIterator.h>

void UTerrainUtilities::GetLandscapeComponents(ALandscape* landscape, TArray<ULandscapeComponent*>& output) {
    output.Empty();
    for (TObjectIterator<ULandscapeComponent> it; it; ++it) {
        output.Add(*it);
    }
}
