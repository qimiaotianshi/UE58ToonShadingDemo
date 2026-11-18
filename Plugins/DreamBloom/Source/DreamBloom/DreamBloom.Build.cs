using UnrealBuildTool;
using System.IO;
public class DreamBloom : ModuleRules {
 // Runtime AI speech uses HTTP + in-memory PCM and the StreamingADA CPU solver.
 public DreamBloom(ReadOnlyTargetRules Target) : base(Target) {
  PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
  PublicDependencyModuleNames.AddRange(new string[] {"Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "LevelSequence", "MovieScene", "Projects", "InputCore", "MoviePlayer", "RenderCore", "AnimGraphRuntime", "KawaiiPhysics", "GameplayTags", "CinematicCamera", "EnhancedInput", "SpeechAnimationSolver"});
  RuntimeDependencies.Add(Path.Combine(PluginDirectory, "Resources", "..."), StagedFileType.NonUFS);
  // Portable companion runtime; omit player databases, credentials, logs and download caches.
  var AIRoot = Path.GetFullPath(Path.Combine(PluginDirectory, "..", "AI"));
  if (File.Exists(Path.Combine(AIRoot, "StartAstrBot.py"))) {
   RuntimeDependencies.Add(Path.Combine(AIRoot, "StartAstrBot.py"), StagedFileType.NonUFS);
   foreach (var Part in new[] {"Defaults", "Runtime/Env/Lib", "AstrBot/astrbot", "AstrBot/data/plugins"})
    if (Directory.Exists(Path.Combine(AIRoot, Part))) RuntimeDependencies.Add(Path.Combine(AIRoot, Part, "..."), StagedFileType.NonUFS);
   RuntimeDependencies.Add(Path.Combine(AIRoot, "AstrBot", "main.py"), StagedFileType.NonUFS);
   foreach (var FileName in new[] {"runtime_bootstrap.py", "LICENSE", "EULA.md", "pyproject.toml"})
    RuntimeDependencies.Add(Path.Combine(AIRoot, "AstrBot", FileName), StagedFileType.NonUFS);
   var PythonRoot = Path.Combine(AIRoot, "Runtime", "Python");
   if (Directory.Exists(PythonRoot)) foreach (var Runtime in Directory.GetDirectories(PythonRoot))
    if ((File.GetAttributes(Runtime) & FileAttributes.ReparsePoint) == 0 && File.Exists(Path.Combine(Runtime, "python.exe")))
     RuntimeDependencies.Add(Path.Combine(Runtime, "..."), StagedFileType.NonUFS);
  }
  PrivateDependencyModuleNames.AddRange(new string[]{"AudioMixer","AudioExtensions","AnimationCore","PhysicsCore","HTTP","Json","JsonUtilities","NNE","MetaHumanCoreTech","DaniyaLipSync"});
  PrivateDependencyModuleNames.AddRange(new string[]{"MovieSceneTracks","MediaAssets","ProceduralMeshComponent"});
  if(Target.bBuildEditor) PublicDependencyModuleNames.AddRange(new string[]{"UnrealEd","AnimGraph","BlueprintGraph","KismetCompiler","KawaiiPhysicsEd"});
 }
}
