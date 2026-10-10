#pragma once
#include "CoreMinimal.h"
#include "BoneControllers/AnimNode_ModifyBone.h"
#include "WardrobePinchNode.generated.h"
/** Join-free worker evaluation: collect the immutable offset on the game thread. */
USTRUCT(BlueprintInternalUseOnly)
struct DREAMBLOOM_API FAnimNode_WardrobePinch : public FAnimNode_ModifyBone {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,Category="Reaction") bool bReactionBone=false;
 FAnimNode_WardrobePinch();
 virtual bool HasPreUpdate() const override{return true;}
 virtual void PreUpdate(const UAnimInstance* Instance) override;
};
