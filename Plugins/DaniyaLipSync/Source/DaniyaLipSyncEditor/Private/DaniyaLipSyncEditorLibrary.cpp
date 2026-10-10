#include "DaniyaLipSyncEditorLibrary.h"
#include "DaniyaSilenceGate.h"
#include "DaniyaSolvedFrames.h"
#include "MetaHumanPerformance.h"
#include "FrameAnimationData.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Engine/SkeletalMesh.h"
#include "Sound/SoundWave.h"
#include "Factories/AnimSequenceFactory.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDaniyaLipSync, Log, All);

bool UDaniyaLipSyncEditorLibrary::CloseDaniyaMouthInSilence(USoundWave* Audio, UAnimSequence* Animation, FString& Error, float Db, float Close, float Open)
{
    return DaniyaSilence::Apply(Audio,Animation,Db,Close,Open,Error);
}

namespace
{
bool ReadPerformance(UMetaHumanPerformance* P, float Gain, TArray<FDaniyaMouthWeights>& Out, double& FPS, double& Duration, FString& Error)
{
    Error.Reset(); Out.Reset();
    if (!IsValid(P) || P->IsProcessing()) { Error=TEXT("Select a valid Performance and wait for Process to finish."); return false; }
    if (!FMath::IsFinite(Gain) || Gain<0) { Error=TEXT("Gain must be a finite non-negative number."); return false; }
    if (P->InputType!=EDataInputType::Audio || !IsValid(P->Audio)) { Error=TEXT("Only a full Audio Performance with a SoundWave is supported."); return false; }
    FPS=P->GetFrameRate().AsDecimal(); Duration=P->Audio->GetDuration();
    if (!FMath::IsFinite(FPS) || FPS<=0 || !FMath::IsFinite(Duration) || Duration<=0 || Duration*FPS>1000000)
    { Error=TEXT("Invalid or excessive audio duration/frame rate (limit: 1,000,000 frames)."); return false; }
    const auto& Limits=P->GetProcessingLimitFrameRange();
    if (!Limits.HasLowerBound() || !Limits.HasUpperBound() || Limits.GetLowerBoundValue().Value!=0 ||
        P->StartFrameToProcess!=0 || P->EndFrameToProcess!=static_cast<uint32>(Limits.GetUpperBoundValue().Value))
    { Error=TEXT("Process the entire audio from frame 0. Partial processing ranges are not supported."); return false; }
    if (!P->CanExportAnimation() || P->AnimationData.Num()<2 || P->AnimationData.Num()>1000000)
    { Error=TEXT("No usable solved frames. Run Process on the Performance first."); return false; }
    TArray<FFrameAnimationData> Frames=P->GetAnimationData();
    if (FMath::Abs(Frames.Num()-Duration*FPS)>2.0)
    { Error=TEXT("Solved frame count does not match the whole audio duration. Reprocess the full range."); return false; }
    if (!DaniyaSolvedFrames::RepairAudioTail(Frames,FPS,Duration,Error)) return false;
    Out.Reserve(Frames.Num());
    for (int32 I=0; I<Frames.Num(); ++I)
    {
        TArray<FString> Problems;
        if (!UDaniyaMouthLibrary::ValidateDaniyaControls(Frames[I].AnimationData,Problems))
        { Error=FString::Printf(TEXT("Invalid solved frame %d: %s. This is not a repairable empty audio tail. Reprocess the full audio and check for excluded or failed frames."),I,*Problems[0]); Out.Reset(); return false; }
        Out.Add(UDaniyaMouthLibrary::BuildDaniyaMouthWeights(Frames[I].AnimationData,Gain));
    }
    return true;
}
bool CheckMesh(USkeletalMesh* Mesh, FString& Error)
{
    if (!IsValid(Mesh) || !IsValid(Mesh->GetSkeleton())) { Error=TEXT("Select Daniya's skeletal mesh with a valid skeleton."); return false; }
    for (FName N:UDaniyaMouthLibrary::MorphNames())
    {
        if (!Mesh->FindMorphTarget(N)) { Error=TEXT("Mesh is missing MMD morph: ")+N.ToString(); return false; }
        if (!Mesh->GetSkeleton()->GetCurveMetaDataMorphTarget(N)) { Error=TEXT("Skeleton is missing Morph Target curve metadata: ")+N.ToString(); return false; }
    }
    return true;
}
bool WriteCurves(UAnimSequence* Anim, const TArray<FDaniyaMouthWeights>& Frames, double FPS, double Duration, bool Smooth, FString& Error)
{
    const double Length=Anim->GetPlayLength();
    const double LastSample=(Frames.Num()-1)/FPS;
    if (!FMath::IsFinite(Length) || Length+0.0001<LastSample || Length>Duration+2.0/FPS)
    { Error=TEXT("Target length does not match the source audio. Use Create Daniya Mouth Animation for a correctly sized asset."); return false; }
    TArray<FRichCurveKey> Keys[5];
    const double End=FMath::Min(Length,Duration);
    for (int32 I=0; I<Frames.Num(); ++I)
    {
        FDaniyaMouthWeights W=Frames[I];
        if (Smooth)
        {
            const auto& Prev=Frames[FMath::Max(0,I-1)]; const auto& Next=Frames[FMath::Min(Frames.Num()-1,I+1)];
            W.A=(Prev.A+2*W.A+Next.A)*.25f; W.I=(Prev.I+2*W.I+Next.I)*.25f;
            W.U=(Prev.U+2*W.U+Next.U)*.25f; W.E=(Prev.E+2*W.E+Next.E)*.25f; W.O=(Prev.O+2*W.O+Next.O)*.25f;
        }
        if (I==0) W=FDaniyaMouthWeights();
        const float T=I/FPS;
        if (T>=End-0.00001) break; // Reserve one unique zero key at End.
        const float Values[]={W.A,W.I,W.U,W.E,W.O};
        for (int32 C=0; C<5; ++C) Keys[C].Add(FRichCurveKey(T,Values[C],0,0,RCIM_Linear));
    }
    for (int32 C=0; C<5; ++C)
    {
        Keys[C].Add(FRichCurveKey(End,0,0,0,RCIM_Linear));
        if (Length>End+0.00001) Keys[C].Add(FRichCurveKey(Length,0,0,0,RCIM_Linear));
    }
    Anim->Modify();
    auto& Controller=Anim->GetController();
    Controller.OpenBracket(NSLOCTEXT("DaniyaLipSync","Bake","Bake Daniya mouth curves"));
    bool Success=true;
    for (int32 C=0; C<5; ++C)
    {
        const FAnimationCurveIdentifier Id(UDaniyaMouthLibrary::MorphNames()[C],ERawCurveTrackTypes::RCT_Float);
        // Remove only when present, to avoid spurious warnings.
        if (Anim->GetDataModel()->FindCurve(Id)) Controller.RemoveCurve(Id);
        Success=Controller.AddCurve(Id,AACF_DefaultCurve) && Success;
        Success=Controller.SetCurveKeys(Id,Keys[C]) && Success;
    }
    Controller.CloseBracket();
    Anim->MarkPackageDirty();
    if (!Success) Error=TEXT("Curve controller rejected a write. Undo the operation and inspect the Output Log.");
    return Success;
}
}

