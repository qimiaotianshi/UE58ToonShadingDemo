#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WardrobeInteraction.h"
#include "WardrobeAudio.h"
#include "WardrobeFeedback.h"
#include "WardrobePinch.h"
#include "WardrobeLook.h"
#include "WardrobeLimbDrag.h"
#include "WardrobeCharacters.generated.h"

class UCurveFloat;
class ULevelSequence;

USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeCharacterOption {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Character") FText Label;
 /** Loops while this character is available in the preview. Interactions interrupt it and restart it from the beginning. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Default Sequence") TSoftObjectPtr<ULevelSequence> DefaultSequence;
 /** Shared duration for fading physics out before a sequence switch and back in afterwards. Zero switches immediately. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sequence Physics Blend",meta=(ClampMin="0",Units="s")) float PhysicsFadeTime=.2f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch") FWardrobePhysicsTouch PhysicsTouch;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pinch") TArray<FWardrobePinchRegion> PinchRegions;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pinch") bool bDrawPinchRegions=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Limb Drag",meta=(TitleProperty="Label")) TArray<FWardrobeLimbDrag> LimbDrags;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Limb Drag") bool bDrawLimbDrags=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Cursor Look") FWardrobeCursorLook CursorLook;
 /** Actor Tags (not component tags): exactly one actor of the required type per tag. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Scene Binding") FName CameraTag;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Scene Binding") FName CharacterTag;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Transition",meta=(ClampMin="0",Units="s")) float Duration=1.5f;
 /** Play when switching into this entry. Empty falls back to DA_WardrobeAudio Station Switch. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Transition") FWardrobeTransitionSound TransitionSound;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Transition") TObjectPtr<UCurveFloat> ProgressCurve;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Transition") FVector DepartureOffset=FVector(0,0,60);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Transition") FVector ArrivalOffset=FVector(0,0,60);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Character") bool bAllowCharacterRotation=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Interaction") TArray<FWardrobeInteractionRegion> Interactions;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Interaction",meta=(ClampMin="0",Units="s")) float InteractionCooldown=.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Interaction",meta=(ClampMin="1",Units="s")) float MaxInteractionDuration=30.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Interaction") bool bShowInteractionRegions=false;
};

UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeCharacterData : public UDataAsset {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Characters") TArray<FWardrobeCharacterOption> Characters;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Characters",meta=(ClampMin="0")) int32 InitialCharacter=0;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Debug") bool bDrawTransitionPath=false;
};
