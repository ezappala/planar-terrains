// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UDLODTerrain : ModuleRules {
    public UDLODTerrain(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp23;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        IWYUSupport = IWYUSupport.Full;

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
                "CoreUObject",
                "Engine",
                "RenderCore",
                "Renderer",
                "RHI",
                "RHICore",
                "DynamicMesh",
                "MeshUtilities",
                "CustomMeshComponent",
                "MaterialUtilities",
                "Landscape",
                "ShaderCompilerCommon",
                "ShaderPreprocessor",
                "ExtendedGraphicsProgramming",
                "GDAL",
                "UnrealGDAL",
                "Imath",
                "Json",
                "UDLODPreprocessor",
                "UDLODExt",
                // ... add other public dependencies that you statically link with here ...
            }
        );


        PrivateDependencyModuleNames.AddRange(
            new[] {
                "Projects", "RiderLink", "RiderSourceCodeAccess",
                "RiderShaderInfo", "RiderBlueprint", "RiderDebuggerSupport",
                "RiderGameControl", "RiderLogging", "RiderLC",
                "GPURuntimeTessellation",
            }
        );


        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                // ... add any modules that your module loads dynamically here ...
            }
        );

        // if (Target.Platform == UnrealTargetPlatform.Win64) {
        //     PublicAdditionalLibraries.Add(
        //         "$(PluginDir)/Binaries/ThirdParty/BevyTerrain/Win64/btpp.lib");
        //     PublicDelayLoadDLLs.Add("btpp.dll");
        //     RuntimeDependencies.Add(
        //         "$(PluginDir)/Binaries/ThirdParty/BevyTerrain/Win64/btpp.dll");
        // }
    }
}