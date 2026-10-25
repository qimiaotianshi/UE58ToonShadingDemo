#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Containers/Ticker.h"
#include "WardrobeStory.generated.h"

class UBoxComponent; class ULevelSequence; class ULevelSequencePlayer; class ALevelSequenceActor;
class UWardrobeMenuData; class APawn; class APlayerController; class SWidget; struct FStreamableHandle; class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeStoryStep {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Story") FName StoryId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Story") FText Title;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Story") TSoftObjectPtr<UWorld> Level;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Story") FName TriggerId;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Story") TSoftObjectPtr<ULevelSequence> Sequence;
};

UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeStoryFlow : public UDataAsset {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Story") TArray<FWardrobeStoryStep> Steps;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Save",meta=(ClampMin="1",ClampMax="16")) int32 SaveSlotCount=3;
 /** Stable name used for this story's saves. Keep it unchanged after release. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Save") FString SaveNamespace=TEXT("DreamBloomStory");
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Story") FText ContinuePrompt=FText::FromString(TEXT("点击屏幕继续"));
 UFUNCTION(BlueprintPure,Category="Story") TArray<FString> ValidateConfiguration() const;
};

UCLASS()
class DREAMBLOOM_API UWardrobeStorySave : public USaveGame {
 GENERATED_BODY()
public:
 UPROPERTY(SaveGame) int32 FormatVersion=1;
 UPROPERTY(SaveGame) FString PlayerName;
 UPROPERTY(SaveGame) FString FlowPath;
 UPROPERTY(SaveGame) TArray<FName> CompletedStories;
 UPROPERTY(SaveGame) FName NextStoryId;
 UPROPERTY(SaveGame) bool bStoryComplete=false;
 UPROPERTY(SaveGame) TSoftObjectPtr<UWorld> Level;
 UPROPERTY(SaveGame) FTransform PlayerTransform=FTransform::Identity;
 UPROPERTY(SaveGame) FRotator ControlRotation=FRotator::ZeroRotator;
 UPROPERTY(SaveGame) bool bHasPlayerTransform=false;
 UPROPERTY(SaveGame) FDateTime SavedAt;
 UPROPERTY(SaveGame) TSoftObjectPtr<UWardrobeMenuData> MenuData;
 UPROPERTY(SaveGame) TSoftObjectPtr<UWorld> ReturnLevel;
};

USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeStorySlot {
 GENERATED_BODY()
 UPROPERTY(BlueprintReadOnly) int32 Index=0;
 UPROPERTY(BlueprintReadOnly) bool bExists=false;
 UPROPERTY(BlueprintReadOnly) bool bValid=false;
 UPROPERTY(BlueprintReadOnly) FString PlayerName;
 UPROPERTY(BlueprintReadOnly) FText Progress;
 UPROPERTY(BlueprintReadOnly) FDateTime SavedAt;
};

/** Place one box in each narrative location. Only the current step is armed. */
UCLASS(Blueprintable)
class DREAMBLOOM_API AWardrobeStoryTrigger : public AActor {
 GENERATED_BODY()
public:
 AWardrobeStoryTrigger();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Trigger") TObjectPtr<UBoxComponent> Box;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Trigger") FName TriggerId;
 /** Optional when testing a map directly; the menu normally configures the flow. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Trigger") TSoftObjectPtr<UWardrobeStoryFlow> StoryFlow;
 bool ContainsPlayer(APawn* Pawn) const;
 virtual void BeginPlay() override;
 UFUNCTION() void OnEntered(UPrimitiveComponent* OverlappedComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,int32 OtherBodyIndex,bool bFromSweep,const FHitResult& SweepResult);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWardrobeStoryProgressEvent,FName,StoryId);

UCLASS()
class DREAMBLOOM_API UWardrobeStorySubsystem : public UGameInstanceSubsystem {
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase&) override;
 virtual void Deinitialize() override;
 UFUNCTION(BlueprintCallable,Category="Story") bool Configure(UWardrobeStoryFlow* InFlow);
 UFUNCTION(BlueprintPure,Category="Story") bool HasCampaign() const{return Save!=nullptr;}
 UFUNCTION(BlueprintPure,Category="Story") bool IsCinematicActive() const{return bActive;}
 UFUNCTION(BlueprintPure,Category="Story") bool IsWaitingForClick() const{return bWaiting;}
 UFUNCTION(BlueprintPure,Category="Story") FName GetCurrentStoryId() const;
 UFUNCTION(BlueprintPure,Category="Story") TArray<FWardrobeStorySlot> GetSaveSlots() const;
 /** Creates a slot without travel. The destination picker owns which map is entered. */
 UFUNCTION(BlueprintCallable,Category="Story") bool StartNewGame(int32 Slot,const FString& Name,UWardrobeMenuData* Menu,bool bOverwrite=false);
 UFUNCTION(BlueprintCallable,Category="Story") bool ContinueGame(int32 Slot,UWardrobeMenuData* Menu);
 UFUNCTION(BlueprintCallable,Category="Story") bool ContinueLatest(UWardrobeMenuData* Menu);
 UFUNCTION(BlueprintCallable,Category="Story") bool SaveCheckpoint();
 UFUNCTION(BlueprintCallable,Category="Story") bool TryTrigger(AWardrobeStoryTrigger* Trigger);
 UFUNCTION(BlueprintCallable,Category="Story") void WaitForClick(ULevelSequencePlayer* ExpectedPlayer);
 UFUNCTION(BlueprintCallable,Category="Story") void ContinueSequence();
 UFUNCTION(BlueprintCallable,Category="Story") void CancelCinematic();
 UPROPERTY(BlueprintAssignable,Category="Story") FWardrobeStoryProgressEvent OnStoryCompleted;
 UPROPERTY(BlueprintReadOnly,Category="Story") FText LastError;
 UPROPERTY(BlueprintReadOnly,Category="Story") TObjectPtr<UWardrobeStoryFlow> Flow;
 UPROPERTY(BlueprintReadOnly,Category="Story") int32 ActiveSlot=INDEX_NONE;
 FString SlotName(int32 Index) const;
 /** Shared with the fullscreen click widget; a release after the wait arms the next click. */
 void ArmContinueClick(){bClickArmed=bWaiting;}
 bool ClickContinue();
 void AdvanceRuntime(float DeltaSeconds);
 ULevelSequencePlayer* GetStoryPlayer() const{return Player;}
