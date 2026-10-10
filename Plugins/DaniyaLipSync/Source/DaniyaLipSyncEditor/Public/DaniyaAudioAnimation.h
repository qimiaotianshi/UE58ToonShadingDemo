#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Containers/Ticker.h"
#include "SpeechAnimationSolverTypes.h"
#include "DaniyaAudioAnimation.generated.h"

class USoundWave;
class USkeletalMesh;
class UAnimSequence;
class UMetaHumanPerformance;
namespace UE::MetaHuman::Pipeline { class FPipelineData; }

USTRUCT(BlueprintType)
struct DANIYALIPSYNCEDITOR_API FDaniyaAudioAnimationResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Daniya") bool bSuccess = false;
    UPROPERTY(BlueprintReadOnly, Category="Daniya") TObjectPtr<UAnimSequence> Animation;
    UPROPERTY(BlueprintReadOnly, Category="Daniya") TObjectPtr<UMetaHumanPerformance> Performance;
    UPROPERTY(BlueprintReadOnly, Category="Daniya") FString SavedAssetPath;
    UPROPERTY(BlueprintReadOnly, Category="Daniya") FString Message;
    UPROPERTY(BlueprintReadOnly, Category="Daniya") TArray<FName> ExpressionCurves;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDaniyaAudioAnimationEvent, const FDaniyaAudioAnimationResult&, Result);

/** Editor-only asynchronous audio solve and MMD retarget. Completed means the assets were saved. */
UCLASS(BlueprintType, meta=(ExposedAsyncProxy="Task"))
class DANIYALIPSYNCEDITOR_API UDaniyaGenerateAudioAnimation : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintAssignable, Category="Daniya") FDaniyaAudioAnimationEvent Completed;
    UPROPERTY(BlueprintAssignable, Category="Daniya") FDaniyaAudioAnimationEvent Failed;
    UPROPERTY(BlueprintReadOnly, Category="Daniya") FDaniyaAudioAnimationResult Result;
    UPROPERTY(BlueprintReadOnly, Category="Daniya") bool bFinished = false;
    UPROPERTY(BlueprintReadOnly, Category="Daniya") float Progress = 0;

    /** Select an imported SoundWave and an MMD mesh with あ/い/う/え/お. No Performance setup is required.
     * Include Expressions enables MetaHuman Full Face, then maps supported eyebrows/eyes/mouth corners.
     * Saves a new animation and its Performance, automatically suffixing existing names. Editor Utility only. */
    UFUNCTION(BlueprintCallable, Category="Daniya|Audio Animation", meta=(BlueprintInternalUseOnly="true", DisplayName="Generate Daniya Animation From Audio", AdvancedDisplay="OutputFolder,AssetName,Mood,MoodIntensity,bSmooth", CPP_Default_MouthGain="1.35", CPP_Default_bIncludeExpressions="true", CPP_Default_ExpressionGain="0.65", CPP_Default_OutputFolder="/Game/DaniyaTest/Generated", CPP_Default_AssetName="", CPP_Default_Mood="AutoDetect", CPP_Default_MoodIntensity="1.0", CPP_Default_bSmooth="true"))
    static UDaniyaGenerateAudioAnimation* GenerateDaniyaAnimationFromAudio(USoundWave* Audio, USkeletalMesh* Mesh, float MouthGain=1.35f, bool bIncludeExpressions=true, float ExpressionGain=.65f, FString OutputFolder=TEXT("/Game/DaniyaTest/Generated"), FString AssetName=TEXT(""), EAudioDrivenAnimationMood Mood=EAudioDrivenAnimationMood::AutoDetect, float MoodIntensity=1.f, bool bSmooth=true, bool bCloseMouthInSilence=true, float SilenceThresholdDb=-42.f, float MouthCloseTime=.08f, float MouthOpenTime=.02f);

    virtual void Activate() override;
    UFUNCTION(BlueprintCallable, Category="Daniya|Audio Animation") void Cancel();

