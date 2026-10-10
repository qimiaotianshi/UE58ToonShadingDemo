#include "WardrobeCharacters.h"
#include "WardrobeShowcase.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

const FWardrobeCharacterOption* AWardrobeShowcase::GetInteractionProfile() const {
 const auto* Data=InteractionData.Get();
 return Data&&Data->Characters.IsValidIndex(InteractionProfileIndex)?&Data->Characters[InteractionProfileIndex]:nullptr;
}
void AWardrobeShowcase::SetInteractionEnabled(bool Enabled){
 bInteractionEnabled=Enabled;
 if(!Enabled){ClearPinch();CancelInteraction(false);StopDefaultSequence();StopPhysicsTouch();FadeSequencePhysics(1,0);bResetSequenceDynamics=false;}
 else if(!IsInteracting())StartDefaultSequence();
}
void AWardrobeShowcase::RestoreCharacterAnimBlueprint(){
 // Preserve a restored instance and its running physics simulation.
 if(Character->GetAnimClass()!=InitialAnimClass.Get())Character->SetAnimInstanceClass(InitialAnimClass.Get());
 if(Character->GetAnimationMode()!=EAnimationMode::AnimationBlueprint)Character->SetAnimationMode(EAnimationMode::AnimationBlueprint);
}
