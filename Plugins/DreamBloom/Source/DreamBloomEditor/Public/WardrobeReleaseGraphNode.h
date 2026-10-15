#pragma once
#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "WardrobeReleaseNode.h"
#include "WardrobeReleaseGraphNode.generated.h"
UCLASS()
class DREAMBLOOMEDITOR_API UAnimGraphNode_WardrobeRelease : public UAnimGraphNode_Base {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="Settings") FAnimNode_WardrobeRelease Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type Type) const override{return FText::FromString(TEXT("Wardrobe Release Blend"));}
 virtual FText GetTooltipText() const override{return FText::FromString(TEXT("Blend held pinch pose into DA release animation. Place after pinch nodes and before KawaiiPhysics."));}
};
