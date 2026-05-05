// Fill out your copyright notice in the Description page of Project Settings.

using System.Collections.Generic;
using UnrealBuildTool;
using System.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

public class PlanarTerrainsTarget : TargetRules {
    public PlanarTerrainsTarget(TargetInfo Target) : base(Target) {
        Type = TargetType.Game;
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

        ExtraModuleNames.AddRange(new[] { "PlanarTerrains", "UDLODExt",
            "UDLODPreprocessor", "UDLODTerrain" });
    }
}