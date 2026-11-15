#include "WardrobeCursor.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Layout/SBox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCursorStateTest,"DreamBloom.Cursor.ClickReleaseAndMultipleButtons",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCursorStateTest::RunTest(const FString&){
 FWardrobeCursorInputState State;State.ReleaseTime=.2f;
 TestTrue(TEXT("Initially default"),State.Get(1)==EWardrobeCursorState::Default);
 State.Up(EKeys::LeftMouseButton,1);TestTrue(TEXT("Unrelated release causes no flash"),State.Get(1)==EWardrobeCursorState::Default);
 State.Down(EKeys::LeftMouseButton);State.Down(EKeys::LeftMouseButton);State.Down(EKeys::RightMouseButton);
 TestEqual(TEXT("Duplicate/double-click press is idempotent"),State.Buttons.Num(),2);
 State.Up(EKeys::LeftMouseButton,2);TestTrue(TEXT("Held right button keeps pressed skin"),State.Get(2)==EWardrobeCursorState::Pressed);
 State.Up(EKeys::RightMouseButton,3);TestTrue(TEXT("Final release shows release skin"),State.Get(3.1)==EWardrobeCursorState::Released);
 TestTrue(TEXT("Release feedback returns to default"),State.Get(3.3)==EWardrobeCursorState::Default);
 State.Down(EKeys::MiddleMouseButton);State.Reset();TestTrue(TEXT("Focus loss clears a held cursor"),State.Get(4)==EWardrobeCursorState::Default);
 State.Down(EKeys::A);TestTrue(TEXT("Keyboard cannot press cursor"),State.Buttons.IsEmpty());
 State.ReleaseTime=0;State.Down(EKeys::LeftMouseButton);State.Up(EKeys::LeftMouseButton,5);TestTrue(TEXT("Zero release duration returns immediately"),State.Get(5)==EWardrobeCursorState::Default);
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCursorRestoreTest,"DreamBloom.Cursor.RestoreViewportAndEditorIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCursorRestoreTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* World=GI->GetWorld();
 ON_SCOPE_EXIT{GI->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);};
 auto* Client=NewObject<UGameViewportClient>(GEngine);GEngine->GetWorldContextFromWorldChecked(World).GameViewport=Client;
 auto Original=SNew(SBox);Client->SetSoftwareCursorWidget(EMouseCursor::Default,Original);Client->SetUseSoftwareCursorWidgets(false);Client->SetHideCursorDuringCapture(true);
 auto* Cursor=GI->GetSubsystem<UWardrobeCursorSubsystem>();FWardrobeCursorSettings Settings;
 Cursor->Configure(Settings);TestTrue(TEXT("Pink cursor is scoped to this game viewport"),Client->GetSoftwareCursorWidget(EMouseCursor::Default)!=Original);
 TestTrue(TEXT("Software cursor enabled for game"),Client->GetIsUsingSoftwareCursorWidgets());
 TestFalse(TEXT("Skin remains visible while dragging in a visible-cursor UI"),Client->HideCursorDuringCapture());
 TestTrue(TEXT("Input field and button use skin too"),Client->HasSoftwareCursor(EMouseCursor::TextEditBeam)&&Client->HasSoftwareCursor(EMouseCursor::Hand));
 Cursor->Configure(Settings);Settings.bEnabled=false;Cursor->Configure(Settings);
 TestTrue(TEXT("Reconfiguration preserves original cursor widget"),Client->GetSoftwareCursorWidget(EMouseCursor::Default)==Original);
 TestFalse(TEXT("Previous software-cursor setting restored"),Client->GetIsUsingSoftwareCursorWidgets());
 TestTrue(TEXT("Previous capture visibility restored"),Client->HideCursorDuringCapture());
 TestFalse(TEXT("Injected hand mapping removed"),Client->HasSoftwareCursor(EMouseCursor::Hand));
 return true;
}
#endif
