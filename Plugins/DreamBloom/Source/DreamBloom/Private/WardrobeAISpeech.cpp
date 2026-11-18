#include "WardrobeAISpeech.h"
#include "WardrobeStartup.h"
#include "WardrobeStory.h"
#include "WardrobeAISpeechTypes.h"
#include "WardrobeAISpeechSubsystem.h"
#include "WardrobeAISpeechWave.h"
#include "WardrobeSpeechBubble.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeLoading.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Blueprint/UserWidget.h"

namespace {
TArray<FWardrobeSpeechEmotion> DefaultSpeechEmotions(){
 TArray<FWardrobeSpeechEmotion> Result;Result.Reserve(36);
 auto AddEmotion=[&](const TCHAR* Name,const TArray<TPair<FName,float>>& Shapes){
  FWardrobeSpeechEmotion E;E.Emotion=Name;
  for(const auto& Shape:Shapes){FWardrobeSpeechEmotionMorph M;M.Morph=Shape.Key;M.Weight=Shape.Value;E.Morphs.Add(M);}Result.Add(E);
 };
 AddEmotion(TEXT("平静"),{{TEXT("口角上げ"),0.12f}});
 AddEmotion(TEXT("温柔"),{{TEXT("にこり"),0.22f},{TEXT("垂れ目"),0.18f},{TEXT("口角上げ"),0.32f}});
 AddEmotion(TEXT("安心"),{{TEXT("困る"),0.10f},{TEXT("なごみ"),0.25f},{TEXT("口角上げ"),0.24f}});
 AddEmotion(TEXT("专注"),{{TEXT("真面目"),0.28f},{TEXT("ｷﾘｯ"),0.22f},{TEXT("口角上げ"),0.06f}});
 AddEmotion(TEXT("开心"),{{TEXT("にこり"),0.55f},{TEXT("笑い目"),0.30f},{TEXT("口角上げ"),0.55f}});
 AddEmotion(TEXT("开心微笑"),{{TEXT("にこり"),0.28f},{TEXT("笑い目"),0.08f},{TEXT("口角上げ"),0.30f}});
 AddEmotion(TEXT("开心灿烂"),{{TEXT("にこり"),0.72f},{TEXT("笑い目"),0.62f},{TEXT("口角上げ"),0.78f}});
 AddEmotion(TEXT("开心俏皮"),{{TEXT("にこり"),0.32f},{TEXT("ウィンク右"),0.70f},{TEXT("左上"),0.16f},{TEXT("右口角上げ"),0.60f},{TEXT("左口角上げ"),0.32f}});
 AddEmotion(TEXT("害羞"),{{TEXT("困る"),0.25f},{TEXT("垂れ目"),0.30f},{TEXT("口角上げ"),0.30f}});
 AddEmotion(TEXT("害羞轻笑"),{{TEXT("困る"),0.18f},{TEXT("にこり"),0.30f},{TEXT("笑い目"),0.18f},{TEXT("口角上げ"),0.38f}});
 AddEmotion(TEXT("害羞紧张"),{{TEXT("困る"),0.45f},{TEXT("右上"),0.18f},{TEXT("左上"),0.18f},{TEXT("垂れ目"),0.12f},{TEXT("口角上げ"),0.10f}});
 AddEmotion(TEXT("害羞偷笑"),{{TEXT("右困る"),0.18f},{TEXT("左にこり"),0.38f},{TEXT("じと目"),0.18f},{TEXT("左口角上げ"),0.45f},{TEXT("右口角上げ"),0.20f}});
 AddEmotion(TEXT("疑惑"),{{TEXT("怪訝右"),0.55f},{TEXT("左上"),0.30f},{TEXT("じと目"),0.15f}});
 AddEmotion(TEXT("疑惑思索"),{{TEXT("怪訝左"),0.48f},{TEXT("右上"),0.20f},{TEXT("じと目"),0.28f},{TEXT("口角下げ"),0.08f}});
 AddEmotion(TEXT("疑惑好奇"),{{TEXT("上"),0.30f},{TEXT("怪訝右"),0.22f},{TEXT("びっくり"),0.18f},{TEXT("口角上げ"),0.15f}});
 AddEmotion(TEXT("无奈"),{{TEXT("困る"),0.35f},{TEXT("じと目"),0.42f},{TEXT("右口角上げ"),0.12f},{TEXT("左口角下げ"),0.12f}});
 AddEmotion(TEXT("惊讶"),{{TEXT("上"),0.55f},{TEXT("びっくり"),0.45f},{TEXT("瞳小"),0.15f}});
 AddEmotion(TEXT("惊喜"),{{TEXT("上"),0.35f},{TEXT("にこり"),0.30f},{TEXT("びっくり"),0.35f},{TEXT("口角上げ"),0.60f}});
 AddEmotion(TEXT("震惊"),{{TEXT("上"),0.70f},{TEXT("びっくり"),0.70f},{TEXT("瞳小"),0.28f},{TEXT("口角下げ"),0.12f}});
 AddEmotion(TEXT("难过"),{{TEXT("困る"),0.65f},{TEXT("垂れ目"),0.30f},{TEXT("口角下げ"),0.30f}});
 AddEmotion(TEXT("委屈"),{{TEXT("困る"),0.78f},{TEXT("垂れ目"),0.42f},{TEXT("口角下げ"),0.38f}});
 AddEmotion(TEXT("失落"),{{TEXT("困る"),0.38f},{TEXT("じと目"),0.36f},{TEXT("垂れ目"),0.18f},{TEXT("口角下げ"),0.25f}});
 AddEmotion(TEXT("担心"),{{TEXT("困る"),0.45f},{TEXT("上"),0.18f},{TEXT("びっくり"),0.16f},{TEXT("口角下げ"),0.14f}});
 AddEmotion(TEXT("不满"),{{TEXT("怒り"),0.55f},{TEXT("じと目"),0.25f},{TEXT("口角下げ"),0.20f}});
 AddEmotion(TEXT("嗔怪"),{{TEXT("右怒り"),0.25f},{TEXT("怪訝左"),0.25f},{TEXT("じと目"),0.26f},{TEXT("右口角上げ"),0.15f},{TEXT("左口角下げ"),0.12f}});
 AddEmotion(TEXT("生气"),{{TEXT("怒り"),0.78f},{TEXT("ｷﾘｯ"),0.38f},{TEXT("口角下げ"),0.38f}});
 AddEmotion(TEXT("严肃"),{{TEXT("真面目"),0.55f},{TEXT("ｷﾘｯ"),0.32f},{TEXT("口角下げ"),0.08f}});
 AddEmotion(TEXT("期待"),{{TEXT("上"),0.24f},{TEXT("びっくり"),0.16f},{TEXT("口角上げ"),0.35f},{TEXT("にこり"),0.18f}});
 AddEmotion(TEXT("期待雀跃"),{{TEXT("上"),0.30f},{TEXT("にこり"),0.48f},{TEXT("びっくり"),0.26f},{TEXT("口角上げ"),0.62f}});
 AddEmotion(TEXT("自信"),{{TEXT("真面目"),0.22f},{TEXT("ｷﾘｯ"),0.20f},{TEXT("口角上げ"),0.40f}});
 AddEmotion(TEXT("自信得意"),{{TEXT("ｷﾘｯ"),0.20f},{TEXT("じと目"),0.25f},{TEXT("右口角上げ"),0.58f},{TEXT("左口角上げ"),0.20f},{TEXT("左上"),0.15f}});
 AddEmotion(TEXT("认真"),{{TEXT("真面目"),0.40f},{TEXT("びっくり"),0.10f},{TEXT("口角上げ"),0.08f}});
 AddEmotion(TEXT("安慰"),{{TEXT("困る"),0.30f},{TEXT("垂れ目"),0.22f},{TEXT("にこり"),0.18f},{TEXT("口角上げ"),0.35f}});
 AddEmotion(TEXT("困倦"),{{TEXT("じと目"),0.62f},{TEXT("垂れ目"),0.20f},{TEXT("困る"),0.12f},{TEXT("口角下げ"),0.06f}});
 AddEmotion(TEXT("疲惫"),{{TEXT("困る"),0.30f},{TEXT("じと目"),0.48f},{TEXT("口角下げ"),0.20f}});
 AddEmotion(TEXT("赞许"),{{TEXT("にこり"),0.35f},{TEXT("右上"),0.12f},{TEXT("笑い目"),0.18f},{TEXT("口角上げ"),0.44f}});
 return Result;
}
}

