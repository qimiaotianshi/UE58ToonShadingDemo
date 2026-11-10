#include "WardrobeSpeechBubble.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "WardrobeShowcase.h"
#include "Blueprint/GameViewportSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCaptionFirstFrameTest,"Wardrobe.AISpeech.CaptionFirstFrameLayout",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCaptionFirstFrameTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* World=GI->GetWorld();
 ON_SCOPE_EXIT{GI->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);};
 auto* Viewport=UGameViewportSubsystem::Get();if(!TestNotNull(TEXT("Viewport slot subsystem"),Viewport))return false;
 auto* Legacy=CreateWidget<UWardrobeSpeechBubble>(GI);
 Legacy->SetAlignmentInViewport(FVector2D::ZeroVector);
 const auto OldSlot=Viewport->GetWidgetSlot(Legacy);
 TestTrue(TEXT("Previous first-frame setup stretched the white card across the viewport"),OldSlot.Anchors.IsStretchedHorizontal()&&OldSlot.Anchors.IsStretchedVertical());
 Legacy->RemoveFromParent();

 auto* Person=World->SpawnActor<AWardrobeShowcase>();
 auto* Bubble=CreateWidget<UWardrobeSpeechBubble>(GI);
 for(int32 Round=0;Round<3;++Round){
  Bubble->ShowCaption(Person,Round==0?TEXT("你好。"):FString::ChrN(120,TCHAR(0x4F60)));
  const auto Slot=Viewport->GetWidgetSlot(Bubble);
  TestFalse(TEXT("Caption is never horizontally stretched before viewport attachment"),Slot.Anchors.IsStretchedHorizontal());
  TestFalse(TEXT("Caption is never vertically stretched before viewport attachment"),Slot.Anchors.IsStretchedVertical());
  TestEqual(TEXT("First frame already has a bounded card width"),Slot.Offsets.Right,360.f);
  TestTrue(TEXT("First frame already has a bounded nonzero card height"),Slot.Offsets.Bottom>=126.f&&Slot.Offsets.Bottom<900.f);
  TestEqual(TEXT("Missing player geometry keeps the prepared card hidden"),Bubble->GetVisibility(),ESlateVisibility::Hidden);
  Bubble->FinishCaption(true);
 }
 return true;
}
#endif
