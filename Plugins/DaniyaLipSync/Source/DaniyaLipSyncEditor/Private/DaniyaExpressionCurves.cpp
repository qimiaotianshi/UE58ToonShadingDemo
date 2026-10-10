#include "DaniyaExpressionCurves.h"
#include "DaniyaSolvedFrames.h"
#include "MetaHumanPerformance.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Sound/SoundWave.h"

namespace DaniyaExpressions
{
TMap<FName,float> MapFrame(const TMap<FString,float>& Controls, float Gain)
{
    auto C=[&](const FString& Short)
    {
        const float* V=Controls.Find(TEXT("CTRL_expressions_")+Short);
        return V && FMath::IsFinite(*V) ? FMath::Clamp(*V,0.f,1.f) : 0.f;
    };
    Gain=FMath::IsFinite(Gain) ? FMath::Max(0.f,Gain) : 0.f;
    TMap<FName,float> Out;
    // Use the combined blink to avoid ambiguous MMD wink names. Never combine it with wink shapes.
    const float Blink=FMath::Clamp((C(TEXT("eyeBlinkL"))+C(TEXT("eyeBlinkR")))*.5f*Gain,0.f,1.f);
    Out.Add(FName(TEXT("まばたき")),Blink);
    for (const FString& Side : {FString(TEXT("L")),FString(TEXT("R"))})
    {
        const FString Prefix=Side==TEXT("L") ? TEXT("左") : TEXT("右");
        float Raise=C(TEXT("browRaiseOuter")+Side)*Gain;
        float Worry=FMath::Max(0.f,C(TEXT("browRaiseIn")+Side)-C(TEXT("browRaiseOuter")+Side))*Gain;
        float Down=C(TEXT("browDown")+Side)*Gain;
        const float BrowScale=FMath::Max(1.f,Raise+Worry+Down);
        Out.Add(FName(*(Prefix+TEXT("上"))),Raise/BrowScale);
        Out.Add(FName(*(Prefix+TEXT("困る"))),Worry/BrowScale);
        Out.Add(FName(*(Prefix+TEXT("怒り"))),Down/BrowScale);
        float Wide=C(TEXT("eyeWiden")+Side)*Gain*(1-Blink);
        float Squint=(C(TEXT("eyeCheekRaise")+Side)+C(TEXT("eyeSquintInner")+Side))*.25f*Gain*(1-Blink);
        const float EyeScale=FMath::Max(1.f,(Wide+Squint)/FMath::Max(.00001f,1-Blink));
        Out.Add(FName(*(Prefix+TEXT("びっくり"))),Wide/EyeScale);
        Out.Add(FName(*(Prefix+TEXT("笑い目"))),Squint/EyeScale);
        // Mouth corners are intentionally subtle because the vowel shapes already widen the mouth.
        const float CornerGain=.35f*Gain*(1-.7f*C(TEXT("jawOpen")));
        const float Pull=C(TEXT("mouthCornerPull")+Side), Depress=C(TEXT("mouthCornerDepress")+Side);
        Out.Add(FName(*(Prefix+TEXT("口角上げ"))),FMath::Clamp(FMath::Max(0.f,Pull-Depress)*CornerGain,0.f,1.f));
        Out.Add(FName(*(Prefix+TEXT("口角下げ"))),FMath::Clamp(FMath::Max(0.f,Depress-Pull)*CornerGain,0.f,1.f));
    }
    return Out;
}

TArray<FName> SupportedMorphs(USkeletalMesh* Mesh)
{
    TArray<FName> Result;
    if (!Mesh || !Mesh->GetSkeleton()) return Result;
    for (const auto& Pair:MapFrame({},1.f))
        if (Mesh->FindMorphTarget(Pair.Key) && Mesh->GetSkeleton()->GetCurveMetaDataMorphTarget(Pair.Key)) Result.Add(Pair.Key);
    Result.Sort(FNameLexicalLess());
    return Result;
}

bool Bake(UMetaHumanPerformance* P, USkeletalMesh* Mesh, UAnimSequence* Anim, float Gain, bool Smooth, TArray<FName>& Written, FString& Error)
{
    Written=SupportedMorphs(Mesh);
    if (Written.IsEmpty()) { Error=TEXT("This mesh has no supported Daniya expression morphs. Disable Include Expressions or use DaniyaToUe."); return false; }
    const double FPS=P->GetFrameRate().AsDecimal(), Duration=P->Audio->GetDuration(), Length=Anim->GetPlayLength();
    TArray<TMap<FName,float>> Mapped;
    const TArray<FString> Bases={TEXT("eyeBlink"),TEXT("eyeWiden"),TEXT("eyeCheekRaise"),TEXT("eyeSquintInner"),TEXT("browRaiseIn"),TEXT("browRaiseOuter"),TEXT("browDown"),TEXT("mouthCornerPull"),TEXT("mouthCornerDepress")};
    TArray<FFrameAnimationData> Solved=P->GetAnimationData();
    if (!DaniyaSolvedFrames::RepairAudioTail(Solved,FPS,Duration,Error)) return false;
    for (const auto& Frame:Solved)
    {
        for (const auto& Base:Bases) for (const auto& Side:{TEXT("L"),TEXT("R")})
        {
            const FString Key=TEXT("CTRL_expressions_")+Base+Side;
            const float* V=Frame.AnimationData.Find(Key);
            if (!V || !FMath::IsFinite(*V)) { Error=TEXT("Full Face solve is missing a valid control: ")+Key; return false; }
        }
        Mapped.Add(MapFrame(Frame.AnimationData,Gain));
    }
    auto& Controller=Anim->GetController();
    Controller.OpenBracket(NSLOCTEXT("DaniyaLipSync","Expressions","Bake Daniya expressions"));
    bool OK=true;
    for (const FName Morph:Written)
    {
        TArray<FRichCurveKey> Keys;
        for (int32 I=0; I<Mapped.Num(); ++I)
        {
            const float Time=I/FPS;
            if (Time>=Duration-0.00001) break;
            float V=Mapped[I][Morph];
            if (Smooth) V=(Mapped[FMath::Max(0,I-1)][Morph]+2*V+Mapped[FMath::Min(Mapped.Num()-1,I+1)][Morph])*.25f;
            // Gently fade inferred emotion at both ends instead of an abrupt brow/eyelid jump.
            V*=FMath::Clamp(static_cast<float>(FMath::Min(Time,Duration-Time)/.12),0.f,1.f);
            Keys.Add(FRichCurveKey(Time,V,0,0,RCIM_Linear));
        }
        Keys.Add(FRichCurveKey(Duration,0,0,0,RCIM_Linear));
        if (Length>Duration+.00001) Keys.Add(FRichCurveKey(Length,0,0,0,RCIM_Linear));
        const FAnimationCurveIdentifier Id(Morph,ERawCurveTrackTypes::RCT_Float);
        OK=Controller.AddCurve(Id,AACF_DefaultCurve) && OK;
        OK=Controller.SetCurveKeys(Id,Keys) && OK;
    }
    Controller.CloseBracket();
    if (!OK) Error=TEXT("Failed to write expression curves. The output was not saved.");
    return OK;
}
}
