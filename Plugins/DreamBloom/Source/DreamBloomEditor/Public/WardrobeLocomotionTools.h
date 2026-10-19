#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WardrobeLocomotionTools.generated.h"
class USkeletalMesh;class UAnimSequence;class UWardrobeMenuData;
UCLASS()
class DREAMBLOOMEDITOR_API UWardrobeLocomotionTools : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor")
 static bool BuildLocomotionAssets(USkeletalMesh* Mesh,UAnimSequence* Idle,UAnimSequence* Walk,UAnimSequence* Run);
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor")
 static bool UpdateLocomotionAssets();
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor")
 static bool AddLocomotionDestination(UWardrobeMenuData* Menu);
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor")
 static bool UpdateLocomotionZoomInput();
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor")
 static bool UpdateLocomotionTransitions();
};
