#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "WardrobeLimbDrag.h"
#include "WardrobeLimbNode.generated.h"

/** Local additive feedback before foot planting, sampled by drag strength rather than wall clock. */
USTRUCT(BlueprintInternalUseOnly)
struct DREAMBLOOM_API FAnimNode_WardrobeLimbReaction : public FAnimNode_Base {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,Category="Links") FPoseLink Source;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> Horizontal;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> Vertical;
 float Force=0,Strength=0,HorizontalWeight=0,VerticalWeight=0;
 virtual bool HasPreUpdate() const override{return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void Initialize_AnyThread(const FAnimationInitializeContext& C) override{Source.Initialize(C);}
 virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& C) override{Source.CacheBones(C);}
 virtual void Update_AnyThread(const FAnimationUpdateContext& C) override{Source.Update(C);}
 virtual void Evaluate_AnyThread(FPoseContext& Output) override;
};

/** Final endpoint IK, after ground planting and before secondary physics. */
USTRUCT(BlueprintInternalUseOnly)
struct DREAMBLOOM_API FAnimNode_WardrobeLimbIK : public FAnimNode_SkeletalControlBase {
 GENERATED_BODY()
 FBoneReference Upper,Joint,End;
 FVector Target=FVector::ZeroVector,PoleDirection=FVector::ForwardVector;
 float DragAlpha=0;
 virtual bool HasPreUpdate() const override{return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void InitializeBoneReferences(const FBoneContainer&) override{}
 virtual bool IsValidToEvaluate(const USkeleton*,const FBoneContainer&) override{return DragAlpha>0;}
 virtual void EvaluateSkeletalControl_AnyThread(FComponentSpacePoseContext& Output,TArray<FBoneTransform>& Out) override;
};
