#include "WardrobeKawaiiForce.h"

float FWardrobeKawaiiClickForce::SpatialWeight(float Distance,float InRadius,float Exponent){
 return FMath::Pow(FMath::Clamp(1.f-Distance/FMath::Max(1.f,InRadius),0.f,1.f),FMath::Max(.1f,Exponent));
}
void FWardrobeKawaiiClickForce::PreApply(FAnimNode_KawaiiPhysics& Node,FComponentSpacePoseContext& Context){
 // No UObject lookups, traces or gameplay state reads on the animation worker.
 StepWeight=FMath::Square(FMath::Clamp(1.f-Age/FMath::Max(.02f,Duration),0.f,1.f));
 Age+=FMath::Max(0.f,Node.GetStepDeltaTime());
}
void FWardrobeKawaiiClickForce::Apply(FKawaiiPhysicsModifyBone& Bone,FAnimNode_KawaiiPhysics& Node,FComponentSpacePoseContext& Context,const FTransform& BoneTM){
 if(!bIsEnabled||!CanApply(Bone)||Bone.ParentIndex<0)return;
 if(bDragging){
  const int32 I=int32(&Bone-Node.ModifyBones.GetData());
  if(!DragOrigins.IsValidIndex(I)||!DragWeights.IsValidIndex(I)||DragWeights[I]<=0)return;
  if(DragBoneNames.IsValidIndex(I)&&DragBoneNames[I]!=Bone.BoneRef.BoneName)return;
  const FVector World=Node.ConvertSimulationSpaceLocation(Context,Node.SimulationSpace,EKawaiiPhysicsSimulationSpace::WorldSpace,Bone.Location);
  const float Dt=FMath::Clamp(Node.GetStepDeltaTime(),0.f,.05f);
  FVector Delta=(DragOrigins[I]+DragDelta-World)*(1.f-FMath::Exp(-DragStiffness*Dt))*DragWeights[I];
  Delta=Delta.GetClampedToMaxSize(MaxDragSpeed*Dt);
  Bone.Location+=Node.ConvertSimulationSpaceVector(Context,EKawaiiPhysicsSimulationSpace::WorldSpace,Node.SimulationSpace,Delta);
  return;
 }
 if(StepWeight<=0)return;
 const FVector Position=Node.ConvertSimulationSpaceLocation(Context,Node.SimulationSpace,EKawaiiPhysicsSimulationSpace::WorldSpace,Bone.Location);
 const FVector Radial=Position-WorldPosition;
 const float Weight=SpatialWeight(Radial.Size(),Radius,Falloff)*StepWeight;
 if(Weight<=0)return;
 const FVector Direction=FMath::Lerp(WorldDirection.GetSafeNormal(),Radial.GetSafeNormal(),FMath::Clamp(RadialBlend,0.f,1.f)).GetSafeNormal();
 const FVector Delta=Node.ConvertSimulationSpaceVector(Context,EKawaiiPhysicsSimulationSpace::WorldSpace,Node.SimulationSpace,Direction*Strength*Weight*FMath::Min(Node.GetStepDeltaTime(),.05f));
 Bone.Location+=Delta;
}

FVector FWardrobeKawaiiClickForce::BoneWorldPosition(const FAnimNode_KawaiiPhysics& Node,const FTransform& Component,const FVector& Position){
 if(Node.SimulationSpace==EKawaiiPhysicsSimulationSpace::WorldSpace)return Position;
 return Component.TransformPosition(Node.SimulationSpace==EKawaiiPhysicsSimulationSpace::BaseBoneSpace?Node.GetBaseBoneSpace2ComponentSpace().TransformPosition(Position):Position);
}
bool FWardrobeKawaiiClickForce::RaySphere(const FVector& Origin,const FVector& Direction,const FVector& Center,float Radius,float& Distance){
 if(Direction.IsNearlyZero()||Radius<=0)return false;
 const FVector D=Direction.GetSafeNormal(),V=Origin-Center;
 const double B=FVector::DotProduct(V,D),C=V.SizeSquared()-FMath::Square(Radius),Disc=B*B-C;
 if(Disc<0)return false;
 const double Far=-B+FMath::Sqrt(Disc);if(Far<0)return false;
 Distance=FMath::Max(0.,-B-FMath::Sqrt(Disc));return true;
}
