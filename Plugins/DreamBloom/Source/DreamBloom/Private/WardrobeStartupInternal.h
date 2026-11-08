#pragma once
#include "CoreMinimal.h"
#include "Misc/Paths.h"
#include "Styling/SlateBrush.h"
#include "Fonts/CompositeFont.h"
#include <atomic>

namespace WardrobeStartup
{
inline FVector2D FitVideo(FVector2D Screen,float Aspect)
{
 if(Screen.X<=0||Screen.Y<=0)return FVector2D::ZeroVector;
 if(!FMath::IsFinite(Aspect)||Aspect<=0)Aspect=16.f/9.f;
 return Screen.X/Screen.Y>Aspect?FVector2D(Screen.Y*Aspect,Screen.Y):FVector2D(Screen.X,Screen.X/Aspect);
}
// Movies are loose staged files. Never retain a developer machine's drive in the runtime URL.
inline FString ResolveMovie(FString Path,const FString& ContentDir)
{
 Path.TrimStartAndEndInline();Path.ReplaceInline(TEXT("\\"),TEXT("/"));
 if(Path.StartsWith(TEXT("./")))Path.RightChopInline(2);
 if(Path.StartsWith(TEXT("Content/"),ESearchCase::IgnoreCase))Path.RightChopInline(8);
 FString Full=FPaths::IsRelative(Path)?FPaths::Combine(ContentDir,Path):Path;
 Full=FPaths::ConvertRelativePathToFull(Full);FPaths::NormalizeFilename(Full);FPaths::CollapseRelativeDirectories(Full);
 FString Movies=FPaths::ConvertRelativePathToFull(FPaths::Combine(ContentDir,TEXT("Movies/")));
 FPaths::NormalizeDirectoryName(Movies);
 if(!Full.StartsWith(Movies+TEXT("/"),ESearchCase::IgnoreCase)||FPaths::GetExtension(Full).ToLower()!=TEXT("mp4"))return FString();
 return Full;
}
}

// Only atomics/immutable Slate data are read on MoviePlayer's rendering/Slate thread.
struct FWardrobeStartupVisual
{
 std::atomic<bool> MovieDone{false},SceneReady{false},MovieFailed{false};
 std::atomic<float> Closing{0.f};
 double NativeStartedAt=0,Duration=0;
 float FadeTime=1.5f;
 bool FadeBaked=false;
 float Aspect=16.f/9.f;
 FSlateBrush MovieBrush;
 TSharedPtr<FCompositeFont> Font;
 bool CanEnter() const { return MovieDone.load()&&SceneReady.load(); }
};
