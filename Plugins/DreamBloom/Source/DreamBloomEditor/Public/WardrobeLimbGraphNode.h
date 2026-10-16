#pragma once
#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "WardrobeLimbNode.h"
#include "WardrobeLimbGraphNode.generated.h"
UCLASS()
class DREAMBLOOMEDITOR_API UAnimGraphNode_WardrobeLimbReaction : public UAnimGraphNode_Base {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="Settings") FAnimNode_WardrobeLimbReaction Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type) const override{return FText::FromString(TEXT("Wardrobe Limb Reaction"));}
 virtual FText GetTooltipText() const override{return FText::FromString(TEXT("DA four-direction additive body feedback. Before ground IK."));}
};
UCLASS()
class DREAMBLOOMEDITOR_API UAnimGraphNode_WardrobeLimbIK : public UAnimGraphNode_SkeletalControlBase {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="Settings") FAnimNode_WardrobeLimbIK Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type) const override{return FText::FromString(TEXT("Wardrobe Limb IK"));}
 virtual FText GetTooltipText() const override{return FText::FromString(TEXT("Mouse hand/foot endpoint IK. After ground IK, before KawaiiPhysics. Settings come from DA_WardrobeCharacters."));}
 virtual FText GetControllerDescription() const override{return GetNodeTitle(ENodeTitleType::FullTitle);}
protected:
 virtual const FAnimNode_SkeletalControlBase* GetNode() const override{return &Node;}
};
