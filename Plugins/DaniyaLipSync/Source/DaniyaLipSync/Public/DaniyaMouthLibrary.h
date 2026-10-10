#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DaniyaMouthLibrary.generated.h"

class USkeletalMeshComponent;

/** Native replacement for ST_DaniyaMouth. A/I/U/E/O correspond to あ/い/う/え/お. */
USTRUCT(BlueprintType)
struct DANIYALIPSYNC_API FDaniyaMouthWeights
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya") float A = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya") float I = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya") float U = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya") float E = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya") float O = 0;
};

UCLASS()
class DANIYALIPSYNC_API UDaniyaMouthLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Input is one frame's String -> Float Animation Data map with full CTRL_expressions_ keys.
     * Missing/non-finite controls become zero. Invalid Gain becomes zero.
     * Pure mapping only: no temporal smoothing and no audio analysis. */
    UFUNCTION(BlueprintPure, Category="Daniya|Lip Sync", meta=(DisplayName="Build Daniya Mouth Weights", Keywords="BuildWeights MMD MetaHuman", CPP_Default_Gain="1.35"))
    static FDaniyaMouthWeights BuildDaniyaMouthWeights(const TMap<FString, float>& Controls, float Gain = 1.35f);

    /** Valid=false if any required control is missing or non-finite. Use to diagnose wrong inputs. */
    UFUNCTION(BlueprintPure, Category="Daniya|Lip Sync", meta=(DisplayName="Validate Daniya Controls"))
    static bool ValidateDaniyaControls(const TMap<FString, float>& Controls, TArray<FString>& Problems);

    /** Apply all five morphs, including zero values. Fails without changing anything if a morph is missing.
     * Use on the game thread; an AnimBP can overwrite these values later in its update. */
    UFUNCTION(BlueprintCallable, Category="Daniya|Lip Sync", meta=(DisplayName="Apply Daniya Mouth Weights"))
    static bool ApplyDaniyaMouthWeights(USkeletalMeshComponent* Mesh, FDaniyaMouthWeights Weights, FString& Error);

    static FDaniyaMouthWeights Sanitize(FDaniyaMouthWeights W);
    static TArray<FString> RequiredControlNames();
    static const TArray<FName>& MorphNames();
};
