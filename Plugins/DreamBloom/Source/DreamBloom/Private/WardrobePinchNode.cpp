#include "WardrobePinchNode.h"
#include "WardrobeShowcase.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
FAnimNode_WardrobePinch::FAnimNode_WardrobePinch(){TranslationMode=BMM_Additive;TranslationSpace=BCS_BoneSpace;RotationMode=BMM_Ignore;ScaleMode=BMM_Ignore;}
void FAnimNode_WardrobePinch::PreUpdate(const UAnimInstance* Instance){
 Translation=FVector::ZeroVector;Rotation=FRotator::ZeroRotator;
 TranslationMode=bReactionBone?BMM_Ignore:BMM_Additive;RotationMode=bReactionBone?BMM_Additive:BMM_Ignore;RotationSpace=BCS_BoneSpace;
 if(Instance)if(auto* Person=Cast<AWardrobeShowcase>(Instance->GetOwningActor())){
  if(bReactionBone){Rotation=(Person->GetCursorLookRotation(BoneToModify.BoneName)*Person->GetPinchReactionRotation(BoneToModify.BoneName)).Rotator();return;}
  const FVector BoneScale=Person->Character->GetSocketTransform(BoneToModify.BoneName).GetScale3D();
  // FBX rigs can carry a 100x root scale; convert centimeter offsets to bone-local units.
  Translation=Person->GetPinchOffset(BoneToModify.BoneName)/BoneScale.ComponentMax(FVector(.0001));
 }
}

