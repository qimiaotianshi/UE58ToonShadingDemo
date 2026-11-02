#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SunLensFlareImportLibrary.generated.h"

UCLASS()
class SUNLENSFLAREEDITOR_API USunLensFlareImportLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Sun Lens Flare|Editor")
    static bool ImportProperties(UObject* Object, const FString& Text);
    UFUNCTION(BlueprintCallable, Category="Sun Lens Flare|Editor")
    static bool ImportMaterialProperties(UMaterial* Material, const FString& Text);
    UFUNCTION(BlueprintCallable, Category="Sun Lens Flare|Editor")
    static FString GetMaterialErrors(UMaterial* Material);
    UFUNCTION(BlueprintCallable, Category="Sun Lens Flare|Editor")
    static bool SetStaticSwitch(UMaterialInstanceConstant* Instance, FName Name, bool Value);
    UFUNCTION(BlueprintCallable, Category="Sun Lens Flare|Editor")
    static void EnablePreviewViewport();
    UFUNCTION(BlueprintCallable, Category="Sun Lens Flare|Editor")
    static void ClearExpressions(UObject* Asset);
    UFUNCTION(BlueprintCallable, Category="Sun Lens Flare|Editor")
    static void LinkFunctionCalls(UObject* Asset);
};
