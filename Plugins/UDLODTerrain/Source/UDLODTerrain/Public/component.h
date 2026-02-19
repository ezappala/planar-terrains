// #pragma once
// #include "EGP_CustomRenderPasses.h"
//
// class UDLODTERRAIN_API FTerrainPassComponent final : U_EGP_RenderPassComponent {
//
// };

#pragma once
#include <Materials/MaterialExpressionCustomOutput.h>

#include "EGP_CustomRenderPasses.h"
#include "component.generated.h"

UENUM(BlueprintType)
enum class ETerrainMeshBlendModes : uint8 {
    Alpha,
    Additive,
    Multiply,

    COUNT UMETA(Hidden)
};

ENUM_RANGE_BY_COUNT(ETerrainMeshBlendModes, static_cast<int>(ETerrainMeshBlendModes::COUNT));

//A POD struct containing all render parameters for one component
//    in our Game of Life effect.
USTRUCT(BlueprintType)
struct UDLODTERRAIN_API FTerrainPrimitiveRenderSettings {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    ETerrainMeshBlendModes blend_mode = ETerrainMeshBlendModes::Alpha;

    bool operator==(const FTerrainPrimitiveRenderSettings& r2) const { return blend_mode == r2.blend_mode; }
};
inline uint32 GetTypeHash(const FTerrainPrimitiveRenderSettings& r) { return GetTypeHash(MakeTuple(r.blend_mode)); }

//Marks a primitive-component (mesh, particle system, etc.) so that it renders into the GoL sim.
UCLASS(meta=(BlueprintSpawnableComponent))
class UDLODTERRAIN_API UTerrainComponent : public U_EGP_RenderPassComponent {
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere, meta=(ShowOnlyInnerProperties))
    FTerrainPrimitiveRenderSettings render_settings;

    virtual TSubclassOf<U_EGP_RenderPass> GetPassType() const override;
    EGP_PASS_COMPONENT_SIMPLE_PROXY_IMPL(FTerrainPrimitiveRenderSettings, render_settings)
};