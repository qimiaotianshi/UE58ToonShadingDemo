#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Animation/AnimInstance.h"
#include "WardrobeLocomotion.generated.h"
class UInputAction;class UInputMappingContext;class USpringArmComponent;class UCameraComponent;class UBlendSpace;class UAnimSequence;class UAnimMontage;
struct FInputActionValue;
UENUM(BlueprintType)
enum class EWardrobeMovementAction : uint8 {None,WalkStart,RunStart,WalkStop,RunStop,Takeoff,Air,Land,IdleVariation=9};
UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeLocomotionData : public UDataAsset {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement",meta=(ClampMin="1",Units="cm/s")) float WalkSpeed=160;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement",meta=(ClampMin="1",Units="cm/s")) float RunSpeed=420;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement",meta=(ClampMin="1")) float Acceleration=900;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement",meta=(ClampMin="1")) float Braking=1100;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Movement",meta=(ClampMin="0")) float TurnSpeed=480;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation",meta=(ClampMin="0",Units="s")) float BlendTime=.2f;
 /** Speed covered by the source animation at playback rate 1. Recalibrate when replacing clips. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation",meta=(ClampMin="1",Units="cm/s")) float WalkAnimationSpeed=106.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation",meta=(ClampMin="1",Units="cm/s")) float RunAnimationSpeed=301.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Animation") TObjectPtr<UBlendSpace> LocomotionBlendSpace;
 /** Left/right variants start on the corresponding planted-foot phase. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Start and Stop") TObjectPtr<UAnimSequence> WalkStartLeft;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Start and Stop") TObjectPtr<UAnimSequence> WalkStartRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Start and Stop") TObjectPtr<UAnimSequence> RunStartLeft;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Start and Stop") TObjectPtr<UAnimSequence> RunStartRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Start and Stop") TObjectPtr<UAnimSequence> WalkStopLeft;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Start and Stop") TObjectPtr<UAnimSequence> WalkStopRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Start and Stop") TObjectPtr<UAnimSequence> RunStopLeft;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Start and Stop") TObjectPtr<UAnimSequence> RunStopRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") TObjectPtr<UAnimSequence> JumpWalkLeft;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") TObjectPtr<UAnimSequence> JumpWalkRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") TObjectPtr<UAnimSequence> JumpRunLeft;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") TObjectPtr<UAnimSequence> JumpRunRight;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") TObjectPtr<UAnimSequence> AirLoop;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") TObjectPtr<UAnimSequence> FallLoop;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") TObjectPtr<UAnimSequence> Landing;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump") TObjectPtr<UAnimSequence> HeavyLanding;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump",meta=(ClampMin="1",Units="cm/s")) float JumpVelocity=420;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump",meta=(ClampMin="0",ClampMax="1")) float AirControl=.3f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Jump",meta=(ClampMin="1",Units="cm/s")) float HeavyLandingSpeed=650;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Idle") TObjectPtr<UAnimSequence> IdleAnimation;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Idle") TArray<TObjectPtr<UAnimSequence>> IdleVariations;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Idle",meta=(ClampMin="0",Units="s")) float IdleMinInterval=8;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Idle",meta=(ClampMin="0",Units="s")) float IdleMaxInterval=15;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Camera",meta=(ClampMin="50",Units="cm")) float CameraDistance=300;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Camera",meta=(ClampMin="50",Units="cm")) float CameraMinDistance=120;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Camera",meta=(ClampMin="50",Units="cm")) float CameraMaxDistance=600;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Camera",meta=(ClampMin="1",Units="cm")) float CameraZoomStep=35;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Input") TObjectPtr<UInputMappingContext> Mapping;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Input") TObjectPtr<UInputAction> MoveAction;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Input") TObjectPtr<UInputAction> LookAction;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Input") TObjectPtr<UInputAction> ZoomAction;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Input") TObjectPtr<UInputAction> SprintAction;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Input") TObjectPtr<UInputAction> JumpAction;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Input") TObjectPtr<UInputAction> PauseAction;
};
UCLASS(Blueprintable)
class DREAMBLOOM_API AWardrobeExplorer : public ACharacter {
 GENERATED_BODY()
public:
 AWardrobeExplorer();
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Locomotion") TObjectPtr<UWardrobeLocomotionData> Settings;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Camera") TObjectPtr<USpringArmComponent> CameraBoom;
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Camera") TObjectPtr<UCameraComponent> FollowCamera;
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
 virtual UInputComponent* CreatePlayerInputComponent() override;
 void Move(const FInputActionValue& Value);
 void Look(const FInputActionValue& Value);
 void Zoom(const FInputActionValue& Value);
 void StartSprint();void StopSprint();void TogglePause();
 void StartJump();void EndJump();
 void ApplySettings();
 bool bWantsSprint=false;
 float CameraZoomOffset=0;
 bool bCameraDistanceInitialized=false;
};
UCLASS(Blueprintable)
class DREAMBLOOM_API UWardrobeLocomotionAnim : public UAnimInstance {
 GENERATED_BODY()
public:
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") float Speed=0;
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") float Gait=0;
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") float LocomotionPlayRate=1;
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") TObjectPtr<UBlendSpace> LocomotionBlendSpace;
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") TObjectPtr<UAnimSequence> IdleAnimation;
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") bool bMoving=false;
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") bool bInAir=false;
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") float GroundIKAlpha=1;
 UPROPERTY(BlueprintReadOnly,Category="Locomotion") EWardrobeMovementAction MovementAction=EWardrobeMovementAction::None;
 virtual void NativeInitializeAnimation() override;
 virtual void NativeUpdateAnimation(float Dt) override;
 bool PlayMovementAction(UAnimSequence* Animation,EWardrobeMovementAction Action,bool Loop=false,float StartTime=0,float Rate=1);
 float FindMatchingTime(UAnimSequence* Animation,float SearchEnd,float* OutError=nullptr) const;
 void MatchMovingPhase();
 void CancelMovementAction();
 UPROPERTY(Transient) TObjectPtr<UAnimMontage> MovementMontage;
 float LastSpeed=0,PreviousVerticalSpeed=0,ActionElapsed=0,IdleElapsed=0,NextIdleTime=0;
 float ActionStartSpeed=0;
 bool bStartPhaseMatched=false,bFallingLoop=false;
 int32 LastIdleIndex=INDEX_NONE;
 bool bHadMovementInput=false,bWasInAir=false;
};
UCLASS(Blueprintable)
class DREAMBLOOM_API AWardrobeExplorerController : public APlayerController {
 GENERATED_BODY()
public:
 AWardrobeExplorerController();
 virtual void BeginPlay() override;
 virtual void OnPossess(APawn* Pawn) override;
 virtual void PlayerTick(float Dt) override;
 void SetExplorerInput();void TogglePauseMenu();void ReturnToShowcase();
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 bool bPauseMenu=false;
private:
 TSharedPtr<class SWidget> PauseWidget;
 double QAStarted=0;
 int32 QAStage=0;
};
UCLASS(Blueprintable)
class DREAMBLOOM_API AWardrobeExplorerGameMode : public AGameModeBase {
 GENERATED_BODY()
public:
 AWardrobeExplorerGameMode();
};
