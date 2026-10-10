#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WardrobePhysicsTools.generated.h"
class USkeletalMesh;
class UAnimSequence;
class UAnimBlueprint;
/** Optional editor helper. Never modifies the input mesh, skeleton or existing AnimBP. */
UCLASS()
class DREAMBLOOM_API UWardrobePhysicsTools : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Editor")
 static UAnimBlueprint* CreateTouchTestBlueprint(USkeletalMesh* Mesh,UAnimSequence* Idle,const TArray<FName>& RootBones,const FString& PackagePath);
};
