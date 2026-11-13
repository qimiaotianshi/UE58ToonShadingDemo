#include "WardrobeReleaseNode.h"
#include "WardrobeShowcase.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationPoseData.h"
void FAnimNode_WardrobeRelease::Initialize_AnyThread(const FAnimationInitializeContext& C){Source.Initialize(C);LastPose.Reset();StartPose.Reset();LastSerial=0;}
void FAnimNode_WardrobeRelease::CacheBones_AnyThread(const FAnimationCacheBonesContext& C){Source.CacheBones(C);LastPose.Reset();StartPose.Reset();}
void FAnimNode_WardrobeRelease::Update_AnyThread(const FAnimationUpdateContext& C){Source.Update(C);}
void FAnimNode_WardrobeRelease::PreUpdate(const UAnimInstance* Instance){
 Clip=nullptr;
 if(Instance)if(auto* P=Cast<AWardrobeShowcase>(Instance->GetOwningActor())){
  Clip=P->GetReleaseAnimation();Time=P->GetReleaseAnimationTime();BlendTime=P->GetReleaseAnimationBlendTime();Serial=P->GetReleaseAnimationSerial();
  if(Clip&&MaskClip!=Clip){
   MaskClip=Clip;
   const auto& Ref=Clip->GetSkeleton()->GetReferenceSkeleton();AnimatedBones.Init(false,Ref.GetNum());
   // Limit the override to tracks that actually move; hair/skirt remain from the live source.
   for(int Index=0;Index<Ref.GetNum();Index++){
    FTransform First,Sample;Clip->GetBoneTransform(First,FSkeletonPoseBoneIndex(Index),FAnimExtractContext(0.),false);
    for(int Step=1;Step<=8;Step++){
     Clip->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(Index),FAnimExtractContext(Clip->GetPlayLength()*Step/8.),false);
     if(!First.Equals(Sample,.0001f)){AnimatedBones[Index]=true;break;}
    }
   }
  }
 }
}
void FAnimNode_WardrobeRelease::Evaluate_AnyThread(FPoseContext& Output){
 Source.Evaluate(Output);
 if(!Clip){LastPose.SetNum(Output.Pose.GetNumBones());for(auto I:Output.Pose.ForEachBoneIndex())LastPose[I.GetInt()]=Output.Pose[I];LastCurve=Output.Curve;return;}
 if(LastSerial!=Serial){StartPose=LastPose;StartCurve=LastCurve;LastSerial=Serial;}
 FPoseContext Animation(Output);Animation.ResetToRefPose();FAnimationPoseData Data(Animation);
 Clip->GetAnimationPose(Data,FAnimExtractContext(FMath::Min(Time,Clip->GetPlayLength())));
 const bool Returning=Time>Clip->GetPlayLength();
 float Alpha=BlendTime<=0?1.f:FMath::Clamp((Returning?Time-Clip->GetPlayLength():Time)/BlendTime,0.f,1.f);Alpha=Alpha*Alpha*(3.f-2.f*Alpha);
 const auto& Bones=Output.Pose.GetBoneContainer();
 for(auto I:Output.Pose.ForEachBoneIndex()){
  const int Index=Bones.GetSkeletonIndex(I);
  if(!AnimatedBones.IsValidIndex(Index)||!AnimatedBones[Index])continue;
  FTransform Target;
  if(Returning)Target.Blend(Animation.Pose[I],Output.Pose[I],Alpha);
  else Target.Blend(StartPose.IsValidIndex(I.GetInt())?StartPose[I.GetInt()]:Output.Pose[I],Animation.Pose[I],Alpha);
  Output.Pose[I]=Target;
 }
 FBlendedCurve Curves;if(Returning)Curves.Lerp(Animation.Curve,Output.Curve,Alpha);else Curves.Lerp(StartCurve,Animation.Curve,Alpha);Output.Curve=Curves;
 Output.Pose.NormalizeRotations();
}
