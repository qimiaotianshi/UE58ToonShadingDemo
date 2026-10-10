#include "DaniyaExpressionCurves.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaniyaExpressionTest,"Daniya.LipSync.Expressions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDaniyaExpressionTest::RunTest(const FString&)
{
    const auto Empty=DaniyaExpressions::MapFrame({},.65f);
    TestEqual(TEXT("15 expression morphs"),Empty.Num(),15);
    for (const auto& V:Empty) TestEqual(TEXT("Neutral"),V.Value,0.f);
    TMap<FString,float> C;
    C.Add(TEXT("CTRL_expressions_browRaiseInL"),1);
    auto W=DaniyaExpressions::MapFrame(C,.65f);
    TestEqual(TEXT("Inner brow becomes worried brow"),W[FName(TEXT("左困る"))],.65f);
    TestEqual(TEXT("Other side neutral"),W[FName(TEXT("右困る"))],0.f);
    FRandomStream R(1472);
    const TArray<FString> Bases={TEXT("eyeBlink"),TEXT("eyeWiden"),TEXT("eyeCheekRaise"),TEXT("eyeSquintInner"),TEXT("browRaiseIn"),TEXT("browRaiseOuter"),TEXT("browDown"),TEXT("mouthCornerPull"),TEXT("mouthCornerDepress")};
    for (int32 I=0;I<500;++I)
    {
        for (const auto& B:Bases) for (const auto& S:{TEXT("L"),TEXT("R")}) C.Add(TEXT("CTRL_expressions_")+B+S,R.FRandRange(-1,2));
        C.Add(TEXT("CTRL_expressions_jawOpen"),R.FRand());
        W=DaniyaExpressions::MapFrame(C,R.FRandRange(0,10));
        for (const auto& V:W) if (!FMath::IsFinite(V.Value) || V.Value<0 || V.Value>1.00001f) { AddError(TEXT("Expression out of bounds")); return false; }
        for (const FString S:{FString(TEXT("左")),FString(TEXT("右"))})
        {
            TestTrue(TEXT("Brow budget"),W[FName(*(S+TEXT("上")))]+W[FName(*(S+TEXT("困る")))]+W[FName(*(S+TEXT("怒り")))]<=1.00001f);
            TestTrue(TEXT("Eye budget"),W[FName(TEXT("まばたき"))]+W[FName(*(S+TEXT("笑い目")))]+W[FName(*(S+TEXT("びっくり")))]<=1.00001f);
        }
    }
    return true;
}
#endif
