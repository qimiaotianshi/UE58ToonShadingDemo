using UnrealBuildTool;
public class DaniyaLipSyncEditor : ModuleRules
{
    public DaniyaLipSyncEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "DaniyaLipSync", "AnimationModifiers", "MetaHumanPerformance", "MetaHumanCoreTech", "SpeechAnimationSolver" });
        PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AssetTools", "AssetRegistry", "AnimationBlueprintLibrary", "MetaHumanPipelineCore", "MetaHumanSpeech2Face", "Slate", "SlateCore", "ToolMenus", "PropertyEditor", "BlueprintGraph", "KismetCompiler", "Blutility", "UMG" });
    }
}
