// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class PlanarTerrains : ModuleRules {
    public PlanarTerrains(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        PublicDependencyModuleNames.AddRange(new[]
            { "Core", "CoreUObject", "Engine", "InputCore", "UDLODTerrain" });

        PrivateDependencyModuleNames.AddRange(new[]
            { "RiderLink", "RiderSourceCodeAccess", "GPURuntimeTessellation", });

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}