bool UDaniyaLipSyncEditorLibrary::GetDaniyaMouthFrame(UMetaHumanPerformance* P, int32 FrameIndex, FDaniyaMouthWeights& W, FString& Error, float Gain)
{
    W=FDaniyaMouthWeights(); Error.Reset();
    if (!IsValid(P) || P->IsProcessing() || FrameIndex<0 || FrameIndex>=P->AnimationData.Num())
    { Error=TEXT("Invalid Performance/frame index, or processing is still running."); return false; }
    if (!FMath::IsFinite(Gain) || Gain<0) { Error=TEXT("Gain must be finite and non-negative."); return false; }
    TArray<FString> Problems;
    const auto& Controls=P->AnimationData[FrameIndex].AnimationData;
    if (!UDaniyaMouthLibrary::ValidateDaniyaControls(Controls,Problems)) { Error=Problems[0]; return false; }
    W=UDaniyaMouthLibrary::BuildDaniyaMouthWeights(Controls,Gain); return true;
}

bool UDaniyaLipSyncEditorLibrary::BakeDaniyaMouthCurves(UMetaHumanPerformance* P, UAnimSequence* Target, USkeletalMesh* Mesh, FString& Error, float Gain, bool Smooth)
{
    Error.Reset();
    if (!CheckMesh(Mesh,Error)) return false;
    if (!IsValid(Target) || Target->GetSkeleton()!=Mesh->GetSkeleton()) { Error=TEXT("Target animation must use the selected mesh's skeleton."); return false; }
    TArray<FDaniyaMouthWeights> Frames; double FPS=0,Duration=0;
    if (!ReadPerformance(P,Gain,Frames,FPS,Duration,Error)) return false;
    return WriteCurves(Target,Frames,FPS,Duration,Smooth,Error);
}

