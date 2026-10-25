#include "WardrobeMenu.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "WardrobeStory.h"
#include "WardrobeShowcase.h"
#include "WardrobeStations.h"
#include "WardrobeLoading.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameViewportClient.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SWidget.h"
#include "Widgets/SOverlay.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStoryMenuTest,"DreamBloom.Story.MenuOverlayKeepsDefaultSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStoryMenuTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();
 ON_SCOPE_EXIT{GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 auto* PC=W->SpawnActor<APlayerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));
 auto* Person=W->SpawnActor<AWardrobeShowcase>();auto* Camera=W->SpawnActor<ACameraActor>();
 Person->Tags.Add(TEXT("MenuTestPerson"));Camera->Tags.Add(TEXT("MenuTestCamera"));
 Person->Character->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube")));
 auto* Flow=LoadObject<UWardrobeStoryFlow>(nullptr,TEXT("/Game/BluePrint/Story/DA_StoryFlow.DA_StoryFlow"));
 if(!TestNotNull(TEXT("Valid story fixture"),Flow))return false;
 auto* Data=NewObject<UWardrobeMenuData>();Data->StoryFlow=DuplicateObject(Flow,GetTransientPackage());
 Data->StoryFlow->SaveNamespace=TEXT("MenuTest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 Data->MenuLevel=FSoftObjectPath(TEXT("/Game/Test/")+UGameplayStatics::GetCurrentLevelName(W,true)+TEXT(".")+UGameplayStatics::GetCurrentLevelName(W,true));
 Data->CharacterData=NewObject<UWardrobeCharacterData>();Data->CharacterData->Characters.SetNum(1);
 auto& Option=Data->CharacterData->Characters[0];Option.CharacterTag=Person->Tags[0];Option.CameraTag=Camera->Tags[0];Option.PhysicsFadeTime=0;
 auto* Sequence=NewObject<ULevelSequence>();Sequence->Initialize();auto* Scene=Sequence->GetMovieScene();
 Scene->SetDisplayRate(FFrameRate(30,1));Scene->SetTickResolutionDirectly(FFrameRate(24000,1));Scene->SetPlaybackRange(0,24000);
 const auto Binding=Scene->AddPossessable(TEXT("MenuCharacter"),AWardrobeShowcase::StaticClass());Sequence->BindPossessableObject(Binding,*Person,W);
 auto* Track=Scene->AddTrack<UMovieScene3DTransformTrack>(Binding);auto* Section=Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());Section->SetRange(TRange<FFrameNumber>::All());Track->AddSection(*Section);
 auto Channels=Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();for(int32 I=0;I<9;++I)Channels[I]->SetDefault(I>=6?1.:0.);
 Channels[0]->AddLinearKey(0,100.);Channels[0]->AddLinearKey(24000,110.);Option.DefaultSequence=Sequence;
 auto* Menu=CreateWidget<UWardrobeMenu>(PC,UWardrobeMenu::StaticClass());Menu->Data=Data;Menu->TakeWidget();
 Data->AudioData=NewObject<UWardrobeAudioData>();
 auto* TestSound=LoadObject<USoundBase>(nullptr,TEXT("/Game/WardrobePinch21/Audio/Grab.Grab"));
 Data->AudioData->LoadGame.Sound=Data->AudioData->NewGame.Sound=Data->AudioData->Back.Sound=TestSound;
 auto* Audio=GI->GetSubsystem<UWardrobeAudioSubsystem>();Audio->Configure(Data->AudioData);
 Menu->StationController=NewObject<UWardrobeStationController>(Menu);
 if(!TestTrue(TEXT("Resolve test character and camera"),Menu->StationController->Initialize(PC,Data->CharacterData,0)))return false;
 Menu->StationController->SetPreviewEnabled(true);auto* Player=Person->GetDefaultSequencePlayer();
 if(!TestNotNull(TEXT("Default Sequence is playing before overlay"),Player))return false;
 TestTrue(TEXT("Test uses actual menu routing"),Menu->IsMenuMap());
 for(bool NewGame:{false,true}){
  GI->GetSubsystem<UWardrobeSession>()->LoggedIn=false;
  Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(.4f,EUpdatePositionMethod::Jump));
  const int32 BeforeOpen=Audio->PlayCount;
  Menu->OpenStorySlots(NewGame);TestTrue(TEXT("Actual save panel opens"),Menu->StorySlotsOpen);
  TestEqual(TEXT("Opening read/new game panel plays exactly one configured UI sound"),Audio->PlayCount,BeforeOpen+1);
  for(int I=0;I<6;++I)Menu->NativeTick(FGeometry(),1.f/60.f);
  TestTrue(TEXT("Default Sequence retains its player and playing state under overlay"),Player==Person->GetDefaultSequencePlayer()&&Person->IsDefaultSequencePlaying());
  TestTrue(TEXT("Opening panel does not reset evaluated pose"),FMath::IsNearlyEqual(Person->GetActorLocation().X,104.,.01));
  TestTrue(TEXT("Playback position is preserved"),FMath::IsNearlyEqual(Player->GetCurrentTime().AsSeconds(),.4,.01));
  TestFalse(TEXT("Panel blocks character input"),Menu->CanPreviewInput());
  Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(.6f,EUpdatePositionMethod::Jump));
  TestTrue(TEXT("Sequence still evaluates a changed pose behind panel"),FMath::IsNearlyEqual(Person->GetActorLocation().X,106.,.01));
  const int32 BeforeClose=Audio->PlayCount;Menu->CloseStorySlots();Menu->NativeTick(FGeometry(),1.f/60.f);
  TestEqual(TEXT("Return button plays exactly one Back sound"),Audio->PlayCount,BeforeClose+1);
  TestTrue(TEXT("Closing overlay restores preview gestures"),Menu->CanPreviewInput());
  TestTrue(TEXT("Closing overlay also preserves player and pose"),Player==Person->GetDefaultSequencePlayer()&&FMath::IsNearlyEqual(Person->GetActorLocation().X,106.,.01));
 }
 Menu->PrepareStoryTravel();TestFalse(TEXT("Real travel still shuts down showcase Sequence"),Person->IsDefaultSequencePlaying());
 Menu->StationController->Shutdown();Menu->ReleaseSlateResources(true);return true;
}

