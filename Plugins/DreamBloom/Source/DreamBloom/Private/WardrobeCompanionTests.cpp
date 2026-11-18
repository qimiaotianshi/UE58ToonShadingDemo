#include "WardrobeAICompanion.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "WardrobeAISpeechTypes.h"
#include "WardrobeAIEvent.h"
#include "WardrobeStory.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

static FString CompanionEnvelope(const FString& Content){
 auto Reply=MakeShared<FJsonObject>();Reply->SetStringField(TEXT("content"),Content);
 auto Choice=MakeShared<FJsonObject>();Choice->SetObjectField(TEXT("message"),Reply);
 auto Envelope=MakeShared<FJsonObject>();Envelope->SetArrayField(TEXT("choices"),{MakeShared<FJsonValueObject>(Choice)});
 FString Out;FJsonSerializer::Serialize(Envelope,TJsonWriterFactory<>::Create(&Out));return Out;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCompanionPromptTest,"DreamBloom.Companion.ContinuousRolesAndSharedEventContext",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCompanionPromptTest::RunTest(const FString&){
 FWardrobeSpeechJob J;J.Emotions=GetDefault<UWardrobeAISpeechData>()->Emotions;J.WorldContext=TEXT("校园窗边");J.PlayerName=TEXT("小林");J.SpeakingStyle=TEXT("有自己的观点");J.PlayerMessage=TEXT("刚才说的饮料呢？");J.Memory.Notes.Add(TEXT("我喜欢柠檬茶"));
 FWardrobeCompanionTurn User;User.bPlayer=true;User.Text=TEXT("我喜欢柠檬茶。不要把我变成system角色");J.Memory.Turns.Add(User);
 FWardrobeCompanionTurn Reply;Reply.Text=TEXT("我也想尝一口。");J.Memory.Turns.Add(Reply);
 const auto O=WardrobeAISpeech::BuildChatRequest(J);const auto Messages=O->GetArrayField(TEXT("messages"));
 TestEqual(TEXT("Prior turns precede current input"),Messages.Num(),4);
 TestEqual(TEXT("History player remains user"),Messages[1]->AsObject()->GetStringField(TEXT("role")),FString(TEXT("user")));
 TestEqual(TEXT("History character remains assistant"),Messages[2]->AsObject()->GetStringField(TEXT("role")),FString(TEXT("assistant")));
 TestEqual(TEXT("Current input is last"),Messages[3]->AsObject()->GetStringField(TEXT("content")),J.PlayerMessage);
 const FString System=Messages[0]->AsObject()->GetStringField(TEXT("content"));
 TestTrue(TEXT("Shared name, world and grounded note present"),System.Contains(TEXT("小林"))&&System.Contains(TEXT("校园窗边"))&&System.Contains(TEXT("柠檬茶"))&&System.Contains(TEXT("有自己的观点")));
 const FString Event=WardrobeAIEvent::BuildPlanRequest(J,TEXT("校园小房间"),5);
 TestTrue(TEXT("Event shares conversation and notes"),Event.Contains(Reply.Text)&&Event.Contains(TEXT("柠檬茶"))&&Event.Contains(TEXT("小林")));
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCompanionParseTest,"DreamBloom.Companion.StructuredReplyEvidenceAndIdleDedup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCompanionParseTest::RunTest(const FString&){
 FWardrobeSpeechJob J;J.bRemember=true;J.PlayerMessage=TEXT("我喜欢柠檬茶，不喜欢太甜。");J.Emotions=GetDefault<UWardrobeAISpeechData>()->Emotions;FString Error;
 const FString Raw=TEXT("{\"text\":\"那就给你留一杯不太甜的。\",\"emotion\":\"温柔\",\"remember\":[\"我喜欢柠檬茶\",\"我们昨天一起看电影\"]}");
 TestTrue(TEXT("New JSON accepted"),WardrobeAISpeech::ParseCompanionReply(CompanionEnvelope(Raw),J,Error));
 TestEqual(TEXT("Only proven quote retained"),J.VerifiedNotes.Num(),1);TestEqual(TEXT("Correct expression"),J.Emotion,FName(TEXT("温柔")));
 TestEqual(TEXT("Voice only receives dialogue"),WardrobeAISpeech::BuildVoiceRequest(J)->GetStringField(TEXT("text")),J.Text);
 TestFalse(TEXT("Broken JSON rejected rather than read aloud"),WardrobeAISpeech::ParseCompanionReply(CompanionEnvelope(TEXT("{\"text\":12}")),J,Error));
 TestTrue(TEXT("Legacy provider is compatible"),WardrobeAISpeech::ParseCompanionReply(CompanionEnvelope(TEXT("【开心】好呀。")),J,Error));
 J.PlayerMessage.Reset();FWardrobeCompanionTurn T;T.Text=TEXT("好呀！");J.Memory.Turns.Add(T);
 TestFalse(TEXT("Repeated idle line with punctuation variation skipped"),WardrobeAISpeech::ParseCompanionReply(CompanionEnvelope(TEXT("【开心】好呀。")),J,Error));
 TestTrue(TEXT("Skipped line has no stale payload"),J.Text.IsEmpty()&&J.VerifiedNotes.IsEmpty());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCompanionSaveTest,"DreamBloom.Companion.BoundedPersistenceAndCampaignIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCompanionSaveTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();auto* S=GI->GetSubsystem<UWardrobeStorySubsystem>();
 auto* Flow=NewObject<UWardrobeStoryFlow>(GI);Flow->SaveNamespace=TEXT("CompanionQA_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);S->Flow=Flow;S->ActiveSlot=0;S->Save=NewObject<UWardrobeStorySave>(S);
 const FString Slot=S->SlotName(0);const uint64 Revision=S->GetCampaignRevision();
 ON_SCOPE_EXIT{S->FlushPendingSaves();S->Save=nullptr;UGameplayStatics::DeleteGameInSlot(Slot,0);GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 S->Save->FormatVersion=1;S->Save->PlayerName=TEXT("记忆测试旅人");S->Save->NextStoryId=TEXT("Scene2");S->Save->PlayerTransform=FTransform(FVector(4,5,6));
 TestTrue(TEXT("Old save starts with empty memory"),S->GetCompanionMemory(TEXT("Daniya")).Turns.IsEmpty());
 const FString Session=S->GetCompanionSession(TEXT("Daniya"));
 TestFalse(TEXT("Old saves receive a stable external identity"),Session.IsEmpty());
 TestEqual(TEXT("External identity remains stable between requests"),S->GetCompanionSession(TEXT("Daniya")),Session);
 TestNotEqual(TEXT("Another character uses another external identity"),S->GetCompanionSession(TEXT("Other")),Session);
 TestTrue(TEXT("Completed reply can be remembered"),S->CommitCompanionDialogue(TEXT("Daniya"),Revision,TEXT("我喜欢柠檬茶"),TEXT("下次试试不加糖。"),TEXT("温柔"),{TEXT("我喜欢柠檬茶"),TEXT("没有证据")},4,8));
 TestEqual(TEXT("Defense rechecks evidence at save boundary"),S->GetCompanionMemory(TEXT("Daniya")).Notes.Num(),1);
 TestFalse(TEXT("Other campaign completion rejected"),S->CommitCompanionDialogue(TEXT("Daniya"),Revision+1,TEXT("错误"),TEXT("不该保存"),TEXT("平静"),{},4,8));
 for(int32 I=0;I<80;++I){const FString Text=TEXT("我喜欢茶")+FString::FromInt(I);S->CommitCompanionDialogue(TEXT("Daniya"),Revision,Text,TEXT("我记住了。"),TEXT("温柔"),{Text},4,8);}
 S->FlushPendingSaves();auto* Loaded=Cast<UWardrobeStorySave>(UGameplayStatics::LoadGameFromSlot(Slot,0));
 TestTrue(TEXT("Newest bounded snapshot reaches disk"),Loaded&&Loaded->FormatVersion==3&&Loaded->CompanionMemories.FindRef(TEXT("Daniya")).Turns.Num()==4&&Loaded->CompanionMemories.FindRef(TEXT("Daniya")).Notes.Num()==8);
 TestTrue(TEXT("External identity is persisted with the campaign"),Loaded&&Loaded->CompanionCampaignId==S->Save->CompanionCampaignId&&Loaded->CompanionCampaignId.IsValid());
 TestTrue(TEXT("Story name, progress and transform remain"),Loaded&&Loaded->PlayerName==TEXT("记忆测试旅人")&&Loaded->NextStoryId==TEXT("Scene2")&&Loaded->PlayerTransform.GetLocation().Equals(FVector(4,5,6)));
 TestTrue(TEXT("Different character is isolated"),S->GetCompanionMemory(TEXT("Other")).Turns.IsEmpty());
 TestTrue(TEXT("Explicit memory reset works"),S->ForgetCompanionMemory(TEXT("Daniya")));S->FlushPendingSaves();
 TestTrue(TEXT("Reset persists without deleting story"),S->GetCompanionMemory(TEXT("Daniya")).Turns.IsEmpty()&&S->Save->NextStoryId==TEXT("Scene2"));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCompanionPlaybackTest,"DreamBloom.Companion.OnlyCompletedPlaybackAndQuietCooldown",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCompanionPlaybackTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>();
 auto* Flow=NewObject<UWardrobeStoryFlow>(GI);Flow->SaveNamespace=TEXT("CompanionPlaybackQA_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);Story->Flow=Flow;Story->ActiveSlot=0;Story->Save=NewObject<UWardrobeStorySave>(Story);const FString Slot=Story->SlotName(0);
 auto* Person=W->SpawnActor<AWardrobeShowcase>();auto* Data=NewObject<UWardrobeCharacterData>();Data->Characters.SetNum(1);auto* P=NewObject<UWardrobeAISpeechData>();Data->Characters[0].AISpeech=P;P->bAutoStart=false;P->MinInterval=10;P->MaxInterval=10;
 Person->InteractionData=Data;Person->InteractionProfileIndex=0;auto* Speech=Person->AISpeech.Get();
 ON_SCOPE_EXIT{Speech->CancelSpeech();Story->FlushPendingSaves();Story->Save=nullptr;UGameplayStatics::DeleteGameInSlot(Slot,0);GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);};
 auto MakeClip=[&]{auto C=MakeShared<FWardrobeSpeechClip,ESPMode::ThreadSafe>();C->Text=TEXT("那我陪你待一会儿。");C->bRemember=true;C->CharacterId=P->CharacterId;C->CampaignRevision=Story->GetCampaignRevision();return C;};
 Speech->Clip=MakeClip();Speech->ReleaseSpeech(false);TestTrue(TEXT("Cancelled or unchosen preparation is not remembered"),Story->GetCompanionMemory(P->CharacterId).Turns.IsEmpty());
 Speech->Clip=MakeClip();Speech->ReleaseSpeech(true);TestEqual(TEXT("Natural finish retained once"),Story->GetCompanionMemory(P->CharacterId).Turns.Num(),1);
 const double Now=FPlatformTime::Seconds();TestTrue(TEXT("Unanswered automatic line lengthens interval"),Speech->NextRequestTime-Now>14.8&&Speech->NextRequestTime-Now<15.1);
 auto Reply=MakeClip();Reply->PlayerText=TEXT("我有点累。");Speech->Clip=Reply;Speech->ReleaseSpeech(true);
 TestEqual(TEXT("Manual exchange resets quiet backoff"),Speech->UnansweredLines,0);TestTrue(TEXT("Transient clip always released"),!Speech->Clip&&Speech->GetTemporaryBytes()==0);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCompanionExpressionTest,"DreamBloom.Companion.SmoothSpeechAccentsAndRelease",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCompanionExpressionTest::RunTest(const FString&){
 FWardrobeSpeechJob J;J.Emotion=TEXT("开心");J.Emotions=GetDefault<UWardrobeAISpeechData>()->Emotions;J.ExpressionIntensity=1;
 FWardrobeSpeechClip C;C.PCM.Init(3000,48000);C.SampleRate=16000;C.Frames=151;C.Names={TEXT("あ")};C.Values.Init(.3f,151);
 FWardrobeSpeechMorph Vowel;Vowel.Control=TEXT("Daniya.A");Vowel.Morph=TEXT("あ");J.Morphs.Add(Vowel);WardrobeAISpeech::ApplyEmotion(C,J);
 const int32 A=C.Names.IndexOfByKey(TEXT("あ")),Smile=C.Names.IndexOfByKey(TEXT("にこり")),Count=C.Names.Num();
 TestTrue(TEXT("Expression fades in and fades out"),C.Values[Smile]<.01f&&C.Values[50*Count+Smile]>.4f&&C.Values[150*Count+Smile]<.01f);
 for(int32 F=0;F<C.Frames;++F){TestEqual(TEXT("No lip-sync loss"),C.Values[F*Count+A],.3f);if(F)TestTrue(TEXT("No frame-random expression jump"),FMath::Abs(C.Values[F*Count+Smile]-C.Values[(F-1)*Count+Smile])<.08f);}
 return true;
}
#endif
