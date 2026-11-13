#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WardrobeStockingEditorTools.generated.h"
class USkeletalMesh;
class UWardrobeStockingSurface;
class USceneCaptureComponent2D;

UCLASS()
class DREAMBLOOMEDITOR_API UWardrobeStockingEditorTools : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 /** Align the exported textile to the existing reference pose and bake portable skin data. */
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor|Stockings")
 static UWardrobeStockingSurface* BakeSurface(USkeletalMesh* Stocking,USkeletalMesh* Body,const FString& PackagePath);
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor|Stockings")
 static FString DescribeReference(USkeletalMesh* Mesh);
 /** Submit procedural mesh changes before capturing from an unattended editor script. */
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor|Stockings")
 static bool CapturePreview(USceneCaptureComponent2D* Capture);
};
