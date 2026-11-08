#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "WardrobeStartup.generated.h"

class UWardrobeMenu;
class UWardrobeMenuData;
class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;
class SWidget;
struct FWardrobeStartupVisual;

/** One opening movie per game instance. PIE uses Media Framework; standalone loading uses MoviePlayer. */
UCLASS()
class DREAMBLOOM_API UWardrobeStartupSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;
 void BeginForMenu(UWardrobeMenu* Menu);
 UFUNCTION(BlueprintPure,Category="Wardrobe|Startup") bool IsActive() const { return bActive; }
 UFUNCTION(BlueprintPure,Category="Wardrobe|Startup") bool CanEnterGame() const;
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Startup") bool EnterGame();
 UFUNCTION(BlueprintPure,Category="Wardrobe|Startup") FString GetStartupStatus() const;
private:
 friend class FWardrobeStartupGateTest;
 bool Tick(float DeltaSeconds);
 bool PrepareNativeMovie();
 void BeginMediaMovie();
 void Finish();
 void GuardInput();
 UFUNCTION() void MediaOpened(FString URL);
 UFUNCTION() void MediaFailed(FString URL);
 UFUNCTION() void MediaEnded();
 void NativeMovieFinished();
 UPROPERTY(Transient) TObjectPtr<UWardrobeMenuData> Settings;
 UPROPERTY(Transient) TObjectPtr<UMediaPlayer> MediaPlayer;
 UPROPERTY(Transient) TObjectPtr<UMediaTexture> MediaTexture;
 UPROPERTY(Transient) TObjectPtr<UMediaSoundComponent> MediaSound;
 TWeakObjectPtr<UWardrobeMenu> MenuOwner;
 TWeakObjectPtr<class APlayerController> GuardedController;
 TSharedPtr<FWardrobeStartupVisual> Visual;
 TSharedPtr<SWidget> Overlay;
 FTSTicker::FDelegateHandle TickHandle;
 FDelegateHandle ClipHandle,FinishedHandle;
 FString MovieFile;
 FString HealthURL;
 TSharedPtr<class IHttpRequest,ESPMode::ThreadSafe> HealthRequest;
 double NextHealthCheck=0;
 bool bTTSReady=true;
 double StartedAt=0,ReadyAt=0,MediaOpenedAt=0;
 double QAReadyAt=0,QAEnteredAt=0;
 int32 WarmupFrames=0;
 bool bActive=false,bCompleted=false,bNativeMovie=false,bSavedCursor=false;
};
