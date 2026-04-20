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
        StaticAnalyzer = StaticAnalyzer.Clang;
        StaticAnalyzerOutputType = StaticAnalyzerOutputType.Text;
        bStaticAnalyzerProjectOnly = true;
        bStaticAnalyzerIncludeGenerated = false;
        StaticAnalyzerMode = StaticAnalyzerMode.Deep;
        var checkers = new HashSet<string> {
                "core",
                "cplusplus",
                "deadcode",
                "nullability",
                "optin.core",
                "optin.cplusplus",
                "optin.performance",
                "optin.taint",
                "security",
                "alpha.clone",
                "alpha.core",
                "alpha.cplusplus",
                "alpha.deadcode",
                "alpha.llvm",
                "alpha.security",
            }
            .Select(selector: static s => "-StaticAnalyzerChecker=" + s)
            .ToArray();

        bUseUnityBuild = false;

        if (Target != null) {
            if (Target.Arguments != null) Target.Arguments.Append(checkers);
            else Target.Arguments = new CommandLineArguments(checkers);
        } else Logger.LogError("Target did not exist!");

        ExtraModuleNames.AddRange(new[]
        {
            "PlanarTerrains", "UDLODExt", "UDLODPreprocessor", "UDLODTerrain"
        });
    }
}