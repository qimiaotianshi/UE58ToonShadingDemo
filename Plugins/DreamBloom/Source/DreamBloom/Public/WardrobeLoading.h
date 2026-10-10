#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "WardrobeLoading.generated.h"

class UWardrobeMenuData;
class UTexture2D;
class SWidget;
struct FWardrobeLoadingVisual;

/** Survives OpenLevel; owns both the blocking MoviePlayer screen and the world warmup overlay. */
UCLASS()
class DREAMBLOOM_API UWardrobeLoadingSubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 virtual void Initialize(FSubsystemCollectionBase& Collection) override;
 virtual void Deinitialize() override;

 UFUNCTION(BlueprintCallable, Category="Wardrobe|Loading")
 bool TravelToLevel(TSoftObjectPtr<UWorld> Level, UWardrobeMenuData* Settings,
                    TSoftObjectPtr<UTexture2D> ImageOverride, FText Destination, FString Options);
 UFUNCTION(BlueprintPure, Category="Wardrobe|Loading") bool IsLoading() const { return bActive; }
 bool IsPreparingScene() const { return bActive&&bMapLoaded; }
 /** For a destination that starts its own async work in BeginPlay. Clear when that work completes. */
 UFUNCTION(BlueprintCallable, Category="Wardrobe|Loading")
 void SetAdditionalLoadingRequired(bool bRequired) { bAdditionalLoading = bRequired; }

private:
 bool Tick(float DeltaSeconds);
 void OnPostLoadMap(UWorld* World);
 void OnTravelFailure(UWorld* World, ETravelFailure::Type Type, const FString& Error);
 void Finish(bool bFailed = false);
 void GuardInput();
 bool IsSceneReady(UWorld* World) const;
 void ShowOverlay();

 UPROPERTY(Transient) TObjectPtr<UTexture2D> LoadingTexture;
 TSharedPtr<FWardrobeLoadingVisual> Visual;
 TSharedPtr<SWidget> Overlay;
 TWeakPtr<SWidget> PreviousFocus;
 TWeakObjectPtr<UWorld> DestinationWorld;
 TWeakObjectPtr<class APlayerController> GuardedController;
 FTSTicker::FDelegateHandle TickHandle;
 FDelegateHandle PostLoadHandle, FailureHandle;
 FString PackageName, TravelOptions;
 bool bActive=false, bTravelIssued=false, bMapLoaded=false, bAdditionalLoading=false;
 bool bManualContinue=false, bSavedCursor=false;
 double StartedAt=0, LoadedAt=0, ReadyAt=0, CompleteAt=0;
 float MinDisplay=1.5f, Warmup=2.f, SlowWarning=45.f;
 uint32 WarmupFrames=0;
 int32 CompletedTravels=0;
 bool bQAScreenshot=false;
};