private:
 friend class FWardrobeStoryRuntimeTest;
 friend class FWardrobeStorySaveTest;
 friend class FWardrobeDestinationMenuTest;
 bool Tick(float DeltaSeconds);
 bool TravelToSave(UWardrobeStorySave* Candidate,int32 Slot,UWardrobeMenuData* Menu);
 bool IsSaveUsable(const UWardrobeStorySave* Candidate) const;
 int32 CurrentStepIndex() const;
 void StartLoadedSequence();
 void RestorePlayer();
 void ShowOverlay();
 void CloseOverlay();
 void OnWorldCleanup(UWorld*,bool,bool);
 UFUNCTION() void OnSequenceFinished();
 UFUNCTION() void OnSequenceStopped();
 UPROPERTY(Transient) TObjectPtr<UWardrobeStorySave> Save;
 UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> Player;
 UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
 TWeakObjectPtr<APawn> HiddenPawn;
 TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<AActor> PreviousView;
 TWeakObjectPtr<AWardrobeStoryTrigger> PendingTrigger;
 TWeakObjectPtr<AWardrobeStoryTrigger> BlockedTrigger;
 TWeakObjectPtr<UWorld> ObservedWorld;
 TSharedPtr<FStreamableHandle> LoadHandle;
 TSharedPtr<SWidget> Overlay;
 TWeakPtr<SWidget> PreviousFocus;
 FTSTicker::FDelegateHandle TickHandle;
 FDelegateHandle CleanupHandle,TravelHandle;
 FTransform BeforeTransform=FTransform::Identity;
 FRotator BeforeControl=FRotator::ZeroRotator;
 bool bActive=false,bWaiting=false,bClickArmed=false,bFinishing=false,bRestorePending=false;
 bool bSavedHidden=false,bSavedCollision=false,bSavedCursor=false,bInputGuarded=false;
 bool bDeferredFinish=false,bDeferredStop=false;
 FName PlayingStoryId;
 uint8 BeforeMovementMode=0,BeforeCustomMovementMode=0;
#if WITH_EDITOR
 void RunStoryQA();
 int32 QAStage=0,QAWaits=0;
 double QAStarted=0,QAHold=0;
#endif
};

/** Call from a Sequence Director's Event Track. Editor preview/other sequences are ignored. */
UCLASS()
class DREAMBLOOM_API UWardrobeStoryLibrary : public UBlueprintFunctionLibrary {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Story",meta=(WorldContext="WorldContextObject"))
 static void WaitForStoryClick(const UObject* WorldContextObject);
};

/** Spawnable helper for a Sequence event track; no scene actor/tag binding is needed. */
UCLASS(Blueprintable)
class DREAMBLOOM_API AWardrobeStoryCue : public AActor {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Story") void WaitForClick();
};
