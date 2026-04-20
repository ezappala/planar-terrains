using UnrealBuildTool;

public class UDLODExtTests : ModuleRules
{
    public UDLODExtTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp23;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        IWYUSupport = IWYUSupport.Full;
        CppCompileWarningSettings = new CppCompileWarnings(this, Logger);
        bWarningsAsErrors = true;
        bUseUnity = false;

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
