// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class PlanarTerrainsEditorTarget : TargetRules {
    public PlanarTerrainsEditorTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        CppStandard = CppStandardVersion.Cpp23;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        bEnforceIWYU = true;

        ExtraModuleNames.AddRange(new[]
        {
            "PlanarTerrains", "UDLODExt", "UDLODPreprocessor", "UDLODTerrain"
        });
    }
}