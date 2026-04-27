// Fill out your copyright notice in the Description page of Project Settings.

using System.Collections.Generic;
using System.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

public class PlanarTerrainsEditorTarget : TargetRules {
    public PlanarTerrainsEditorTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        CppStandard = CppStandardVersion.Cpp23;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        bEnforceIWYU = true;
        // StaticAnalyzer = StaticAnalyzer.Clang;
        // StaticAnalyzerOutputType = StaticAnalyzerOutputType.Text;
        // bStaticAnalyzerProjectOnly = true;
        // bStaticAnalyzerIncludeGenerated = false;
        // StaticAnalyzerMode = StaticAnalyzerMode.Deep;
        // bUseUnityBuild = false;

        ExtraModuleNames.AddRange(new[] {
            "PlanarTerrains", "UDLODExt", "UDLODPreprocessor", "UDLODTerrain"
        });
    }
}