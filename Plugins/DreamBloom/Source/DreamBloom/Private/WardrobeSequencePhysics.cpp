#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Components/SkeletalMeshComponent.h"
#include "LevelSequencePlayer.h"
void AWardrobeShowcase::FadeSequencePhysics(float Target,float Duration){
 PhysicsBlendFrom=SequencePhysicsWeight;PhysicsBlendTarget=Target;PhysicsBlendAge=0;
 PhysicsBlendDuration=FMath::IsFinite(Duration)?FMath::Max(0.f,Duration):0.f;
 if(PhysicsBlendDuration<=0)SequencePhysicsWeight=Target;
}
void AWardrobeShowcase::BeginSequencePhysicsFadeIn(){
 const auto* P=GetInteractionProfile();SequencePhysicsWeight=0;
 FadeSequencePhysics(1,P?P->PhysicsFadeTime:0);
 bResetSequenceDynamics=P&&P->bResetPhysicsOnSequenceSwitch;
}
void AWardrobeShowcase::TickSequencePhysicsBlend(float Dt){
 if(bResetSequenceDynamics){
  // Run on the next actor tick: the incoming Sequence has already supplied its base pose.
  if(Character)Character->ResetAnimInstanceDynamics(ETeleportType::ResetPhysics);
  bResetSequenceDynamics=false;
 }
 if(PhysicsBlendDuration>0){
  PhysicsBlendAge+=FMath::Max(0.f,Dt);const float T=FMath::Clamp(PhysicsBlendAge/PhysicsBlendDuration,0.f,1.f);
  const float A=T*T*(3.f-2.f*T);SequencePhysicsWeight=FMath::Lerp(PhysicsBlendFrom,PhysicsBlendTarget,A);
  if(T>=1)PhysicsBlendDuration=0;
 }
 if(PendingSequenceSwitch!=0&&PhysicsBlendDuration<=0){
  if(PendingSequenceSwitch==1)StartPreparedInteraction();
  else{
   CancelInteraction(false);
   if(bInteractionEnabled&&!bEndingShowcase)if(!StartDefaultSequence())FadeSequencePhysics(1,0);
  }
 }
}
