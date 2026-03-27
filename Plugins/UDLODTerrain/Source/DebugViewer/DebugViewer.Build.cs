using UnrealBuildTool;

public class DebugViewer : ModuleRules {
    public DebugViewer(ReadOnlyTargetRules Target) : base(Target) {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "UDLODTerrain",
                "UDLODPreprocessor"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Slate",
                "SlateCore",
                "InputCore",
                "ApplicationCore",
                "AppFramework",
                "EditorFramework",
                "UnrealEd",
                "Projects",
                "EditorStyle",
            }
        );
    }
}