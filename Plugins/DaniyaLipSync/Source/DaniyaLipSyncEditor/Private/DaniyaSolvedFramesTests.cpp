#include "DaniyaSolvedFrames.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaniyaTailTest,"Daniya.LipSync.AudioTail",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDaniyaTailTest::RunTest(const FString&)
{
    TArray<FFrameAnimationData> Frames; Frames.SetNum(391);
    for (int32 I=0;I<390;++I) Frames[I].AnimationData.Add(TEXT("CTRL_expressions_jawOpen"),.1f);
    FString Error;
    TestTrue(TEXT("13.035 second audio tail accepted"),DaniyaSolvedFrames::RepairAudioTail(Frames,30,13.03510189,Error));
    TestEqual(TEXT("Frame count preserved"),Frames.Num(),391);
    TestEqual(TEXT("Final value extended"),Frames[390].AnimationData.FindRef(TEXT("CTRL_expressions_jawOpen")),.1f);
    Frames[100].AnimationData.Reset();
    DaniyaSolvedFrames::RepairAudioTail(Frames,30,13.03510189,Error);
    TestTrue(TEXT("Interior gap not repaired"),Frames[100].AnimationData.IsEmpty());
    for (int32 I=388;I<391;++I) Frames[I].AnimationData.Reset();
    TestFalse(TEXT("Long missing tail rejected"),DaniyaSolvedFrames::RepairAudioTail(Frames,30,13.03510189,Error));
    return true;
}
#endif
