#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WardrobeAudio.generated.h"

class USoundBase;
class UAudioComponent;
UENUM(BlueprintType)
enum class EWardrobeUISound : uint8 {
 ActionSwitch=0 UMETA(DisplayName="Station Switch"), MapSelect=1, StartGame=4, Login=5,
 PinchGrab=14, PinchMove=15, PinchRelease=16, Back=6, Confirm=7, Cancel=8, Unavailable=9, ResetCamera=13
};
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeSoundEntry {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sound") TSoftObjectPtr<USoundBase> Sound;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sound",meta=(ClampMin="0.0",ClampMax="2.0")) float Volume=.65f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sound",meta=(ClampMin="0.5",ClampMax="2.0")) float Pitch=1.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Sound",meta=(ClampMin="0.0",Units="s")) float MinInterval=.08f;
};
UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeAudioData : public UDataAsset {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Master") bool bEnabled=true;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Master",meta=(ClampMin="0.0",ClampMax="1.0")) float MasterVolume=.7f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Master",meta=(ClampMin="1",ClampMax="16")) int32 MaxSimultaneousSounds=4;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events",meta=(DisplayName="Station Switch")) FWardrobeSoundEntry ActionSwitch;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events") FWardrobeSoundEntry MapSelect;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events") FWardrobeSoundEntry StartGame;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events") FWardrobeSoundEntry Login;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events") FWardrobeSoundEntry Back;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events") FWardrobeSoundEntry Confirm;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events") FWardrobeSoundEntry Cancel;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events") FWardrobeSoundEntry Unavailable;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Events") FWardrobeSoundEntry ResetCamera;
 const FWardrobeSoundEntry& GetEntry(EWardrobeUISound Event) const;
};

/** 2D UI sounds persist through OpenLevel and are independent of the preview camera. */
UCLASS()
class DREAMBLOOM_API UWardrobeAudioSubsystem : public UGameInstanceSubsystem {
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Audio") void Configure(UWardrobeAudioData* Data);
 UFUNCTION(BlueprintCallable,Category="Wardrobe|Audio") bool PlayEvent(EWardrobeUISound Event);
 bool PlayEntry(EWardrobeUISound Event,const FWardrobeSoundEntry& Entry);
 virtual void Deinitialize() override;
 int32 PlayCount=0;
 void AuditTravelAudio();
private:
 UPROPERTY(Transient) TObjectPtr<UWardrobeAudioData> Settings;
 UPROPERTY(Transient) TArray<TObjectPtr<USoundBase>> LoadedSounds;
 UPROPERTY(Transient) TArray<TObjectPtr<UAudioComponent>> ActiveSounds;
 TMap<EWardrobeUISound,double> LastPlayed;
};
