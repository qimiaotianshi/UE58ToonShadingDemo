#include "WardrobePinch.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeKawaiiForce.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/MorphTarget.h"
#include "Engine/SkeletalMesh.h"

FVector AWardrobeShowcase::GetPinchOffset(FName Bone) const{
 FVector Result=FVector::ZeroVector;for(const auto& State:PinchStates)if(State.Bone==Bone)Result+=State.Position;return Result;
}
void AWardrobeShowcase::PlayPinchSound(const FWardrobeSoundEntry& Entry,EWardrobeUISound Event){
 if(GetGameInstance())if(auto* Audio=GetGameInstance()->GetSubsystem<UWardrobeAudioSubsystem>())Audio->PlayEntry(Event,Entry);
}
bool AWardrobeShowcase::BeginPinch(const FVector& Origin,const FVector& Direction){
 const auto* Profile=GetInteractionProfile();if(!bInteractionEnabled||IsInteracting()||!Profile||!Character||Direction.IsNearlyZero())return false;
 Character->HandleExistingParallelEvaluationTask(true,true);
 FCollisionQueryParams Params(SCENE_QUERY_STAT(WardrobePinch),false);Params.AddIgnoredActor(this);
 FHitResult Blocker;float Best=100000;const FVector Ray=Direction.GetSafeNormal();
 if(GetWorld()->LineTraceSingleByChannel(Blocker,Origin,Origin+Ray*Best,ECC_Visibility,Params))Best=Blocker.Distance;
 int32 Selected=INDEX_NONE;
 for(int32 I=0;I<Profile->PinchRegions.Num();I++){
  const auto& Region=Profile->PinchRegions[I];
  if(!Region.bEnabled||Character->GetBoneIndex(Region.Bone)==INDEX_NONE)continue;
  const FName Anchor=Region.AnchorBone.IsNone()?Region.Bone:Region.AnchorBone;
  if(!Character->DoesSocketExist(Anchor))continue;
  const FTransform TM=Character->GetSocketTransform(Anchor);
  const FVector Center=TM.TransformPosition(Region.Offset);float Distance=0;
  if(FWardrobeKawaiiClickForce::RaySphere(Origin,Ray,Center,FMath::Max(.1f,Region.HitRadius)*Character->GetComponentTransform().GetMaximumAxisScale(),Distance)&&Distance<Best){Selected=I;Best=Distance;}
 }
 if(Selected==INDEX_NONE)return false;
 if(PinchStates.Num()!=Profile->PinchRegions.Num()){ClearPinch();PinchStates.SetNum(Profile->PinchRegions.Num());}
 const auto& Region=Profile->PinchRegions[Selected];auto& State=PinchStates[Selected];
 State.Bone=Region.Bone;State.PullMorph=Region.PullMorph;State.Target=State.Position;State.bReturning=false;
 ActivePinch=Selected;PinchPlanePoint=Origin+Ray*Best;PinchPlaneNormal=Ray;PinchStartRotation=Character->GetSocketQuaternion(Region.Bone);
 State.StartComponentRotation=Character->GetComponentQuat().Inverse()*PinchStartRotation;
 PinchGrabOffset=PinchStartRotation.RotateVector(State.Position);
 LastPinchSoundTarget=State.Target;
 StopPhysicsTouch();PlayPinchSound(Region.GrabSound,EWardrobeUISound::PinchGrab);return true;
}
void AWardrobeShowcase::UpdatePinch(const FVector& Origin,const FVector& Direction){
 const auto* Profile=GetInteractionProfile();if(!bInteractionEnabled||IsInteracting()||!Profile||!Profile->PinchRegions.IsValidIndex(ActivePinch)||!PinchStates.IsValidIndex(ActivePinch)){ReleasePinch();return;}
 const auto& Region=Profile->PinchRegions[ActivePinch];if(!Region.bEnabled){ReleasePinch();return;}
 const double Dot=FVector::DotProduct(Direction,PinchPlaneNormal);if(FMath::Abs(Dot)<KINDA_SMALL_NUMBER)return;
 const double Distance=FVector::DotProduct(PinchPlanePoint-Origin,PinchPlaneNormal)/Dot;if(Distance<=0)return;
 auto& State=PinchStates[ActivePinch];
 const FVector MouseDelta=Origin+Direction*Distance-PinchPlanePoint;
 const FVector NewOffset=PinchStartRotation.UnrotateVector(MouseDelta)*Region.AxisScale*FMath::Clamp(Region.DragSensitivity,.01f,4.f);
 State.Target=(PinchStartRotation.UnrotateVector(PinchGrabOffset)+NewOffset).GetClampedToMaxSize(FMath::Max(.1f,Region.MaxDistance));
 if(FVector::Distance(State.Target,LastPinchSoundTarget)>=FMath::Max(.05f,Region.MoveSoundDistance)){
  LastPinchSoundTarget=State.Target;PlayPinchSound(Region.MoveSound,EWardrobeUISound::PinchMove);
 }
}
void AWardrobeShowcase::ReleasePinch(){
 if(ActivePinch==INDEX_NONE)return;
 const auto* Profile=GetInteractionProfile();
 for(auto& State:PinchStates){State.ReleaseReaction=State.Position;State.ReturnAge=0;State.bReturning=true;}
 if(Profile&&Profile->PinchRegions.IsValidIndex(ActivePinch))PlayPinchSound(Profile->PinchRegions[ActivePinch].ReleaseSound,EWardrobeUISound::PinchRelease);
 ActivePinch=INDEX_NONE;for(auto& State:PinchStates)State.Target=FVector::ZeroVector;
}
void AWardrobeShowcase::ClearPinch(){
 if(IsValid(Character))for(const auto& State:PinchStates){if(!State.PullMorph.IsNone())Character->SetMorphTarget(State.PullMorph,0);}
 for(FName Name:PinchDrivenCurves)if(IsValid(Character))Character->SetMorphTarget(Name,0);PinchDrivenCurves.Reset();
 ActivePinch=INDEX_NONE;PinchStates.Reset();
}
void AWardrobeShowcase::TickPinch(float Dt){
 const auto* Profile=GetInteractionProfile();if(!bInteractionEnabled||IsInteracting()||!Profile){if(!PinchStates.IsEmpty())ClearPinch();return;}
 if(!PinchStates.IsEmpty()&&PinchStates.Num()!=Profile->PinchRegions.Num()){ClearPinch();return;}
 for(int32 I=0;I<PinchStates.Num();I++){
  auto& State=PinchStates[I];const auto& Region=Profile->PinchRegions[I];
  if(!Region.bEnabled)State.Target=FVector::ZeroVector;
  if(State.bReturning)State.ReturnAge+=FMath::Max(0.f,Dt);
  const bool bReleased=ActivePinch!=I;
  const float Omega=2*PI*FMath::Clamp(bReleased?Region.ReleaseFrequency*FMath::Clamp(Region.CheekRecoverySpeed,.1f,5.f):Region.Frequency,.1f,30.f),Damping=FMath::Clamp(bReleased?Region.ReleaseDampingRatio:Region.DampingRatio,.1f,2.f);
  float Remaining=FMath::Clamp(Dt,0.f,.1f);
  while(Remaining>SMALL_NUMBER){const float Step=FMath::Min(Remaining,1.f/480.f);State.Velocity+=((State.Target-State.Position)*Omega*Omega-2*Damping*Omega*State.Velocity)*Step;State.Position+=State.Velocity*Step;Remaining-=Step;}
  State.Position=State.Position.GetClampedToMaxSize(FMath::Max(.1f,Region.MaxDistance)*1.15f);
  if(State.Target.IsNearlyZero(.001f)&&State.Position.IsNearlyZero(.001f)&&State.Velocity.IsNearlyZero(.01f)){State.Position=FVector::ZeroVector;State.Velocity=FVector::ZeroVector;}
  const float Amount=FVector::DotProduct(State.Position,Region.PullAxis.GetSafeNormal())/FMath::Max(.1f,Region.MaxDistance)*FMath::Clamp(Region.MorphStrength,0.f,1.f);
  if(!Region.PullMorph.IsNone())Character->SetMorphTarget(Region.PullMorph,FMath::Clamp(Amount,0.f,1.f));
 }
 TMap<FName,float> CurveValues;
 for(int32 I=0;I<PinchStates.Num();I++){
  const auto& Region=Profile->PinchRegions[I];const auto& State=PinchStates[I];
  if(!Region.bEnabled||!Character->GetSkeletalMeshAsset())continue;
  FVector ReactionOffset=State.Position;
  if(State.bReturning){const float T=FMath::Clamp(State.ReturnAge/FMath::Max(.05f,Region.ReactionReturnTime),0.f,1.f);ReactionOffset=State.ReleaseReaction*(1.f-T*T*(3.f-2.f*T));}
  const FVector Displacement=State.StartComponentRotation.RotateVector(ReactionOffset);
  const float Force=FMath::Pow(FMath::Clamp(float(Displacement.Size())/FMath::Max(.1f,Region.MaxDistance),0.f,1.f),FMath::Clamp(Region.ReactionExponent,.1f,4.f));
  const float X=FVector::DotProduct(Displacement,Region.BodyRightAxis.GetSafeNormal()),Y=FVector::DotProduct(Displacement,Region.BodyUpAxis.GetSafeNormal()),Sum=FMath::Abs(X)+FMath::Abs(Y);
  if(Sum<KINDA_SMALL_NUMBER)continue;
  auto ApplyCurves=[&](UAnimSequence* Clip,float Weight){
   if(!Clip||Weight<=0||Clip->GetSkeleton()!=Character->GetSkeletalMeshAsset()->GetSkeleton())return;
   for(const auto& Morph:Character->GetSkeletalMeshAsset()->GetMorphTargets())if(Clip->HasCurveData(Morph->GetFName(),false)){
    const FName Name=Morph->GetFName();
    const float Value=(Clip->EvaluateCurveData(Name,FAnimExtractContext(Force*Clip->GetPlayLength()))-Clip->EvaluateCurveData(Name,FAnimExtractContext(0.)))*FMath::Clamp(Region.ReactionStrength,0.f,1.f)*Weight;
    CurveValues.FindOrAdd(Name)+=Value;
   }
  };
  ApplyCurves(X<0?Region.BodyLeft.Get():Region.BodyRight.Get(),FMath::Abs(X)/Sum);
  ApplyCurves(Y<0?Region.BodyDown.Get():Region.BodyUp.Get(),FMath::Abs(Y)/Sum);
 }
 for(FName Name:PinchDrivenCurves)if(!CurveValues.Contains(Name))Character->SetMorphTarget(Name,0);
 PinchDrivenCurves.Reset();
 for(const auto& Pair:CurveValues){Character->SetMorphTarget(Pair.Key,FMath::Clamp(Pair.Value,-1.f,1.f));PinchDrivenCurves.Add(Pair.Key);}
 if(Profile->bDrawPinchRegions)for(const auto& Region:Profile->PinchRegions){if(!Region.bEnabled)continue;const FName Anchor=Region.AnchorBone.IsNone()?Region.Bone:Region.AnchorBone;if(Character->DoesSocketExist(Anchor)){const FTransform TM=Character->GetSocketTransform(Anchor);DrawDebugSphere(GetWorld(),TM.TransformPosition(Region.Offset),Region.HitRadius*Character->GetComponentTransform().GetMaximumAxisScale(),16,FColor::Green,false,0);}}
}

