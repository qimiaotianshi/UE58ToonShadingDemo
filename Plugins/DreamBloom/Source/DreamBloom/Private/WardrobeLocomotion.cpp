#include "WardrobeLocomotion.h"
#include "WardrobeLoading.h"
#include "WardrobeShowcase.h"
#include "WardrobeMenu.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/Skeleton.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "UObject/UnrealType.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputActionValue.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Input/Reply.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
AWardrobeExplorer::AWardrobeExplorer(){
 PrimaryActorTick.bCanEverTick=true;bUseControllerRotationYaw=false;
 GetCapsuleComponent()->InitCapsuleSize(28,85);
 GetMesh()->SetRelativeLocation(FVector(0,0,-85));GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);
 CameraBoom=CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));CameraBoom->SetupAttachment(GetRootComponent());
 CameraBoom->TargetArmLength=350;CameraBoom->SocketOffset=FVector(0,25,25);CameraBoom->bUsePawnControlRotation=true;
 CameraBoom->bEnableCameraLag=true;CameraBoom->CameraLagSpeed=8;CameraBoom->ProbeSize=12;
 FollowCamera=CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));FollowCamera->SetupAttachment(CameraBoom);FollowCamera->FieldOfView=65;
 GetCharacterMovement()->bOrientRotationToMovement=true;GetCharacterMovement()->RotationRate=FRotator(0,480,0);
 GetCharacterMovement()->MaxWalkSpeed=160;GetCharacterMovement()->MaxAcceleration=900;GetCharacterMovement()->BrakingDecelerationWalking=1100;
 GetCharacterMovement()->bUseSeparateBrakingFriction=true;GetCharacterMovement()->BrakingFriction=.5f;GetCharacterMovement()->BrakingFrictionFactor=1.f;
 GetCharacterMovement()->MaxStepHeight=35;GetCharacterMovement()->SetWalkableFloorAngle(45);
}
void AWardrobeExplorer::BeginPlay(){Super::BeginPlay();ApplySettings();}
void AWardrobeExplorer::ApplySettings(){
 if(!Settings)return;auto* M=GetCharacterMovement();M->MaxWalkSpeed=bWantsSprint?Settings->RunSpeed:Settings->WalkSpeed;
 M->MaxAcceleration=Settings->Acceleration;M->BrakingDecelerationWalking=Settings->Braking;M->RotationRate.Yaw=Settings->TurnSpeed;
 M->JumpZVelocity=Settings->JumpVelocity;M->AirControl=Settings->AirControl;
 M->BrakingFriction=.5f;
 if(auto* Anim=Cast<UWardrobeLocomotionAnim>(GetMesh()->GetAnimInstance()))if(Anim->MovementMontage&&!M->IsFalling()){
  const auto Action=Anim->MovementAction;
  const bool Stop=Action==EWardrobeMovementAction::WalkStop||Action==EWardrobeMovementAction::RunStop;
  const bool Start=Action==EWardrobeMovementAction::WalkStart||Action==EWardrobeMovementAction::RunStart;
  if(Stop||Start){
   const auto* Track=Anim->MovementMontage->SlotAnimTracks.IsValidIndex(0)?&Anim->MovementMontage->SlotAnimTracks[0]:nullptr;
   const auto* Clip=Track&&Track->AnimTrack.AnimSegments.IsValidIndex(0)?Cast<UAnimSequence>(Track->AnimTrack.AnimSegments[0].GetAnimReference()):nullptr;
   if(Clip&&Clip->HasCurveData(TEXT("ALS_TravelSpeed"),false)){
    const float Position=Anim->Montage_GetPosition(Anim->MovementMontage);
    const float Envelope=FMath::Clamp(Clip->EvaluateCurveData(TEXT("ALS_TravelSpeed"),FAnimExtractContext(double(Position)),false),0.f,1.f);
    if(Start)M->MaxWalkSpeed*=FMath::Max(.03f,Envelope);
    else{
     // CharacterMovement still handles floor, walls and steps. Never restore speed lost to a collision.
     M->BrakingDecelerationWalking=0;M->BrakingFriction=0;
     const float Target=Anim->ActionStartSpeed*Envelope;
     if(M->Velocity.Size2D()>Target)M->Velocity=FVector(M->Velocity.GetSafeNormal2D()*Target)+FVector(0,0,M->Velocity.Z);
    }
   }
  }
 }
 const float Min=FMath::Max(50.f,Settings->CameraMinDistance),Max=FMath::Max(Min,Settings->CameraMaxDistance);
 CameraZoomOffset=FMath::Clamp(Settings->CameraDistance+CameraZoomOffset,Min,Max)-Settings->CameraDistance;
 if(!bCameraDistanceInitialized){CameraBoom->TargetArmLength=Settings->CameraDistance+CameraZoomOffset;bCameraDistanceInitialized=true;}
}
void AWardrobeExplorer::Tick(float Dt){
 Super::Tick(Dt);if(auto* GI=GetGameInstance())if(auto* L=GI->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading()){bWantsSprint=false;GetCharacterMovement()->StopMovementImmediately();}
 ApplySettings();
 if(Settings)CameraBoom->TargetArmLength=FMath::FInterpTo(CameraBoom->TargetArmLength,Settings->CameraDistance+CameraZoomOffset,Dt,12.f);
}
void AWardrobeExplorer::SetupPlayerInputComponent(UInputComponent* Input){
 Super::SetupPlayerInputComponent(Input);auto* E=Cast<UEnhancedInputComponent>(Input);if(!E||!Settings)return;
 if(Settings->MoveAction)E->BindAction(Settings->MoveAction,ETriggerEvent::Triggered,this,&AWardrobeExplorer::Move);
 if(Settings->LookAction)E->BindAction(Settings->LookAction,ETriggerEvent::Triggered,this,&AWardrobeExplorer::Look);
 if(Settings->ZoomAction)E->BindAction(Settings->ZoomAction,ETriggerEvent::Triggered,this,&AWardrobeExplorer::Zoom);
 if(Settings->SprintAction){E->BindAction(Settings->SprintAction,ETriggerEvent::Started,this,&AWardrobeExplorer::StartSprint);E->BindAction(Settings->SprintAction,ETriggerEvent::Completed,this,&AWardrobeExplorer::StopSprint);E->BindAction(Settings->SprintAction,ETriggerEvent::Canceled,this,&AWardrobeExplorer::StopSprint);}
 if(Settings->JumpAction){E->BindAction(Settings->JumpAction,ETriggerEvent::Started,this,&AWardrobeExplorer::StartJump);E->BindAction(Settings->JumpAction,ETriggerEvent::Completed,this,&AWardrobeExplorer::EndJump);E->BindAction(Settings->JumpAction,ETriggerEvent::Canceled,this,&AWardrobeExplorer::EndJump);}
 if(Settings->PauseAction)E->BindAction(Settings->PauseAction,ETriggerEvent::Started,this,&AWardrobeExplorer::TogglePause);
}
UInputComponent* AWardrobeExplorer::CreatePlayerInputComponent(){return NewObject<UEnhancedInputComponent>(this,TEXT("ExplorerInput"));}
void AWardrobeExplorer::Move(const FInputActionValue& V){
 if(!Controller||Controller->IsMoveInputIgnored())return;
 const FVector2D Input=V.Get<FVector2D>();const FRotator Yaw(0,Controller->GetControlRotation().Yaw,0);
 AddMovementInput(Yaw.Vector(),Input.Y);AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y),Input.X);
}
void AWardrobeExplorer::Look(const FInputActionValue& V){
 if(!Controller||Controller->IsLookInputIgnored())return;const FVector2D Input=V.Get<FVector2D>();AddControllerYawInput(Input.X*.7f);AddControllerPitchInput(-Input.Y*.7f);
}
void AWardrobeExplorer::Zoom(const FInputActionValue& V){
 if(!Settings||!Controller||Controller->IsLookInputIgnored()||!FMath::IsFinite(V.Get<float>()))return;
 if(auto* PC=Cast<AWardrobeExplorerController>(Controller))if(PC->bPauseMenu)return;
 if(auto* GI=GetGameInstance())if(auto* L=GI->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return;
 CameraZoomOffset-=V.Get<float>()*FMath::Max(1.f,Settings->CameraZoomStep);ApplySettings();
#if WITH_EDITOR
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeCameraZoomQA")))UE_LOG(LogTemp,Display,TEXT("ALS72_ZOOM_INPUT value=%.1f target=%.1f current=%.1f"),V.Get<float>(),Settings->CameraDistance+CameraZoomOffset,CameraBoom->TargetArmLength);
#endif
}
void AWardrobeExplorer::StartSprint(){bWantsSprint=true;ApplySettings();}
void AWardrobeExplorer::StopSprint(){bWantsSprint=false;ApplySettings();}
void AWardrobeExplorer::TogglePause(){if(auto* PC=Cast<AWardrobeExplorerController>(Controller))PC->TogglePauseMenu();}
void AWardrobeExplorer::StartJump(){
 if(!Controller||Controller->IsMoveInputIgnored())return;
 if(auto* PC=Cast<AWardrobeExplorerController>(Controller))if(PC->bPauseMenu)return;
 if(auto* GI=GetGameInstance())if(auto* L=GI->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return;
 Jump();
}
void AWardrobeExplorer::EndJump(){StopJumping();}
void UWardrobeLocomotionAnim::NativeInitializeAnimation(){
 Super::NativeInitializeAnimation();LastSpeed=PreviousVerticalSpeed=ActionElapsed=IdleElapsed=NextIdleTime=0;
 bHadMovementInput=bWasInAir=bMoving=bInAir=false;GroundIKAlpha=1;MovementAction=EWardrobeMovementAction::None;MovementMontage=nullptr;
 bStartPhaseMatched=bFallingLoop=false;ActionStartSpeed=0;
}
bool UWardrobeLocomotionAnim::PlayMovementAction(UAnimSequence* Animation,EWardrobeMovementAction Action,bool Loop,float StartTime,float Rate){
 if(!Animation||!GetSkelMeshComponent()||!GetSkelMeshComponent()->GetSkeletalMeshAsset()||Animation->GetSkeleton()!=GetSkelMeshComponent()->GetSkeletalMeshAsset()->GetSkeleton())return false;
 const auto* A=Cast<AWardrobeExplorer>(TryGetPawnOwner());const float Time=A&&A->Settings?A->Settings->BlendTime:.2f;
 FMontageBlendSettings In(FMath::Max(.03f,Time)),Out(FMath::Max(.03f,Time));
 In.BlendMode=Out.BlendMode=EMontageBlendMode::Inertialization;
 auto* M=PlaySlotAnimationAsDynamicMontage_WithBlendSettings(Animation,TEXT("DefaultSlot"),In,Out,FMath::Clamp(Rate,.5f,2.f),Loop?10000:1,-1.f,FMath::Clamp(StartTime,0.f,Animation->GetPlayLength()-.01f));
 if(!M)return false;MovementMontage=M;MovementAction=Action;ActionElapsed=0;ActionStartSpeed=Speed;bStartPhaseMatched=false;
 return true;
}
float UWardrobeLocomotionAnim::FindMatchingTime(UAnimSequence* Animation,float SearchEnd,float* OutError) const{
 auto* Mesh=GetSkelMeshComponent();if(!Animation||!Mesh||!Mesh->GetSkeletalMeshAsset())return 0;
 const auto Pose=Mesh->GetBoneSpaceTransforms();const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
 const auto& Skeleton=Animation->GetSkeleton()->GetReferenceSkeleton();float Best=MAX_flt,Time=0;
 const float End=FMath::Min(SearchEnd,Animation->GetPlayLength()-.001f);
 for(float T=0;T<=End;T+=1.f/30){float Error=0;int Count=0;
  for(FName Name:{FName(TEXT("足D_L")),FName(TEXT("ひざD_L")),FName(TEXT("足首D_L")),FName(TEXT("足D_R")),FName(TEXT("ひざD_R")),FName(TEXT("足首D_R")),FName(TEXT("上半身"))}){
   const int M=Ref.FindBoneIndex(Name),S=Skeleton.FindBoneIndex(Name);if(!Pose.IsValidIndex(M)||S==INDEX_NONE)continue;
   FTransform Sample;Animation->GetBoneTransform(Sample,FSkeletonPoseBoneIndex(S),FAnimExtractContext(double(T)),false);
   if(Sample.ContainsNaN()||Pose[M].ContainsNaN())continue;
   const float Angle=Sample.GetRotation().AngularDistance(Pose[M].GetRotation());Error+=Angle*Angle;Count++;
  }
  if(Count&&Error<Best){Best=Error;Time=T;}
 }
 if(OutError)*OutError=Best;return Time;
}
void UWardrobeLocomotionAnim::MatchMovingPhase(){
 if(!LocomotionBlendSpace)return;
 const auto* A=Cast<AWardrobeExplorer>(TryGetPawnOwner());if(!A||!A->Settings)return;
 // Update runs on the game thread before parallel animation evaluation. Node pointers never escape this call.
 for(TFieldIterator<FStructProperty> P(GetClass());P;++P)if(P->Struct==FAnimNode_BlendSpacePlayer::StaticStruct()){
  auto* Player=P->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayer>(this);
  if(Player->GetBlendSpace()!=LocomotionBlendSpace)continue;
  const auto& Samples=LocomotionBlendSpace->GetBlendSamples();UAnimSequence* Clip=nullptr;
  float Best=MAX_flt;for(const auto& Sample:Samples){const float Distance=FMath::Abs(Sample.SampleValue.X-(A->bWantsSprint?2.f:1.f));if(Distance<Best){Best=Distance;Clip=Sample.Animation;}}
  if(Clip)Player->SetAccumulatedTime(FindMatchingTime(Clip,Clip->GetPlayLength())/FMath::Max(.01f,Clip->GetPlayLength()));
 }
}
void UWardrobeLocomotionAnim::CancelMovementAction(){
 const auto* A=Cast<AWardrobeExplorer>(TryGetPawnOwner());FMontageBlendSettings Out(A&&A->Settings?FMath::Max(.03f,A->Settings->BlendTime):.2f);Out.BlendMode=EMontageBlendMode::Inertialization;
 if(MovementMontage)Montage_StopWithBlendSettings(Out,MovementMontage);MovementAction=EWardrobeMovementAction::None;MovementMontage=nullptr;ActionElapsed=0;
}
void UWardrobeLocomotionAnim::NativeUpdateAnimation(float Dt){
 Super::NativeUpdateAnimation(Dt);auto* A=Cast<AWardrobeExplorer>(TryGetPawnOwner());if(!A){Speed=Gait=0;LocomotionPlayRate=1;return;}
 LocomotionBlendSpace=A->Settings?A->Settings->LocomotionBlendSpace:nullptr;
 Speed=A->GetVelocity().Size2D();const float Walk=A->Settings?FMath::Max(1.f,A->Settings->WalkSpeed):160;
 const float Run=A->Settings?FMath::Max(Walk+1,A->Settings->RunSpeed):420;
 const float Target=Speed<=Walk?Speed/Walk:1.f+FMath::Clamp((Speed-Walk)/(Run-Walk),0.f,1.f);
 const float Time=A->Settings?A->Settings->BlendTime:.2f;Gait=FMath::Lerp(Gait,Target,Time<=0?1.f:1.f-FMath::Exp(-4.6f*FMath::Max(0.f,Dt)/Time));
 // Calibrate authored stride speed, including acceleration, braking and changed
 // movement settings. The blend-space samples themselves stay at rate 1.
 const float WalkSource=A->Settings?FMath::Max(1.f,A->Settings->WalkAnimationSpeed):106.f;
 const float RunSource=A->Settings?FMath::Max(1.f,A->Settings->RunAnimationSpeed):301.f;
 const float ExpectedSpeed=Gait<=1?Gait*WalkSource:FMath::Lerp(WalkSource,RunSource,FMath::Clamp(Gait-1,0.f,1.f));
 LocomotionPlayRate=Gait<.01f?1.f:FMath::Clamp(FMath::Max(Speed,20.f)/FMath::Max(1.f,ExpectedSpeed),.35f,2.f);
 auto* D=A->Settings.Get();if(!D)return;
 IdleAnimation=D->IdleAnimation;
 const auto* Move=A->GetCharacterMovement();const bool HasInput=!A->GetLastMovementInputVector().IsNearlyZero(.05f)||!Move->GetCurrentAcceleration().IsNearlyZero(.5f);
 bMoving=Speed>3.f;bInAir=Move->IsFalling();GroundIKAlpha=FMath::FInterpTo(GroundIKAlpha,bInAir?0.f:1.f,FMath::Max(0.f,Dt),18.f);
 const bool Left=GetCurveValue(TEXT("ALS_FootPlant_L"))>=GetCurveValue(TEXT("ALS_FootPlant_R"));
 ActionElapsed+=FMath::Max(0.f,Dt);
 if(MovementMontage&&!Montage_IsPlaying(MovementMontage)){
  MovementAction=EWardrobeMovementAction::None;MovementMontage=nullptr;
 }
 if(bInAir){
  IdleElapsed=0;
  if(!bWasInAir){
   if(A->GetVelocity().Z>30){const bool RunJump=Speed>Walk*1.2f;PlayMovementAction(RunJump?(Left?D->JumpRunLeft.Get():D->JumpRunRight.Get()):(Left?D->JumpWalkLeft.Get():D->JumpWalkRight.Get()),EWardrobeMovementAction::Takeoff);}
   else {bFallingLoop=true;PlayMovementAction(D->FallLoop?D->FallLoop.Get():D->AirLoop.Get(),EWardrobeMovementAction::Air,true);}
  }else if(MovementAction==EWardrobeMovementAction::None||(MovementAction==EWardrobeMovementAction::Takeoff&&ActionElapsed>FMath::Min(.45f,MovementMontage?MovementMontage->GetPlayLength()*.6f:.32f))){bFallingLoop=false;PlayMovementAction(D->AirLoop,EWardrobeMovementAction::Air,true);}
  else if(MovementAction==EWardrobeMovementAction::Air&&!bFallingLoop&&A->GetVelocity().Z< -100&&D->FallLoop){bFallingLoop=true;PlayMovementAction(D->FallLoop,EWardrobeMovementAction::Air,true);}
 }else if(bWasInAir){
  PlayMovementAction(FMath::Abs(PreviousVerticalSpeed)>D->HeavyLandingSpeed?D->HeavyLanding.Get():D->Landing.Get(),EWardrobeMovementAction::Land);
 }else{
  if(HasInput){
   IdleElapsed=0;NextIdleTime=0;
   if(MovementAction==EWardrobeMovementAction::WalkStop||MovementAction==EWardrobeMovementAction::RunStop||MovementAction==EWardrobeMovementAction::IdleVariation)CancelMovementAction();
   if(!bHadMovementInput&&LastSpeed<10&&MovementAction==EWardrobeMovementAction::None)PlayMovementAction(A->bWantsSprint?(Left?D->RunStartLeft.Get():D->RunStartRight.Get()):(Left?D->WalkStartLeft.Get():D->WalkStartRight.Get()),A->bWantsSprint?EWardrobeMovementAction::RunStart:EWardrobeMovementAction::WalkStart,false,0,FMath::Clamp((A->bWantsSprint?Run:Walk)/(A->bWantsSprint?RunSource:WalkSource),.75f,1.8f));
   if(MovementAction==EWardrobeMovementAction::Land&&ActionElapsed>.12f)CancelMovementAction();
   if(MovementMontage&&(MovementAction==EWardrobeMovementAction::WalkStart||MovementAction==EWardrobeMovementAction::RunStart)&&!bStartPhaseMatched&&Montage_GetPosition(MovementMontage)>=MovementMontage->GetPlayLength()-Time*Montage_GetPlayRate(MovementMontage)-.03f){MatchMovingPhase();bStartPhaseMatched=true;}
   if(MovementMontage&&((MovementAction==EWardrobeMovementAction::WalkStart&&A->bWantsSprint)||(MovementAction==EWardrobeMovementAction::RunStart&&!A->bWantsSprint))){MatchMovingPhase();CancelMovementAction();}
  }else if(bHadMovementInput&&LastSpeed>20&&MovementAction!=EWardrobeMovementAction::Land){
   const bool RunStop=LastSpeed>Walk*1.35f;
   auto* L=RunStop?D->RunStopLeft.Get():D->WalkStopLeft.Get();auto* R=RunStop?D->RunStopRight.Get():D->WalkStopRight.Get();
   // Find closest leg pose in the short entry window of both authored stop clips.
   float LE=MAX_flt,RE=MAX_flt;const float LT=FindMatchingTime(L,.23f,&LE),RT=FindMatchingTime(R,.23f,&RE);
   auto* Clip=LE<=RE?L:R;float Start=LE<=RE?LT:RT;
   const float Source=RunStop?RunSource:WalkSource;
   PlayMovementAction(Clip,RunStop?EWardrobeMovementAction::RunStop:EWardrobeMovementAction::WalkStop,false,Start,FMath::Clamp(LastSpeed/Source,.75f,1.8f));
  }
  if(!HasInput&&Speed<3&&MovementAction==EWardrobeMovementAction::None){
   IdleElapsed+=FMath::Max(0.f,Dt);
   if(NextIdleTime<=0)NextIdleTime=FMath::FRandRange(FMath::Max(.1f,D->IdleMinInterval),FMath::Max(D->IdleMinInterval,D->IdleMaxInterval));
   if(IdleElapsed>=NextIdleTime&&!D->IdleVariations.IsEmpty()){
    int32 Index=FMath::RandRange(0,D->IdleVariations.Num()-1);if(D->IdleVariations.Num()>1&&Index==LastIdleIndex)Index=(Index+1)%D->IdleVariations.Num();
    PlayMovementAction(D->IdleVariations[Index],EWardrobeMovementAction::IdleVariation);LastIdleIndex=Index;IdleElapsed=NextIdleTime=0;
   }
  }
 }
 LastSpeed=Speed;PreviousVerticalSpeed=A->GetVelocity().Z;bHadMovementInput=HasInput;bWasInAir=bInAir;
}
AWardrobeExplorerController::AWardrobeExplorerController(){OverridePlayerInputClass=UEnhancedPlayerInput::StaticClass();}
void AWardrobeExplorerController::SetExplorerInput(){
 bShowMouseCursor=false;FInputModeGameOnly Input;Input.SetConsumeCaptureMouseDown(false);SetInputMode(Input);
 if(PlayerCameraManager){PlayerCameraManager->ViewPitchMin=-65;PlayerCameraManager->ViewPitchMax=35;}
}
void AWardrobeExplorerController::BeginPlay(){Super::BeginPlay();SetControlRotation(FRotator(-12,GetControlRotation().Yaw,0));SetExplorerInput();}
void AWardrobeExplorerController::OnPossess(APawn* P){
 Super::OnPossess(P);SetExplorerInput();
 if(auto* A=Cast<AWardrobeExplorer>(P))if(A->Settings&&A->Settings->Mapping)if(auto* Local=GetLocalPlayer())if(auto* E=Local->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())E->AddMappingContext(A->Settings->Mapping,0);
}
void AWardrobeExplorerController::TogglePauseMenu(){
 if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return;
 if(bPauseMenu){
  if(PauseWidget&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(PauseWidget.ToSharedRef());PauseWidget.Reset();bPauseMenu=false;SetPause(false);ResetIgnoreMoveInput();ResetIgnoreLookInput();FlushPressedKeys();SetExplorerInput();return;
 }
 bPauseMenu=true;SetPause(true);SetIgnoreMoveInput(true);SetIgnoreLookInput(true);bShowMouseCursor=true;
 if(auto* A=Cast<AWardrobeExplorer>(GetPawn()))A->StopSprint();
 PauseWidget=SNew(SBorder).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24)
 [SNew(SVerticalBox)
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(STextBlock).Text(FText::FromString(TEXT("旅途暂停")))]
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).Text(FText::FromString(TEXT("继续游戏"))).OnClicked_Lambda([this](){TogglePauseMenu();return FReply::Handled();})]
 +SVerticalBox::Slot().AutoHeight().Padding(12)[SNew(SButton).Text(FText::FromString(TEXT("返回角色展示"))).OnClicked_Lambda([this](){ReturnToShowcase();return FReply::Handled();})]];
 if(auto* V=GetWorld()->GetGameViewport())V->AddViewportWidgetContent(PauseWidget.ToSharedRef(),90);
 FInputModeUIOnly Input;SetInputMode(Input);
}
void AWardrobeExplorerController::ReturnToShowcase(){
 if(bPauseMenu)TogglePauseMenu();auto* S=GetGameInstance()->GetSubsystem<UWardrobeSession>();auto* Menu=S&&S->ExplorationMenuData?S->ExplorationMenuData.Get():LoadObject<UWardrobeMenuData>(nullptr,TEXT("/Game/Wardrobe/DA_WardrobeMenu.DA_WardrobeMenu"));
 if(Menu)if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>()){
  const auto Level=S&&!S->ExplorationReturnLevel.IsNull()?S->ExplorationReturnLevel:Menu->MenuLevel;
  L->TravelToLevel(Level,Menu,{},FText::FromString(TEXT("返回奇境")),TEXT("game=/Script/DreamBloom.WardrobeMenuGameMode"));
 }
}
void AWardrobeExplorerController::EndPlay(const EEndPlayReason::Type R){
 if(PauseWidget&&GetWorld()->GetGameViewport())GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(PauseWidget.ToSharedRef());PauseWidget.Reset();Super::EndPlay(R);
}
AWardrobeExplorerGameMode::AWardrobeExplorerGameMode(){
 PlayerControllerClass=AWardrobeExplorerController::StaticClass();
 DefaultPawnClass=AWardrobeExplorer::StaticClass();
}

