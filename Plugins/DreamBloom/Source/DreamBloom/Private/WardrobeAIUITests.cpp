#include "WardrobeAudio.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "WardrobeMenu.h"
#include "WardrobeShowcase.h"
#include "WardrobeAISpeech.h"
#include "WardrobeStations.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Widgets/Input/SEditableTextBox.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeAISoundMappingTest,"DreamBloom.AIUI.IndependentEventMappings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeAISoundMappingTest::RunTest(const FString&){
 auto* Data=NewObject<UWardrobeAudioData>();Data->Confirm.Sound=Data->Back.Sound=Data->Unavailable.Sound=FSoftObjectPath(TEXT("/Game/Legacy.Sound"));
 const EWardrobeUISound Events[]={EWardrobeUISound::AIOpen,EWardrobeUISound::AIInputFocus,EWardrobeUISound::AITyping,EWardrobeUISound::AISend,EWardrobeUISound::AIClose,EWardrobeUISound::AIUnavailable};
 FWardrobeSoundEntry* Entries[]={&Data->AIOpen,&Data->AIInputFocus,&Data->AITyping,&Data->AISend,&Data->AIClose,&Data->AIUnavailable};
 for(int32 I=0;I<6;++I){TestTrue(TEXT("Unset AI event is silent without generic fallback"),Data->GetEntry(Events[I]).Sound.IsNull());
  Entries[I]->Sound=FSoftObjectPath(FString::Printf(TEXT("/Game/AIUI/Separate%d.Sound"),I));TestTrue(TEXT("Each AI event selects its own DA entry"),&Data->GetEntry(Events[I])==Entries[I]);}
 TestTrue(TEXT("Old confirm remains unchanged"),Data->GetEntry(EWardrobeUISound::Confirm).Sound.ToSoftObjectPath()==FSoftObjectPath(TEXT("/Game/Legacy.Sound")));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeAITypingTest,"DreamBloom.AIUI.ActualTextEditAndSilentProgrammaticClear",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeAITypingTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* World=GI->GetWorld();
 ON_SCOPE_EXIT{GI->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);};
 auto* PC=World->SpawnActor<APlayerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
 auto* Person=World->SpawnActor<AWardrobeShowcase>();auto* Camera=World->SpawnActor<ACameraActor>();Person->Tags.Add(TEXT("AIUIPerson"));Camera->Tags.Add(TEXT("AIUICamera"));
 Person->Character->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube")));
 auto* Data=NewObject<UWardrobeMenuData>();Data->bPlayStartupMovie=false;
 Data->MenuLevel=FSoftObjectPath(TEXT("/Game/Test/")+UGameplayStatics::GetCurrentLevelName(World,true)+TEXT(".")+UGameplayStatics::GetCurrentLevelName(World,true));
 Data->CharacterData=NewObject<UWardrobeCharacterData>();Data->CharacterData->Characters.SetNum(1);auto& Option=Data->CharacterData->Characters[0];
 Option.CharacterTag=Person->Tags[0];Option.CameraTag=Camera->Tags[0];Option.AISpeech=NewObject<UWardrobeAISpeechData>();Option.AISpeech->bAutoStart=false;
 Data->AudioData=NewObject<UWardrobeAudioData>();auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Engine/EditorSounds/Notifications/CompileSuccess.CompileSuccess"));
 Data->AudioData->AIOpen.Sound=Data->AudioData->AITyping.Sound=Data->AudioData->AIClose.Sound=Data->AudioData->AIInputFocus.Sound=Sound;
 auto* Audio=GI->GetSubsystem<UWardrobeAudioSubsystem>();Audio->Configure(Data->AudioData);
 auto* Menu=CreateWidget<UWardrobeMenu>(PC,UWardrobeMenu::StaticClass());Menu->Data=Data;auto Root=Menu->TakeWidget();
 Menu->StationController=NewObject<UWardrobeStationController>(Menu);if(!Menu->StationController->Initialize(PC,Data->CharacterData,0))return false;Menu->StationController->SetPreviewEnabled(true);
 const int32 Before=Audio->PlayCount;Menu->OpenDialogue();TestEqual(TEXT("Open plays its configured sound once"),Audio->PlayCount,Before+1);Menu->OpenDialogue();TestEqual(TEXT("Already-open panel does not retrigger"),Audio->PlayCount,Before+1);
 TSharedPtr<SEditableTextBox> Input;TFunction<void(TSharedRef<SWidget>)> Find=[&](TSharedRef<SWidget> Widget){
  if(Widget->GetType()==FName(TEXT("SEditableTextBox"))){auto Text=StaticCastSharedRef<SEditableTextBox>(Widget);if(Text->GetHintText().ToString()==TEXT("想和她说些什么？"))Input=Text;}
  auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)Find(Children->GetChildAt(I));};Find(Root);
 if(!TestTrue(TEXT("Actual dialogue editable widget located"),Input.IsValid()))return false;
 Input->SetText(FText::FromString(TEXT("你")));TestEqual(TEXT("Text change plays typing sound"),Audio->PlayCount,Before+2);
 Input->SetText(FText::FromString(TEXT("你好")));TestEqual(TEXT("Fast typing is throttled without sound pileup"),Audio->PlayCount,Before+2);
 Menu->ClickDialogueInput();TestEqual(TEXT("Input click has independent feedback"),Audio->PlayCount,Before+3);
 Menu->CloseDialogue();TestEqual(TEXT("Closing plays only close; clearing text is silent"),Audio->PlayCount,Before+4);
 Menu->DialogueTextEdited(TEXT("a"),TEXT("b"));TestEqual(TEXT("Closed panel cannot produce typing sound"),Audio->PlayCount,Before+4);
 Menu->StationController->Shutdown();Menu->ReleaseSlateResources(true);return true;
}
#endif
