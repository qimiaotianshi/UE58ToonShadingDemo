#pragma once
#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "WardrobeFootIKNode.h"
#include "WardrobeFootIKGraphNode.generated.h"
UCLASS()
class DREAMBLOOMEDITOR_API UAnimGraphNode_WardrobeFootIK : public UAnimGraphNode_SkeletalControlBase {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="Settings") FAnimNode_WardrobeFootIK Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type) const override {return FText::FromString(TEXT("Wardrobe Foot IK"));}
 virtual FText GetTooltipText() const override {return FText::FromString(TEXT("Adapt both feet and pelvis to collision ground. Component space; after animation/release blending, before KawaiiPhysics."));}
 virtual FText GetControllerDescription() const override {return GetNodeTitle(ENodeTitleType::FullTitle);}
protected:
 virtual const FAnimNode_SkeletalControlBase* GetNode() const override {return &Node;}
};
