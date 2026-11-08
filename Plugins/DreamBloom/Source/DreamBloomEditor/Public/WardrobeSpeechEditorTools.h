#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WardrobeSpeechEditorTools.generated.h"
class UAnimBlueprint;
UCLASS()
class DREAMBLOOMEDITOR_API UWardrobeSpeechEditorTools : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Wardrobe|AI Speech") static bool AddSpeechNode(UAnimBlueprint* Blueprint);
};
