#pragma once
#include "CoreMinimal.h"
#include "WardrobeLimbDrag.generated.h"
class UAnimSequence;

/** Explicit deform chain; twist/helper bones between these three bones are supported. */
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeLimbDrag {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Limb") FText Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Limb") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Limb") FName UpperBone;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Limb") FName JointBone;
 /** Wrist or skinned ankle bone, not an MMD IK control. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Limb") FName EndBone;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Limb") bool bFoot=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Picking") FVector HitOffset=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Picking",meta=(ClampMin="0.1",Units="cm")) float HitRadius=7.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dragging",meta=(ClampMin="1",Units="cm")) float MaxDistance=35.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dragging",meta=(ClampMin="0.01",ClampMax="4")) float DragSensitivity=.65f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dragging",meta=(ClampMin="0",Units="s")) float FollowTime=.08f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Dragging",meta=(ClampMin="0",Units="s")) float ReturnTime=.45f;
 /** First frame is neutral; last frame is maximum pull. Sample time follows drag strength. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") TObjectPtr<UAnimSequence> BodyLeft;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") TObjectPtr<UAnimSequence> BodyRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") TObjectPtr<UAnimSequence> BodyUp;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction") TObjectPtr<UAnimSequence> BodyDown;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Body Reaction",meta=(ClampMin="0",ClampMax="1")) float ReactionStrength=.65f;
};

// Game-thread snapshot copied by AnimBP nodes before worker evaluation.
struct DREAMBLOOM_API FWardrobeLimbSnapshot {
 FWardrobeLimbDrag Settings;
 FVector TargetWorld=FVector::ZeroVector;
 FVector PoleDirection=FVector::ForwardVector;
 FVector2D Direction=FVector2D::ZeroVector;
 float Alpha=0,Force=0;
};
