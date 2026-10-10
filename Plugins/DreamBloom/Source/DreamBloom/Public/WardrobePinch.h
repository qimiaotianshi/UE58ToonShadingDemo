#pragma once
#include "CoreMinimal.h"
#include "WardrobeAudio.h"
#include "WardrobePinch.generated.h"

class UAnimSequence;

/** A temporary local deformation; helper bones must have authored skin weights. */
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobePinchRegion {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pinch") FText Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Pinch") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Binding") FName Bone;
 /** Optional stable picking anchor. None uses the deform bone. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Binding") FName AnchorBone;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Picking") FVector Offset=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Picking",meta=(ClampMin="0.1",Units="cm")) float HitRadius=2.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Deformation",meta=(ClampMin="0.1",Units="cm")) float MaxDistance=2.5f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Deformation") FVector AxisScale=FVector::OneVector;
 /** Multiplier from mouse drag distance to deformation; smaller values need longer drags. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Deformation",meta=(ClampMin="0.01",ClampMax="4",UIMin="0.05",UIMax="1")) float DragSensitivity=.25f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Deformation",meta=(ClampMin="0.1",ClampMax="30")) float Frequency=6.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Deformation",meta=(ClampMin="0.1",ClampMax="2")) float DampingRatio=.8f;
 /** Scales only cheek spring recovery, independently of body/expression return time. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Release",meta=(ClampMin="0.1",ClampMax="5",DisplayName="Cheek Recovery Speed")) float CheekRecoverySpeed=2.f;
 /** Visible spring after releasing the mouse, separate from held-drag tracking. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Release",meta=(ClampMin="0.1",ClampMax="15")) float ReleaseFrequency=2.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Release",meta=(ClampMin="0.1",ClampMax="2")) float ReleaseDampingRatio=.45f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Release",meta=(ClampMin="0.05",ClampMax="3",Units="s")) float ReactionReturnTime=.65f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Morph") FName PullMorph;
 /** Direction in the helper bone's local space; positive drives PullMorph. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Morph") FVector PullAxis=FVector(1,0,0);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Morph",meta=(ClampMin="0",ClampMax="1")) float MorphStrength=1.f;

 /** Body poses are selected and mixed by drag direction, including facial morph curves. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") TObjectPtr<UAnimSequence> BodyLeft;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") TObjectPtr<UAnimSequence> BodyRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") TObjectPtr<UAnimSequence> BodyUp;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") TObjectPtr<UAnimSequence> BodyDown;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") FVector BodyRightAxis=FVector(1,0,0);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") FVector BodyUpAxis=FVector(0,0,1);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Reaction",meta=(ClampMin="0",ClampMax="1")) float ReactionStrength=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Reaction",meta=(ClampMin="0.1",ClampMax="4")) float ReactionExponent=1.3f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") FWardrobeSoundEntry GrabSound;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") FWardrobeSoundEntry MoveSound;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio") FWardrobeSoundEntry ReleaseSound;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Audio",meta=(ClampMin="0.05",Units="cm")) float MoveSoundDistance=.8f;
};
