#include "DaniyaAudioAnimation.h"
#include "DaniyaLipSyncEditorLibrary.h"
#include "DaniyaMouthLibrary.h"
#include "DaniyaExpressionCurves.h"
#include "DaniyaSilenceGate.h"
#include "MetaHumanPerformance.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Sound/SoundWave.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Containers/Ticker.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"
#include "HAL/FileManager.h"
#include "Editor.h"

DEFINE_LOG_CATEGORY_STATIC(LogDaniyaAudio,Log,All);
namespace
{
TWeakObjectPtr<UDaniyaGenerateAudioAnimation> ActiveJob;
bool SaveAsset(UObject* Asset)
{
    UPackage* Package=Asset->GetOutermost();
    const FString Filename=FPackageName::LongPackageNameToFilename(Package->GetName(),FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename),true);
    FSavePackageArgs Args;
    Args.TopLevelFlags=RF_Public|RF_Standalone;
    Args.SaveFlags=SAVE_NoError;
    return UPackage::SavePackage(Package,Asset,*Filename,Args);
}
bool Occupied(const FString& Path) { return FPackageName::DoesPackageExist(Path) || FindPackage(nullptr,*Path)!=nullptr; }
}

UDaniyaGenerateAudioAnimation* UDaniyaGenerateAudioAnimation::GenerateDaniyaAnimationFromAudio(USoundWave* Audio, USkeletalMesh* Mesh, float MouthGain, bool IncludeExpressions, float ExpressionGain, FString OutputFolder, FString AssetName, EAudioDrivenAnimationMood Mood, float MoodIntensity, bool Smooth, bool CloseMouthInSilence, float SilenceThresholdDb, float MouthCloseTime, float MouthOpenTime)
{
    auto* Task=NewObject<UDaniyaGenerateAudioAnimation>();
    Task->SourceAudio=Audio; Task->TargetMesh=Mesh; Task->MouthGainValue=MouthGain;
    Task->bExpressions=IncludeExpressions; Task->ExpressionGainValue=ExpressionGain;
    Task->Folder=OutputFolder; Task->RequestedName=AssetName; Task->MoodValue=Mood;
    Task->MoodIntensityValue=MoodIntensity; Task->bSmoothValue=Smooth;
    Task->bSilence=CloseMouthInSilence; Task->SilenceDb=SilenceThresholdDb; Task->CloseTime=MouthCloseTime; Task->OpenTime=MouthOpenTime;
    return Task;
}

