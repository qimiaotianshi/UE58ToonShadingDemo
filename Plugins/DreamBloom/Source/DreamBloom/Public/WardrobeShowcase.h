#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameModeBase.h"
#include "WardrobeShowcase.generated.h"

class USkeletalMeshComponent;
class UAnimInstance;
class ULevelSequence;
class ULevelSequencePlayer;
class ALevelSequenceActor;
class UWardrobeAudioData;
class UWardrobeCharacterData;
class UWardrobeClickEffectData;
enum class EWardrobeUISound : uint8;

USTRUCT(BlueprintType)
struct DREAMBLOOM_API FWardrobeLevelOption {
 GENERATED_BODY()
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Level") FText Label;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Level") TSoftObjectPtr<UWorld> Level;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Level") TSoftObjectPtr<UTexture2D> Thumbnail;
 /** Empty uses the global Loading Background. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Level",meta=(DisplayName="Loading Background Override")) TSoftObjectPtr<UTexture2D> LoadingBackground;
};

UCLASS(BlueprintType)
class DREAMBLOOM_API UWardrobeMenuData : public UDataAsset {
 GENERATED_BODY()
public:
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Menu") TArray<FWardrobeLevelOption> Levels;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Menu") TSoftObjectPtr<UWorld> MenuLevel=TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Wardrobe/Maps/L_WardrobeMain.L_WardrobeMain")));
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Characters") TObjectPtr<UWardrobeCharacterData> CharacterData;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Interaction",meta=(ClampMin="1",ClampMax="50")) float ClickDragThreshold=6.f;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="UI Audio") TObjectPtr<UWardrobeAudioData> AudioData;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Click Effect") TObjectPtr<UWardrobeClickEffectData> ClickEffect;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Loading Screen") TSoftObjectPtr<UTexture2D> LoadingBackground;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Loading Screen") FText LoadingHint=FText::FromString(TEXT("新的风景，即将展开。"));
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Loading Screen",meta=(ClampMin="0.0",Units="s")) float MinimumLoadingScreenTime=1.5f;
 /** Render the destination under the cover, then require streaming to stay quiet for 0.5 seconds. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Loading Screen",meta=(ClampMin="0.5",Units="s")) float SceneWarmupTime=2.f;
 /** After this time, let the player explicitly enter while remaining resources continue loading. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Loading Screen",meta=(ClampMin="5.0",Units="s")) float SlowLoadingWarningTime=45.f;
};

/** One character placed in the scene; its mesh and optional physics AnimBP are authored on Character. */
UCLASS(Blueprintable)
class DREAMBLOOM_API AWardrobeShowcase : public AActor {
 GENERATED_BODY()
public:
 AWardrobeShowcase();
 UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Character") TObjectPtr<USkeletalMeshComponent> Character;
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Menu") TObjectPtr<UWardrobeMenuData> MenuData;
 /** Set automatically from the DA entry whose CharacterTag resolves to this actor. */
 UPROPERTY(Transient,BlueprintReadOnly,Category="Interaction") TObjectPtr<UWardrobeCharacterData> InteractionData;
 UPROPERTY(Transient,BlueprintReadOnly,Category="Interaction") int32 InteractionProfileIndex=INDEX_NONE;
 const struct FWardrobeCharacterOption* GetInteractionProfile() const;
 UFUNCTION(BlueprintCallable,Category="Interaction") void SetInteractionEnabled(bool Enabled);
 UFUNCTION(BlueprintPure,Category="Interaction") bool IsInteractionEnabled() const{return bInteractionEnabled;}
 void RestoreCharacterAnimBlueprint();
 UFUNCTION(BlueprintCallable,Category="Default Sequence") bool StartDefaultSequence();
 UFUNCTION(BlueprintCallable,Category="Default Sequence") void StopDefaultSequence();
 UFUNCTION(BlueprintPure,Category="Default Sequence") bool IsDefaultSequencePlaying() const;
 UFUNCTION(BlueprintPure,Category="Default Sequence") ULevelSequencePlayer* GetDefaultSequencePlayer() const{return DefaultSequencePlayer;}

