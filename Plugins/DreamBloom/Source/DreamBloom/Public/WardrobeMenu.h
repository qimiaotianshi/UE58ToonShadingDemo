#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "WardrobeAudio.h"
#include "WardrobeMenu.generated.h"

class UWardrobeMenuData;
class AWardrobeShowcase;
class UWardrobeStationController;
class SWardrobeMenu;
class UTexture2D;

UCLASS()
class DREAMBLOOM_API UWardrobeSession : public UGameInstanceSubsystem {
 GENERATED_BODY()
public:
 bool LoggedIn=false;
 int32 Mode=INDEX_NONE;
 FString Name=TEXT("旅人");
 int32 SelectedMap=0;
};

UCLASS(Blueprintable)
class DREAMBLOOM_API UWardrobeMenu : public UUserWidget {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Menu") TObjectPtr<UWardrobeMenuData> Data;
 UPROPERTY(BlueprintReadOnly,Category="Menu") TObjectPtr<AWardrobeShowcase> Showcase;
 UPROPERTY(BlueprintReadOnly,Category="Menu") TObjectPtr<UWardrobeStationController> StationController;
 UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> LoadedThumbnails;
 UFUNCTION(BlueprintCallable,Category="Menu") void Login(const FString& Name);
 UFUNCTION(BlueprintCallable,Category="Menu") void ChangeMode(int32 Index);
 UFUNCTION(BlueprintCallable,Category="Menu") void OpenChoice(int32 Index);
 UFUNCTION(BlueprintCallable,Category="Menu") void SelectChoice(int32 Index);
 UFUNCTION(BlueprintCallable,Category="Menu") void StartGame();
 UFUNCTION(BlueprintCallable,Category="Menu") void ReturnToMenu();
 UFUNCTION(BlueprintCallable,Category="Menu") void Quit();
 UFUNCTION(BlueprintCallable,Category="UI Audio") void PlayUISound(EWardrobeUISound Event);
 void SetQuitPrompt(bool Enabled);
 void TurnCharacter(float Degrees);
 void ZoomCharacter(float Amount);
 void PanCamera(FVector2D Delta);
 void ClickCharacter(FVector2D ScreenPosition);
 bool BeginPhysicsDrag(FVector2D ScreenPosition);
 void UpdatePhysicsDrag(FVector2D ScreenPosition);
 void EndPhysicsDrag();
 bool HasLimbDragGesture() const;
 void PlayClickEffect(FVector2D ScreenPosition);
 void StopClickEffect();
 void CapturePreviewCamera();
 int32 CharacterCount() const;
 bool CanPreviewInput() const;
 int32 ChoiceCount() const;
 int32 SelectedChoice() const;
 bool ChoiceReady(int32 Index) const;
 FText ChoiceLabel(int32 Index) const;
 TSoftObjectPtr<UTexture2D> ChoiceThumbnail(int32 Index) const;
 FString SelectionDescription() const;
 void ResetCharacter();
 UWardrobeSession* Session() const;
 bool IsMenuMap() const;
 bool IsLoggedIn() const;
 int32 CurrentMode() const;
 FString ModeLabel(int32 Index) const;
 FString Message;
 bool QuitPrompt=false; void RestoreMenuFocus();
 virtual void ReleaseSlateResources(bool bReleaseChildren) override;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeConstruct() override;
 virtual void NativeDestruct() override;
 virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
private:
 TSharedPtr<SWardrobeMenu> SlateMenu;
 UPROPERTY(Transient) TObjectPtr<class ULevelSequencePlayer> ClickEffectPlayer;
 UPROPERTY(Transient) TObjectPtr<class ALevelSequenceActor> ClickEffectActor;
 float ClickEffectAge=0;
 double NextClickEffectTime=0;
 float NoticeTime=0;
 TWeakObjectPtr<AWardrobeShowcase> DragCharacter;
 TWeakObjectPtr<class ACameraActor> PreviewCamera;

};

UCLASS(Blueprintable)
class DREAMBLOOM_API AWardrobeMenuGameMode : public AGameModeBase {
 GENERATED_BODY()
public:
 AWardrobeMenuGameMode();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Menu") TSubclassOf<UWardrobeMenu> MenuClass;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Menu") TObjectPtr<UWardrobeMenuData> MenuData;
 virtual void BeginPlay() override;
private:
 UPROPERTY(Transient) TObjectPtr<UWardrobeMenu> Menu;
};
