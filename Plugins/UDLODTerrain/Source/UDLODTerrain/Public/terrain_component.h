#pragma once

#include "Components.h"
#include "dispatcher.h"
#include "Components/PrimitiveComponent.h"

#include "Components/SceneComponent.h"
#include "terrain_component.generated.h"

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UDLODTERRAIN_API UTerrainComponent : public UPrimitiveComponent {
    GENERATED_BODY()

public:
    UTerrainComponent();

    // virtual void OnComponentCreated() override;
    // virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(CallInEditor)
    void PreprocessHeightmap();

    TUniquePtr<FCSDispatcher> dispatcher;

    /** @brief Texture to sample from. */
    UPROPERTY(EditAnywhere, Category="UDLOD|Heightmap",
        meta=(ToolTip=
            "Texture to sample height from. Preferred format is RF32, other floating point types like RF16 may work"))
    TObjectPtr<UTexture2D> heightmap = nullptr;
};