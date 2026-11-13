using UnrealBuildTool;
using System.IO;
public class DreamBloom : ModuleRules {
 // Runtime AI speech uses HTTP + in-memory PCM and the StreamingADA CPU solver.
 public DreamBloom(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "LevelSequence", "MovieScene", "Projects", "InputCore", "MoviePlayer", "RenderCore", "AnimGraphRuntime", "KawaiiPhysics", "GameplayTags", "CinematicCamera", "EnhancedInput", "SpeechAnimationSolver"});
  RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "..."), StagedFileType.NonUFS);
  PrivateDependencyModuleNames.AddRange(new string[]{"AudioMixer","AudioExtensions","AnimationCore","PhysicsCore","HTTP","Json","JsonUtilities","NNE","MetaHumanCoreTech","DaniyaLipSync"});
  PrivateDependencyModuleNames.AddRange(new string[]{"MovieSceneTracks","MediaAssets","ProceduralMeshComponent"});
  if(Target.bBuildEditor) PublicDependencyModuleNames.AddRange(new string[]{"UnrealEd","AnimGraph","BlueprintGraph","KismetCompiler","KawaiiPhysicsEd"});
 }
}
