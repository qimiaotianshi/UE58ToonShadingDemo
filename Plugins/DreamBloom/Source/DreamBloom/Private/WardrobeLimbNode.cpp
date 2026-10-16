#include "WardrobeLimbNode.h"
#include "WardrobeShowcase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "Animation/Skeleton.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TwoBoneIK.h"

void FAnimNode_WardrobeLimbReaction::PreUpdate(const UAnimInstance* Instance){
 Horizontal=nullptr;Vertical=nullptr;Force=Strength=HorizontalWeight=VerticalWeight=0;
 if(Instance)if(auto* A=Cast<AWardrobeShowcase>(Instance->GetOwningActor())){
  const auto S=A->GetLimbSnapshot();if(S.Alpha<=0)return;
  Horizontal=S.Direction.X<0?S.Settings.BodyLeft:S.Settings.BodyRight;Vertical=S.Direction.Y<0?S.Settings.BodyDown:S.Settings.BodyUp;
  const float Sum=FMath::Abs(S.Direction.X)+FMath::Abs(S.Direction.Y);
  if(Sum>SMALL_NUMBER){HorizontalWeight=FMath::Abs(S.Direction.X)/Sum;VerticalWeight=FMath::Abs(S.Direction.Y)/Sum;}
  Force=S.Force;Strength=FMath::Clamp(S.Settings.ReactionStrength,0.f,1.f);
  auto* Mesh=Instance->GetSkelMeshComponent()->GetSkeletalMeshAsset();
  if(!Mesh||!Horizontal||Horizontal->GetSkeleton()!=Mesh->GetSkeleton())Horizontal=nullptr;
  if(!Mesh||!Vertical||Vertical->GetSkeleton()!=Mesh->GetSkeleton())Vertical=nullptr;
 }
}
void FAnimNode_WardrobeLimbReaction::Evaluate_AnyThread(FPoseContext& Output){
 Source.Evaluate(Output);if(Force<=SMALL_NUMBER||Strength<=0)return;
 auto Apply=[&](UAnimSequence* Clip,float Weight){
  if(!Clip||Weight<=0)return;
  FPoseContext Neutral(Output),Pull(Output);Neutral.ResetToRefPose();Pull.ResetToRefPose();
  FAnimationPoseData N(Neutral),P(Pull);Clip->GetAnimationPose(N,FAnimExtractContext(0.));Clip->GetAnimationPose(P,FAnimExtractContext(Force*Clip->GetPlayLength()));
  const float W=Weight*Strength;
  for(auto I:Output.Pose.ForEachBoneIndex()){
   auto& T=Output.Pose[I];const auto& Base=Neutral.Pose[I];const auto& Goal=Pull.Pose[I];
   const FQuat Delta=(Goal.GetRotation()*Base.GetRotation().Inverse()).GetNormalized();
   T.SetRotation((FQuat::Slerp(FQuat::Identity,Delta,W)*T.GetRotation()).GetNormalized());
   T.AddToTranslation((Goal.GetLocation()-Base.GetLocation())*W);
  }
  // Facial curves authored in the four body clips blend with the default sequence as well.
  Pull.Curve.Accumulate(Neutral.Curve,-1.f);Output.Curve.Accumulate(Pull.Curve,W);
 };
 Apply(Horizontal,HorizontalWeight);Apply(Vertical,VerticalWeight);Output.Pose.NormalizeRotations();
}
void FAnimNode_WardrobeLimbIK::PreUpdate(const UAnimInstance* Instance){
 DragAlpha=0;if(!Instance)return;
 auto* A=Cast<AWardrobeShowcase>(Instance->GetOwningActor());auto* Mesh=Instance->GetSkelMeshComponent();if(!A||!Mesh)return;
 const auto S=A->GetLimbSnapshot();DragAlpha=S.Alpha;Upper.BoneName=S.Settings.UpperBone;Joint.BoneName=S.Settings.JointBone;End.BoneName=S.Settings.EndBone;
 Target=Mesh->GetComponentTransform().InverseTransformPosition(S.TargetWorld);
 PoleDirection=Mesh->GetComponentTransform().InverseTransformVectorNoScale(S.PoleDirection).GetSafeNormal();
}
void FAnimNode_WardrobeLimbIK::EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& O,TArray<FBoneTransform>& Out){
 const auto& B=O.Pose.GetPose().GetBoneContainer();Upper.Initialize(B);Joint.Initialize(B);End.Initialize(B);
 if(!Upper.IsValidToEvaluate(B)||!Joint.IsValidToEvaluate(B)||!End.IsValidToEvaluate(B))return;
 const auto U=Upper.GetCompactPoseIndex(B),J=Joint.GetCompactPoseIndex(B),E=End.GetCompactPoseIndex(B);
 auto Descendant=[&](FCompactPoseBoneIndex Child,FCompactPoseBoneIndex Parent){while((Child=B.GetParentBoneIndex(Child))!=INDEX_NONE)if(Child==Parent)return true;return false;};
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
  auto I=B.GetParentBoneIndex(Child);while(I!=INDEX_NONE&&I!=Parent){const FTransform Local=O.Pose.GetComponentSpaceTransform(I).GetRelativeTransform(OldParent);Out.Emplace(I,Local*NewParent);I=B.GetParentBoneIndex(I);}
 };
 PreserveIntermediate(J,U,OriginalU,UT);PreserveIntermediate(E,J,OriginalJ,JT);Out.Sort(FCompareBoneTransformIndex());
}
