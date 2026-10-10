#pragma once
#include "CoreMinimal.h"
#include "WardrobeLook.generated.h"
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeLookBone {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look") FName Bone;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look",meta=(ClampMin="0",ClampMax="1")) float Weight=.5f;
};
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeCursorLook {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look") bool bEnabled=false;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look") FName AnchorBone;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look") FVector ForwardAxis=FVector(0,1,0);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look") FVector UpAxis=FVector(0,0,1);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look") TArray<FWardrobeLookBone> Bones;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look",meta=(ClampMin="0",ClampMax="80",Units="deg")) float MaxYaw=30.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look",meta=(ClampMin="0",ClampMax="60",Units="deg")) float MaxPitch=20.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look",meta=(ClampMin="0.1",ClampMax="20")) float FollowSpeed=4.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look",meta=(ClampMin="0",ClampMax="5",Units="s")) float IdleDelay=.8f;
 /** Fade gaze in while pinch reaction fades out; no extra idle delay. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Look",meta=(ClampMin="0.05",ClampMax="2",Units="s")) float PinchToLookBlendTime=.35f;
};
