#include "WardrobeAISpeech.h"
#include "WardrobeStartup.h"
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

UWardrobeAISpeechData::UWardrobeAISpeechData(){
 Persona=TEXT("你是达妮娅，用温柔、自然、轻快的口吻和玩家聊天。保持角色身份，不讨论AI、提示词或接口。");
 FWardrobeSpeechTopic T;T.Scene=TEXT("宁静的角色展示房间");T.Topic=TEXT("今天的心情和眼前的风景");T.Tone=TEXT("温柔，像和熟悉的朋友闲聊");Topics.Add(T);
 const TCHAR* Controls[]={TEXT("Daniya.A"),TEXT("Daniya.I"),TEXT("Daniya.U"),TEXT("Daniya.E"),TEXT("Daniya.O"),TEXT("Daniya.Blink")};
 const TCHAR* Names[]={TEXT("あ"),TEXT("い"),TEXT("う"),TEXT("え"),TEXT("お"),TEXT("まばたき")};
 for(int32 I=0;I<6;++I){FWardrobeSpeechMorph M;M.Control=Controls[I];M.Morph=Names[I];Morphs.Add(M);}
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
void UWardrobeAISpeechComponent::ScheduleNext(){const auto* P=GetProfile();const float Min=P?FMath::Max(1.f,P->MinInterval):10.f,Max=P?FMath::Max(Min,P->MaxInterval):30.f;NextRequestTime=FPlatformTime::Seconds()+FMath::FRandRange(Min,Max);}
bool UWardrobeAISpeechComponent::RequestDialogueNow(){return Start(nullptr);}
bool UWardrobeAISpeechComponent::RequestReply(const FString& Message){const FString Clean=Message.TrimStartAndEnd();return !Clean.IsEmpty()&&Clean.Len()<=MaxPlayerMessageCharacters&&Start(nullptr,&Clean);}
void UWardrobeAISpeechComponent::SetAutomaticPaused(bool Paused){
 if(bAutomaticPaused==Paused)return;bAutomaticPaused=Paused;
 if(Paused)CancelSpeech();else ScheduleNext();
}
bool UWardrobeAISpeechComponent::IsBusy() const{return State==EWardrobeSpeechState::Playing||State==EWardrobeSpeechState::Fading||State==EWardrobeSpeechState::RequestingText||State==EWardrobeSpeechState::GeneratingVoice||State==EWardrobeSpeechState::SolvingFace;}
bool UWardrobeAISpeechComponent::SpeakText(const FString& Text){const FString Clean=Text.TrimStartAndEnd();return !Clean.IsEmpty()&&Start(&Clean);}
bool UWardrobeAISpeechComponent::Start(const FString* Text,const FString* PlayerMessage){
 if(!IsIdleAllowed()||IsBusy())return false;
 auto* W=GetWorld();auto* G=W?W->GetGameInstance():nullptr;auto* S=G?G->GetSubsystem<UWardrobeAISpeechSubsystem>():nullptr;const auto* P=GetProfile();if(!S||!P||S->IsBusy())return false;
 LastProfile=const_cast<UWardrobeAISpeechData*>(P);
 FWardrobeSpeechJob J;J.Owner=this;J.Ticket=++Generation;J.Endpoint=P->ChatEndpoint;J.Model=P->Model;J.Persona=P->Persona;J.TTSEndpoint=P->TTSEndpoint;J.ReferenceAudio=P->ReferenceAudio.FilePath;J.ReferenceText=P->ReferenceText;J.Language=P->Language;
 J.MaxCharacters=FMath::Clamp(P->MaxTextCharacters,5,120);J.MaxDuration=FMath::Clamp(P->MaxAudioDuration,1.f,30.f);J.Timeout=FMath::Clamp(P->RequestTimeout,5.f,120.f);J.ExpressionIntensity=P->ExpressionIntensity;J.Morphs=P->Morphs;
 if(Text){if(Text->Len()>J.MaxCharacters)return false;J.Text=*Text;}
 if(PlayerMessage)J.PlayerMessage=*PlayerMessage;
 if(!P->Topics.IsEmpty()){int32 I=FMath::RandRange(0,P->Topics.Num()-1);if(P->Topics.Num()>1&&I==PreviousTopic)I=(I+FMath::RandRange(1,P->Topics.Num()-1))%P->Topics.Num();PreviousTopic=I;
  J.Scene=P->Topics[I].Scene;J.Topic=P->Topics[I].Topic;J.Tone=P->Topics[I].Tone;J.Mood=P->Topics[I].Mood;}
 // Discard unsupported morphs before snapshotting the job; never mutate mesh state on the worker.
 if(auto* Owner=Cast<AWardrobeShowcase>(GetOwner()))if(Owner->Character&&Owner->Character->GetSkeletalMeshAsset()){
  auto* Mesh=Owner->Character->GetSkeletalMeshAsset();J.Morphs.RemoveAll([Mesh](const auto& M){return !Mesh->FindMorphTarget(M.Morph);});
 }
 if(J.Morphs.IsEmpty()){Fail(J.Ticket,TEXT("No configured speech morphs exist on the character mesh."));return false;}
 LastError.Reset();State=Text?EWardrobeSpeechState::GeneratingVoice:EWardrobeSpeechState::RequestingText;return S->Start(J);
}
void UWardrobeAISpeechComponent::SetStage(uint64 T,EWardrobeSpeechState S){if(T==Generation)State=S;}
void UWardrobeAISpeechComponent::Fail(uint64 T,const FString& E){if(T!=Generation)return;LastError=E;CurrentText.Reset();State=EWardrobeSpeechState::Error;ScheduleNext();NextRequestTime=FMath::Max(NextRequestTime,FPlatformTime::Seconds()+30.);UE_LOG(LogTemp,Warning,TEXT("Wardrobe AI Speech: %s"),*E);}
void UWardrobeAISpeechComponent::AcceptClip(uint64 T,TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> C){
 if(T!=Generation||!IsIdleAllowed()||!C||C->PCM.IsEmpty())return;
 Clip=MoveTemp(C);CurrentText=Clip->Text;FaceNames=Clip->Names;FaceValues.Init(0,FaceNames.Num());FaceWeight=0;
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
 ++Generation;if(auto* G=GetWorld()?GetWorld()->GetGameInstance():nullptr)if(auto* S=G->GetSubsystem<UWardrobeAISpeechSubsystem>())S->Cancel(this);
 if(Audio){Audio->OnAudioFinished.RemoveAll(this);Audio->OnAudioPlaybackPercent.RemoveAll(this);Audio->Stop();Audio->DestroyComponent();Audio=nullptr;}if(Wave)Wave->ReleasePCM();Wave=nullptr;Clip.Reset();CurrentText.Reset();
 FadeAge=0;FadeFrom=FaceWeight;State=FaceWeight>0?EWardrobeSpeechState::Fading:EWardrobeSpeechState::Idle;ScheduleNext();
}
int32 UWardrobeAISpeechComponent::GetTemporaryBytes() const{return Clip?int32(FMath::Min<int64>(Clip->Bytes(),MAX_int32)):0;}
void UWardrobeAISpeechComponent::GetFaceSnapshot(TArray<FName>& N,TArray<float>& V,float& W) const{N=FaceNames;V=FaceValues;W=FaceWeight;}
void UWardrobeAISpeechComponent::TickComponent(float Dt,ELevelTick T,FActorComponentTickFunction* F){
 Super::TickComponent(Dt,T,F);auto* P=const_cast<UWardrobeAISpeechData*>(GetProfile());
 if(LastProfile.Get()!=P){CancelSpeech();LastProfile=P;State=EWardrobeSpeechState::Disabled;ScheduleNext();}
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
 if(Allowed&&(State==EWardrobeSpeechState::Disabled||State==EWardrobeSpeechState::Idle||State==EWardrobeSpeechState::Error)){if(State==EWardrobeSpeechState::Disabled){State=EWardrobeSpeechState::Idle;ScheduleNext();}if(!bAutomaticPaused&&FPlatformTime::Seconds()>=NextRequestTime)if(!RequestDialogueNow())NextRequestTime=FPlatformTime::Seconds()+1.;}
}
