#pragma once
#include <Materials/MaterialExpressionCustomOutput.h>
#include <UObject/ObjectMacros.h>

#include "mat.generated.h"

UCLASS(CollapseCategories, HideCategories=Object, DisplayName="Terrain Outputs: Init")
class UDLODTERRAIN_API UMaterialExpressionTerrainInitOutputs : public UMaterialExpressionCustomOutput {
    GENERATED_BODY()

public:
    //Automatically snapped to 0 or 1.
    UPROPERTY(meta=(RequiredInput=true))
    FExpressionInput InitialBinaryState;

    //If not set, defaults to the binary state value (before it's snapped to 0 or 1).
    UPROPERTY(meta=(RequiredInput=false))
    FExpressionInput InitialContinuousState;

    virtual FString GetFunctionName() const override { return TEXT("Terrain_Outputs_Init_"); }
    virtual FString GetDisplayName() const override { return TEXT("Terrain Outputs: Init"); }

#if WITH_EDITOR
    virtual void GetCaption(TArray<FString>& output) const override {
        output.Add(TEXT("Terrain Outputs: Init pass"));
    }

    virtual int32 GetNumOutputs() const override { return 2; }
    [[deprecated]] virtual EShaderFrequency GetShaderFrequency() override { return SF_Pixel; }

    virtual EShaderFrequency GetShaderFrequency(uint32 OutputIndex) override {
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        // ReSharper disable once CppDeprecatedEntity
        return GetShaderFrequency();
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    virtual int32 Compile(FMaterialCompiler*, int32 pinIdx) override;
#endif
};

UCLASS(CollapseCategories, HideCategories=Object, DisplayName="Terrain Outputs: Simulate (pt 1)")
class UDLODTERRAIN_API UMaterialExpressionTerrain1Outputs : public UMaterialExpressionCustomOutput {
    GENERATED_BODY()

public:
    UPROPERTY(meta=(RequiredInput=false), DisplayName="Threshold: Underpopulated")
    FExpressionInput ThresholdTooFew;

    UPROPERTY(meta=(OverridingInputProperty=ThresholdTooFew))
    float ThresholdTooFewConst = 2.0f;

    UPROPERTY(meta=(RequiredInput=false), DisplayName="Threshold: Ideal")
    FExpressionInput ThresholdResurrect;

    UPROPERTY(meta=(OverridingInputProperty=ThresholdResurrect))
    float ThresholdResurrectConst = 2.5f;

    UPROPERTY(meta=(RequiredInput=false), DisplayName="Threshold: Overpopulation")
    FExpressionInput ThresholdTooMany;

    UPROPERTY(meta=(OverridingInputProperty=ThresholdTooMany))
    float ThresholdTooManyConst = 3.0f;

    virtual FString GetFunctionName() const override { return TEXT("Terrain_Outputs_Simulate_Pt1_"); }
    virtual FString GetDisplayName() const override { return TEXT("Terrain Outputs: Simulate (part 1)"); }

#if WITH_EDITOR
    virtual void GetCaption(TArray<FString>& output) const override {
        output.Add(TEXT("Terrain Outputs: Simulate pass (part 1)"));
    }

    virtual int32 GetNumOutputs() const override { return 3; }
    [[deprecated]] virtual EShaderFrequency GetShaderFrequency() override { return SF_Compute; }

    virtual EShaderFrequency GetShaderFrequency(uint32 OutputIndex) override {
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        // ReSharper disable once CppDeprecatedEntity
        return GetShaderFrequency();
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    virtual int32 Compile(FMaterialCompiler*, int32 pinIdx) override;
#endif
};

UCLASS(CollapseCategories, HideCategories=Object, DisplayName="Terrain Outputs: Simulate (pt 2)")
class UDLODTERRAIN_API UMaterialExpressionTerrain2Outputs : public UMaterialExpressionCustomOutput {
    GENERATED_BODY()

public:
    //If not provided, the continuous state is set to match the discrete one.
    UPROPERTY(meta=(RequiredInput=false))
    FExpressionInput ContinuousValue;

    virtual FString GetFunctionName() const override { return TEXT("Terrain_Outputs_Simulate_Pt2_"); }
    virtual FString GetDisplayName() const override { return TEXT("Terrain Outputs: Simulate (part 2)"); }

#if WITH_EDITOR
    virtual void GetCaption(TArray<FString>& output) const override {
        output.Add(TEXT("Terrain Outputs: Simulate pass (part 2)"));
    }

    virtual int32 GetNumOutputs() const override { return 1; }
    [[deprecated]] virtual EShaderFrequency GetShaderFrequency() override { return SF_Compute; }

    virtual EShaderFrequency GetShaderFrequency(uint32 OutputIndex) override {
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        // ReSharper disable once CppDeprecatedEntity
        return GetShaderFrequency();
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    virtual int32 Compile(FMaterialCompiler*, int32 pinIdx) override;
#endif
};

UCLASS(CollapseCategories, HideCategories=Object, DisplayName="Terrain Outputs: Mesh")
class UDLODTERRAIN_API UMaterialExpressionTerrainMeshOutputs : public UMaterialExpressionCustomOutput {
    GENERATED_BODY()

public:
    UPROPERTY(meta=(RequiredInput=false))
    FExpressionInput DiscreteOutput;
    UPROPERTY(meta=(RequiredInput=false))
    FExpressionInput ContinuousOutput;
    UPROPERTY(meta=(RequiredInput=false))
    FExpressionInput OutputAlpha;

    UPROPERTY(EditAnywhere, meta=(OverridingInputProperty=DiscreteOutput))
    float DiscreteOutputConst = 0.5f;

    UPROPERTY(EditAnywhere, meta=(OverridingInputProperty=ContinuousOutput))
    float ContinuousOutputConst = 0.5f;

    UPROPERTY(EditAnywhere, meta=(OverridingInputProperty=OutputAlpha))
    float OutputAlphaConst = 1.0f;

    virtual FString GetFunctionName() const override { return TEXT("Terrain_Outputs_Mesh_"); }
    virtual FString GetDisplayName() const override { return TEXT("Terrain Outputs: Mesh (pixel shader)"); }

#if WITH_EDITOR
    virtual void GetCaption(TArray<FString>& output) const override {
        output.Add(TEXT("Terrain Outputs: Mesh pass (pixel shader)"));
    }

    virtual int32 GetNumOutputs() const override { return 3; }
    [[deprecated]] virtual EShaderFrequency GetShaderFrequency() override { return SF_Pixel; }

    virtual EShaderFrequency GetShaderFrequency(uint32 OutputIndex) override {
        PRAGMA_DISABLE_DEPRECATION_WARNINGS
        // ReSharper disable once CppDeprecatedEntity
        return GetShaderFrequency();
        PRAGMA_ENABLE_DEPRECATION_WARNINGS
    }

    virtual int32 Compile(FMaterialCompiler*, int32 pinIdx) override;
#endif
};
