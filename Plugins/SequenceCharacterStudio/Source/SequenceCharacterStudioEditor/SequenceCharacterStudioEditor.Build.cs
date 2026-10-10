using UnrealBuildTool;
public class SequenceCharacterStudioEditor:ModuleRules {
 public SequenceCharacterStudioEditor(ReadOnlyTargetRules Target):base(Target){PCHUsage=PCHUsageMode.UseExplicitOrSharedPCHs;PublicDependencyModuleNames.AddRange(new string[]{"Core","CoreUObject","Engine","SequenceCharacterStudio","LevelSequence","MovieScene","ControlRig"});PrivateDependencyModuleNames.AddRange(new string[]{"UnrealEd","Slate","SlateCore","ToolMenus","ControlRigEditor","LevelSequenceEditor","MovieSceneTracks","Sequencer","RigVM","AssetRegistry","PropertyEditor"});}
}
