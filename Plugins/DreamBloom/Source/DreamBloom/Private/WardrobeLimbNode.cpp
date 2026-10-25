#include "WardrobeLimbNode.h"
#include "WardrobeAnimGuard.h"
#include "WardrobeShowcase.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TwoBoneIK.h"

void FAnimNode_WardrobeLimbIK::PreUpdate(const UAnimInstance* Instance){
 DragAlpha=0;if(!Instance)return;
 auto* A=Cast<AWardrobeShowcase>(Instance->GetOwningActor());auto* Mesh=Instance->GetSkelMeshComponent();if(!A||!Mesh)return;
 const auto S=A->GetLimbSnapshot();DragAlpha=S.Alpha;Upper.BoneName=S.Settings.UpperBone;Joint.BoneName=S.Settings.JointBone;End.BoneName=S.Settings.EndBone;
 Target=Mesh->GetComponentTransform().InverseTransformPosition(S.TargetWorld);
 PoleDirection=Mesh->GetComponentTransform().InverseTransformVectorNoScale(S.PoleDirection).GetSafeNormal();
}
void FAnimNode_WardrobeLimbIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& O,TArray<FBoneTransform>& Out){
 const FCompactPose& Pose=O.Pose.GetPose();
 const auto& B=Pose.GetBoneContainer();Upper.Initialize(B);Joint.Initialize(B);End.Initialize(B);
 if(!Upper.IsValidToEvaluate(B)||!Joint.IsValidToEvaluate(B)||!End.IsValidToEvaluate(B))return;
 const auto U=Upper.GetCompactPoseIndex(B),J=Joint.GetCompactPoseIndex(B),E=End.GetCompactPoseIndex(B);
 // IsValidIndex() only proves the bone is addressable. Every parent on the way to the root must be
 // addressable too, otherwise GetComponentSpaceTransform below stops inside TArray::RangeCheck.
 auto ValidIndex=[&](FCompactPoseBoneIndex I){return WardrobeAnimGuard::IsUsable(Pose,I);};
 if(!ValidIndex(U)||!ValidIndex(J)||!ValidIndex(E)){WardrobeAnimGuard::ReportBrokenChain(TEXT("WardrobeLimbIK"),U,Pose.GetNumBones());return;}
 auto Descendant=[&](FCompactPoseBoneIndex Child,FCompactPoseBoneIndex Parent){
  if(!ValidIndex(Child)||!ValidIndex(Parent))return false;
  while(ValidIndex(Child)){Child=B.GetParentBoneIndex(Child);if(!ValidIndex(Child))return false;if(Child==Parent)return true;}
  return false;
 };
 if(!Descendant(J,U)||!Descendant(E,J))return;
 FTransform UT=O.Pose.GetComponentSpaceTransform(U),JT=O.Pose.GetComponentSpaceTransform(J),ET=O.Pose.GetComponentSpaceTransform(E);
 const FTransform OriginalU=UT,OriginalJ=JT;const FQuat Rotation=ET.GetRotation();
 FVector Goal=FMath::Lerp(ET.GetLocation(),Target,DragAlpha);
 const float Reach=FVector::Distance(UT.GetLocation(),JT.GetLocation())+FVector::Distance(JT.GetLocation(),ET.GetLocation());
 Goal=UT.GetLocation()+(Goal-UT.GetLocation()).GetClampedToMaxSize(Reach*.9999f);
 const FVector Pole=UT.GetLocation()+PoleDirection*FMath::Max(Reach,10.f);
 AnimationCore::SolveTwoBoneIK(UT,JT,ET,Pole,Goal,false,1.,1.);ET.SetRotation(Rotation);
 Out.Emplace(U,UT);Out.Emplace(J,JT);Out.Emplace(E,ET);
 // MMD twist bones can sit between shoulder/elbow/wrist. Transform them with
 // their solved segment instead of assuming the three deform bones are immediate parents.
 auto PreserveIntermediate=[&](FCompactPoseBoneIndex Child,FCompactPoseBoneIndex Parent,const FTransform& OldParent,const FTransform& NewParent){
  if(!ValidIndex(Child)||!ValidIndex(Parent))return;
  auto I=B.GetParentBoneIndex(Child);while(ValidIndex(I)&&I!=Parent){const FTransform Local=O.Pose.GetComponentSpaceTransform(I).GetRelativeTransform(OldParent);Out.Emplace(I,Local*NewParent);I=B.GetParentBoneIndex(I);}
 };
 PreserveIntermediate(J,U,OriginalU,UT);PreserveIntermediate(E,J,OriginalJ,JT);Out.Sort(FCompareBoneTransformIndex());
}
