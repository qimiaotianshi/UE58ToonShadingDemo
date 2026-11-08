#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WardrobeMovementAudio.generated.h"

class USoundBase;
class USoundAttenuation;

UENUM(BlueprintType)
enum class EWardrobeMovementSound : uint8 { Walk, Run, Jump, Land, HeavyLand };

USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeSurfaceFootsteps {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Surface") TEnumAsByte<EPhysicalSurface> Surface=SurfaceType_Default;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Surface") TArray<TObjectPtr<USoundBase>> Walk;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Surface") TArray<TObjectPtr<USoundBase>> Run;
};

/** One-shot movement sounds. Every event selects a variation without immediate repeats. */
UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeMovementAudioData : public UDataAsset {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement Audio") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement Audio") TArray<TObjectPtr<USoundBase>> Walk;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement Audio") TArray<TObjectPtr<USoundBase>> Run;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement Audio") TArray<TObjectPtr<USoundBase>> Jump;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement Audio") TArray<TObjectPtr<USoundBase>> Land;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement Audio") TArray<TObjectPtr<USoundBase>> HeavyLand;
 /** Unlisted surfaces use the existing default Walk/Run sounds. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Surface Footsteps") TArray<FWardrobeSurfaceFootsteps> Surfaces;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Surface Footsteps") FName LeftFootBone=TEXT("foot_l");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Surface Footsteps") FName RightFootBone=TEXT("foot_r");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement Audio",AdvancedDisplay) TObjectPtr<USoundAttenuation> Attenuation;
 const TArray<TObjectPtr<USoundBase>>& GetSounds(EWardrobeMovementSound Event,EPhysicalSurface Surface=SurfaceType_Default) const;
};

/** Place at foot contact, not on every frame the foot remains planted. Silent in asset previews. */
UCLASS(const,hidecategories=Object,meta=(DisplayName="Wardrobe Footstep"))
class DREAMBLOOM_API UAnimNotify_WardrobeFootstep : public UAnimNotify {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Footstep") bool bLeftFoot=true;
 virtual FString GetNotifyName_Implementation() const override;
 virtual float GetDefaultTriggerWeightThreshold_Implementation() const override {return .35f;}
 virtual void Notify(USkeletalMeshComponent* MeshComp,UAnimSequenceBase* Animation,const FAnimNotifyEventReference& EventReference) override;
};
