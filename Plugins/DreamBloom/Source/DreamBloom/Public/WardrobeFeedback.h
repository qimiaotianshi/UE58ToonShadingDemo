#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WardrobeFeedback.generated.h"

class UMaterialParameterCollection;
class ULevelSequence;

USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobePhysicsTouch {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch") bool bEnabled=false;
 /** Extra mouse picking tolerance beyond each simulated bone's own radius. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="0",ClampMax="20",Units="cm")) float HitPadding=2.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="0.1",ClampMax="50")) float DragStiffness=12.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="1",ClampMax="200",Units="cm")) float MaxDragDistance=40.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="1",ClampMax="1000")) float MaxDragSpeed=200.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="1",Units="cm")) float Radius=30.f;
 /** Local push speed before spatial and temporal falloff (cm/s). */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="0",ClampMax="2000")) float Strength=100.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="0.02",ClampMax="2",Units="s")) float Duration=.25f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="0.1",ClampMax="8")) float Falloff=2.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="0",Units="s")) float Cooldown=.15f;
 /** Blend camera ray push with outward radial push. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch",meta=(ClampMin="0",ClampMax="1")) float RadialBlend=.3f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Physics Touch") bool bDrawDebug=false;
};

/** Global menu click effect. The Sequence animates the remaining material parameters. */
UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeClickEffectData : public UDataAsset {
 GENERATED_BODY()
public:
 bool WriteParameters(UWorld* World,const FVector& Position,const FVector2D& UV,bool bHit) const;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Click Effect") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Click Effect") TObjectPtr<UMaterialParameterCollection> Collection;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Click Effect") TSoftObjectPtr<ULevelSequence> Sequence;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parameters") FName WorldPositionParameter=TEXT("ClickWorldPosition");
 /** R=viewport U, G=viewport V, B=scene hit (0/1), A=1. None skips writing. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parameters") FName ScreenUVParameter=TEXT("ClickScreenUV");
 /** Optional time in world seconds, matching the material Time node (not Ignore Pause). */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Parameters") FName TimeParameter=TEXT("ClickTime");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Click Effect",meta=(ClampMin="0",Units="s")) float MinInterval=.05f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Click Effect",meta=(ClampMin="0.1",Units="s")) float MaxDuration=5.f;
 /** With no scene hit, use a camera-facing plane through the active character. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="World Position") bool bUseCharacterPlane=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="World Position",meta=(ClampMin="1",Units="cm")) float FallbackDistance=500.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="World Position",meta=(ClampMin="100",Units="cm")) float TraceDistance=100000.f;
};
