#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "WardrobeAISpeechSubsystem.generated.h"
class UWardrobeAISpeechComponent;class UNNEModelData;
struct FWardrobeSpeechJob;struct FWardrobeSpeechRequest;struct FWardrobeSpeechWorker;
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
 void Cancel(UWardrobeAISpeechComponent* Owner);
 UFUNCTION(BlueprintPure,Category="AI Speech") bool IsBusy() const;
 UFUNCTION(BlueprintPure,Category="AI Speech") FString GetLocalConfigPath() const;
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
 void EnsureService();
 TSharedPtr<FWardrobeSpeechRequest,ESPMode::ThreadSafe> Active;
 TSharedPtr<FWardrobeSpeechWorker,ESPMode::ThreadSafe> Worker;
 TSharedPtr<FStreamableHandle> LoadHandle;
 // A reflected soft reference makes the streaming model discoverable by the cooker.
 UPROPERTY() TSoftObjectPtr<UNNEModelData> FaceModel;
 FString ApiKey;
 bool bEnding=false,bPrewarming=false;
};
