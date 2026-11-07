#include "WardrobeMovementAudio.h"

#include "WardrobeLocomotion.h"
#include "WardrobeLoading.h"
#include "WardrobeStory.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

const TArray<TObjectPtr<USoundBase>>& UWardrobeMovementAudioData::GetSounds(EWardrobeMovementSound Event) const {
 switch(Event){
 case EWardrobeMovementSound::Walk:return Walk;
 case EWardrobeMovementSound::Run:return Run.IsEmpty()?Walk:Run;
 case EWardrobeMovementSound::Jump:return Jump;
 case EWardrobeMovementSound::Land:return Land;
 case EWardrobeMovementSound::HeavyLand:return HeavyLand.IsEmpty()?Land:HeavyLand;
 default:{static const TArray<TObjectPtr<USoundBase>> Empty;return Empty;}
 }
}
FString UAnimNotify_WardrobeFootstep::GetNotifyName_Implementation() const {return bLeftFoot?TEXT("Wardrobe Footstep L"):TEXT("Wardrobe Footstep R");}
void UAnimNotify_WardrobeFootstep::Notify(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference){
 Super::Notify(MeshComp,Animation,EventReference);
 if(MeshComp)if(auto* Explorer=Cast<AWardrobeExplorer>(MeshComp->GetOwner()))Explorer->PlayFootstep(bLeftFoot);
}
bool AWardrobeExplorer::CanPlayMovementAudio() const {
 if(!GetWorld()||!GetWorld()->IsGameWorld()||GetNetMode()==NM_DedicatedServer||!Settings||!Settings->MovementAudio||!Settings->MovementAudio->bEnabled||!Controller||IsHidden()||!GetMesh()||!GetMesh()->IsVisible()||UGameplayStatics::IsGamePaused(this))return false;
 if(auto* GI=GetGameInstance()){
  if(auto* Loading=GI->GetSubsystem<UWardrobeLoadingSubsystem>())if(Loading->IsLoading())return false;
  if(auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>())if(Story->IsCinematicActive())return false;
 }
 return true;
}
void AWardrobeExplorer::PlayFootstep(bool Left){
 const auto* Movement=GetCharacterMovement();
 if(!CanPlayMovementAudio()||!Movement||!Movement->IsMovingOnGround()||GetVelocity().Size2D()<12.f)return;
 const double Now=GetWorld()->GetTimeSeconds();const int32 Foot=Left?0:1;
 // Blend-space followers and overlapping transition clips must not double-trigger one contact.
 if(Now<FootstepsBlockedUntil||Now-LastFootstepTime[Foot]<.16||Now-LastAnyFootstepTime<.075)return;
 const bool Running=GetVelocity().Size2D()>FMath::Max(1.f,Settings->WalkSpeed)*1.2f;
 if(PlayMovementSound(Running?EWardrobeMovementSound::Run:EWardrobeMovementSound::Walk)){
  LastFootstepTime[Foot]=LastAnyFootstepTime=Now;
 }
}
bool AWardrobeExplorer::PlayMovementSound(EWardrobeMovementSound Event){
 if(!CanPlayMovementAudio())return false;
 const auto& Sounds=Settings->MovementAudio->GetSounds(Event);
 const int32 Group=int32(Event);LastMovementSounds.SetNum(5);
 TArray<USoundBase*,TInlineAllocator<8>> Choices;
 for(auto Sound:Sounds)if(IsValid(Sound.Get()))Choices.AddUnique(Sound.Get());
 if(Choices.IsEmpty())return false;
 // Compare assets, not indices: duplicate entries cannot defeat non-repeat selection.
 if(Choices.Num()>1)Choices.Remove(LastMovementSounds[Group].Get());
 auto* Sound=Choices[FMath::RandRange(0,Choices.Num()-1)];
 MovementSounds.RemoveAll([](const TObjectPtr<UAudioComponent>& C){return !IsValid(C.Get())||!C->IsPlaying();});
 while(MovementSounds.Num()>=4){if(IsValid(MovementSounds[0]))MovementSounds[0]->Stop();MovementSounds.RemoveAt(0);}
 const float Pitch=FMath::FRandRange(.97f,1.03f);
 const float Volume=FMath::FRandRange(.93f,1.f);
 auto* Audio=UGameplayStatics::SpawnSoundAttached(Sound,GetMesh(),NAME_None,FVector::ZeroVector,FRotator::ZeroRotator,EAttachLocation::KeepRelativeOffset,true,Volume,Pitch,0,Settings->MovementAudio->Attenuation,nullptr,true);
 if(!Audio)return false;
 MovementSounds.Add(Audio);LastMovementSounds[Group]=Sound;++MovementSoundPlayCount;
#if WITH_EDITOR
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeMovementAudioQA")))UE_LOG(LogTemp,Display,TEXT("ALS_MOVEMENT_SOUND event=%s sound=%s speed=%.1f pitch=%.3f count=%d"),*StaticEnum<EWardrobeMovementSound>()->GetNameStringByValue(int64(Event)),*Sound->GetPathName(),GetVelocity().Size2D(),Pitch,MovementSoundPlayCount);
#endif
 return true;
}
void AWardrobeExplorer::StopMovementAudio(){
 for(auto Audio:MovementSounds)if(IsValid(Audio.Get()))Audio->Stop();
 MovementSounds.Reset();
}
void AWardrobeExplorer::OnJumped_Implementation(){
 Super::OnJumped_Implementation();FootstepsBlockedUntil=GetWorld()->GetTimeSeconds()+.15;
 PlayMovementSound(EWardrobeMovementSound::Jump);
}
void AWardrobeExplorer::Landed(const FHitResult& Hit){
 const float ImpactSpeed=FMath::Abs(GetVelocity().Z);
 Super::Landed(Hit);FootstepsBlockedUntil=GetWorld()->GetTimeSeconds()+.18;
 PlayMovementSound(Settings&&ImpactSpeed>Settings->HeavyLandingSpeed?EWardrobeMovementSound::HeavyLand:EWardrobeMovementSound::Land);
}
void AWardrobeExplorer::EndPlay(const EEndPlayReason::Type Reason){StopMovementAudio();Super::EndPlay(Reason);}
