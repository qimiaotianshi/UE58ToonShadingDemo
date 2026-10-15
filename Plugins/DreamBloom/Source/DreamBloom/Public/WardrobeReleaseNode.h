#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "WardrobeReleaseNode.generated.h"
class UAnimSequence;
/** Place after pinch controls and before secondary physics. Uses the last evaluated held pose. */
USTRUCT(BlueprintInternalUseOnly)
struct DREAMBLOOM_API FAnimNode_WardrobeRelease : public FAnimNode_Base {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,Category="Links") FPoseLink Source;
 virtual bool HasPreUpdate() const override{return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
 virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
 virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
 virtual void Evaluate_AnyThread(FPoseContext& Output) override;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> Clip;
 UPROPERTY(Transient) TObjectPtr<UAnimSequence> MaskClip;
 float Time=0,BlendTime=.25f;
 int32 Serial=0,LastSerial=0;
 TArray<FTransform> LastPose,StartPose;
 TArray<bool> AnimatedBones;
 FBlendedCurve LastCurve,StartCurve;
};
