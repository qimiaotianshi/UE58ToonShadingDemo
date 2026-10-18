#include "WardrobeLocomotion.h"
#include "WardrobeLoading.h"
#include "WardrobeShowcase.h"
#include "WardrobeMenu.h"
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
 GetCharacterMovement()->MaxStepHeight=35;GetCharacterMovement()->SetWalkableFloorAngle(45);
}
void AWardrobeExplorer::BeginPlay(){Super::BeginPlay();ApplySettings();}
void AWardrobeExplorer::ApplySettings(){
 if(!Settings)return;auto* M=GetCharacterMovement();M->MaxWalkSpeed=bWantsSprint?Settings->RunSpeed:Settings->WalkSpeed;
 M->MaxAcceleration=Settings->Acceleration;M->BrakingDecelerationWalking=Settings->Braking;M->RotationRate.Yaw=Settings->TurnSpeed;
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
 LocomotionPlayRate=Gait<.01f?1.f:(Speed<1.f?0.f:FMath::Clamp(Speed/FMath::Max(1.f,ExpectedSpeed),.1f,2.f));
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
