#include "WardrobeAISpeechPaths.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace WardrobeAISpeech {
FString ResolveTTSRoot(const FString& ConfiguredRoot,const FString& ProjectDirectory,FString& Error){
 Error.Reset();FString Root=ConfiguredRoot.TrimStartAndEnd();
 if(Root.IsEmpty())Root=TEXT("Content/AI");
 const FString ProjectRoot=FPaths::ConvertRelativePathToFull(ProjectDirectory);
 Root=FPaths::IsRelative(Root)?FPaths::ConvertRelativePathToFull(ProjectRoot,Root):FPaths::ConvertRelativePathToFull(Root);
 FPaths::NormalizeDirectoryName(Root);FPaths::CollapseRelativeDirectories(Root);
 auto IsBundle=[](const FString& P){return FPaths::FileExists(FPaths::Combine(P,TEXT("runtime/python.exe")))&&FPaths::FileExists(FPaths::Combine(P,TEXT("api_v2.py")));};
 if(IsBundle(Root))return Root;
 // Bounded discovery avoids scanning Python's site-packages or model directories on the game thread.
 TArray<FString> Candidates,Level;Level.Add(Root);
 for(int32 Depth=0;Depth<2;++Depth){
  TArray<FString> Next;
  for(const auto& Parent:Level){TArray<FString> Children;IFileManager::Get().FindFiles(Children,*FPaths::Combine(Parent,TEXT("*")),false,true);Children.Sort();
   for(const auto& Child:Children){const FString Path=FPaths::Combine(Parent,Child);if(IsBundle(Path))Candidates.AddUnique(Path);else Next.Add(Path);}
  }
  Level=MoveTemp(Next);
 }
 if(Candidates.Num()==1)return Candidates[0];
 Error=Candidates.Num()>1?TEXT("Multiple TTS bundles found. Set TTSRoot to one bundle relative to the project."):TEXT("No TTS bundle found. Stage the loose Content/AI directory with runtime/python.exe and api_v2.py.");
 return {};
}
}