UAnimSequence* UDaniyaLipSyncEditorLibrary::CreateDaniyaMouthAnimation(UMetaHumanPerformance* P, USkeletalMesh* Mesh, const FString& Path, FString& Error, float Gain, bool Smooth)
{
    Error.Reset();
    if (!CheckMesh(Mesh,Error)) return nullptr;
    FText Reason;
    if (!Path.StartsWith(TEXT("/Game/")) || !FPackageName::IsValidLongPackageName(Path,false,&Reason) || Path.Contains(TEXT(".")))
    { Error=TEXT("Use a new package path, e.g. /Game/DaniyaTest/LipSyncDemo/AS_Daniya_CPP (no .uasset)."); return nullptr; }
    if (FPackageName::DoesPackageExist(Path) || FindPackage(nullptr,*Path)) { Error=TEXT("An asset/package already exists at that path. Choose a new name."); return nullptr; }
    TArray<FDaniyaMouthWeights> Frames; double FPS=0,Duration=0;
    if (!ReadPerformance(P,Gain,Frames,FPS,Duration,Error)) return nullptr;
    UAnimSequenceFactory* Factory=NewObject<UAnimSequenceFactory>();
    Factory->TargetSkeleton=Mesh->GetSkeleton(); Factory->PreviewSkeletalMesh=Mesh;
    UAnimSequence* Anim=Cast<UAnimSequence>(FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get().CreateAsset(FPackageName::GetLongPackageAssetName(Path),FPackageName::GetLongPackagePath(Path),UAnimSequence::StaticClass(),Factory));
    if (!Anim) { Error=TEXT("Could not create animation asset."); return nullptr; }
    auto& C=Anim->GetController();
    C.OpenBracket(NSLOCTEXT("DaniyaLipSync","Init","Initialize Daniya animation"));
    C.SetFrameRate(P->GetFrameRate());
    C.SetNumberOfFrames(FFrameNumber(static_cast<int32>(FMath::CeilToInt(Duration*FPS))));
    C.CloseBracket();
    Anim->SetPreviewMesh(Mesh);
    if (!WriteCurves(Anim,Frames,FPS,Duration,Smooth,Error)) return nullptr;
    return Anim;
}

void UDaniyaMouthAnimationModifier::OnApply_Implementation(UAnimSequence* Anim)
{
    FString Error;
    if (UDaniyaLipSyncEditorLibrary::BakeDaniyaMouthCurves(Performance,Anim,Mesh,Error,Gain,bSmooth))
    { LastResult=TEXT("Success. Five mouth curves baked. Save the animation."); UE_LOG(LogDaniyaLipSync,Display,TEXT("%s"),*LastResult); }
    else { LastResult=Error; UE_LOG(LogDaniyaLipSync,Error,TEXT("%s"),*Error); }
}
