// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class PlanarTerrainsEditorTarget : TargetRules {
    public PlanarTerrainsEditorTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        CppStandard = CppStandardVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        ExtraModuleNames.AddRange(new[] { "PlanarTerrains" });
    }
}