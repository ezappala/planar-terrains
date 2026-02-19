#pragma once
#include <Kismet/BlueprintFunctionLibrary.h>
#include <UObject/ObjectMacros.h>

#include "terrain_utils.generated.h"

UCLASS(BlueprintType)
class UDLODTERRAIN_API UTerrainUtilities : public UBlueprintFunctionLibrary {
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintPure)
    static void GetLandscapeComponents(class ALandscape* landscape, TArray<class ULandscapeComponent*>& output);
};
