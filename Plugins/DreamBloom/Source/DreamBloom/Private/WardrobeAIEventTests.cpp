#include "WardrobeAIEvent.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "WardrobeAISpeechTypes.h"
#include "WardrobeAISpeechSubsystem.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeStations.h"
#include "WardrobeStory.h"
#include "WardrobeMenu.h"

#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

static FString EventFixture(){return TEXT("{\"title\":\"窗边\",\"description\":\"女孩看向窗外。\",\"opening\":{\"text\":\"再陪我一会儿吧。\",\"emotion\":\"温柔\"},\"options\":[{\"id\":\"A\",\"text\":\"好呀\",\"mood_delta\":4,\"reply\":{\"text\":\"有你在真好。\",\"emotion\":\"开心\"}},{\"id\":\"B\",\"text\":\"先写作业吧\",\"mood_delta\":-2,\"reply\":{\"text\":\"好吧，我们一起。\",\"emotion\":\"无奈\"}}]}");}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeAIEventParserTest,"DreamBloom.AIEvents.StrictPlanAndMoodPrompt",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeAIEventParserTest::RunTest(const FString&){
 FWardrobeSpeechJob J;J.Emotions=GetDefault<UWardrobeAISpeechData>()->Emotions;J.MoodScore=22;J.RecentEvents=TEXT("一起写作业");
 FWardrobeAIEventPlan Plan;FString Error;
 TestTrue(TEXT("Full event accepted"),WardrobeAIEvent::ParsePlan(EventFixture(),J,5,Plan,Error));
 TestEqual(TEXT("Two prepared branches"),Plan.Options.Num(),2);TestEqual(TEXT("Stable branch A"),Plan.Options[0].Id,FString(TEXT("A")));
 TestEqual(TEXT("Mood and spoken expression kept separate"),Plan.Options[0].Reply.Emotion,FName(TEXT("开心")));
 for(const auto& Bad:{EventFixture().Replace(TEXT("\"mood_delta\":4"),TEXT("\"mood_delta\":6")),EventFixture().Replace(TEXT("\"mood_delta\":4"),TEXT("\"mood_delta\":1.5")),EventFixture().Replace(TEXT("\"id\":\"B\""),TEXT("\"id\":\"A\"")),EventFixture().Replace(TEXT("无奈"),TEXT("未知心情")),EventFixture().Replace(TEXT("好呀"),TEXT("【开心】好呀")),FString(TEXT("{\"options\":[]}")),FString::ChrN(8001,TEXT('x'))}){
  TestFalse(TEXT("Malformed or unbounded plan rejected"),WardrobeAIEvent::ParsePlan(Bad,J,5,Plan,Error));TestTrue(TEXT("No partial plan escapes validation"),Plan.Options.IsEmpty());
 }
 const FString Body=WardrobeAIEvent::BuildPlanRequest(J,TEXT("校园小房间"),5);
 TestTrue(TEXT("One request asks for both branches and expected mood"),Body.Contains(TEXT("校园小房间"))&&Body.Contains(TEXT("22/100"))&&Body.Contains(TEXT("mood_delta"))&&Body.Contains(TEXT("一起写作业")));
 const auto Chat=WardrobeAISpeech::BuildChatRequest(J);const FString Prompt=Chat->GetArrayField(TEXT("messages"))[0]->AsObject()->GetStringField(TEXT("content"));
 TestTrue(TEXT("Manual and automatic speech consume current mood and recent choices"),Prompt.Contains(TEXT("22/100"))&&Prompt.Contains(TEXT("一起写作业")));
 J.Text=TEXT("你好");J.Emotion=TEXT("开心");TestEqual(TEXT("No mood metadata reaches TTS"),WardrobeAISpeech::BuildVoiceRequest(J)->GetStringField(TEXT("text")),FString(TEXT("你好")));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeAIEventSaveTest,"DreamBloom.AIEvents.SaveIsolationAndSerialWrites",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeAIEventSaveTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>();
 auto* Flow=NewObject<UWardrobeStoryFlow>(GI);Flow->SaveNamespace=TEXT("AIEventTest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 Story->Flow=Flow;Story->ActiveSlot=0;Story->Save=NewObject<UWardrobeStorySave>(Story);auto* Saved=Story->Save.Get();
 const FString Slot=Story->SlotName(0);
 ON_SCOPE_EXIT{Story->FlushPendingSaves();Story->Save=nullptr;UGameplayStatics::DeleteGameInSlot(Slot,0);GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 Saved->FormatVersion=1;Saved->PlayerName=TEXT("测试旅人");Saved->Level=FSoftObjectPath(TEXT("/Game/Wardrobe/Maps/L_WardrobeMain.L_WardrobeMain"));Saved->PlayerTransform=FTransform(FRotator(0,21,0),FVector(33,44,155));Saved->bHasPlayerTransform=true;Saved->CompletedStories.Add(TEXT("Scene1"));Saved->NextStoryId=TEXT("Scene2");
 const auto Location=Saved->PlayerTransform;const auto Level=Saved->Level;
 TestEqual(TEXT("Legacy missing field defaults to 60"),Story->GetCharacterMood(TEXT("Daniya"),60),60);
 TestTrue(TEXT("First choice accepted"),Story->CommitCharacterChoice(TEXT("Daniya"),TEXT("one:A"),60,4,60,TEXT("陪她坐一会儿")));
 TestFalse(TEXT("Double-click cannot settle again"),Story->CommitCharacterChoice(TEXT("Daniya"),TEXT("one:A"),64,4,60,TEXT("重复")));
 TestTrue(TEXT("Checkpoint queues behind choice"),Story->SaveCheckpoint());
 TestTrue(TEXT("Second choice updates latest snapshot"),Story->CommitCharacterChoice(TEXT("Daniya"),TEXT("two:B"),64,-2,60,TEXT("一起写作业")));
 Story->FlushPendingSaves();auto* Loaded=Cast<UWardrobeStorySave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
 TestTrue(TEXT("Latest serialized update wins"),Loaded&&Loaded->CharacterMoods.FindRef(TEXT("Daniya")).Value==62&&Loaded->FormatVersion==3);
 TestTrue(TEXT("Mood commit and menu checkpoint preserve map, pose and story"),Loaded&&Loaded->Level==Level&&Loaded->PlayerTransform.Equals(Location)&&Loaded->CompletedStories.Contains(TEXT("Scene1"))&&Loaded->NextStoryId==TEXT("Scene2")&&Loaded->PlayerName==TEXT("测试旅人"));
 TestEqual(TEXT("Different stable character has independent default"),Story->GetCharacterMood(TEXT("Other"),45),45);
 TestFalse(TEXT("Stale prepared mood cannot overwrite newer state"),Story->CommitCharacterChoice(TEXT("Daniya"),TEXT("stale"),60,2,60,TEXT("stale")));
 Saved->CharacterMoods.FindChecked(TEXT("Daniya")).Value=99;TestTrue(TEXT("Upper clamp"),Story->CommitCharacterChoice(TEXT("Daniya"),TEXT("clamp"),99,5,60,TEXT("clamp")));TestEqual(TEXT("Never above 100"),Story->GetCharacterMood(TEXT("Daniya")),100);
 for(int32 I=0;I<70;++I)Story->CommitCharacterChoice(TEXT("Daniya"),FString::FromInt(I),100,0,60,TEXT("short"));
 TestEqual(TEXT("Bounded summary history"),Saved->CharacterMoods.FindChecked(TEXT("Daniya")).RecentEvents.Num(),8);TestEqual(TEXT("Bounded settlement ledger"),Saved->CharacterMoods.FindChecked(TEXT("Daniya")).SettledChoices.Num(),64);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeAIEventLifecycleTest,"DreamBloom.AIEvents.PreparationGateAndCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeAIEventLifecycleTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();auto* E=GI->GetSubsystem<UWardrobeAIEventSubsystem>();
 ON_SCOPE_EXIT{GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 TSharedPtr<FWardrobeSpeechClip,ESPMode::ThreadSafe> Clip=MakeShared<FWardrobeSpeechClip,ESPMode::ThreadSafe>();Clip->PCM.SetNumZeroed(320);Clip->Names.Add(TEXT("あ"));Clip->Values.SetNumZeroed(2);TWeakPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Weak=Clip;
 E->State=EWardrobeAIEventState::Preparing;E->Clips.SetNum(3);E->Clips[0]=Clip;Clip.Reset();E->PreparedIndex=1;
 TestFalse(TEXT("Partial preparation cannot show options"),E->IsVisible());TestFalse(TEXT("Partial preparation cannot choose"),E->CanChoose());TestTrue(TEXT("Cache owns prepared heap data"),E->GetTemporaryBytes()>0);
 const uint64 Old=E->Generation;E->CancelEvent();TestEqual(TEXT("Cancel clears entire cache"),E->GetTemporaryBytes(),0);TestFalse(TEXT("Cancelled clip lifetime released"),Weak.IsValid());
 E->ReceivePlan(Old,EventFixture(),TEXT(""));E->ReceiveClip(Old,1,nullptr,TEXT(""));TestTrue(TEXT("Old async completions cannot resurrect event"),E->State==EWardrobeAIEventState::Waiting&&!E->IsVisible()&&E->GetTemporaryBytes()==0);
 TestFalse(TEXT("Logged-out or no-campaign session cannot request event"),E->RequestEventNow());return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeAIEventChoiceTest,"DreamBloom.AIEvents.CachedChoiceAndSingleSettlement",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeAIEventChoiceTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>();auto* E=GI->GetSubsystem<UWardrobeAIEventSubsystem>();
 auto* PC=W->SpawnActor<APlayerController>();auto* M=NewObject<UWardrobeMenu>(PC);M->SetPlayerContext(FLocalPlayerContext());M->Data=NewObject<UWardrobeMenuData>();
 M->Data->MenuLevel=FSoftObjectPath(TEXT("/Game/Test/")+UGameplayStatics::GetCurrentLevelName(W,true)+TEXT(".")+UGameplayStatics::GetCurrentLevelName(W,true));
 M->StationController=NewObject<UWardrobeStationController>(M);GI->GetSubsystem<UWardrobeSession>()->LoggedIn=true;
 auto* Person=W->SpawnActor<AWardrobeShowcase>();auto* Data=NewObject<UWardrobeCharacterData>();Data->Characters.SetNum(1);auto* Profile=NewObject<UWardrobeAISpeechData>();Profile->bAutoStart=false;Data->Characters[0].AISpeech=Profile;
 Person->InteractionData=Data;Person->InteractionProfileIndex=0;Person->SetInteractionEnabled(true);auto* Speech=Person->AISpeech.Get();Speech->State=EWardrobeSpeechState::Idle;
 auto* Flow=NewObject<UWardrobeStoryFlow>(GI);Flow->SaveNamespace=TEXT("AIChoiceTest_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);Story->Flow=Flow;Story->ActiveSlot=0;Story->Save=NewObject<UWardrobeStorySave>(Story);const FString Slot=Story->SlotName(0);
 ON_SCOPE_EXIT{E->CancelEvent();Story->FlushPendingSaves();Story->Save=nullptr;UGameplayStatics::DeleteGameInSlot(Slot,0);GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 // Use the actual widget world without constructing the startup movie.
 E->Menu=M;E->Speech=Speech;E->Profile=Profile;E->CharacterId=Profile->CharacterId;E->ExpectedMood=60;E->InitialMood=60;E->EventId=TEXT("event-choice");E->State=EWardrobeAIEventState::AwaitingChoice;
 FWardrobeSpeechJob J;J.Emotions=Profile->Emotions;FString Error;WardrobeAIEvent::ParsePlan(EventFixture(),J,5,E->Plan,Error);
 E->Clips.SetNum(3);for(int32 I=1;I<3;++I){auto C=MakeShared<FWardrobeSpeechClip,ESPMode::ThreadSafe>();C->PCM.SetNumZeroed(8000);C->Text=E->Plan.Options[I-1].Reply.Text;C->Emotion=E->Plan.Options[I-1].Reply.Emotion;C->Names.Add(TEXT("あ"));C->Frames=1;C->Values.Add(0);E->Clips[I]=C;}
 TestTrue(TEXT("Eligible menu for cached response"),E->Eligible());
 TestTrue(TEXT("Choice plays immediately"),E->ChooseOption(1));TestEqual(TEXT("Correct selected branch"),Speech->CurrentText,FString(TEXT("好吧，我们一起。")));
 TestEqual(TEXT("Correct delta applied"),Story->GetCharacterMood(TEXT("Daniya")),58);TestFalse(TEXT("Double-click rejected"),E->ChooseOption(1));
 TestFalse(TEXT("Selection starts no HTTP/TTS/solver work"),GI->GetSubsystem<UWardrobeAISpeechSubsystem>()->IsBusy());TestEqual(TEXT("Both event-owned branches released after handoff"),E->GetTemporaryBytes(),0);
 Speech->CancelSpeech();TestEqual(TEXT("Playback PCM released on interruption"),Speech->GetTemporaryBytes(),0);return true;
}
#endif
