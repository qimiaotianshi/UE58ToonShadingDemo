#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "DaniyaAudioAnimation.h"
#include "EditorUtilityWidget.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_AsyncAction.h"
#include "K2Node_CustomEvent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDaniyaAsyncBlueprintTest,"Daniya.LipSync.AsyncBlueprintNode",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDaniyaAsyncBlueprintTest::RunTest(const FString&)
{
    UBlueprint* BP=FKismetEditorUtilities::CreateBlueprint(UEditorUtilityWidget::StaticClass(),GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("DaniyaNodeTest")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    UEdGraph* Graph=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("DaniyaTestGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
    FBlueprintEditorUtils::AddUbergraphPage(BP,Graph);
    auto* Event=NewObject<UK2Node_CustomEvent>(Graph);
    Event->CustomFunctionName=TEXT("GenerateTest"); Graph->AddNode(Event); Event->CreateNewGuid(); Event->PostPlacedNewNode(); Event->AllocateDefaultPins();
    auto* Async=NewObject<UK2Node_AsyncAction>(Graph);
    Async->InitializeProxyFromFunction(UDaniyaGenerateAudioAnimation::StaticClass()->FindFunctionByName(TEXT("GenerateDaniyaAnimationFromAudio")));
    Graph->AddNode(Async); Async->CreateNewGuid(); Async->PostPlacedNewNode(); Async->AllocateDefaultPins();
    const auto* Schema=GetDefault<UEdGraphSchema_K2>();
    TestTrue(TEXT("Execution wire"),Schema->TryCreateConnection(Event->FindPin(UEdGraphSchema_K2::PN_Then),Async->FindPin(UEdGraphSchema_K2::PN_Execute)));
    TestNotNull(TEXT("Expression boolean"),Async->FindPin(TEXT("bIncludeExpressions")));
    TestNotNull(TEXT("Completed callback"),Async->FindPin(TEXT("Completed")));
    TestNotNull(TEXT("Failed callback"),Async->FindPin(TEXT("Failed")));
    TestNotNull(TEXT("Result pin"),Async->FindPin(TEXT("Result")));
    FKismetEditorUtilities::CompileBlueprint(BP);
    TestTrue(TEXT("Editor Utility Blueprint compiles"),BP->Status!=BS_Error);
    return true;
}
#endif
