#pragma once
#include "CoreMinimal.h"
#include "ExternalForces/KawaiiPhysicsExternalForce.h"
#include "WardrobeKawaiiForce.generated.h"

/** Immutable click snapshot copied into each matching animation node. */
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeKawaiiClickForce : public FKawaiiPhysics_ExternalForce {
 GENERATED_BODY()
 UPROPERTY() FVector WorldPosition=FVector::ZeroVector;
 UPROPERTY() FVector WorldDirection=FVector::ForwardVector;
 UPROPERTY() float Radius=30;
 UPROPERTY() float Strength=100;
 UPROPERTY() float Duration=.25f;
 UPROPERTY() float Falloff=2;
 UPROPERTY() float RadialBlend=.3f;
 // Numeric snapshots only; updated on the game thread after joining anim evaluation.
 bool bDragging=false;
 FVector DragDelta=FVector::ZeroVector;
 float DragStiffness=12;
 float MaxDragSpeed=200;
 TArray<FVector> DragOrigins;
 TArray<float> DragWeights;
 TArray<FName> DragBoneNames;
 static FVector BoneWorldPosition(const FAnimNode_KawaiiPhysics& Node,const FTransform& Component,const FVector& Position);
 static bool RaySphere(const FVector& Origin,const FVector& Direction,const FVector& Center,float Radius,float& Distance);
 float Age=0;
 float StepWeight=0;
 virtual void PreApply(FAnimNode_KawaiiPhysics& Node,FComponentSpacePoseContext& Context) override;
 virtual void Apply(FKawaiiPhysicsModifyBone& Bone,FAnimNode_KawaiiPhysics& Node,FComponentSpacePoseContext& Context,const FTransform& BoneTM) override;
 static float SpatialWeight(float Distance,float InRadius,float Exponent);
};
