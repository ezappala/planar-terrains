// Fill out your copyright notice in the Description page of Project Settings.

using System.Collections.Generic;
using UnrealBuildTool;

public class PlanarTerrains : ModuleRules {
    public PlanarTerrains(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp23;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        IWYUSupport = IWYUSupport.Full;
        CppCompileWarningSettings = new CppCompileWarnings(this, Logger);
        bWarningsAsErrors = true;
        bUseUnity = false;

        PublicIncludePaths.AddRange(new[]
            { "PlanarTerrains/Public" });
        PrivateIncludePaths.AddRange(new[]
            { "PlanarTerrains/Private" });

        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "UDLODTerrain",
            "GDAL", "UnrealGDAL", "RenderCore", "Renderer", "RHI", "RHICore",
            "DynamicMesh", "MeshUtilities", "CustomMeshComponent",
            "MaterialUtilities", "Landscape", "ShaderCompilerCommon",
            "ShaderPreprocessor", "Imath", "Json",
            "UDLODPreprocessor", "UDLODExt", "DebugViewer"
        });

        PrivateDependencyModuleNames.AddRange(new[] {
            "Projects", "RiderLink", "RiderSourceCodeAccess",
            "RiderShaderInfo", "RiderBlueprint", "RiderDebuggerSupport",
            "RiderGameControl", "RiderLogging", "RiderLC",
        });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}