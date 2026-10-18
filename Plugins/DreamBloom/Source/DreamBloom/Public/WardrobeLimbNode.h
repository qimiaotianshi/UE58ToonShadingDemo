#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "BoneControllers/AnimNode_SkeletalControlBase.h"
#include "WardrobeLimbDrag.h"
#include "WardrobeLimbNode.generated.h"

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