private:
    UPROPERTY() TObjectPtr<USoundWave> SourceAudio;
    UPROPERTY() TObjectPtr<USkeletalMesh> TargetMesh;
    UPROPERTY() TObjectPtr<UMetaHumanPerformance> WorkingPerformance;
    float MouthGainValue=1.35f, ExpressionGainValue=.65f, MoodIntensityValue=1.f;
    bool bExpressions=true, bSmoothValue=true, bActivated=false, bCancelled=false;
    bool bSilence=true;
    float SilenceDb=-42.f, CloseTime=.08f, OpenTime=.02f;
    EAudioDrivenAnimationMood MoodValue=EAudioDrivenAnimationMood::AutoDetect;
    FString Folder, RequestedName, OutputPath, PerformancePath, PipelineError;
    bool bPipelineFinished=false, bPipelineSucceeded=false;
    FDelegateHandle PipelineHandle, FrameHandle;
    FTSTicker::FDelegateHandle TickHandle;
    void OnPipelineFinished(TSharedPtr<const UE::MetaHuman::Pipeline::FPipelineData> Data);
    bool Tick(float DeltaTime);
    void Finalize();
    void Finish(bool bSuccess, const FString& Message);
};

/** Native settings panel available in Tools > Daniya Audio Animation. */
UCLASS(BlueprintType)
class DANIYALIPSYNCEDITOR_API UDaniyaAudioAnimationTool : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Input") TObjectPtr<USoundWave> Audio;
    UPROPERTY(EditAnywhere, Category="Input") TObjectPtr<USkeletalMesh> Mesh;
    UPROPERTY(EditAnywhere, Category="Input", meta=(ClampMin="0",UIMax="3")) float MouthGain=1.35f;
    UPROPERTY(EditAnywhere, Category="Input") bool bIncludeExpressions=true;
    UPROPERTY(EditAnywhere, Category="Input", meta=(ClampMin="0",UIMax="2",EditCondition="bIncludeExpressions")) float ExpressionGain=.65f;
    UPROPERTY(EditAnywhere, Category="Input") bool bSmooth=true;
    UPROPERTY(EditAnywhere, Category="Silence") bool bCloseMouthInSilence=true;
    UPROPERTY(EditAnywhere, Category="Silence", meta=(ClampMin="-90",ClampMax="-6",EditCondition="bCloseMouthInSilence", ToolTip="RMS dBFS below this level closes the mouth. Higher values close more aggressively. Default -42.")) float SilenceThresholdDb=-42.f;
    UPROPERTY(EditAnywhere, Category="Silence", meta=(ClampMin="0.01",ClampMax="0.5",EditCondition="bCloseMouthInSilence",Units="s")) float MouthCloseTime=.08f;
    UPROPERTY(EditAnywhere, Category="Silence", meta=(ClampMin="0.01",ClampMax="0.2",EditCondition="bCloseMouthInSilence",Units="s")) float MouthOpenTime=.02f;
    UPROPERTY(EditAnywhere, Category="Emotion") EAudioDrivenAnimationMood Mood=EAudioDrivenAnimationMood::AutoDetect;
    UPROPERTY(EditAnywhere, Category="Emotion", meta=(ClampMin="0",ClampMax="1")) float MoodIntensity=1.f;
    UPROPERTY(EditAnywhere, Category="Output") FString OutputFolder=TEXT("/Game/DaniyaTest/Generated");
    UPROPERTY(EditAnywhere, Category="Output") FString AssetName;
    UPROPERTY(VisibleAnywhere, Category="Result") FString Status=TEXT("Select Audio and Mesh, then Generate. Assets are saved automatically.");
    UPROPERTY(VisibleAnywhere, Category="Result") TObjectPtr<UAnimSequence> Animation;
    UPROPERTY(VisibleAnywhere, Category="Result") TObjectPtr<UMetaHumanPerformance> Performance;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Actions") void Generate();
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Actions") void Cancel();
    UPROPERTY(BlueprintReadOnly, Category="Daniya") TObjectPtr<UDaniyaGenerateAudioAnimation> Task;
private:
    UFUNCTION() void OnFinished(const FDaniyaAudioAnimationResult& InResult);
};
