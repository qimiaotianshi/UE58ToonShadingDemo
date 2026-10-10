#include "WardrobeInteraction.h"
#include "WardrobeCharacters.h"
#include "WardrobeShowcase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"

void AWardrobeShowcase::BuildInteractionRegions(){
 for(auto Shape:InteractionShapes)if(IsValid(Shape))Shape->DestroyComponent();
 InteractionShapes.Reset();InteractionRegionIndices.Reset();RegionReadyAt.Reset();LastReaction.Reset();InteractionReadyAt=0;
 const auto* Profile=GetInteractionProfile();if(!Profile)return;
 const auto& Regions=Profile->Interactions;
 for(int32 I=0;I<Regions.Num();++I){
  const auto& R=Regions[I];if(!R.bEnabled)continue;
  if(R.BoneOrSocket.IsNone()||!Character->DoesSocketExist(R.BoneOrSocket)){UE_LOG(LogTemp,Warning,TEXT("Interaction region %d has invalid bone/socket %s"),I,*R.BoneOrSocket.ToString());continue;}
  UShapeComponent* Shape=nullptr;
  if(R.Shape==EWardrobeRegionShape::Sphere){auto* C=NewObject<USphereComponent>(this);C->SetSphereRadius(FMath::Max(.1f,R.Radius));Shape=C;}
  else if(R.Shape==EWardrobeRegionShape::Capsule){auto* C=NewObject<UCapsuleComponent>(this);C->SetCapsuleSize(FMath::Max(.1f,R.Radius),FMath::Max(R.Radius,R.HalfHeight));Shape=C;}
  else{auto* C=NewObject<UBoxComponent>(this);C->SetBoxExtent(R.BoxExtent.GetAbs().ComponentMax(FVector(.1f)));Shape=C;}
  Shape->SetupAttachment(Character,R.BoneOrSocket);Shape->SetRelativeLocationAndRotation(R.Offset,R.Rotation);
  Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Shape->SetCollisionResponseToAllChannels(ECR_Ignore);Shape->SetGenerateOverlapEvents(false);Shape->SetHiddenInGame(true);Shape->RegisterComponent();
  InteractionShapes.Add(Shape);InteractionRegionIndices.Add(I);
 }
}
int32 AWardrobeShowcase::HitInteraction(const FVector& Origin,const FVector& Direction)const{
 if(!bInteractionEnabled||Direction.IsNearlyZero()||!GetWorld())return INDEX_NONE;
 const FVector End=Origin+Direction.GetSafeNormal()*100000.f;float Best=100000.f;int32 Result=INDEX_NONE;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(WardrobeInteraction),false);Params.AddIgnoredActor(this);
 FHitResult Obstacle;if(GetWorld()->LineTraceSingleByChannel(Obstacle,Origin,End,ECC_Visibility,Params))Best=Obstacle.Distance;
 // Optional existing Physics Asset provides body occlusion in addition to configured blocker volumes.
 FHitResult Body;if(Character->LineTraceComponent(Body,Origin,End,Params))Best=FMath::Min(Best,Body.Distance+2.f);
 for(int32 I=0;I<InteractionShapes.Num();++I){FHitResult Hit;
  if(IsValid(InteractionShapes[I])&&InteractionShapes[I]->LineTraceComponent(Hit,Origin,End,Params)&&Hit.Distance<Best){Best=Hit.Distance;Result=InteractionRegionIndices[I];}
 }
 return Result;
}
bool AWardrobeShowcase::ClickInteraction(const FVector& Origin,const FVector& Direction){return TriggerInteraction(HitInteraction(Origin,Direction));}
bool AWardrobeShowcase::TriggerInteraction(int32 RegionIndex){
 const auto* Profile=GetInteractionProfile();
 if(!bInteractionEnabled||IsInteracting()||!Profile)return false;
 const auto& Person=*Profile;
 if(!Person.Interactions.IsValidIndex(RegionIndex))return false;
 const auto& R=Person.Interactions[RegionIndex];const double Now=GetWorld()->GetTimeSeconds();
 if(!R.bEnabled||R.bOccluderOnly||R.bPhysicsTouchOnly||Now<InteractionReadyAt||Now<RegionReadyAt.FindRef(RegionIndex))return false;
 TArray<int32> Choices;TArray<ULevelSequence*> Sequences;float Total=0;
 for(int32 I=0;I<R.Reactions.Num();++I){const auto& Reaction=R.Reactions[I];if(Reaction.Weight<=0)continue;
  auto* Sequence=Reaction.Sequence.LoadSynchronous();
  if(!Sequence||!Sequence->GetMovieScene()||Sequence->GetMovieScene()->GetPlaybackRange().IsEmpty())continue;
  Choices.Add(I);Sequences.Add(Sequence);
 }
 if(Choices.IsEmpty()){CharacterMessage=FText::FromString(TEXT("此部位尚未配置有效的互动 Sequence"));return false;}
 if(R.bAvoidImmediateRepeat&&Choices.Num()>1)if(const int32* Last=LastReaction.Find(RegionIndex)){int32 At=Choices.Find(*Last);if(At!=INDEX_NONE){Choices.RemoveAt(At);Sequences.RemoveAt(At);}}
 for(int32 I:Choices)Total+=R.Reactions[I].Weight;
 float Pick=FMath::FRand()*Total;int32 Slot=Choices.Num()-1;
 for(int32 I=0;I<Choices.Num();I++){Pick-=R.Reactions[Choices[I]].Weight;if(Pick<=0){Slot=I;break;}}
 const auto& Reaction=R.Reactions[Choices[Slot]];
 FMovieSceneSequencePlaybackSettings Settings;Settings.LoopCount.Value=0;Settings.bPauseAtEnd=true;Settings.PlayRate=FMath::Max(.01f,Reaction.PlayRate);
 Settings.FinishCompletionStateOverride=EMovieSceneCompletionModeOverride::ForceRestoreState;
 ALevelSequenceActor* Actor=nullptr;
 auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(this,Sequences[Slot],Settings,Actor);
 if(!Player||!Actor){if(Actor)Actor->Destroy();return false;}
 if(!Reaction.CharacterBindingTag.IsNone()){
  if(Actor->FindNamedBindings(Reaction.CharacterBindingTag).IsEmpty()){
   CharacterMessage=FText::FromString(TEXT("Sequence 中未找到指定的人物绑定标签"));Actor->Destroy();return false;
  }
  Actor->SetBindingByTag(Reaction.CharacterBindingTag,{this},false);
 }
 InteractionPlayer=Player;InteractionSequenceActor=Actor;
 InteractionAge=0;ActiveInteraction=RegionIndex;LastReaction.Add(RegionIndex,Choices[Slot]);++InteractionPlayCount;
 PendingSequenceSwitch=1;ClearPinch();StopPhysicsTouch();
 FadeSequencePhysics(0,Person.PhysicsFadeTime);
 if(PhysicsBlendDuration<=0)StartPreparedInteraction();
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_INTERACTION_SEQUENCE region=%d reaction=%d sequence=%s tag=%s"),RegionIndex,Choices[Slot],*Sequences[Slot]->GetPathName(),*Reaction.CharacterBindingTag.ToString());return true;
}
void AWardrobeShowcase::StartPreparedInteraction(){
 if(PendingSequenceSwitch!=1||!IsValid(InteractionPlayer))return;
 PendingSequenceSwitch=0;
 StopDefaultSequence();
 InteractionOriginalTransform=GetActorTransform();InteractionPreviousCamera=nullptr;InteractionOriginalCameraFov=0;
 if(auto* PC=UGameplayStatics::GetPlayerController(this,0)){
  InteractionPreviousCamera=PC->GetViewTarget();
  if(IsValid(InteractionPreviousCamera))InteractionOriginalCameraTransform=InteractionPreviousCamera->GetActorTransform();
  if(auto* Camera=Cast<ACameraActor>(InteractionPreviousCamera))InteractionOriginalCameraFov=Camera->GetCameraComponent()->FieldOfView;
 }
 InteractionAge=0;bInteractionStarted=true;
 InteractionPlayer->Play();BeginSequencePhysicsFadeIn();
}
void AWardrobeShowcase::CancelInteraction(bool RestoreIdle){
 if(RestoreIdle&&IsInteracting()&&bInteractionEnabled&&!bEndingShowcase){
  if(PendingSequenceSwitch==2)return;
  const auto* P=GetInteractionProfile();
  if(bInteractionStarted&&P&&P->PhysicsFadeTime>0){
   // Hold the final pose (including at natural completion) while physics fades away.
   if(IsValid(InteractionPlayer)&&InteractionPlayer->IsPlaying())InteractionPlayer->Pause();
   PendingSequenceSwitch=2;FadeSequencePhysics(0,P->PhysicsFadeTime);return;
  }
 }
 const bool WasInteracting=IsInteracting();
 const bool HadPose=bInteractionStarted;
 PendingSequenceSwitch=0;bInteractionStarted=false;
 FadeSequencePhysics(1,0);bResetSequenceDynamics=false;
 // Stop and restore even when changing character, so old evaluation cannot overwrite the new mesh.
 if(IsValid(InteractionPlayer)){InteractionPlayer->Stop();InteractionPlayer->RestoreState();}
 if(IsValid(InteractionSequenceActor))InteractionSequenceActor->Destroy();
 InteractionPlayer=nullptr;InteractionSequenceActor=nullptr;
 if(HadPose){
  SetActorTransform(InteractionOriginalTransform);
  if(IsValid(InteractionPreviousCamera)){
   InteractionPreviousCamera->SetActorTransform(InteractionOriginalCameraTransform);
   if(auto* Camera=Cast<ACameraActor>(InteractionPreviousCamera))if(InteractionOriginalCameraFov>0)Camera->GetCameraComponent()->SetFieldOfView(InteractionOriginalCameraFov);
   if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(InteractionPreviousCamera);
  }
 }
 InteractionPreviousCamera=nullptr;
 const auto* Profile=GetInteractionProfile();
 if(WasInteracting&&Profile){
  const auto& P=*Profile;

  const double Now=GetWorld()->GetTimeSeconds();InteractionReadyAt=Now+FMath::Max(0.f,P.InteractionCooldown);
  if(P.Interactions.IsValidIndex(ActiveInteraction))RegionReadyAt.Add(ActiveInteraction,Now+FMath::Max(0.f,P.Interactions[ActiveInteraction].Cooldown));
 }
 ActiveInteraction=INDEX_NONE;
 if(WasInteracting&&RestoreIdle&&bInteractionEnabled&&!bEndingShowcase)StartDefaultSequence();
}
void AWardrobeShowcase::TickInteraction(float Dt){
 const auto* Profile=GetInteractionProfile();if(!Profile)return;
 const auto& P=*Profile;
 if(P.bShowInteractionRegions)for(auto Shape:InteractionShapes){if(!IsValid(Shape))continue;
  const auto T=Shape->GetComponentTransform();const float Scale=T.GetScale3D().GetAbsMax();
  if(auto* C=Cast<USphereComponent>(Shape))DrawDebugSphere(GetWorld(),T.GetLocation(),C->GetUnscaledSphereRadius()*Scale,16,FColor::Green,false,0);
  else if(auto* Capsule=Cast<UCapsuleComponent>(Shape))DrawDebugCapsule(GetWorld(),T.GetLocation(),Capsule->GetUnscaledCapsuleHalfHeight()*Scale,Capsule->GetUnscaledCapsuleRadius()*Scale,T.GetRotation(),FColor::Green,false,0);
  else if(auto* Box=Cast<UBoxComponent>(Shape))DrawDebugBox(GetWorld(),T.GetLocation(),Box->GetUnscaledBoxExtent()*T.GetScale3D().GetAbs(),T.GetRotation(),FColor::Green,false,0);
 }
 if(!IsInteracting()||PendingSequenceSwitch!=0)return;
 InteractionAge+=Dt;
 if(!IsValid(InteractionPlayer)||!InteractionPlayer->IsPlaying()||InteractionAge>=FMath::Max(1.f,P.MaxInteractionDuration))CancelInteraction(true);
}
