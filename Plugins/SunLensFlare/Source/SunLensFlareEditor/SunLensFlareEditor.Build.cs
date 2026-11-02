using UnrealBuildTool;
public class SunLensFlareEditor : ModuleRules
{
    public SunLensFlareEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "MaterialEditor", "SunLensFlare", "RHI" });
    }
}
