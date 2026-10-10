#include "DaniyaSilenceGate.h"
#include "Misc/AutomationTest.h"
#include "DaniyaLipSyncEditorLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Sound/SoundWave.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaniyaSilenceTest,"Daniya.LipSync.SilenceGate",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDaniyaSilenceTest::RunTest(const FString&)
{
    constexpr int32 Rate=16000;
    TArray<int16> PCM; PCM.SetNumZeroed(Rate*2);
    TArray<float> Gate; FString Error;
    TestTrue(TEXT("Silent audio accepted"),DaniyaSilence::Analyze(PCM,Rate,1,-42,.08,.02,Gate,Error));
    for (float G:Gate) TestEqual(TEXT("Silence exactly zero"),G,0.f);
    // Speech .2-.7 s and 1.2-1.7 s with a 20ms internal consonant gap.
    for (int32 I=0;I<PCM.Num();++I)
    {
        const double T=static_cast<double>(I)/Rate;
        const bool Speech=((T>=.2 && T<.7)||(T>=1.2 && T<1.7)) && !(T>=.4 && T<.42);
        PCM[I]=static_cast<int16>((Speech?.2:.0001)*32767*FMath::Sin(2*PI*220*T));
    }
    TestTrue(TEXT("Segmented audio accepted"),DaniyaSilence::Analyze(PCM,Rate,1,-42,.08,.02,Gate,Error));
    TestEqual(TEXT("Sustained speech unchanged"),Gate[30],1.f);
    TestEqual(TEXT("Brief consonant gap held"),Gate[41],1.f);
    TestEqual(TEXT("Quiet pause closed after release"),Gate[90],0.f);
    TestEqual(TEXT("Next phrase reopened"),Gate[130],1.f);
    TestEqual(TEXT("Tail closed"),Gate[190],0.f);
    for (int32 I=1;I<Gate.Num();++I) TestTrue(TEXT("Bounded smooth transitions"),FMath::Abs(Gate[I]-Gate[I-1])<=.50001f);
    TArray<int16> Stereo;
    for (int16 V:PCM) { Stereo.Add(V); Stereo.Add(-V); }
    TArray<float> StereoGate;
    TestTrue(TEXT("Stereo accepted"),DaniyaSilence::Analyze(Stereo,Rate,2,-42,.08,.02,StereoGate,Error));
    TestTrue(TEXT("Opposite phase does not cancel detection"),StereoGate==Gate);
    TestFalse(TEXT("Invalid settings rejected"),DaniyaSilence::Analyze(PCM,Rate,1,-42,0,.02,Gate,Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaniyaRealSilenceTest,"Daniya.LipSync.RealAudioSilence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDaniyaRealSilenceTest::RunTest(const FString&)
{
    auto* Audio=LoadObject<USoundWave>(nullptr,TEXT("/Game/DaniyaTest/达尼亚_这样多累呀.达尼亚_这样多累呀"));
    auto* Source=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/DaniyaTest/Generated/AS_Daniya_AutoFace.AS_Daniya_AutoFace"));
    if (!Audio || !Source) { AddWarning(TEXT("Daniya fixture assets unavailable; skipped real audio test.")); return true; }
    auto* Copy=DuplicateObject<UAnimSequence>(Source,GetTransientPackage());
    FString Error; TArray<uint8> Raw; uint32 Rate; uint16 Channels;
    TestTrue(TEXT("Read PCM"),Audio->GetImportedSoundWaveData(Raw,Rate,Channels));
    TArray<float> Gates;
    if (!DaniyaSilence::Analyze(MakeArrayView(reinterpret_cast<const int16*>(Raw.GetData()),Raw.Num()/2),Rate,Channels,-42,.08,.02,Gates,Error)) { AddError(Error); return false; }
    TestTrue(TEXT("Apply gate"),UDaniyaLipSyncEditorLibrary::CloseDaniyaMouthInSilence(Audio,Copy,Error));
    const FAnimationCurveIdentifier Mouth(TEXT("あ"),ERawCurveTrackTypes::RCT_Float);
    const auto& Before=Source->GetDataModel()->FindFloatCurve(Mouth)->FloatCurve;
    const auto& After=Copy->GetDataModel()->FindFloatCurve(Mouth)->FloatCurve;
    int32 Closed=0, Preserved=0;
    for (int32 I=1;I<Gates.Num()-1;++I)
    {
        const float T=I*.01f;
        if (T>=Copy->GetPlayLength()) break;
        const float A=After.Eval(T),B=Before.Eval(T);
        TestTrue(TEXT("Gate product matches PCM envelope"),FMath::Abs(A-B*Gates[I])<.00002f);
        if (Gates[I]==0 && B>.001) { TestEqual(TEXT("Residual mouth removed"),A,0.f); ++Closed; }
        if (Gates[I]==1 && B>.001) { TestTrue(TEXT("Speech preserved"),FMath::Abs(A-B)<.00002f); ++Preserved; }
    }
    TestTrue(TEXT("Actually closed residual mouth samples"),Closed>0);
    TestTrue(TEXT("Actually preserved speech samples"),Preserved>0);
    const FAnimationCurveIdentifier Brow(TEXT("左困る"),ERawCurveTrackTypes::RCT_Float);
    TestTrue(TEXT("Expression keys unchanged"),Source->GetDataModel()->FindFloatCurve(Brow)->FloatCurve.GetConstRefOfKeys()==Copy->GetDataModel()->FindFloatCurve(Brow)->FloatCurve.GetConstRefOfKeys());
    TestEqual(TEXT("Additive type unchanged"),Copy->AdditiveAnimType,Source->AdditiveAnimType);
    AddInfo(FString::Printf(TEXT("Real audio: %d residual A samples closed; %d speech A samples unchanged (100Hz)."),Closed,Preserved));
    return true;
}
#endif
