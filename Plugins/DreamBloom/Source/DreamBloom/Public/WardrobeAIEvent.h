#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WardrobeAIEvent.generated.h"

class UWardrobeMenu;class UWardrobeAISpeechComponent;class UWardrobeAISpeechData;
struct FWardrobeSpeechClip;struct FWardrobeSpeechJob;

UENUM(BlueprintType)
enum class EWardrobeAIEventState:uint8 {Waiting,Fetching,Preparing,Opening,AwaitingChoice,Replying};

struct FWardrobeAIEventLine {FString Text;FName Emotion;};
struct FWardrobeAIEventOption {FString Id,Label;FWardrobeAIEventLine Reply;int32 MoodDelta=0;};
struct FWardrobeAIEventPlan {
 FString Title,Description,AstrBotReceipt;FWardrobeAIEventLine Opening;TArray<FWardrobeAIEventOption> Options;
};
namespace WardrobeAIEvent {
 FString BuildPlanRequest(const FWardrobeSpeechJob& Job,const FString& Background,int32 MaxChange);
 bool ParsePlan(const FString& JSON,const FWardrobeSpeechJob& Job,int32 MaxChange,FWardrobeAIEventPlan& Out,FString& Error);
}

/** Owns at most one event; all async completions are guarded by a generation and weak owner. */
UCLASS()
class DREAMBLOOM_API UWardrobeAIEventSubsystem : public UGameInstanceSubsystem {
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase&) override;
 virtual void Deinitialize() override;
 void TickForMenu(UWardrobeMenu* Menu,UWardrobeAISpeechComponent* ActiveSpeech);
 UFUNCTION(BlueprintCallable,Category="AI Events") bool RequestEventNow();
 UFUNCTION(BlueprintCallable,Category="AI Events") bool ChooseOption(int32 Index);
 UFUNCTION(BlueprintCallable,Category="AI Events") void CancelEvent();
 UFUNCTION(BlueprintPure,Category="AI Events") bool IsVisible() const;
 UFUNCTION(BlueprintPure,Category="AI Events") bool CanChoose() const{return State==EWardrobeAIEventState::AwaitingChoice;}
 UFUNCTION(BlueprintPure,Category="AI Events") FString GetTitle() const{return Plan.Title;}
 UFUNCTION(BlueprintPure,Category="AI Events") FString GetDescription() const{return Plan.Description;}
 UFUNCTION(BlueprintPure,Category="AI Events") FString GetOptionLabel(int32 Index) const;
 UFUNCTION(BlueprintPure,Category="AI Events") int32 GetMood() const;
 UFUNCTION(BlueprintPure,Category="AI Events") int32 GetTemporaryBytes() const;
 UPROPERTY(Transient,BlueprintReadOnly,Category="AI Events") EWardrobeAIEventState State=EWardrobeAIEventState::Waiting;
 UPROPERTY(Transient,BlueprintReadOnly,Category="AI Events") FString LastError;
private:
 friend class FWardrobeAIEventLifecycleTest;
 friend class FWardrobeAIEventChoiceTest;
 bool Eligible() const;void ScheduleNext();void PrepareNext();void FailEvent(const FString& Error);
 void ReceivePlan(uint64 Ticket,const FString& JSON,const FString& Error);
 void ReceiveClip(uint64 Ticket,int32 Index,TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe> Clip,const FString& Error);
 void OnWorldCleanup(UWorld*,bool,bool);
 TWeakObjectPtr<UWardrobeMenu> Menu;
 TWeakObjectPtr<UWardrobeAISpeechComponent> Speech;
 TWeakObjectPtr<const UWardrobeAISpeechData> Profile;
 FWardrobeAIEventPlan Plan;
 TArray<TSharedPtr<const FWardrobeSpeechClip,ESPMode::ThreadSafe>> Clips;
 FName CharacterId;FString EventId;uint64 CampaignRevision=0,Generation=0;
 int32 PreparedIndex=0,ExpectedMood=60,InitialMood=60,MaxChange=5;
 double NextEventTime=0,Deadline=0;
 FDelegateHandle CleanupHandle;
};
