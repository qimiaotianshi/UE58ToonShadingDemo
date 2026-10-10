#include "DaniyaMouthLibrary.h"
#include "Misc/AutomationTest.h"
#include <limits>

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaniyaMouthMathTest,"Daniya.LipSync.MathAndValidation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDaniyaMouthMathTest::RunTest(const FString& Parameters)
{
    TMap<FString,float> C;
    auto W=UDaniyaMouthLibrary::BuildDaniyaMouthWeights(C);
    TestEqual(TEXT("Empty map is neutral"),W.A+W.I+W.U+W.E+W.O,0.f);
    TArray<FString> Problems;
    TestFalse(TEXT("Empty map diagnosed"),UDaniyaMouthLibrary::ValidateDaniyaControls(C,Problems));
    for (const auto& Name:UDaniyaMouthLibrary::RequiredControlNames()) C.Add(Name,0.f);
    TestTrue(TEXT("Full neutral map valid"),UDaniyaMouthLibrary::ValidateDaniyaControls(C,Problems));
    C[TEXT("CTRL_expressions_jawOpen")]=.5f;
    W=UDaniyaMouthLibrary::BuildDaniyaMouthWeights(C);
    TestTrue(TEXT("Jaw-only matches reference 0.675"),FMath::IsNearlyEqual(W.A,.675f));
    TestEqual(TEXT("Jaw-only no rounded mouth"),W.O,0.f);
    W=UDaniyaMouthLibrary::BuildDaniyaMouthWeights(C,-1);
    TestEqual(TEXT("Negative gain neutral"),W.A,0.f);
    C[TEXT("CTRL_expressions_jawOpen")]=std::numeric_limits<float>::quiet_NaN();
    TestFalse(TEXT("NaN diagnosed"),UDaniyaMouthLibrary::ValidateDaniyaControls(C,Problems));
    W=UDaniyaMouthLibrary::BuildDaniyaMouthWeights(C);
    TestEqual(TEXT("NaN sanitized"),W.A,0.f);
    FRandomStream Rng(42);
    for (int32 I=0;I<1000;++I)
    {
        for (auto& Pair:C) Pair.Value=Rng.FRandRange(-1,2);
        W=UDaniyaMouthLibrary::BuildDaniyaMouthWeights(C,Rng.FRandRange(0,4));
        const float V[]={W.A,W.I,W.U,W.E,W.O};
        for (float X:V) if (!FMath::IsFinite(X)||X<0||X>1) { AddError(TEXT("Weight out of range")); return false; }
        if (W.A+W.I+W.U+W.E+W.O>1.00001f) { AddError(TEXT("Weight sum exceeded one")); return false; }
    }
    return true;
}
#endif