void AWardrobeExplorerController::PlayerTick(float Dt){
 Super::PlayerTick(Dt);
#if WITH_EDITOR
 if(!FParse::Param(FCommandLine::Get(),TEXT("WardrobeLocomotionQA")))return;
 if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return;
 auto* A=Cast<AWardrobeExplorer>(GetPawn());auto* E=Cast<UEnhancedPlayerInput>(PlayerInput);if(!A||!A->Settings||!E)return;
 if(QAStarted==0)QAStarted=FPlatformTime::Seconds();const double T=FPlatformTime::Seconds()-QAStarted;
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeJumpTrajectoryQA"))){
  auto* Anim=Cast<UWardrobeLocomotionAnim>(A->GetMesh()->GetAnimInstance());if(!Anim)return;
  if(QAStage==0){A->SetActorLocation(FVector(-1000,-1000,87.15));SetControlRotation(FRotator(-10,90,0));QAStage=1;}
  if(QAStage==1&&T>3){E->InjectInputForAction(A->Settings->JumpAction,FInputActionValue(true));QAStage=2;}
  if(QAStage==2&&T>3.12){const auto Local=A->GetMesh()->GetComponentTransform().InverseTransformPosition(A->GetMesh()->GetSocketLocation(TEXT("腰")));UE_LOG(LogTemp,Display,TEXT("ALS76_JUMP_PUSH_OFF actorZ=%.2f localPelvisZ=%.2f action=%d"),A->GetActorLocation().Z,Local.Z,int(Anim->MovementAction));FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ALS76-JumpPushOff.png"),false,false);QAStage=3;}
  if(QAStage==3&&T>3.43){const auto Local=A->GetMesh()->GetComponentTransform().InverseTransformPosition(A->GetMesh()->GetSocketLocation(TEXT("腰")));UE_LOG(LogTemp,Display,TEXT("ALS76_JUMP_APEX actorZ=%.2f localPelvisZ=%.2f action=%d"),A->GetActorLocation().Z,Local.Z,int(Anim->MovementAction));FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ALS76-JumpApex.png"),false,false);QAStage=4;}
  if(QAStage==4&&T>6){UE_LOG(LogTemp,Display,TEXT("ALS76_JUMP_LANDED actorZ=%.2f action=%d"),A->GetActorLocation().Z,int(Anim->MovementAction));QAStage=5;}
  if(T>8)FPlatformMisc::RequestExit(false);return;
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeExpandedMovementQA"))){
  auto* Anim=Cast<UWardrobeLocomotionAnim>(A->GetMesh()->GetAnimInstance());if(!Anim)return;
  const FString Folder=FPaths::ProjectSavedDir()/TEXT("Locomotion75QA");
  auto Shot=[&](const TCHAR* Name){IFileManager::Get().MakeDirectory(*Folder,true);FScreenshotRequest::RequestScreenshot(Folder/Name,false,false);UE_LOG(LogTemp,Display,TEXT("ALS75_QA %s speed=%.1f z=%.1f action=%d ground=%.2f slot=%.3f leg=%s actorYaw=%.2f cameraYaw=%.2f"),Name,Anim->Speed,A->GetActorLocation().Z,int(Anim->MovementAction),Anim->GroundIKAlpha,Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")),*A->GetMesh()->GetSocketRotation(TEXT("足D_L")).ToString(),A->GetActorRotation().Yaw,GetControlRotation().Yaw);};
  if(QAStage==0){A->SetActorLocation(FVector(-1000,-1000,85));SetControlRotation(FRotator(-10,90,0));QAStage++;}
  if(T>2&&T<4)E->InjectInputForAction(A->Settings->MoveAction,FInputActionValue(FVector2D(0,1)));
  if(QAStage==1&&T>4.18){Shot(TEXT("01-walk-stop.png"));QAStage++;}
  if(T>5&&T<7){E->InjectInputForAction(A->Settings->MoveAction,FInputActionValue(FVector2D(0,1)));E->InjectInputForAction(A->Settings->SprintAction,FInputActionValue(true));}
  if(QAStage==2&&T>7.2){Shot(TEXT("02-run-stop.png"));QAStage++;}
  if(QAStage==3&&T>8.5){E->InjectInputForAction(A->Settings->JumpAction,FInputActionValue(true));QAStage++;}
  if(QAStage==4&&T>8.98){Shot(TEXT("03-jump.png"));QAStage++;}
  if(QAStage==5&&Anim->MovementAction==EWardrobeMovementAction::Land&&Anim->ActionElapsed>.1f){Shot(TEXT("04-land.png"));QAStage++;}
  if(QAStage==6&&T>10.5){SetControlRotation(FRotator(-10,190,0));QAStage++;}
  if(QAStage==7&&T>10.85){Shot(TEXT("05-camera-orbit.png"));QAStage++;}
  if(QAStage==8&&T>12){Shot(TEXT("06-idle.png"));QAStage++;}
  if(QAStage==9&&Anim->MovementAction==EWardrobeMovementAction::IdleVariation&&Anim->ActionElapsed>.5f){Shot(TEXT("07-idle-variation.png"));QAStage++;}
  if(T>30){FPlatformMisc::RequestExit(false);}return;
 }
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("LocomotionQA");
 auto Capture=[&](const TCHAR* Name){IFileManager::Get().MakeDirectory(*Dir,true);FScreenshotRequest::RequestScreenshot(Dir/Name,true,false);UE_LOG(LogTemp,Display,TEXT("ALS68_QA %s speed=%.2f gait=%.3f position=%s"),Name,A->GetVelocity().Size2D(),Cast<UWardrobeLocomotionAnim>(A->GetMesh()->GetAnimInstance())->Gait,*A->GetActorLocation().ToString());};
 if(QAStage==0&&T>3){SetControlRotation(FRotator(-12,180,0));QAStage++;}
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeCameraZoomQA"))){
  if(A->Settings->ZoomAction&&T>3.1&&T<3.2)E->InjectInputForAction(A->Settings->ZoomAction,FInputActionValue(1.f));
  if(A->Settings->ZoomAction&&T>5.2&&T<5.3)E->InjectInputForAction(A->Settings->ZoomAction,FInputActionValue(-1.f));
 }
 if(QAStage==1&&T>5){Capture(TEXT("01-front.png"));QAStage++;}
 if(QAStage==2&&T>6){SetControlRotation(FRotator(-12,0,0));QAStage++;}
 if(QAStage==3&&T>8){Capture(TEXT("02-walk.png"));QAStage++;}
 if(QAStage==4&&T>10){Capture(TEXT("03-run.png"));QAStage++;}
 if(QAStage==5&&T>12){Capture(TEXT("04-stop.png"));QAStage++;}
 if(QAStage==6&&T>13){TogglePauseMenu();Capture(TEXT("05-pause.png"));QAStage++;}
 // Paused input is intentionally UI only; resume with the same button handler through a core ticker.
 if(QAStage==7){QAStage++;TWeakObjectPtr<AWardrobeExplorerController> Weak=this;FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float){if(Weak.IsValid()){Weak->TogglePauseMenu();UE_LOG(LogTemp,Display,TEXT("ALS68_QA_RESUMED"));}return false;}),1.f);}
 if(QAStage==8&&T>16){QAStage++;if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeLocomotionReturnQA")))ReturnToShowcase();else FPlatformMisc::RequestExit(false);}
 if(T>6&&T<11){E->InjectInputForAction(A->Settings->MoveAction,FInputActionValue(FVector2D(1,0)));if(T>8.5)E->InjectInputForAction(A->Settings->SprintAction,FInputActionValue(true));}
#endif
}
