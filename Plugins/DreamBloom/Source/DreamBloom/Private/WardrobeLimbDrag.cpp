#include "WardrobeLimbDrag.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeKawaiiForce.h"
#include "WardrobeLimbNode.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "UObject/UnrealType.h"

namespace WardrobeLimb {
bool ValidChain(const USkeletalMesh* Mesh,const FWardrobeLimbDrag& R){
 if(!Mesh)return false;const auto& Ref=Mesh->GetRefSkeleton();
 const int U=Ref.FindBoneIndex(R.UpperBone),J=Ref.FindBoneIndex(R.JointBone),E=Ref.FindBoneIndex(R.EndBone);
 auto Descendant=[&](int Child,int Parent){if(Child==INDEX_NONE||Parent==INDEX_NONE||Child==Parent)return false;while((Child=Ref.GetParentIndex(Child))!=INDEX_NONE)if(Child==Parent)return true;return false;};
 return Descendant(J,U)&&Descendant(E,J);
}
}
bool AWardrobeShowcase::BeginLimbDrag(const FVector& Origin,const FVector& Direction){
 const auto* P=GetInteractionProfile();
 if(!bInteractionEnabled||IsInteracting()||IsSequenceSwitching()||IsPinching()||IsReleaseAnimationPlaying()||!P||!Character||Direction.IsNearlyZero())return false;
 auto* Instance=Character->GetAnimInstance();bool HasSolver=false;
 if(Instance)for(TFieldIterator<FStructProperty> Prop(Instance->GetClass());Prop;++Prop)if(Prop->Struct==FAnimNode_WardrobeLimbIK::StaticStruct()){HasSolver=true;break;}
 if(!HasSolver)return false;
 Character->HandleExistingParallelEvaluationTask(true,true);
 const FVector Ray=Direction.GetSafeNormal();float Best=100000;int Pick=INDEX_NONE;
 FCollisionQueryParams Params(SCENE_QUERY_STAT(WardrobeLimb),false);Params.AddIgnoredActor(this);FHitResult Block;
 if(GetWorld()->LineTraceSingleByChannel(Block,Origin,Origin+Ray*Best,ECC_Visibility,Params))Best=Block.Distance;
 for(int I=0;I<P->LimbDrags.Num();I++){
  const auto& R=P->LimbDrags[I];if(!R.bEnabled||!WardrobeLimb::ValidChain(Character->GetSkeletalMeshAsset(),R))continue;
  const FVector Center=Character->GetSocketTransform(R.EndBone).TransformPosition(R.HitOffset);float Distance=0;
  if(FWardrobeKawaiiClickForce::RaySphere(Origin,Ray,Center,FMath::Max(.1f,R.HitRadius)*Character->GetComponentTransform().GetMaximumAxisScale(),Distance)&&Distance<Best){Best=Distance;Pick=I;}
 }
 if(Pick==INDEX_NONE)return false;
 const auto& R=P->LimbDrags[Pick];
 ClearLimbDrag();StopPhysicsTouch();ActiveLimb=Pick;
 LimbStartWorld=Character->GetSocketLocation(R.EndBone);LimbPlanePoint=Origin+Ray*Best;LimbPlaneNormal=Ray;
 // Camera-facing plane; preserve the original click offset so the endpoint never snaps to the pointer.
 LimbScreenRight=FVector::CrossProduct(Ray,FVector::UpVector).GetSafeNormal();
 if(LimbScreenRight.IsNearlyZero())LimbScreenRight=Character->GetRightVector();
 LimbScreenUp=FVector::CrossProduct(LimbScreenRight,Ray).GetSafeNormal();
 const FVector Upper=Character->GetSocketLocation(R.UpperBone),Joint=Character->GetSocketLocation(R.JointBone);
 const FVector Line=(LimbStartWorld-Upper).GetSafeNormal();LimbPole=Joint-Upper;LimbPole-=Line*FVector::DotProduct(LimbPole,Line);
 if(!LimbPole.Normalize()){LimbPole=Character->GetComponentTransform().TransformVectorNoScale(FVector::ForwardVector);LimbPole-=Line*FVector::DotProduct(LimbPole,Line);if(!LimbPole.Normalize())LimbPole=LimbScreenRight;}
 return true;
}
void AWardrobeShowcase::UpdateLimbDrag(const FVector& Origin,const FVector& Direction){
 if(!IsLimbDragging())return;const auto* P=GetInteractionProfile();
 if(!bInteractionEnabled||IsInteracting()||!P||!P->LimbDrags.IsValidIndex(ActiveLimb)||!P->LimbDrags[ActiveLimb].bEnabled){ReleaseLimbDrag();return;}
 const double Dot=FVector::DotProduct(Direction,LimbPlaneNormal);if(FMath::Abs(Dot)<KINDA_SMALL_NUMBER)return;
 const double T=FVector::DotProduct(LimbPlanePoint-Origin,LimbPlaneNormal)/Dot;if(T<=0)return;
 const auto& R=P->LimbDrags[ActiveLimb];const float Scale=Character->GetComponentScale().GetAbsMax();
 LimbTargetDelta=((Origin+Direction*T-LimbPlanePoint)*FMath::Clamp(R.DragSensitivity,.01f,4.f)).GetClampedToMaxSize(FMath::Max(1.f,R.MaxDistance)*Scale);
}
void AWardrobeShowcase::ReleaseLimbDrag(){if(IsLimbDragging()){bLimbReturning=true;LimbReturnAge=0;}}
void AWardrobeShowcase::ClearLimbDrag(){ActiveLimb=INDEX_NONE;bLimbReturning=false;LimbReturnAge=0;LimbDelta=LimbTargetDelta=FVector::ZeroVector;}
void AWardrobeShowcase::TickLimbDrag(float Dt){
 const auto* P=GetInteractionProfile();
 if(!bInteractionEnabled||IsInteracting()||!P){ClearLimbDrag();return;}
 if(HasLimbPose()){
  if(!P->LimbDrags.IsValidIndex(ActiveLimb)||!P->LimbDrags[ActiveLimb].bEnabled){ClearLimbDrag();return;}
  const auto& R=P->LimbDrags[ActiveLimb];
  if(bLimbReturning){LimbReturnAge+=FMath::Max(0.f,Dt);if(LimbReturnAge>=FMath::Max(0.f,R.ReturnTime))ClearLimbDrag();}
  else LimbDelta=FMath::Lerp(LimbDelta,LimbTargetDelta,R.FollowTime<=0?1.f:1.f-FMath::Exp(-4.6f*FMath::Max(0.f,Dt)/R.FollowTime));
 }
 if(P->bDrawLimbDrags){
  for(const auto& R:P->LimbDrags)if(R.bEnabled&&Character->DoesSocketExist(R.EndBone))DrawDebugSphere(GetWorld(),Character->GetSocketTransform(R.EndBone).TransformPosition(R.HitOffset),R.HitRadius*Character->GetComponentScale().GetAbsMax(),16,FColor::Cyan,false,0);
  if(HasLimbPose())DrawDebugLine(GetWorld(),LimbStartWorld,GetLimbSnapshot().TargetWorld,FColor::Yellow,false,0,0,2);
 }
}
FWardrobeLimbSnapshot AWardrobeShowcase::GetLimbSnapshot() const{
 FWardrobeLimbSnapshot S;const auto* P=GetInteractionProfile();
 if(!bInteractionEnabled||!P||!P->LimbDrags.IsValidIndex(ActiveLimb))return S;
 S.Settings=P->LimbDrags[ActiveLimb];
 const float T=bLimbReturning?FMath::Clamp(LimbReturnAge/FMath::Max(.001f,S.Settings.ReturnTime),0.f,1.f):0.f;
 S.Alpha=1.f-T*T*(3.f-2.f*T);S.TargetWorld=LimbStartWorld+LimbDelta;S.PoleDirection=LimbPole;
 S.Direction=FVector2D(FVector::DotProduct(LimbDelta,LimbScreenRight),FVector::DotProduct(LimbDelta,LimbScreenUp));
 S.Force=FMath::Clamp(float(LimbDelta.Size())/FMath::Max(1.f,S.Settings.MaxDistance*Character->GetComponentScale().GetAbsMax()),0.f,1.f)*S.Alpha;
 return S;
}
float AWardrobeShowcase::GetLimbGroundSuppression(FName Foot) const{const auto S=GetLimbSnapshot();return S.Settings.bFoot&&S.Settings.EndBone==Foot?S.Alpha:0.f;}