 UFUNCTION(BlueprintCallable,Category="Interaction") void BuildInteractionRegions();
 void TickInteraction(float DeltaSeconds);
 /** AnimBP: Try Get Pawn Owner is unsuitable for this actor; use Get Owning Actor. */
 UFUNCTION(BlueprintPure,Category="Sequence Physics Blend") float GetSequencePhysicsWeight() const{return SequencePhysicsWeight;}
 UFUNCTION(BlueprintPure,Category="Sequence Physics Blend") bool IsSequenceSwitching() const{return PendingSequenceSwitch!=0;}
 void TickSequencePhysicsBlend(float Dt);
 void BeginSequencePhysicsFadeIn();
 void StartPreparedInteraction();
 void CancelInteraction(bool RestartDefaultSequence);
 UFUNCTION(BlueprintCallable,Category="Interaction") bool TriggerInteraction(int32 RegionIndex);
 bool ClickInteraction(const FVector& Origin,const FVector& Direction);
 bool TraceTouch(const FVector& Origin,const FVector& Direction,FHitResult& Hit) const;
 bool ApplyPhysicsTouch(const FVector& Position,const FVector& Direction);
 bool BeginPhysicsDrag(const FVector& Origin,const FVector& Direction);
 void UpdatePhysicsDrag(const FVector& Origin,const FVector& Direction);
 bool IsPhysicsDragging() const{return bPhysicsDragging;}
 bool BeginPinch(const FVector& Origin,const FVector& Direction);
 void UpdatePinch(const FVector& Origin,const FVector& Direction);
 void ReleasePinch();
 void ClearPinch();
 void TickPinch(float Dt);
 FVector GetPinchOffset(FName Bone) const;
 FQuat GetPinchReactionRotation(FName Bone) const;
 FQuat GetCursorLookRotation(FName Bone) const;
 void TickCursorLook(float Dt);
 void UpdateCursorLook(float Dt,const FVector& Origin,const FVector& Ray,bool bHasPointer);
 bool IsPinching() const{return ActivePinch!=INDEX_NONE;}
 void TickPhysicsTouch(float Dt);
 void StopPhysicsTouch();
 int32 HitInteraction(const FVector& Origin,const FVector& Direction) const;
 UFUNCTION(BlueprintPure,Category="Interaction") bool IsInteracting() const{return ActiveInteraction!=INDEX_NONE;}
 int32 InteractionPlayCount=0;
 UPROPERTY(BlueprintReadOnly,Category="Interaction") FText CharacterMessage;
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
 bool bInteractionEnabled=false;
 float SequencePhysicsWeight=1.f;
 float PhysicsBlendFrom=1.f,PhysicsBlendTarget=1.f,PhysicsBlendAge=0.f,PhysicsBlendDuration=0.f;
 uint8 PendingSequenceSwitch=0; // 1 = prepared reaction, 2 = return to default
 bool bInteractionStarted=false;
 bool bResetSequenceDynamics=false;
 void FadeSequencePhysics(float Target,float Duration);
 FVector2D CursorLookAngles=FVector2D::ZeroVector;
 float CursorLookIdleAge=0;
 struct FPinchState {FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector,Target=FVector::ZeroVector;FName Bone,PullMorph;FVector ReleaseReaction=FVector::ZeroVector;float ReturnAge=0;bool bReturning=false;FQuat StartComponentRotation=FQuat::Identity;};
 TArray<FPinchState> PinchStates;
 int32 ActivePinch=INDEX_NONE;
 FVector PinchPlanePoint=FVector::ZeroVector,PinchPlaneNormal=FVector::ForwardVector;
 FQuat PinchStartRotation=FQuat::Identity;
 FVector PinchGrabOffset=FVector::ZeroVector;
 TSet<FName> PinchDrivenCurves;
 FVector LastPinchSoundTarget=FVector::ZeroVector;
 void PlayPinchSound(const struct FWardrobeSoundEntry& Entry,EWardrobeUISound Event);
 bool bPhysicsTouchActive=false;
 bool bPhysicsDragging=false;
 FVector DragPlanePoint=FVector::ZeroVector;
 FVector DragPlaneNormal=FVector::ForwardVector;
 float PhysicsTouchRemaining=0;
 double PhysicsTouchReadyAt=0;
 UPROPERTY(Transient) TSubclassOf<UAnimInstance> InitialAnimClass;
 UPROPERTY(Transient) TArray<TObjectPtr<class UShapeComponent>> InteractionShapes;
 TArray<int32> InteractionRegionIndices;
 TMap<int32,double> RegionReadyAt;
 TMap<int32,int32> LastReaction;
 UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> DefaultSequencePlayer;
 UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> DefaultSequenceActor;
 UPROPERTY(Transient) TObjectPtr<ULevelSequence> PlayingDefaultSequence;
 bool bEndingShowcase=false;
 UPROPERTY(Transient) TObjectPtr<ULevelSequencePlayer> InteractionPlayer;
 UPROPERTY(Transient) TObjectPtr<ALevelSequenceActor> InteractionSequenceActor;
 UPROPERTY(Transient) TObjectPtr<AActor> InteractionPreviousCamera;
 FTransform InteractionOriginalTransform;
 FTransform InteractionOriginalCameraTransform;
 float InteractionOriginalCameraFov=0;
 int32 ActiveInteraction=INDEX_NONE;
 float InteractionAge=0;
 double InteractionReadyAt=0;
};

UCLASS()
class DREAMBLOOM_API AWardrobePreviewGameMode : public AGameModeBase {
 GENERATED_BODY()
public:
 AWardrobePreviewGameMode();
};

