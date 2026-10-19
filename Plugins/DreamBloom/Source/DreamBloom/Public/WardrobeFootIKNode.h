#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "WardrobeFootIKNode.generated.h"

/** Ground adaptation after the authored pose, before secondary physics. */
USTRUCT(BlueprintInternalUseOnly)
struct DREAMBLOOM_API FAnimNode_WardrobeFootIK : public FAnimNode_SkeletalControlBase
{
 GENERATED_BODY()
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ground", meta=(PinHiddenByDefault)) bool bEnabled=true;
 /** Use baked shoe sole contact samples instead of a fixed ankle height. */
 UPROPERTY(EditAnywhere, Category="Ground") bool bAutoSoleContact=true;
 UPROPERTY(EditAnywhere, Category="Bones") FBoneReference Pelvis;
 UPROPERTY(EditAnywhere, Category="Bones") FBoneReference LeftFoot;
 UPROPERTY(EditAnywhere, Category="Bones") FBoneReference RightFoot;
 UPROPERTY(EditAnywhere, Category="Bones") FBoneReference LeftToe;
 UPROPERTY(EditAnywhere, Category="Bones") FBoneReference RightToe;
 /** Ankle to shoe sole distance in mesh/component centimeters. */
 UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="0", Units="cm")) float SoleHeight=6.f;
 UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="1", Units="cm")) float TraceDistance=100.f;
 UPROPERTY(EditAnywhere, Category="Ground") TEnumAsByte<ECollisionChannel> TraceChannel=ECC_Visibility;
 UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="0", Units="cm")) float MaxPelvisOffset=60.f;
 /** Lifted feet outside this ground distance keep their authored animation. */
 UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="0", Units="cm")) float MaxPlantHeight=18.f;
 UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="0",ClampMax="60",Units="deg")) float MaxSlopeAngle=45.f;
 UPROPERTY(EditAnywhere, Category="Ground", meta=(ClampMin="0",Units="s")) float BlendTime=.15f;
 /** Optional locomotion-only horizontal foot locking, driven by authored plant curves. */
 UPROPERTY(EditAnywhere, Category="Foot Lock") bool bLockPlantedFeet=false;
 UPROPERTY(EditAnywhere, Category="Foot Lock") FName LeftPlantCurve=TEXT("ALS_FootPlant_L");
 UPROPERTY(EditAnywhere, Category="Foot Lock") FName RightPlantCurve=TEXT("ALS_FootPlant_R");
 UPROPERTY(EditAnywhere, Category="Foot Lock", meta=(ClampMin="0",Units="cm")) float MaxFootLockDistance=35.f;
 /** Ease the existing correction back to the authored swing pose on lift-off. */
 UPROPERTY(EditAnywhere, Category="Foot Lock", meta=(ClampMin="0.01",Units="s")) float FootLockReleaseTime=.12f;
 UPROPERTY(EditAnywhere, Category="Debug") bool bDebugDraw=false;
 virtual bool HasPreUpdate() const override {return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
 virtual void UpdateInternal(const FAnimationUpdateContext& Context) override;
 virtual void InitializeBoneReferences(const FBoneContainer& Bones) override;
 virtual bool IsValidToEvaluate(const USkeleton*, const FBoneContainer& Bones) override;
 virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out) override;
 bool BuildContactData(class USkeletalMesh* Mesh);
 UPROPERTY() TObjectPtr<USkeletalMesh> ContactMesh;
 UPROPERTY() TArray<FVector> LeftSolePoints;
 UPROPERTY() TArray<FVector> RightSolePoints;
 UPROPERTY() FVector LeftSoleNormal=FVector::UpVector;
 UPROPERTY() FVector RightSoleNormal=FVector::UpVector;
 UPROPERTY() float LeftReferenceHeight=0;
 UPROPERTY() float RightReferenceHeight=0;
 struct FGroundSample {bool bHit=false;FVector Point=FVector::ZeroVector,Normal=FVector::UpVector;};
 FGroundSample Ground[2];
 FTransform MeshWorld=FTransform::Identity;
 FVector SmoothedOffset[2]={FVector::ZeroVector,FVector::ZeroVector};
 FQuat SmoothedRotation[2]={FQuat::Identity,FQuat::Identity};
 float PelvisOffset=0,DeltaTime=0;
 bool bSnap=true;
 float LastPlant[2]={0,0};
 float RuntimeGroundAlpha=1;
 bool bRuntimeInAir=false;
 float DragSuppression[2]={0,0};
 bool bFootLocked[2]={false,false};
 FVector LockedFootWorld[2]={FVector::ZeroVector,FVector::ZeroVector};
 FVector LockForward[2]={FVector::ForwardVector,FVector::ForwardVector};
 bool bLockReleasing[2]={false,false};
 bool bLockCanAcquire[2]={true,true};
 FVector LockReleaseOffset[2]={FVector::ZeroVector,FVector::ZeroVector};
 float LockReleaseElapsed[2]={0,0};
 FVector UpdateFootLock(int Side,const FVector& WorldFoot,float Lock,bool bCanLock);
};