static TArray<FString> VisibleMenuText(const TSharedRef<SWidget>& Root){
 TArray<FString> Text;TFunction<void(const TSharedRef<SWidget>&)> Visit;
 Visit=[&](const TSharedRef<SWidget>& Widget){
  Widget->Invalidate(EInvalidateWidgetReason::Layout);Widget->SlatePrepass(1.f);
  if(!Widget->GetVisibility().IsVisible())return;
  if(Widget->GetType()==FName(TEXT("STextBlock")))Text.Add(StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString());
  auto* Children=Widget->GetChildren();for(int32 I=0;I<Children->Num();++I)Visit(Children->GetChildAt(I));
 };Visit(Root);return Text;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeDestinationMenuTest,"DreamBloom.Story.LoginSlotsAndSelectedDestination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeDestinationMenuTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>();auto* Loading=GI->GetSubsystem<UWardrobeLoadingSubsystem>();
 auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));auto* PC=W->SpawnActor<APlayerController>();PC->SetPlayer(Local);
 auto Overlay=SNew(SOverlay);auto* Viewport=NewObject<UGameViewportClient>(GEngine);Viewport->SetViewportOverlayWidget(nullptr,Overlay);
 GEngine->GetWorldContextFromWorldChecked(W).GameViewport=Viewport;
 auto* Source=LoadObject<UWardrobeStoryFlow>(nullptr,TEXT("/Game/BluePrint/Story/DA_StoryFlow.DA_StoryFlow"));
 auto* Data=NewObject<UWardrobeMenuData>();Data->StoryFlow=DuplicateObject(Source,GI);Data->StoryFlow->SaveNamespace=TEXT("DestinationTest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 ON_SCOPE_EXIT{Loading->Finish(true);Story->Save=nullptr;for(int32 I=0;I<3;++I)UGameplayStatics::DeleteGameInSlot(Story->SlotName(I),0);GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 Data->MenuLevel=FSoftObjectPath(TEXT("/Game/Test/")+UGameplayStatics::GetCurrentLevelName(W,true)+TEXT(".")+UGameplayStatics::GetCurrentLevelName(W,true));
 Data->Levels.SetNum(2);Data->Levels[0].Level=Source->Steps[0].Level;Data->Levels[1].Level=Source->Steps.Last().Level;
 if(!TestTrue(TEXT("Destination differs from first story map"),Data->Levels[0].Level!=Data->Levels[1].Level))return false;
 auto* Menu=CreateWidget<UWardrobeMenu>(PC,UWardrobeMenu::StaticClass());Menu->Data=Data;auto Root=Menu->TakeWidget();auto* Session=GI->GetSubsystem<UWardrobeSession>();Session->SelectedMap=1;
 auto Text=VisibleMenuText(Root);
 TestTrue(TEXT("Login screen owns new/continue/load"),Text.Contains(TEXT("新游戏"))&&Text.Contains(TEXT("继续游戏"))&&Text.Contains(TEXT("读取存档")));
 TestFalse(TEXT("Destination start is hidden before login"),Text.Contains(TEXT("开始游戏")));
 Menu->Login(TEXT("  测试旅人  "));TestTrue(TEXT("New game opens slots while remaining on login"),Menu->StorySlotsOpen&&!Session->LoggedIn&&!Loading->IsLoading());
 Menu->CloseStorySlots();TestFalse(TEXT("Cancelling does not create a campaign or log in"),Story->HasCampaign()||Session->LoggedIn);
 Menu->Login(TEXT("  测试旅人  "));Menu->ChooseStorySlot(0,false);
 TestTrue(TEXT("Selecting new slot enters destination without map travel"),Session->LoggedIn&&!Menu->StorySlotsOpen&&!Loading->IsLoading());
 TestEqual(TEXT("Name committed after slot creation"),Session->Name,FString(TEXT("测试旅人")));
 TestEqual(TEXT("Story starts at first narrative ID"),Story->GetCurrentStoryId(),Source->Steps[0].StoryId);
 auto* Saved=Cast<UWardrobeStorySave>(UGameplayStatics::LoadGameFromSlot(Story->SlotName(0),0));
 TestTrue(TEXT("Initial save uses a menu level instead of story level"),Saved&&Saved->Level==Data->Levels[1].Level);
 Text=VisibleMenuText(Root);TestTrue(TEXT("Destination contains start and login return"),Text.Contains(TEXT("开始游戏"))&&Text.Contains(TEXT("返回登录")));
 for(const FString& Label:{TEXT("新游戏"),TEXT("继续游戏"),TEXT("继续"),TEXT("读取存档"),TEXT("读档")})TestFalse(TEXT("No save actions on destination screen"),Text.Contains(Label));
 Menu->OpenStorySlots(false);Menu->OpenStorySlots(true);Menu->ContinueGame();TestFalse(TEXT("Destination cannot reopen save actions"),Menu->StorySlotsOpen||Loading->IsLoading());
 const int32 Slot=Story->ActiveSlot;Story->ActiveSlot=INDEX_NONE;
 Menu->StartGame();TestTrue(TEXT("Start never forces a new slot prompt"),Loading->IsLoading()&&!Menu->StorySlotsOpen);
 TestEqual(TEXT("Travel targets chosen Levels entry"),Loading->PackageName,Data->Levels[1].Level.ToSoftObjectPath().GetLongPackageName());
 TestEqual(TEXT("Travel preserves narrative progress"),Story->GetCurrentStoryId(),Source->Steps[0].StoryId);
 Loading->Finish(true);Story->ActiveSlot=Slot;Menu->SelectChoice(0);Menu->StartGame();
 TestEqual(TEXT("Another destination also follows Levels"),Loading->PackageName,Data->Levels[0].Level.ToSoftObjectPath().GetLongPackageName());Loading->Finish(true);
 Menu->ReturnToLogin();Text=VisibleMenuText(Root);TestTrue(TEXT("Login return exposes continue/load without deleting progress"),!Session->LoggedIn&&Text.Contains(TEXT("继续游戏"))&&Text.Contains(TEXT("读取存档"))&&Story->HasCampaign());
 Menu->OpenStorySlots(false);Menu->ChooseStorySlot(0,false);
 TestTrue(TEXT("Login read starts travel to saved map"),Loading->IsLoading()&&!Menu->StorySlotsOpen);
 TestEqual(TEXT("Read follows saved map even after destination selection changes"),Loading->PackageName,Saved->Level.ToSoftObjectPath().GetLongPackageName());Loading->Finish(true);
 Menu->ReturnToLogin();Menu->ContinueGame();TestTrue(TEXT("Login continue starts saved travel"),Loading->IsLoading());
 TestEqual(TEXT("Continue follows latest slot"),Loading->PackageName,Saved->Level.ToSoftObjectPath().GetLongPackageName());Loading->Finish(true);
 Menu->ReleaseSlateResources(true);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStoryAudioTest,"DreamBloom.Story.UIAudioOverridesAndLegacyFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStoryAudioTest::RunTest(const FString&){
 auto* Data=NewObject<UWardrobeAudioData>();
 struct FRoute{EWardrobeUISound Event;FWardrobeSoundEntry* Override;FWardrobeSoundEntry* Fallback;};
 FRoute Routes[]={{EWardrobeUISound::NewGame,&Data->NewGame,&Data->Confirm},{EWardrobeUISound::ContinueGame,&Data->ContinueGame,&Data->StartGame},{EWardrobeUISound::LoadGame,&Data->LoadGame,&Data->Confirm},{EWardrobeUISound::SaveSlotSelect,&Data->SaveSlotSelect,&Data->Confirm},{EWardrobeUISound::Pause,&Data->Pause,&Data->Confirm},{EWardrobeUISound::Resume,&Data->Resume,&Data->Cancel}};
 auto* Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/WardrobePinch21/Audio/Grab.Grab"));if(!TestNotNull(TEXT("Sound fixture"),Sound))return false;
 Data->Confirm.Sound=Data->Cancel.Sound=Data->StartGame.Sound=Sound;
 for(auto& Route:Routes){
  TestTrue(TEXT("Empty new entry resolves to existing configured sound"),&Data->GetEntry(Route.Event)==Route.Fallback);
  Route.Override->Sound=Sound;TestTrue(TEXT("Each new button has its own configurable entry"),&Data->GetEntry(Route.Event)==Route.Override);
  Route.Override->Sound.Reset();TestTrue(TEXT("Clearing override restores fallback"),&Data->GetEntry(Route.Event)==Route.Fallback);
 }
 TestTrue(TEXT("Back and overwrite confirmation remain configurable through old entries"),&Data->GetEntry(EWardrobeUISound::Back)==&Data->Back&&&Data->GetEntry(EWardrobeUISound::Confirm)==&Data->Confirm);
 TestEqual(TEXT("Serialized StartGame event value remains compatible"),int32(EWardrobeUISound::StartGame),4);
 TestEqual(TEXT("Serialized pinch event value remains compatible"),int32(EWardrobeUISound::PinchGrab),14);return true;
}
#endif
