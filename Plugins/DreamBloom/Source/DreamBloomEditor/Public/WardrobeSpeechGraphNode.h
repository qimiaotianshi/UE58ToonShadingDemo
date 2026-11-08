#pragma once
#include "CoreMinimal.h"
#include "AnimGraphNode_Base.h"
#include "WardrobeSpeechNode.h"
#include "WardrobeSpeechGraphNode.generated.h"
UCLASS()
class DREAMBLOOMEDITOR_API UAnimGraphNode_WardrobeSpeech : public UAnimGraphNode_Base {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,Category="Settings") FAnimNode_WardrobeSpeech Node;
 virtual FText GetNodeTitle(ENodeTitleType::Type) const override{return FText::FromString(TEXT("Wardrobe AI Speech"));}
 virtual FText GetTooltipText() const override{return FText::FromString(TEXT("Overlay transient audio-driven face curves. Place before Output Pose; bones and physics are preserved."));}
 virtual FString GetNodeCategory() const override{return TEXT("Wardrobe");}
};
