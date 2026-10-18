using UnrealBuildTool;
public class DreamBloomEditor : ModuleRules {
 public DreamBloomEditor(ReadOnlyTargetRules Target):base(Target){PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","DreamBloom","AnimGraph","BlueprintGraph","UnrealEd","AnimGraphRuntime"});PrivateDependencyModuleNames.AddRange(new string[]{"KismetCompiler","KawaiiPhysicsEd","KawaiiPhysics","AssetRegistry","EnhancedInput","InputCore"});}
}
