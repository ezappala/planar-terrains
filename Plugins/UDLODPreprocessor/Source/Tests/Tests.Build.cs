using UnrealBuildTool;

public class Tests : ModuleRules
{
    public Tests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp23;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        IWYUSupport = IWYUSupport.Full;

        PublicDependencyModuleNames.AddRange(new[]
        {
            "Core",
        });

        PrivateDependencyModuleNames.AddRange(new[]
        {
            "CoreUObject",
            "Engine",
            "Json",
            "GDAL",
            "UnrealGDAL",
            "UDLODExt",
            "UDLODTerrain",
            "UDLODPreprocessor",
        });
    }
}
