#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WardrobeFootstepSurfaceRegion.generated.h"

class UBoxComponent;

/** Sound-only region for shallow water without blocking collision. Its top is the waterline. */
UCLASS(Blueprintable,meta=(DisplayName="Wardrobe Footstep Surface Region"))
class DREAMBLOOM_API AWardrobeFootstepSurfaceRegion : public AActor {
 GENERATED_BODY()
public:
 AWardrobeFootstepSurfaceRegion();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Footstep Surface") TObjectPtr<UBoxComponent> Bounds;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Footstep Surface") TEnumAsByte<EPhysicalSurface> Surface=SurfaceType1;
 /** Higher priority wins; equally prioritized overlapping regions choose the smaller box. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Footstep Surface",AdvancedDisplay) int32 Priority=0;
 UFUNCTION(BlueprintPure,Category="Footstep Surface") bool ContainsContact(FVector WorldLocation) const;
};
