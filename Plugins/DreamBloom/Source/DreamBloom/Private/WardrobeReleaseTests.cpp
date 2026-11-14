#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeReleaseNode.h"
#include "Misc/MemStack.h"
#include "Animation/AnimInstanceProxy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeReleaseCurveLifetimeTest,"DreamBloom.Release.CurveSnapshotLifetime",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeReleaseCurveLifetimeTest::RunTest(const FString&){
 FAnimNode_WardrobeRelease Node;
 const FName Face(TEXT("WardrobeReleaseTestFace")),Blink(TEXT("WardrobeReleaseTestBlink"));
 // A controlled reproduction of the removed implementation proves that the
 // test covers the corruption: only a canary we own is overwritten here.
 {
  FMemMark Outer(FMemStack::Get());FBlendedCurve LegacyCache;
  // Keep a large page alive so popping OldFrame rewinds within that same page.
  // Otherwise the later 8K canary can select a different page from the small curve.
  FMemStack::Get().Alloc(8192,16);
  {FMemMark OldFrame(FMemStack::Get());FBlendedCurve Source;Source.Set(Face,.25f);LegacyCache.CopyFrom(Source);}
  auto* Canary=static_cast<uint8*>(FMemStack::Get().Alloc(8192,16));FMemory::Memset(Canary,0x5a,8192);
  FBlendedCurve Next;Next.Set(Face,.8f);LegacyCache.CopyFrom(Next);
  int32 Changed=0;for(int32 I=0;I<8192;++I)Changed+=Canary[I]!=0x5a;
  TestTrue(TEXT("Legacy persistent stack curve reproduces the observed memory overwrite"),Changed>0);
 }
 // Capture exactly the data stored between animation evaluations, then pop that frame.
 {
  FMemMark Frame(FMemStack::Get());FBlendedCurve Source;
  Source.Set(Face,.25f);Source.SetFlags(Face,UE::Anim::ECurveElementFlags::MorphTarget);
  Source.Set(Blink,.75f);Node.LastCurve.CopyFrom(Source);Node.StartCurve.CopyFrom(Node.LastCurve);
 }
 {
  FMemMark Frame(FMemStack::Get());
  // Reuse the old frame allocation as another subsystem's pointer table. The old
  // stack-backed member wrote over this canary on its next CopyFrom, as in RDG.
  constexpr int32 Size=8192;auto* Canary=static_cast<uint8*>(FMemStack::Get().Alloc(Size,16));FMemory::Memset(Canary,0x5a,Size);
  TestEqual(TEXT("Start snapshot survives frame-memory reuse"),Node.StartCurve.Get(Face),.25f);
  TestEqual(TEXT("Start snapshot retains second expression"),Node.StartCurve.Get(Blink),.75f);
  TestTrue(TEXT("Morph flags survive the snapshot"),EnumHasAnyFlags(Node.StartCurve.GetFlags(Face),UE::Anim::ECurveElementFlags::MorphTarget));
  FBlendedCurve Next;Next.Set(Face,.8f);Next.Set(Blink,.1f);Node.LastCurve.CopyFrom(Next);
  int32 Changed=0;for(int32 I=0;I<Size;++I)Changed+=Canary[I]!=0x5a;
  TestEqual(TEXT("Next evaluation does not overwrite another frame's memory"),Changed,0);
  TestEqual(TEXT("Last snapshot receives new expression"),Node.LastCurve.Get(Face),.8f);
  TestEqual(TEXT("Start snapshot remains independent of last"),Node.StartCurve.Get(Face),.25f);
  FBlendedCurve Mixed;Mixed.Lerp(Node.StartCurve,Next,.5f);
  TestTrue(TEXT("Heap snapshot blends into frame-local animation curves"),FMath::IsNearlyEqual(Mixed.Get(Face),.525f));
 }
 FAnimInstanceProxy Proxy;Node.Initialize_AnyThread(FAnimationInitializeContext(&Proxy));
 TestEqual(TEXT("Initialization clears held curves"),Node.LastCurve.Num(),0);TestEqual(TEXT("Initialization clears transition curves"),Node.StartCurve.Num(),0);
 Node.LastCurve.Set(Face,1.f);Node.StartCurve.Set(Face,1.f);Node.CacheBones_AnyThread(FAnimationCacheBonesContext(&Proxy));
 TestEqual(TEXT("Bone-cache rebuild clears held curves"),Node.LastCurve.Num(),0);TestEqual(TEXT("Bone-cache rebuild clears transition curves"),Node.StartCurve.Num(),0);
 return true;
}
#endif
