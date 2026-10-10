using UnrealBuildTool;
public class SequenceCharacterStudio:ModuleRules {
 public SequenceCharacterStudio(ReadOnlyTargetRules Target):base(Target){PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","ControlRig","RigVM","MovieScene","MovieSceneTracks"});}
}
