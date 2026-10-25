#include "WardrobePinchNode.h"
#include "WardrobeAnimGuard.h"
#include "WardrobeShowcase.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
FAnimNode_WardrobePinch::FAnimNode_WardrobePinch(){TranslationMode=BMM_Additive;TranslationSpace=BCS_BoneSpace;RotationMode=BMM_Ignore;ScaleMode=BMM_Ignore;}
void FAnimNode_WardrobePinch::PreUpdate(const UAnimInstance* Instance){
 Translation=FVector::ZeroVector;Rotation=FRotator::ZeroRotator;
 TranslationMode=bReactionBone?BMM_Ignore:BMM_Additive;RotationMode=bReactionBone?BMM_Additive:BMM_Ignore;RotationSpace=BCS_BoneSpace;
 if(Instance)if(auto* Person=Cast<AWardrobeShowcase>(Instance->GetOwningActor())){
  if(bReactionBone){Rotation=(Person->GetCursorLookRotation(BoneToModify.BoneName)*Person->GetPinchReactionRotation(BoneToModify.BoneName)).Rotator();return;}
  if(!Person->Character)return;
  const FVector BoneScale=Person->Character->GetSocketTransform(BoneToModify.BoneName).GetScale3D();
  // FBX rigs can carry a 100x root scale; convert centimeter offsets to bone-local units.
  Translation=Person->GetPinchOffset(BoneToModify.BoneName)/BoneScale.ComponentMax(FVector(.0001));
 }
}
void FAnimNode_WardrobePinch::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out){
 // FAnimNode_ModifyBone::EvaluateSkeletalControl_AnyThread transforms BoneToModify through
 // FCSPose::GetComponentSpaceTransform, which walks the compact pose parent chain without any
 // range check. If that chain is broken the base implementation dies inside TArray::RangeCheck
 // on a worker thread and takes the editor with it, so bail out before calling it.
 const FCompactPose& Pose=Output.Pose.GetPose();
 const FCompactPoseBoneIndex Index=BoneToModify.GetCompactPoseIndex(Pose.GetBoneContainer());
 if(!WardrobeAnimGuard::IsUsable(Pose,Index)){WardrobeAnimGuard::ReportBrokenChain(TEXT("WardrobePinch"),Index,Pose.GetNumBones());return;}
 FAnimNode_ModifyBone::EvaluateSkeletalControl_AnyThread(Output,Out);
}
