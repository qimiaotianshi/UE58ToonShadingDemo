#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "WardrobeSpeechNode.generated.h"
USTRUCT(BlueprintInternalUseOnly)
struct DREAMBLOOM_API FAnimNode_WardrobeSpeech : public FAnimNode_Base {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,Category="Links") FPoseLink Source;
 virtual bool HasPreUpdate() const override{return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
 virtual void Initialize_AnyThread(const FAnimationInitializeContext& C) override;
 virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& C) override{Source.CacheBones(C);}
 virtual void Update_AnyThread(const FAnimationUpdateContext& C) override{Source.Update(C);}
 virtual void Evaluate_AnyThread(FPoseContext& Output) override;
 TArray<FName> Names;TArray<float> Values;float Weight=0;
};
