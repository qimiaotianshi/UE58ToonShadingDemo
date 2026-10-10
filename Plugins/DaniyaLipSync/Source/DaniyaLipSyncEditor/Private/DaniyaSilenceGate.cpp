#include "DaniyaSilenceGate.h"
#include "DaniyaMouthLibrary.h"
#include "Sound/SoundWave.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimData/IAnimationDataController.h"

namespace DaniyaSilence
{
constexpr double Step=.01; // 100 Hz volume envelope, independent of animation frame rate.
bool Validate(float Db, float Close, float Open, FString& Error)
{
    if (!FMath::IsFinite(Db) || Db < -90 || Db > -6 || !FMath::IsFinite(Close) || Close<.01 || Close>.5 || !FMath::IsFinite(Open) || Open<.01 || Open>.2)
    { Error=TEXT("Silence threshold must be -90 to -6 dBFS; CloseTime 0.01 to 0.5s; OpenTime 0.01 to 0.2s."); return false; }
    return true;
}
bool Analyze(TArrayView<const int16> PCM, int32 Rate, int32 Channels, float Db, float Close, float Open, TArray<float>& Gates, FString& Error)
{
    Gates.Reset();
    if (!Validate(Db,Close,Open,Error)) return false;
    if (Rate<=0 || Channels<=0 || Channels>64 || PCM.Num()==0 || PCM.Num()%Channels!=0)
    { Error=TEXT("Invalid imported PCM audio for silence detection."); return false; }
    const int32 Samples=PCM.Num()/Channels;
    const double Duration=static_cast<double>(Samples)/Rate;
    if (Duration>1000000./30.) { Error=TEXT("Audio too long for silence detection."); return false; }
    const int32 Count=FMath::CeilToInt(Duration/Step)+1;
    TArray<float> Activity; Activity.SetNumZeroed(Count);
    for (int32 I=0; I<Count; ++I)
    {
        // Centered 20 ms RMS. Clamp to valid samples without padding short clips with silence.
        const int32 First=FMath::Clamp(FMath::FloorToInt((I*Step-.01)*Rate),0,Samples);
        const int32 Last=FMath::Clamp(FMath::CeilToInt((I*Step+.01)*Rate),0,Samples);
        double MaxPower=0;
        if (Last>First) for (int32 C=0; C<Channels; ++C)
        {
            double Sum=0;
            for (int32 S=First; S<Last; ++S) { const double V=PCM[S*Channels+C]/32768.; Sum+=V*V; }
            MaxPower=FMath::Max(MaxPower,Sum/(Last-First));
        }
        const double Level=10.*FMath::LogX(10.,FMath::Max(MaxPower,1.e-12));
        const float X=FMath::Clamp(static_cast<float>((Level-Db)/6.),0.f,1.f);
        Activity[I]=X*X*(3-2*X); // Quiet syllables fade through a 6 dB knee, not an on/off threshold.
    }
    float Value=0;
    for (int32 I=0; I<Count; ++I)
    {
        float Target=0;
        // Hold for 30ms after speech, anticipate by 20ms to retain consonant onsets.
        for (int32 J=FMath::Max(0,I-3); J<=FMath::Min(Count-1,I+2); ++J) Target=FMath::Max(Target,Activity[J]);
        const float Delta=Step/(Target>Value ? Open : Close);
        Value=Target>Value ? FMath::Min(Target,Value+Delta) : FMath::Max(Target,Value-Delta);
        if (Value<1.e-6f) Value=0; // Exact zero after bounded release, not an infinite exponential tail.
        Gates.Add(Value);
    }
    return true;
}

bool Apply(USoundWave* Audio, UAnimSequence* Anim, float Db, float Close, float Open, FString& Error)
{
    Error.Reset();
    if (!IsValid(Audio) || !IsValid(Anim)) { Error=TEXT("Select a valid SoundWave and animation for silence gating."); return false; }
    if (!Validate(Db,Close,Open,Error)) return false;
    TArray<uint8> Raw; uint32 Rate=0; uint16 Channels=0;
    if (!Audio->GetImportedSoundWaveData(Raw,Rate,Channels) || Raw.Num()%2!=0)
    { Error=TEXT("Cannot read imported PCM audio. Reimport the SoundWave, or disable Close Mouth In Silence."); return false; }
    TArray<float> Gates;
    if (!Analyze(MakeArrayView(reinterpret_cast<const int16*>(Raw.GetData()),Raw.Num()/2),Rate,Channels,Db,Close,Open,Gates,Error)) return false;
    const double Duration=static_cast<double>(Raw.Num()/2/Channels)/Rate, Length=Anim->GetPlayLength();
    const double FPS=Anim->GetDataModel()->GetFrameRate().AsDecimal();
    if (FMath::Abs(Duration-Audio->GetDuration())>.05 || !FMath::IsFinite(Length) || Length<=0 || !FMath::IsFinite(FPS) || FPS<=0 || FMath::Abs(Length-Duration)>2./FPS+.001)
    { Error=TEXT("Animation/source PCM duration mismatch. Use the original full audio without time offsets or cropping."); return false; }
    TArray<FRichCurveKey> Output[5];
    const auto& Names=UDaniyaMouthLibrary::MorphNames();
    for (int32 C=0; C<5; ++C)
    {
        const FFloatCurve* Curve=Anim->GetDataModel()->FindFloatCurve(FAnimationCurveIdentifier(Names[C],ERawCurveTrackTypes::RCT_Float));
        if (!Curve) { Error=TEXT("Animation is missing mouth curve: ")+Names[C].ToString(); return false; }
        TArray<float> Times;
        for (const auto& Key:Curve->FloatCurve.GetConstRefOfKeys()) if (Key.Time>=0 && Key.Time<=Length) Times.Add(Key.Time);
        for (int32 I=0; I<Gates.Num() && I*Step<Length; ++I) Times.Add(I*Step);
        Times.Add(Length); Times.Sort();
        for (float Time:Times)
        {
            if (!Output[C].IsEmpty() && Time-Output[C].Last().Time<.000001f) continue;
            const float Position=Time/Step;
            const int32 Index=FMath::Clamp(FMath::FloorToInt(Position),0,Gates.Num()-1);
            const float Gate=FMath::Lerp(Gates[Index],Gates[FMath::Min(Index+1,Gates.Num()-1)],FMath::Clamp(Position-Index,0.f,1.f));
            const float Original=Curve->FloatCurve.Eval(Time);
            if (!FMath::IsFinite(Original)) { Error=TEXT("Mouth curve contains invalid values; gating aborted."); return false; }
            Output[C].Add(FRichCurveKey(Time,Original*Gate,0,0,RCIM_Linear));
        }
    }
    // Validate and build every replacement first. Only the five vowel curves are modified.
    Anim->Modify(); auto& Controller=Anim->GetController();
    Controller.OpenBracket(NSLOCTEXT("Daniya","Silence","Close mouth during audio silence"));
    bool OK=true;
    for (int32 C=0; C<5; ++C) OK=Controller.SetCurveKeys(FAnimationCurveIdentifier(Names[C],ERawCurveTrackTypes::RCT_Float),Output[C]) && OK;
    Controller.CloseBracket(); Anim->MarkPackageDirty();
    if (!OK) Error=TEXT("Curve write failed. Undo and inspect the Output Log.");
    return OK;
}
}
