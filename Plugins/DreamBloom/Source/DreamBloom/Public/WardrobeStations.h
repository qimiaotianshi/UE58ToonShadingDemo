#pragma once
#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraTypes.h"
#include "WardrobeCharacters.h"
#include "WardrobeStations.generated.h"

class AWardrobeShowcase;
class UCurveFloat;
class APlayerController;
class UWardrobeMenuData;

/** Resolved scene objects are transient; all authoring is in the character DA. */
USTRUCT()
struct FWardrobeStation {
 GENERATED_BODY()
 UPROPERTY(Transient) TObjectPtr<ACameraActor> Camera;
 UPROPERTY(Transient) TObjectPtr<AWardrobeShowcase> Character;
 UPROPERTY(Transient) FWardrobeCharacterOption Settings;
};

/** Runtime camera keeps authored cameras untouched by preview orbit/pan/zoom. */
UCLASS(NotBlueprintable,Transient)
class DREAMBLOOM_API AWardrobeViewCamera : public ACameraActor {
 GENERATED_BODY()
public:
 void ApplyView(const FMinimalViewInfo& View);
 virtual void CalcCamera(float DeltaTime,FMinimalViewInfo& OutResult) override;
private:
 FMinimalViewInfo AuthoredView;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWardrobeStationArrived,int32,Index);

/** Owned and ticked by the menu. No scene manager actor needs to be placed. */
UCLASS(Transient)
class DREAMBLOOM_API UWardrobeStationController : public UObject {
 GENERATED_BODY()
public:
 UPROPERTY(Transient) TObjectPtr<UWardrobeCharacterData> CharacterData;
 UPROPERTY(Transient) TArray<FWardrobeStation> Stations;
 UPROPERTY(BlueprintReadOnly,Category="Stations") int32 CurrentStation=INDEX_NONE;
 UPROPERTY(BlueprintReadOnly,Category="Stations") int32 TargetStation=INDEX_NONE;
 UPROPERTY(BlueprintReadOnly,Category="Stations") FText LastError;
 UPROPERTY(BlueprintAssignable,Category="Stations") FWardrobeStationArrived OnStationArrived;
 UFUNCTION(BlueprintCallable,Category="Stations") bool Initialize(APlayerController* Controller,UWardrobeCharacterData* Data,int32 PreferredStation=-1);
 UFUNCTION(BlueprintCallable,Category="Stations") bool SelectStation(int32 Index,bool bInstant=false);
 UFUNCTION(BlueprintCallable,Category="Stations") void SetPreviewEnabled(bool Enabled);
 UFUNCTION(BlueprintPure,Category="Stations") bool IsTransitioning() const{return bTransitioning;}
 UFUNCTION(BlueprintPure,Category="Stations") AWardrobeShowcase* GetActiveCharacter() const;
 UFUNCTION(BlueprintPure,Category="Stations") ACameraActor* GetPreviewCamera() const;
 UFUNCTION(BlueprintCallable,Category="Stations") void ResetPreview();
 bool CanManipulate() const;
 bool CanRotateCharacter() const;
 static float EvaluateProgress(const UCurveFloat* Curve,float Time);
 static FVector EvaluatePath(const FVector& Start,const FVector& Control1,const FVector& Control2,const FVector& End,float Alpha);
 void Tick(float DeltaSeconds);
 void Shutdown();
 virtual UWorld* GetWorld() const override;
private:
 bool ResolveStation(int32 Index);
 bool ValidateStation(int32 Index);
 void FinishTransition();
 void AbortTransition();
 UPROPERTY(Transient) TObjectPtr<AWardrobeViewCamera> ViewCamera;
 UPROPERTY(Transient) TObjectPtr<APlayerController> Player;
 UPROPERTY(Transient) TObjectPtr<AActor> PreviousViewTarget;
 UPROPERTY(Transient) TArray<TObjectPtr<AWardrobeShowcase>> ManagedCharacters;
 TMap<TWeakObjectPtr<AWardrobeShowcase>,FRotator> InitialRotations;
 FMinimalViewInfo StartView,EndView;
 FVector Control1,Control2;
 bool bTransitioning=false;
 bool bPreviewEnabled=false;
 float Elapsed=0;
};
