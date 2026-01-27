// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UDLODTerrain : ModuleRules {
    public UDLODTerrain(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

        PublicIncludePaths.AddRange(
            new string[] {
                // ... add public include paths required here ...
            }
        );


        PrivateIncludePaths.AddRange(
            new string[] {
                // ... add other private include paths required here ...
            }
        );


        PublicDependencyModuleNames.AddRange(
            new[] {
                "Core",
                "RenderCore",
                "Renderer",
                "RHI",
                "RHICore",
                "ShaderCompilerCommon",
                "ShaderPreprocessor",
                // ... add other public dependencies that you statically link with here ...
            }
        );


        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "CoreUObject",
                "Engine",
                "Projects",
                "RiderLink",
                "RiderSourceCodeAccess",
                "RiderShaderInfo",
            }
        );


        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                // ... add any modules that your module loads dynamically here ...
            }
        );
    }
}