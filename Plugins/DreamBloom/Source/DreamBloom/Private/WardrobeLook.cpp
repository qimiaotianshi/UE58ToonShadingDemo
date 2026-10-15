#include "WardrobeLook.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
namespace {
FTransform ReferenceComponent(const USkeletalMesh* Mesh,FName Bone){
 const auto& Ref=Mesh->GetRefSkeleton();int32 I=Ref.FindBoneIndex(Bone);if(I==INDEX_NONE)return FTransform::Identity;
 FTransform T=Ref.GetRefBonePose()[I];while((I=Ref.GetParentIndex(I))!=INDEX_NONE)T=T*Ref.GetRefBonePose()[I];return T;
}
}
void AWardrobeShowcase::TickCursorLook(float Dt){
 auto* PC=GetWorld()?GetWorld()->GetFirstPlayerController():nullptr;FVector O,D;float X=0,Y=0;
 bool Valid=PC&&PC->GetMousePosition(X,Y)&&PC->DeprojectScreenPositionToWorld(X,Y,O,D);
 if(PC){int32 W=0,H=0;PC->GetViewportSize(W,H);Valid&=X>=0&&Y>=0&&X<W&&Y<H;
  Valid&=!PC->IsInputKeyDown(EKeys::LeftMouseButton)&&!PC->IsInputKeyDown(EKeys::RightMouseButton)&&!PC->IsInputKeyDown(EKeys::MiddleMouseButton);
 }
 UpdateCursorLook(Dt,O,D,Valid);
}
void AWardrobeShowcase::UpdateCursorLook(float Dt,const FVector& Origin,const FVector& Ray,bool bHasPointer){
 const auto* Profile=GetInteractionProfile();FVector2D Target=FVector2D::ZeroVector;
 bool Busy=!bInteractionEnabled||IsInteracting()||IsReleaseAnimationPlaying()||IsPinching()||bPhysicsDragging||bPhysicsTouchActive;
 bool bPinchReturning=false;float GazeBlend=1.f;
 for(int32 I=0;I<PinchStates.Num();I++){
  const auto& State=PinchStates[I];
  if(State.bReturning&&Profile&&Profile->PinchRegions.IsValidIndex(I)){
   bPinchReturning=true;
   const float Duration=FMath::Max(.05f,Profile->PinchRegions[I].ReactionReturnTime),Blend=FMath::Min(Duration,FMath::Max(.05f,Profile->CursorLook.PinchToLookBlendTime));
   const float T=FMath::Clamp((State.ReturnAge-(Duration-Blend))/Blend,0.f,1.f);
   GazeBlend=FMath::Min(GazeBlend,T*T*(3.f-2.f*T));
  }else Busy|=State.Position.SizeSquared()>.0001;
 }
 if(Busy||!bHasPointer)CursorLookIdleAge=0;else if(bPinchReturning)CursorLookIdleAge=Profile->CursorLook.IdleDelay;else CursorLookIdleAge+=FMath::Max(0.f,Dt);
 if(Profile&&Profile->CursorLook.bEnabled&&Character&&Character->GetSkeletalMeshAsset()&&!Busy&&bHasPointer&&CursorLookIdleAge>=Profile->CursorLook.IdleDelay){
  const auto& S=Profile->CursorLook;const auto* Mesh=Character->GetSkeletalMeshAsset();
  if(Mesh->GetRefSkeleton().FindBoneIndex(S.AnchorBone)!=INDEX_NONE){
   // Stable reference anchor avoids a feedback loop from the already rotated head.
   const FVector Anchor=Character->GetComponentTransform().TransformPosition(ReferenceComponent(Mesh,S.AnchorBone).GetLocation());
   const FVector LocalDirection=Character->GetComponentQuat().UnrotateVector(Origin+Ray.GetSafeNormal()*FVector::Distance(Origin,Anchor)*.65f-Anchor).GetSafeNormal();
   const FVector Forward=S.ForwardAxis.GetSafeNormal(),Up=S.UpAxis.GetSafeNormal(),Right=FVector::CrossProduct(Forward,Up).GetSafeNormal();
   const float Front=FVector::DotProduct(LocalDirection,Forward);
   if(Front>0&&!Right.IsNearlyZero()){
    Target.X=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(LocalDirection,Right),Front)),-S.MaxYaw,S.MaxYaw);
    Target.Y=FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(LocalDirection,Up),FMath::Sqrt(FMath::Square(Front)+FMath::Square(FVector::DotProduct(LocalDirection,Right))))),-S.MaxPitch,S.MaxPitch);
   }
  }
 }
 Target*=GazeBlend;
 const float Speed=Profile?FMath::Max(.1f,Profile->CursorLook.FollowSpeed):4.f;
 CursorLookAngles=FMath::Lerp(CursorLookAngles,Target,1.f-FMath::Exp(-Speed*FMath::Clamp(Dt,0.f,.1f)));
}
FQuat AWardrobeShowcase::GetCursorLookRotation(FName Bone) const{
 const auto* Profile=GetInteractionProfile();if(!Profile||!Profile->CursorLook.bEnabled||!Character||!Character->GetSkeletalMeshAsset())return FQuat::Identity;
 const auto& S=Profile->CursorLook;float Weight=0;for(const auto& Entry:S.Bones)if(Entry.Bone==Bone)Weight+=Entry.Weight;
 if(Weight<=0||Character->GetSkeletalMeshAsset()->GetRefSkeleton().FindBoneIndex(Bone)==INDEX_NONE)return FQuat::Identity;
 const FVector Forward=S.ForwardAxis.GetSafeNormal(),Up=S.UpAxis.GetSafeNormal(),Right=FVector::CrossProduct(Forward,Up).GetSafeNormal();
 FQuat ComponentDelta=FQuat(Up,FMath::DegreesToRadians(-float(CursorLookAngles.X)*Weight))*FQuat(Right,FMath::DegreesToRadians(float(CursorLookAngles.Y)*Weight));
 const FQuat RefRotation=ReferenceComponent(Character->GetSkeletalMeshAsset(),Bone).GetRotation();
 return RefRotation.Inverse()*ComponentDelta*RefRotation;
}
