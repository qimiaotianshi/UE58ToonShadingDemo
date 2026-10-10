#pragma once
#include "CoreMinimal.h"
#include "WardrobeInteraction.generated.h"
class ULevelSequence;
UENUM(BlueprintType)
enum class EWardrobeRegionShape : uint8 { Sphere, Capsule, Box };
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeReaction {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Reaction") FText Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Reaction") TSoftObjectPtr<ULevelSequence> Sequence;
 /** Optional Sequencer binding tag for the current showcase actor; None preserves authored bindings. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Reaction") FName CharacterBindingTag;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Reaction",meta=(ClampMin="0")) float Weight=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Reaction",meta=(ClampMin="0.01")) float PlayRate=1.f;
};
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeInteractionRegion {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") FText Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") FName BoneOrSocket;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") EWardrobeRegionShape Shape=EWardrobeRegionShape::Sphere;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") FVector Offset=FVector::ZeroVector;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") FRotator Rotation=FRotator::ZeroRotator;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region",meta=(ClampMin="0.1")) float Radius=12.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region",meta=(ClampMin="0.1")) float HalfHeight=20.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") FVector BoxExtent=FVector(12.f);
 /** Blocks clicks on regions behind this volume, without triggering a reaction. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") bool bOccluderOnly=false;
 /** A hit area for cloth physics with no character reaction Sequence. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") bool bPhysicsTouchOnly=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region",meta=(ClampMin="0",Units="s")) float Cooldown=2.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") bool bAvoidImmediateRepeat=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Region") TArray<FWardrobeReaction> Reactions;
};
