#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameModeBase.h"
#include "DreamBloomWidget.generated.h"

class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class SDreamBloom;

UCLASS(Blueprintable)
class DREAMBLOOM_API UDreamBloomWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="DreamBloom")
 TSoftObjectPtr<ULevelSequence> Sequence;
 UFUNCTION(BlueprintCallable, Category="DreamBloom") void Login(const FString& DisplayName);
 UFUNCTION(BlueprintCallable, Category="DreamBloom") void StartMemory();
 UFUNCTION(BlueprintCallable, Category="DreamBloom") void TogglePause();
 UFUNCTION(BlueprintCallable, Category="DreamBloom") void RestartMemory();
 UFUNCTION(BlueprintCallable, Category="DreamBloom") void ReturnToLobby();
 UFUNCTION(BlueprintCallable, Category="DreamBloom") void Logout();
 int32 Page = 0;
 int32 LoopCount = 0;
 FString Traveler = TEXT("旅人");
 FString Error;
 bool IsPaused() const;
 float Progress() const;
 FString Status() const;
 virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaTime) override;
private:
 UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> Player;
 UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> SequenceActor;
 UPROPERTY(Transient) TObjectPtr<AActor> PreviousCamera;
 TSharedPtr<SDreamBloom> View;
 double LastPosition = -1.0;
 float TestTime = 0.f;
 int32 TestStep = 0;
 int32 NaturalLoops = 0;
 double PausedAt = 0;
 bool PauseVerified = false;
 bool CameraVerified = false;
 void Screenshot(const FString& Name);
};

UCLASS(Blueprintable)
class DREAMBLOOM_API ADreamBloomGameMode : public AGameModeBase
{
 GENERATED_BODY()
public:
 ADreamBloomGameMode();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DreamBloom") TSubclassOf<UDreamBloomWidget> LoginWidgetClass;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="DreamBloom") TSoftObjectPtr<ULevelSequence> MemorySequence;
 virtual void BeginPlay() override;
private:
 UPROPERTY(Transient) TObjectPtr<UDreamBloomWidget> LoginWidget;
};
