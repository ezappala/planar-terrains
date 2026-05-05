using UnrealBuildTool;

public class UDLODExtTests : ModuleRules
{
    public UDLODExtTests(ReadOnlyTargetRules Target) : base(Target)
    {
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

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "CoreUObject",
            "Engine",
            "GDAL",
            "GeometryCore",
            "Imath",
            "Json",
            "Projects",
            "RenderCore",
            "RHI",
            "UDLODExt",
            "UnrealGDAL",
        });
    }
}
