#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "WardrobeAISpeechSubsystem.generated.h"
class UWardrobeAISpeechComponent;class UWardrobeAISpeechData;class UNNEModelData;
struct FWardrobeSpeechJob;struct FWardrobeSpeechRequest;struct FWardrobeSpeechWorker;struct FWardrobeSpeechClip;
struct FStreamableHandle;

/** One in-flight request per game instance. Lifetime pins survive cancellation and world travel. */
UCLASS()
class DREAMBLOOM_API UWardrobeAISpeechSubsystem : public UGameInstanceSubsystem {
 GENERATED_BODY()
public:
 UWardrobeAISpeechSubsystem();
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 bool Start(const FWardrobeSpeechJob& Job);
 void CommitAstrBotClip(const FWardrobeSpeechClip& Clip);
 void Cancel(UWardrobeAISpeechComponent* Owner);
 UFUNCTION(BlueprintPure,Category="AI Speech") bool IsBusy() const;
 /** Configure before the startup movie warmup, from the selected character's DA. */
 UFUNCTION(BlueprintCallable,Category="AI Speech") void ConfigureService(UWardrobeAISpeechData* Profile);
 /** Absolute runtime path resolved from the project-relative TTSRoot configuration. */
 UFUNCTION(BlueprintPure,Category="AI Speech") FString GetResolvedTTSRoot() const;
 /** Load and initialize the reusable face solver during the startup cover, without generating dialogue. */
 void Prewarm();
 bool IsPrewarming() const {return bPrewarming;}
private:
 void RequestText(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R);
 void RequestVoice(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R);
 void Solve(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R,TArray<uint8> Bytes);
 void Finish(TSharedRef<FWardrobeSpeechRequest,ESPMode::ThreadSafe> R,const FString& Error);
 void EnsureService(const UWardrobeAISpeechData* Profile);
 void EnsureAstrBot(const UWardrobeAISpeechData* Profile);
 void PumpAstrBotCommits();
 TArray<FString> AstrBotCommits;
 bool bAstrBotCommitPending=false,bAstrBotCheckPending=false;
 TWeakObjectPtr<UWardrobeAISpeechData> ServiceProfile;
 bool bServiceConfigured=false,bServiceCheckPending=false,bServiceStarted=false;
 TSharedPtr<FWardrobeSpeechRequest,ESPMode::ThreadSafe> Active;
 TSharedPtr<FWardrobeSpeechWorker,ESPMode::ThreadSafe> Worker;
 TSharedPtr<FStreamableHandle> LoadHandle;
 // A reflected soft reference makes the streaming model discoverable by the cooker.
 UPROPERTY() TSoftObjectPtr<UNNEModelData> FaceModel;
 bool bEnding=false,bPrewarming=false;
};
