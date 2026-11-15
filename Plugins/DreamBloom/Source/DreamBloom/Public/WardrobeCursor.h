#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "InputCoreTypes.h"
#include "WardrobeCursor.generated.h"

class UTexture2D;class UGameViewportClient;class SWidget;class FWardrobeCursorProcessor;
UENUM(BlueprintType)
enum class EWardrobeCursorState : uint8 { Default, Pressed, Released };
USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeCursorSettings {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite) bool bEnabled=true;
 /** Empty textures use the bundled CC0 pink cursor images. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UTexture2D> DefaultTexture;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UTexture2D> PressedTexture;
 UPROPERTY(EditAnywhere,BlueprintReadWrite) TSoftObjectPtr<UTexture2D> ReleasedTexture;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="16",ClampMax="128")) float Size=40.f;
 /** Normalized location of the arrow tip within the image. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite) FVector2D HotSpot=FVector2D(.0625,.0625);
 UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(ClampMin="0",ClampMax="1",Units="s")) float ReleaseTime=.16f;
};
/** Pure state keeps click feedback independent of UMG mouse capture. */
struct DREAMBLOOM_API FWardrobeCursorInputState {
 TSet<FKey> Buttons;double ReleasedUntil=0;float ReleaseTime=.16f;
 void Down(FKey Button);void Up(FKey Button,double Now);void Reset();
 EWardrobeCursorState Get(double Now) const;
};
UCLASS()
class DREAMBLOOM_API UWardrobeCursorSubsystem : public UGameInstanceSubsystem {
 GENERATED_BODY()
public:
 void Configure(const FWardrobeCursorSettings& Settings);
 UFUNCTION(BlueprintPure,Category="Wardrobe|Cursor") EWardrobeCursorState GetCursorState() const;
 virtual void Deinitialize() override;
private:
 void Restore();
 TWeakObjectPtr<UGameViewportClient> Viewport;
 TSharedPtr<FWardrobeCursorProcessor> Processor;
 TMap<int32,TSharedPtr<SWidget>> PreviousWidgets;
 bool bPreviousSoftware=true,bPreviousHideDuringCapture=true;
 UPROPERTY(Transient) TArray<TObjectPtr<UTexture2D>> LoadedTextures;
};
