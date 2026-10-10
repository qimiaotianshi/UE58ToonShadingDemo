#pragma once
#include "CoreMinimal.h"
#include "AnimGraphNode_SkeletalControlBase.h"
#include "WardrobePinchNode.h"
#include "WardrobePinchGraphNode.generated.h"
UCLASS()
class DREAMBLOOMEDITOR_API UAnimGraphNode_WardrobePinch : public UAnimGraphNode_SkeletalControlBase {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="Settings") FAnimNode_WardrobePinch Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type Type) const override{return FText::FromString(TEXT("Wardrobe Pinch (")+Node.BoneToModify.BoneName.ToString()+TEXT(")"));}
 virtual FText GetTooltipText() const override{return FText::FromString(TEXT("Apply temporary DA-configured mouse deformation after the input pose."));}
protected:
 virtual FText GetControllerDescription() const override{return FText::FromString(TEXT("Wardrobe Pinch"));}
 virtual const FAnimNode_SkeletalControlBase* GetNode() const override{return &Node;}
};