void UDaniyaGenerateAudioAnimation::Activate()
{
    if (bActivated) return;
    bActivated=true;
    AddToRoot(); // Editor utilities have no GameInstance. Hold the job until its completion callback.
    if (!IsInGameThread() || (GEditor && GEditor->PlayWorld)) { Finish(false,TEXT("Run this node in an Editor Utility outside Play mode.")); return; }
    if (ActiveJob.IsValid()) { Finish(false,TEXT("Another Daniya audio job is running. Wait for it to finish or cancel it.")); return; }
    if (!IsValid(SourceAudio) || !FMath::IsFinite(SourceAudio->GetDuration()) || SourceAudio->GetDuration()<=0 || SourceAudio->GetDuration()*30>1000000)
    { Finish(false,TEXT("Select a valid imported SoundWave (not a SoundCue or file path). Maximum duration is 1,000,000 frames at 30 fps.")); return; }
    if (!IsValid(TargetMesh) || !IsValid(TargetMesh->GetSkeleton())) { Finish(false,TEXT("Select a Skeletal Mesh with a valid skeleton.")); return; }
    for (FName N:UDaniyaMouthLibrary::MorphNames())
        if (!TargetMesh->FindMorphTarget(N) || !TargetMesh->GetSkeleton()->GetCurveMetaDataMorphTarget(N))
        { Finish(false,TEXT("Mesh/skeleton is missing required MMD mouth morph metadata: ")+N.ToString()); return; }
    if (bExpressions && DaniyaExpressions::SupportedMorphs(TargetMesh).IsEmpty())
    { Finish(false,TEXT("No supported expression morphs found. Disable Include Expressions or select DaniyaToUe.")); return; }
    if (!FMath::IsFinite(MouthGainValue) || MouthGainValue<0 || !FMath::IsFinite(ExpressionGainValue) || ExpressionGainValue<0 ||
        !FMath::IsFinite(MoodIntensityValue) || MoodIntensityValue<0 || MoodIntensityValue>1 || !StaticEnum<EAudioDrivenAnimationMood>()->IsValidEnumValue(static_cast<int64>(MoodValue)))
    { Finish(false,TEXT("Gains must be finite and non-negative. MoodIntensity must be in [0,1], and Mood must be valid.")); return; }
    Folder.RemoveFromEnd(TEXT("/"));
    FString SilenceError;
    if (bSilence && !DaniyaSilence::Validate(SilenceDb,CloseTime,OpenTime,SilenceError)) { Finish(false,SilenceError); return; }
    if (RequestedName.IsEmpty()) RequestedName=TEXT("AS_")+SourceAudio->GetName()+(bExpressions?TEXT("_Face"):TEXT("_Mouth"));
    const FString Base=Folder+TEXT("/")+RequestedName;
    FText Reason;
    if (!Folder.StartsWith(TEXT("/Game/")) || RequestedName.Contains(TEXT("/")) || !FPackageName::IsValidLongPackageName(Base,false,&Reason) || Base.Contains(TEXT(".")))
    { Finish(false,TEXT("Use an output folder under /Game and a simple asset name without extension, slashes, spaces or punctuation.")); return; }
    OutputPath=Base;
    for (int32 Index=1; Occupied(OutputPath) || Occupied(OutputPath+TEXT("_Performance")); ++Index) OutputPath=Base+FString::Printf(TEXT("_%03d"),Index);
    PerformancePath=OutputPath+TEXT("_Performance");
    // Keep the solve transient until successful. Failures never leave empty Performance assets on disk.
    WorkingPerformance=NewObject<UMetaHumanPerformance>();
    WorkingPerformance->SetInputType(EDataInputType::Audio);
    WorkingPerformance->SetAudio(SourceAudio);
    WorkingPerformance->AudioDrivenAnimationOutputControls=bExpressions ? EAudioDrivenAnimationOutputControls::FullFace : EAudioDrivenAnimationOutputControls::MouthOnly;
    WorkingPerformance->HeadMovementMode=EPerformanceHeadMovementMode::Disabled;
    WorkingPerformance->bGenerateBlinks=bExpressions;
    WorkingPerformance->AudioDrivenAnimationSolveOverrides.Mood=MoodValue;
    WorkingPerformance->AudioDrivenAnimationSolveOverrides.MoodIntensity=MoodIntensityValue;
    WorkingPerformance->SetBlockingProcessing(false);
    if (!WorkingPerformance->CanProcess()) { Finish(false,WorkingPerformance->GetCannotProcessTooltipText().ToString()); return; }
    ActiveJob=this;
    PipelineHandle=WorkingPerformance->OnProcessingFinished().AddUObject(this,&UDaniyaGenerateAudioAnimation::OnPipelineFinished);
    FrameHandle=WorkingPerformance->OnFrameProcessed().AddWeakLambda(this,[this](int32 Frame)
    { Progress=FMath::Clamp(static_cast<float>(Frame+1)/FMath::Max(1u,WorkingPerformance->EndFrameToProcess)*.9f,0.f,.9f); });
    TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UDaniyaGenerateAudioAnimation::Tick));
    UE_LOG(LogDaniyaAudio,Display,TEXT("Processing %s, IncludeExpressions=%s"),*SourceAudio->GetName(),bExpressions?TEXT("true"):TEXT("false"));
    const auto Start=WorkingPerformance->StartPipeline(true);
    if (Start!=EStartPipelineErrorType::None)
    {
        bPipelineFinished=true; bPipelineSucceeded=false;
        PipelineError=FString::Printf(TEXT("MetaHuman StartPipeline failed (%d). %s"),static_cast<int32>(Start),*WorkingPerformance->GetCannotProcessTooltipText().ToString());
    }
}

void UDaniyaGenerateAudioAnimation::OnPipelineFinished(TSharedPtr<const UE::MetaHuman::Pipeline::FPipelineData> Data)
{
    bPipelineSucceeded=Data.IsValid() && Data->GetExitStatus()==UE::MetaHuman::Pipeline::EPipelineExitStatus::Ok;
    if (!bPipelineSucceeded) PipelineError=Data.IsValid() ? Data->GetErrorMessage()+TEXT(" ")+Data->GetErrorNodeMessage() : TEXT("MetaHuman returned no pipeline result.");
    bPipelineFinished=true;
    // The native delegate fires before MetaHuman releases pipeline objects. Finalize on a later ticker call.
}

bool UDaniyaGenerateAudioAnimation::Tick(float)
{
    if (!bPipelineFinished || WorkingPerformance->IsProcessing()) return true;
    TickHandle.Reset();
    if (bCancelled) Finish(false,TEXT("Cancelled. No output assets were saved."));
    else if (!bPipelineSucceeded) Finish(false,TEXT("MetaHuman audio solve failed. ")+PipelineError);
    else Finalize();
    return false;
}