UWardrobeAISpeechData::UWardrobeAISpeechData(){
 EventBackground=TEXT("放学后的校园。玩家与女孩待在校园活动楼的一间小房间里，暂时没有事情要做。窗外偶尔传来操场上的声音，房间里有书桌、两把椅子、书包和几本没看完的书。女孩与玩家已经熟悉，正在一起发呆。围绕校园小事、今天的经历、窗外风景和彼此的想法，自然发生短小互动。保持轻松、细腻的日常氛围，不擅自更换场景或增加重要人物。");
 Persona=TEXT("你是达妮娅，一位有自己想法的校园女孩。说话温柔但不一味讨好；会害羞、好奇、偶尔吐槽，也可以认真表达不同意见。把玩家当成已经熟悉的同学，保持角色身份。");
 WorldContext=TEXT("放学后，玩家陪你在校园活动楼的小房间休息。窗边有书桌、书包和几本书，外面偶尔有操场上的声音。关系从熟悉的同学自然发展，记住真正聊过的事情。没有证据时，不编造玩家经历、以前的约定或离开画面后发生的大事。");
 SpeakingStyle=TEXT("像当面说话：先接住玩家的具体意思，再表达自己的小想法。句子可以有长短、迟疑、轻微停顿，适度使用口语，避免每句话都称呼名字、问问题或夸奖玩家。不用客服腔，不说‘作为AI’，不输出括号动作或大段旁白。自动闲聊可以接着上个话题或提眼前的小发现；不反复问今天心情如何。允许安静陪伴，不逼玩家回答。");
 FWardrobeSpeechTopic T;T.Scene=TEXT("宁静的角色展示房间");T.Topic=TEXT("今天的心情和眼前的风景");T.Tone=TEXT("温柔，像和熟悉的朋友闲聊");Topics.Add(T);
 const TCHAR* Controls[]={TEXT("Daniya.A"),TEXT("Daniya.I"),TEXT("Daniya.U"),TEXT("Daniya.E"),TEXT("Daniya.O"),TEXT("Daniya.Blink")};
 const TCHAR* Names[]={TEXT("あ"),TEXT("い"),TEXT("う"),TEXT("え"),TEXT("お"),TEXT("まばたき")};
 for(int32 I=0;I<6;++I){FWardrobeSpeechMorph M;M.Control=Controls[I];M.Morph=Names[I];Morphs.Add(M);}
 Emotions=DefaultSpeechEmotions();
}
void UWardrobeAISpeechData::AddMissingDefaultEmotions(){
 const auto Defaults=DefaultSpeechEmotions();bool Changed=false;
 for(const auto& E:Defaults)if(!Emotions.ContainsByPredicate([&](const auto& Existing){return Existing.Emotion==E.Emotion;})){
#if WITH_EDITOR
  if(!Changed)Modify();
#endif
  Emotions.Add(E);Changed=true;
 }
#if WITH_EDITOR
 if(Changed)MarkPackageDirty();
#endif
}
UWardrobeAISpeechComponent::UWardrobeAISpeechComponent(){PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PrePhysics;}
void UWardrobeAISpeechComponent::BeginPlay(){Super::BeginPlay();if(auto* S=Cast<AWardrobeShowcase>(GetOwner()))if(S->Character)S->Character->AddTickPrerequisiteComponent(this);ScheduleNext();}
void UWardrobeAISpeechComponent::EndPlay(const EEndPlayReason::Type R){CancelSpeech();Clip.Reset();FaceNames.Empty();FaceValues.Empty();Super::EndPlay(R);}
const UWardrobeAISpeechData* UWardrobeAISpeechComponent::GetProfile() const{
 const auto* S=Cast<AWardrobeShowcase>(GetOwner());const auto* P=S?S->GetInteractionProfile():nullptr;return P?P->AISpeech.Get():nullptr;
}
bool UWardrobeAISpeechComponent::IsIdleAllowed() const{
 if(GetWorld()&&GetWorld()->GetGameInstance())if(auto* Startup=GetWorld()->GetGameInstance()->GetSubsystem<UWardrobeStartupSubsystem>())if(Startup->IsActive())return false;
 const auto* S=Cast<AWardrobeShowcase>(GetOwner());const auto* P=GetProfile();const auto* W=GetWorld();
 if(!S||!P||!P->bEnabled||!W||!W->IsGameWorld()||W->IsPaused()||S->IsHidden()||!S->IsInteractionEnabled()||S->IsInteracting()||S->IsPinching()||S->HasLimbPose()||S->IsPhysicsDragging()||S->IsReleaseAnimationPlaying()||S->IsSequenceSwitching())return false;
 if(auto* G=W->GetGameInstance())if(auto* L=G->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return false;
 return true;
}
void UWardrobeAISpeechComponent::ScheduleNext(){
 const auto* P=GetProfile();const float Min=P&&FMath::IsFinite(P->MinInterval)?FMath::Max(1.f,P->MinInterval):10.f;
 const float Max=P&&FMath::IsFinite(P->MaxInterval)?FMath::Max(Min,P->MaxInterval):30.f;
 const float Scale=P&&FMath::IsFinite(P->UnansweredCooldownScale)?FMath::Clamp(P->UnansweredCooldownScale,1.f,3.f):1.5f;
 NextRequestTime=FPlatformTime::Seconds()+FMath::FRandRange(Min,Max)*FMath::Pow(Scale,FMath::Clamp(UnansweredLines,0,3));
}
bool UWardrobeAISpeechComponent::RequestDialogueNow(){return Start(nullptr);}
bool UWardrobeAISpeechComponent::RequestReply(const FString& Message){const FString Clean=Message.TrimStartAndEnd();return !Clean.IsEmpty()&&Clean.Len()<=MaxPlayerMessageCharacters&&Start(nullptr,&Clean);}
void UWardrobeAISpeechComponent::SetAutomaticPaused(bool Paused){
 if(bAutomaticPaused==Paused)return;bAutomaticPaused=Paused;
 if(Paused)CancelSpeech();else ScheduleNext();
}
bool UWardrobeAISpeechComponent::IsBusy() const{return State==EWardrobeSpeechState::Playing||State==EWardrobeSpeechState::Fading||State==EWardrobeSpeechState::RequestingText||State==EWardrobeSpeechState::GeneratingVoice||State==EWardrobeSpeechState::SolvingFace;}
bool UWardrobeAISpeechComponent::SpeakText(const FString& Text){const FString Clean=Text.TrimStartAndEnd();return !Clean.IsEmpty()&&Start(&Clean);}
bool UWardrobeAISpeechComponent::Start(const FString* Text,const FString* PlayerMessage){
 if(!IsIdleAllowed()||IsBusy()||bEventReserved)return false;
 auto* W=GetWorld();auto* G=W?W->GetGameInstance():nullptr;auto* S=G?G->GetSubsystem<UWardrobeAISpeechSubsystem>():nullptr;const auto* P=GetProfile();if(!S||!P||S->IsBusy())return false;
 LastProfile=const_cast<UWardrobeAISpeechData*>(P);
 FWardrobeSpeechJob J;if(!BuildPreparationJob(J))return false;J.Ticket=++Generation;
 LastCampaignRevision=J.CampaignRevision;LastCharacterId=P->CharacterId;
 if(Text){J.bRemember=false;FString Error;if(!WardrobeAISpeech::CleanDialogue(*Text,J.MaxCharacters,J.Emotions,J.Text,J.Emotion,Error))return false;}
 if(PlayerMessage)J.PlayerMessage=*PlayerMessage;
 if(!P->Topics.IsEmpty()){int32 I=FMath::RandRange(0,P->Topics.Num()-1);if(P->Topics.Num()>1&&I==PreviousTopic)I=(I+FMath::RandRange(1,P->Topics.Num()-1))%P->Topics.Num();PreviousTopic=I;
  J.Scene=P->Topics[I].Scene;J.Topic=P->Topics[I].Topic;J.Tone=P->Topics[I].Tone;J.Mood=P->Topics[I].Mood;}
 // Discard unsupported morphs before snapshotting the job; never mutate mesh state on the worker.
 if(auto* Owner=Cast<AWardrobeShowcase>(GetOwner()))if(Owner->Character&&Owner->Character->GetSkeletalMeshAsset()){
  auto* Mesh=Owner->Character->GetSkeletalMeshAsset();J.Morphs.RemoveAll([Mesh](const auto& M){return !Mesh->FindMorphTarget(M.Morph);});
  for(auto& E:J.Emotions)E.Morphs.RemoveAll([Mesh](const auto& M){return !Mesh->FindMorphTarget(M.Morph);});
 }
 if(J.Morphs.IsEmpty()){Fail(J.Ticket,TEXT("No configured speech morphs exist on the character mesh."));return false;}
 LastError.Reset();State=Text?EWardrobeSpeechState::GeneratingVoice:EWardrobeSpeechState::RequestingText;return S->Start(J);
}
bool UWardrobeAISpeechComponent::BuildPreparationJob(FWardrobeSpeechJob& J) const{
 check(IsInGameThread());const auto* P=GetProfile();if(!P)return false;
 J.Owner=const_cast<UWardrobeAISpeechComponent*>(this);J.Ticket=Generation;
 J.Endpoint=P->ChatEndpoint;J.ApiKey=P->ApiKey;J.Model=P->Model;J.Persona=P->Persona;
 J.TTSEndpoint=P->TTSEndpoint;J.ReferenceAudio=P->ReferenceAudio.FilePath;J.ReferenceText=P->ReferenceText;J.Language=P->Language;
 J.MaxCharacters=FMath::Clamp(P->MaxTextCharacters,5,120);J.MaxDuration=FMath::Clamp(P->MaxAudioDuration,1.f,30.f);J.Timeout=FMath::Clamp(P->RequestTimeout,5.f,120.f);
 J.ExpressionIntensity=P->ExpressionIntensity;J.Morphs=P->Morphs;J.Emotions=P->Emotions;J.PreviousEmotion=LastPlayedEmotion;
 J.MoodScore=FMath::Clamp(P->InitialMood,0,100);
 J.WorldContext=P->WorldContext.Left(4000);J.SpeakingStyle=P->SpeakingStyle.Left(2000);J.CharacterId=P->CharacterId;
 J.RecentTurnLimit=FMath::Clamp(P->RecentTurnLimit,4,32);J.MemoryNoteLimit=FMath::Clamp(P->MemoryNoteLimit,8,64);
 J.bRemember=P->bRememberConversations&&!P->CharacterId.IsNone();
 const FDateTime Now=FDateTime::Now();J.ClockContext=FString::Printf(TEXT("本机日期时间：%04d-%02d-%02d %02d:%02d；当前画面场景：%s。只作时间和场景参考，不据此编造亲历。"),Now.GetYear(),Now.GetMonth(),Now.GetDay(),Now.GetHour(),Now.GetMinute(),GetWorld()?*GetWorld()->GetMapName():TEXT("角色展示"));
 if(auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr)if(auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>()){
  J.MoodScore=Story->GetCharacterMood(P->CharacterId,P->InitialMood);J.RecentEvents=Story->GetCharacterEventHistory(P->CharacterId);
  J.CampaignRevision=Story->GetCampaignRevision();J.PlayerName=Story->GetPlayerName();
  if(P->bUseAstrBot){J.AstrBotSession=Story->GetCompanionSession(P->CharacterId);if(!J.AstrBotSession.IsEmpty()){J.Upstream=P->ChatEndpoint;J.Endpoint=TEXT("http://127.0.0.1:6186/v1/chat/completions");}}
  if(J.bRemember)J.Memory=Story->GetCompanionMemory(P->CharacterId,J.RecentTurnLimit,J.MemoryNoteLimit);
 }
 if(auto* Owner=Cast<AWardrobeShowcase>(GetOwner()))if(Owner->Character&&Owner->Character->GetSkeletalMeshAsset()){
  auto* Mesh=Owner->Character->GetSkeletalMeshAsset();J.Morphs.RemoveAll([Mesh](const auto& M){return !Mesh->FindMorphTarget(M.Morph);});
  for(auto& E:J.Emotions)E.Morphs.RemoveAll([Mesh](const auto& M){return !Mesh->FindMorphTarget(M.Morph);});
 }
 return !J.Morphs.IsEmpty();
}
void UWardrobeAISpeechComponent::SetEventReserved(bool Reserved){
 bEventReserved=Reserved;if(Reserved)LastProfile=const_cast<UWardrobeAISpeechData*>(GetProfile());else ScheduleNext();
}
bool UWardrobeAISpeechComponent::PlayPreparedClip(TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Prepared){
 if(!Prepared||IsBusy()||!IsIdleAllowed())return false;
 LastProfile=const_cast<UWardrobeAISpeechData*>(GetProfile());LastError.Reset();AcceptClip(++Generation,MoveTemp(Prepared));return State==EWardrobeSpeechState::Playing;
}
void UWardrobeAISpeechComponent::SetStage(uint64 T,EWardrobeSpeechState S){if(T==Generation)State=S;}
void UWardrobeAISpeechComponent::Fail(uint64 T,const FString& E){if(T!=Generation)return;LastError=E;CurrentText.Reset();CurrentEmotion=NAME_None;State=EWardrobeSpeechState::Error;ScheduleNext();NextRequestTime=FMath::Max(NextRequestTime,FPlatformTime::Seconds()+30.);UE_LOG(LogTemp,Warning,TEXT("Wardrobe AI Speech: %s"),*E);}
void UWardrobeAISpeechComponent::AcceptClip(uint64 T,TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> C){
 if(T!=Generation||!IsIdleAllowed()||!C||C->PCM.IsEmpty())return;
 Clip=MoveTemp(C);CurrentText=Clip->Text;CurrentEmotion=Clip->Emotion;LastPlayedEmotion=CurrentEmotion;FaceNames=Clip->Names;FaceValues.Init(0,FaceNames.Num());FaceWeight=0;
 Wave=NewObject<UWardrobeAISpeechWave>(this);Wave->Initialize(Clip);
 Audio=NewObject<UAudioComponent>(GetOwner());Audio->bAutoActivate=false;Audio->bAutoDestroy=false;Audio->bIsUISound=true;Audio->bAllowSpatialization=false;Audio->SetSound(Wave);Audio->RegisterComponentWithWorld(GetWorld());Audio->OnAudioFinished.AddDynamic(this,&UWardrobeAISpeechComponent::OnAudioFinished);
 const auto* Profile=GetProfile();Audio->SetVolumeMultiplier(Profile&&FMath::IsFinite(Profile->VoiceVolume)?FMath::Clamp(Profile->VoiceVolume,0.f,100.f):1.f);
 State=EWardrobeSpeechState::Playing;StartedAt=FPlatformTime::Seconds();PlaybackTime=0;Audio->OnAudioPlaybackPercent.AddDynamic(this,&UWardrobeAISpeechComponent::OnPlaybackPercent);Audio->Play();
 if(auto* PC=UGameplayStatics::GetPlayerController(this,0))if(PC->IsLocalController()){
  if(!SpeechBubble)SpeechBubble=CreateWidget<UWardrobeSpeechBubble>(PC);
  if(SpeechBubble){SpeechBubble->ShowCaption(Cast<AWardrobeShowcase>(GetOwner()),CurrentText);if(!SpeechBubble->IsInViewport())SpeechBubble->AddToPlayerScreen(90);}
 }
}
void UWardrobeAISpeechComponent::OnAudioFinished(){if(State==EWardrobeSpeechState::Playing)ReleaseSpeech(true);}
void UWardrobeAISpeechComponent::OnPlaybackPercent(const USoundWave* Sound,float Percent){if(State==EWardrobeSpeechState::Playing&&Clip&&Sound==Wave&&FMath::IsFinite(Percent))PlaybackTime=FMath::Clamp(Percent,0.f,1.f)*Clip->Duration();}
void UWardrobeAISpeechComponent::CancelSpeech(){
 ReleaseSpeech(false);
}
void UWardrobeAISpeechComponent::ReleaseSpeech(bool NaturalFinish){
 if(SpeechBubble)SpeechBubble->FinishCaption(!NaturalFinish);
 if(NaturalFinish&&Clip&&Clip->bRemember){
  const auto* P=GetProfile();if(P&&P->bRememberConversations&&P->CharacterId==Clip->CharacterId){
   if(auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr)if(auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>()){
    if(Story->CommitCompanionDialogue(Clip->CharacterId,Clip->CampaignRevision,Clip->PlayerText,Clip->Text,Clip->Emotion,Clip->VerifiedNotes,Clip->RecentTurnLimit,Clip->MemoryNoteLimit)&&!Clip->AstrBotSession.IsEmpty()&&Clip->AstrBotSession==Story->GetCompanionSession(Clip->CharacterId))
     if(auto* Service=GI->GetSubsystem<UWardrobeAISpeechSubsystem>())Service->CommitAstrBotClip(*Clip);
   }
   UnansweredLines=Clip->PlayerText.IsEmpty()?FMath::Min(3,UnansweredLines+1):0;
  }
 }
 ++Generation;if(auto* G=GetWorld()?GetWorld()->GetGameInstance():nullptr)if(auto* S=G->GetSubsystem<UWardrobeAISpeechSubsystem>())S->Cancel(this);
 if(Audio){Audio->OnAudioFinished.RemoveAll(this);Audio->OnAudioPlaybackPercent.RemoveAll(this);Audio->Stop();Audio->DestroyComponent();Audio=nullptr;}if(Wave)Wave->ReleasePCM();Wave=nullptr;Clip.Reset();CurrentText.Reset();CurrentEmotion=NAME_None;
 FadeAge=0;FadeFrom=FaceWeight;State=FaceWeight>0?EWardrobeSpeechState::Fading:EWardrobeSpeechState::Idle;ScheduleNext();
}
int32 UWardrobeAISpeechComponent::GetTemporaryBytes() const{return Clip?int32(FMath::Min<int64>(Clip->Bytes(),MAX_int32)):0;}
void UWardrobeAISpeechComponent::GetFaceSnapshot(TArray<FName>& N,TArray<float>& V,float& W) const{N=FaceNames;V=FaceValues;W=FaceWeight;}
void UWardrobeAISpeechComponent::TickComponent(float Dt,ELevelTick T,FActorComponentTickFunction* F){
 Super::TickComponent(Dt,T,F);auto* P=const_cast<UWardrobeAISpeechData*>(GetProfile());
 auto* GI=GetWorld()?GetWorld()->GetGameInstance():nullptr;auto* Story=GI?GI->GetSubsystem<UWardrobeStorySubsystem>():nullptr;
 const uint64 Revision=Story?Story->GetCampaignRevision():0;const FName Id=P?P->CharacterId:NAME_None;
 if(LastCampaignRevision==MAX_uint64){LastCampaignRevision=Revision;LastCharacterId=Id;}
 else if(LastCampaignRevision!=Revision||LastCharacterId!=Id){CancelSpeech();LastCampaignRevision=Revision;LastCharacterId=Id;UnansweredLines=0;LastPlayedEmotion=NAME_None;PreviousTopic=INDEX_NONE;ScheduleNext();}
 if(LastProfile.Get()!=P){CancelSpeech();LastProfile=P;State=EWardrobeSpeechState::Disabled;ScheduleNext();}
 if(P&&GetWorld()&&GetWorld()->GetGameInstance())if(const auto* Owner=Cast<AWardrobeShowcase>(GetOwner()))if(Owner->IsInteractionEnabled())if(auto* Service=GetWorld()->GetGameInstance()->GetSubsystem<UWardrobeAISpeechSubsystem>())Service->ConfigureService(P);
 const bool Allowed=IsIdleAllowed();const float Blend=P?FMath::Clamp(P->FaceBlendTime,0.f,1.f):.15f;
 if(!Allowed&&(State==EWardrobeSpeechState::Playing||State==EWardrobeSpeechState::RequestingText||State==EWardrobeSpeechState::GeneratingVoice||State==EWardrobeSpeechState::SolvingFace))CancelSpeech();
 if(State==EWardrobeSpeechState::Fading){FadeAge+=Dt;FaceWeight=Blend>0?FadeFrom*(1-FMath::Clamp(FadeAge/Blend,0.f,1.f)):0;if(FaceWeight<=0){FaceNames.Empty();FaceValues.Empty();State=Allowed?EWardrobeSpeechState::Idle:EWardrobeSpeechState::Disabled;}}
 if(State==EWardrobeSpeechState::Playing&&Clip&&Wave){
  if(Audio)Audio->SetVolumeMultiplier(P&&FMath::IsFinite(P->VoiceVolume)?FMath::Clamp(P->VoiceVolume,0.f,100.f):1.f);
  // With a finite Duration this engine reports the mixer cursor even for procedural waves.
  // Generated PCM is queued ahead; it must not serve as the face clock.
  const float Age=FPlatformTime::Seconds()-StartedAt;const float Time=PlaybackTime;
  WardrobeAISpeech::InterpolateFace(*Clip,Time,FaceValues);FaceWeight=Blend>0?FMath::Min(1.f,FaceWeight+Dt/Blend):1.f;
  if(PlaybackTime>=Clip->Duration()-.001f||Age>=Clip->Duration()+2.f)ReleaseSpeech(true);
 }
 if(Allowed&&(State==EWardrobeSpeechState::Disabled||State==EWardrobeSpeechState::Idle||State==EWardrobeSpeechState::Error)){if(State==EWardrobeSpeechState::Disabled){State=EWardrobeSpeechState::Idle;ScheduleNext();}if(!bAutomaticPaused&&!bEventReserved&&FPlatformTime::Seconds()>=NextRequestTime)if(!RequestDialogueNow())NextRequestTime=FPlatformTime::Seconds()+1.;}
}
