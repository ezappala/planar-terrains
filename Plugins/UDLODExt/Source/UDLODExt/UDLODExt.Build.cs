// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UDLODExt : ModuleRules {
    public UDLODExt(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp23;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        IWYUSupport = IWYUSupport.Full;
        // bWarningsAsErrors = true;
        // bUseUnity = false;
        // StaticAnalyzerAdditionalCheckers.Add("core");
        // StaticAnalyzerAdditionalCheckers.Add("cplusplus");
        // StaticAnalyzerAdditionalCheckers.Add("deadcode");
        // StaticAnalyzerAdditionalCheckers.Add("nullability");
        // StaticAnalyzerAdditionalCheckers.Add("optin.core");
        // StaticAnalyzerAdditionalCheckers.Add("optin.cplusplus");
        // StaticAnalyzerAdditionalCheckers.Add("optin.performance");
        // StaticAnalyzerAdditionalCheckers.Add("optin.taint");
        // StaticAnalyzerAdditionalCheckers.Add("security");
        // StaticAnalyzerAdditionalCheckers.Add("alpha.clone");
        // StaticAnalyzerAdditionalCheckers.Add("alpha.core");
        // StaticAnalyzerAdditionalCheckers.Add("alpha.cplusplus");
        // StaticAnalyzerAdditionalCheckers.Add("alpha.deadcode");
        // StaticAnalyzerAdditionalCheckers.Add("alpha.llvm");
        // StaticAnalyzerAdditionalCheckers.Add("alpha.security");

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
                "GDAL",
                "UnrealGDAL",
                "Imath",
                "Json",
                // ... add other public dependencies that you statically link with here ...
            }
        );


        PrivateDependencyModuleNames.AddRange(
            new[] {
                "Projects", "RiderLink", "RiderSourceCodeAccess",
                "RiderShaderInfo", "RiderBlueprint", "RiderDebuggerSupport",
                "RiderGameControl", "RiderLogging", "RiderLC",
                // ... add private dependencies that you statically link with here ...
            }
        );


        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                // ... add any modules that your module loads dynamically here ...
            }
        );
    }
}