#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimationModifier.h"
#include "DaniyaMouthLibrary.h"
#include "DaniyaLipSyncEditorLibrary.generated.h"

class UMetaHumanPerformance;
class USkeletalMesh;
class UAnimSequence;
class USoundWave;

UCLASS()
class DANIYALIPSYNCEDITOR_API UDaniyaLipSyncEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Editor only. Attenuate ONLY the five vowel curves using the original full SoundWave.
     * Duplicate the animation before calling. Preserves expressions, bones and additive settings.
     * Does not save. Reapplying compounds attenuation; use a fresh original when tuning. */
    UFUNCTION(BlueprintCallable, Category="Daniya|Lip Sync|Editor", meta=(DisplayName="Close Daniya Mouth In Silence"))
    static bool CloseDaniyaMouthInSilence(USoundWave* Audio, UAnimSequence* Animation, FString& Error, float SilenceThresholdDb=-42.f, float MouthCloseTime=.08f, float MouthOpenTime=.02f);
    /** Editor only. Read one solved frame without manually breaking the frame struct. FrameIndex is zero-based. */
    UFUNCTION(BlueprintCallable, Category="Daniya|Lip Sync|Editor", meta=(DisplayName="Get Daniya Mouth Frame", CPP_Default_Gain="1.35"))
    static bool GetDaniyaMouthFrame(UMetaHumanPerformance* Performance, int32 FrameIndex, FDaniyaMouthWeights& Weights, FString& Error, float Gain=1.35f);

    /** Editor only. Bake into an existing animation (e.g. OnApply). Replaces ONLY five mouth curves.
     * Does not resize animation, save assets, or change other curves/bone tracks. Full-audio solve only. */
    UFUNCTION(BlueprintCallable, Category="Daniya|Lip Sync|Editor", meta=(DisplayName="Bake Daniya Mouth Curves", CPP_Default_Gain="1.35", CPP_Default_bSmooth="true"))
    static bool BakeDaniyaMouthCurves(UMetaHumanPerformance* Performance, UAnimSequence* TargetAnimation, USkeletalMesh* Mesh, FString& Error, float Gain=1.35f, bool bSmooth=true);

    /** Editor only. Generate a NEW animation from an already processed full-audio Performance.
     * AssetPath example /Game/DaniyaTest/LipSyncDemo/AS_Daniya_CPP. Refuses overwrites. Save the returned asset with Ctrl+S. */
    UFUNCTION(BlueprintCallable, Category="Daniya|Lip Sync|Editor", meta=(DisplayName="Create Daniya Mouth Animation", CPP_Default_Gain="1.35", CPP_Default_bSmooth="true"))
    static UAnimSequence* CreateDaniyaMouthAnimation(UMetaHumanPerformance* Performance, USkeletalMesh* Mesh, const FString& AssetPath, FString& Error, float Gain=1.35f, bool bSmooth=true);
};

/** Add directly in Animation Data Modifiers. No Blueprint subclass or loops needed. */
UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Daniya MetaHuman Mouth Modifier"))
class DANIYALIPSYNCEDITOR_API UDaniyaMouthAnimationModifier : public UAnimationModifier
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya") TObjectPtr<UMetaHumanPerformance> Performance;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya") TObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya", meta=(ClampMin="0",UIMax="3")) float Gain=1.35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Daniya") bool bSmooth=true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Daniya") FString LastResult;
    virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
};
