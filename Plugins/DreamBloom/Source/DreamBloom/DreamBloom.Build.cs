using UnrealBuildTool;
using System.IO;
public class DreamBloom : ModuleRules {
 public DreamBloom(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "LevelSequence", "MovieScene", "Projects", "InputCore", "MoviePlayer", "RenderCore", "AnimGraphRuntime", "KawaiiPhysics", "GameplayTags", "CinematicCamera", "EnhancedInput"});
  RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "..."), StagedFileType.NonUFS);
  PrivateDependencyModuleNames.AddRange(new string[]{"AudioMixer","AnimationCore"});
  PrivateDependencyModuleNames.Add("MovieSceneTracks");
  if(Target.bBuildEditor) PublicDependencyModuleNames.AddRange(new string[]{"UnrealEd","AnimGraph","BlueprintGraph","KismetCompiler","KawaiiPhysicsEd"});
 }
}