FQuat AWardrobeShowcase::GetPinchReactionRotation(FName Bone) const{
 const auto* Profile=GetInteractionProfile();if(!Profile||!Character)return FQuat::Identity;
 FQuat Result=FQuat::Identity;
 for(int32 I=0;I<PinchStates.Num();I++){
  if(!Profile->PinchRegions.IsValidIndex(I))continue;
  const auto& Region=Profile->PinchRegions[I];if(!Region.bEnabled)continue;
  if(!Character->GetSkeletalMeshAsset())continue;
  const auto& State=PinchStates[I];
  FVector ReactionOffset=State.Position;
  if(State.bReturning){const float T=FMath::Clamp(State.ReturnAge/FMath::Max(.05f,Region.ReactionReturnTime),0.f,1.f);ReactionOffset=State.ReleaseReaction*(1.f-T*T*(3.f-2.f*T));}
  const FVector Displacement=State.StartComponentRotation.RotateVector(ReactionOffset);
  const float Force=FMath::Pow(FMath::Clamp(float(Displacement.Size())/FMath::Max(.1f,Region.MaxDistance),0.f,1.f),FMath::Clamp(Region.ReactionExponent,.1f,4.f));
  if(Force<KINDA_SMALL_NUMBER)continue;
  auto Apply=[&](UAnimSequence* Clip,float DirectionWeight){
   if(!Clip||Clip->GetSkeleton()!=Character->GetSkeletalMeshAsset()->GetSkeleton()||DirectionWeight<=0)return;
   const int32 Index=Clip->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(Bone);if(Index==INDEX_NONE)return;
   FTransform Neutral,Pull;
   Clip->GetBoneTransform(Neutral,FSkeletonPoseBoneIndex(Index),FAnimExtractContext(0.),false);
   Clip->GetBoneTransform(Pull,FSkeletonPoseBoneIndex(Index),FAnimExtractContext(Force*Clip->GetPlayLength()),false);
   FQuat Delta=Pull.GetRotation()*Neutral.GetRotation().Inverse();Delta.Normalize();
   Result=FQuat::Slerp(FQuat::Identity,Delta,DirectionWeight*FMath::Clamp(Region.ReactionStrength,0.f,1.f))*Result;Result.Normalize();
  };
  if(Region.BodyLeft||Region.BodyRight||Region.BodyUp||Region.BodyDown){
   const float X=FVector::DotProduct(Displacement,Region.BodyRightAxis.GetSafeNormal()),Y=FVector::DotProduct(Displacement,Region.BodyUpAxis.GetSafeNormal());
   const float Sum=FMath::Abs(X)+FMath::Abs(Y);if(Sum>KINDA_SMALL_NUMBER){
    Apply(X<0?Region.BodyLeft.Get():Region.BodyRight.Get(),FMath::Abs(X)/Sum);
    Apply(Y<0?Region.BodyDown.Get():Region.BodyUp.Get(),FMath::Abs(Y)/Sum);
   }
  }

 }
 return Result;
}