void UDaniyaGenerateAudioAnimation::Cancel()
{
    if (!bActivated || bFinished) return;
    bCancelled=true;
    if (WorkingPerformance && WorkingPerformance->IsProcessing()) WorkingPerformance->CancelPipeline();
}

void UDaniyaGenerateAudioAnimation::Finalize()
{
    if (Occupied(OutputPath) || Occupied(PerformancePath)) { Finish(false,TEXT("Output name was created by another operation while solving. Please generate again.")); return; }
    FString Error;
    UAnimSequence* Anim=UDaniyaLipSyncEditorLibrary::CreateDaniyaMouthAnimation(WorkingPerformance,TargetMesh,OutputPath,Error,MouthGainValue,bSmoothValue);
    if (!Anim) { Finish(false,Error); return; }
    Result.Animation=Anim;
    if (bExpressions && !DaniyaExpressions::Bake(WorkingPerformance,TargetMesh,Anim,ExpressionGainValue,bSmoothValue,Result.ExpressionCurves,Error))
    { Finish(false,Error); return; }
    if (bSilence && !DaniyaSilence::Apply(SourceAudio,Anim,SilenceDb,CloseTime,OpenTime,Error)) { Finish(false,Error); return; }
    UPackage* Package=CreatePackage(*PerformancePath);
    auto* SavedPerformance=DuplicateObject<UMetaHumanPerformance>(WorkingPerformance,Package,*FPackageName::GetLongPackageAssetName(PerformancePath));
    SavedPerformance->SetFlags(RF_Public|RF_Standalone);
    FAssetRegistryModule::AssetCreated(SavedPerformance);
    SavedPerformance->MarkPackageDirty();
    Result.Performance=SavedPerformance;
    // Saving is explicit in this node's contract. Report failures and retain references for manual recovery.
    if (!SaveAsset(SavedPerformance)) { Finish(false,TEXT("Could not save the generated Performance. Output assets remain in memory for recovery.")); return; }
    if (!SaveAsset(Anim)) { Finish(false,TEXT("Performance saved, but animation could not be saved. Check output folder permissions/disk space.")); return; }
    Result.SavedAssetPath=Anim->GetPathName();
    FString Message=FString::Printf(TEXT("Saved %s. %d mouth curves, %d expression curves."),*Result.SavedAssetPath,5,Result.ExpressionCurves.Num());
    if (bExpressions && Result.ExpressionCurves.Num()<15) Message+=TEXT(" Some Daniya expression morphs are absent; only supported morphs were written.");
    Finish(true,Message);
}

void UDaniyaGenerateAudioAnimation::Finish(bool Success, const FString& Message)
{
    if (bFinished) return;
    bFinished=true; Result.bSuccess=Success; Result.Message=Message;
    if (WorkingPerformance)
    {
        WorkingPerformance->OnProcessingFinished().Remove(PipelineHandle);
        WorkingPerformance->OnFrameProcessed().Remove(FrameHandle);
    }
    if (TickHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickHandle); TickHandle.Reset(); }
    if (ActiveJob.Get()==this) ActiveJob.Reset();
    if (Success) { Progress=1.f; UE_LOG(LogDaniyaAudio,Display,TEXT("%s"),*Message); Completed.Broadcast(Result); }
    else { UE_LOG(LogDaniyaAudio,Warning,TEXT("%s"),*Message); Failed.Broadcast(Result); }
    WorkingPerformance=nullptr;
    RemoveFromRoot(); SetReadyToDestroy();
}

void UDaniyaAudioAnimationTool::Generate()
{
    if (Task && !Task->bFinished) { Status=TEXT("Already processing. Wait or click Cancel."); return; }
    Status=TEXT("Processing audio. Please wait; you can continue working in the editor.");
    Animation=nullptr; Performance=nullptr;
    Task=UDaniyaGenerateAudioAnimation::GenerateDaniyaAnimationFromAudio(Audio,Mesh,MouthGain,bIncludeExpressions,ExpressionGain,OutputFolder,AssetName,Mood,MoodIntensity,bSmooth,bCloseMouthInSilence,SilenceThresholdDb,MouthCloseTime,MouthOpenTime);
    Task->Completed.AddDynamic(this,&UDaniyaAudioAnimationTool::OnFinished);
    Task->Failed.AddDynamic(this,&UDaniyaAudioAnimationTool::OnFinished);
    Task->Activate();
}
void UDaniyaAudioAnimationTool::Cancel() { if (Task) Task->Cancel(); }
void UDaniyaAudioAnimationTool::OnFinished(const FDaniyaAudioAnimationResult& InResult)
{
    Status=InResult.Message; Animation=InResult.Animation; Performance=InResult.Performance;
}
