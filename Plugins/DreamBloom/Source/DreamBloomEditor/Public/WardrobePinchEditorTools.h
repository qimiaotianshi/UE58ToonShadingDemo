#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WardrobePinchEditorTools.generated.h"
class USkeletalMesh;class UAnimBlueprint;class UAnimSequence;
UCLASS()
class DREAMBLOOMEDITOR_API UWardrobePinchEditorTools : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor")
 static UAnimBlueprint* CreatePinchTestBlueprint(USkeletalMesh* Mesh,const TArray<FName>& Bones,const FString& PackagePath);
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor")
 static bool SetPinchBaseAnimation(UAnimBlueprint* Blueprint,UAnimSequence* Animation);
};
