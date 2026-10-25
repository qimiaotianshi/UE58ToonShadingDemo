#include "WardrobeLocomotion.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeMenu.h"
#include "WardrobeShowcase.h"
#include "Sound/SoundBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "InputActionValue.h"
#include "EnhancedInputComponent.h"
#include "InputMappingContext.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Events.h"
#include "Engine/GameInstance.h"
#include "WardrobeFootIKNode.h"
#include "AnimNode_KawaiiPhysics.h"
#include "UObject/UnrealType.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeLocomotionTest,"DreamBloom.Locomotion.WalkRunBlendAndCollision",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeLocomotionTest::RunTest(const FString&){
 auto* Class=LoadClass<AWardrobeExplorer>(nullptr,TEXT("/Game/BluePrint/ALS/Characters/BP_ALSCharacter.BP_ALSCharacter_C"));
 if(!TestNotNull(TEXT("Saved playable character blueprint"),Class))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeExplorer>(Class,FVector(0,0,85),FRotator::ZeroRotator);
 auto* PC=W->SpawnActor<AWardrobeExplorerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));PC->Possess(A);A->ApplySettings();
 PC->ResetIgnoreMoveInput();PC->ResetIgnoreLookInput();
 A->GetCharacterMovement()->bRunPhysicsWithNoController=true;
 TestNotNull(TEXT("Saved data asset retained"),A->Settings.Get());
 TestNotNull(TEXT("Saved skeletal mesh retained"),A->GetMesh()->GetSkeletalMeshAsset());
 const float MeshHalfHeight=A->GetMesh()->GetSkeletalMeshAsset()->GetImportedBounds().BoxExtent.Z;
 TestTrue(TEXT("FBX unit conversion retains a character about 155 cm tall"),MeshHalfHeight>60&&MeshHalfHeight<100);
 TestTrue(TEXT("X-forward FBX requires no extra mesh yaw compensation"),FMath::Abs(A->GetMesh()->GetRelativeRotation().Yaw)<.01f);
 auto* Anim=Cast<UWardrobeLocomotionAnim>(A->GetMesh()->GetAnimInstance());
 if(!TestNotNull(TEXT("Saved animation blueprint retained"),Anim)){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 auto* FloorActor=W->SpawnActor<AActor>();auto* Floor=NewObject<UBoxComponent>(FloorActor);FloorActor->SetRootComponent(Floor);Floor->SetBoxExtent(FVector(10000,10000,10));Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Floor->RegisterComponent();FloorActor->SetActorLocation(FVector(0,0,-10));
 auto Eval=[&](){A->GetMesh()->TickAnimation(1.f/60,false);A->GetMesh()->RefreshBoneTransforms();A->GetMesh()->HandleExistingParallelEvaluationTask(true,true);};
 auto Step=[&](int Frames,bool Moving){for(int I=0;I<Frames;I++){if(Moving)A->Move(FInputActionValue(FVector2D(0,1)));A->GetCharacterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);Eval();}};
 A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);PC->SetControlRotation(FRotator(0,0,0));Eval();
 TestTrue(TEXT("Idle gait begins neutral"),Anim->Gait<.01f);
 const FQuat IdleLeg=A->GetMesh()->GetSocketQuaternion(TEXT("足D_L"));Step(90,true);
 TestTrue(TEXT("Walk head stays above the pelvis after FBX coordinate conversion"),A->GetMesh()->GetSocketLocation(TEXT("頭")).Z>A->GetMesh()->GetSocketLocation(TEXT("腰")).Z+20);
 AddInfo(FString::Printf(TEXT("Walk: location=%s velocity=%s mode=%d local=%d ignored=%d input=%s"),*A->GetActorLocation().ToString(),*A->GetVelocity().ToString(),int(A->GetCharacterMovement()->MovementMode),A->IsLocallyControlled(),PC->IsMoveInputIgnored(),*A->GetPendingMovementInputVector().ToString()));
 TestTrue(TEXT("W walks at configured speed"),FMath::Abs(A->GetVelocity().Size2D()-A->Settings->WalkSpeed)<1);
 TestTrue(TEXT("Walking moves character through the map"),A->GetActorLocation().X>100);
 TestTrue(TEXT("Walk sample selected smoothly"),FMath::Abs(Anim->Gait-1)<.02f);
 TestTrue(TEXT("Walk playback matches source stride speed"),FMath::Abs(Anim->LocomotionPlayRate-A->Settings->WalkSpeed/A->Settings->WalkAnimationSpeed)<.02f);
 TestTrue(TEXT("Walking changes actual leg pose"),IdleLeg.AngularDistance(A->GetMesh()->GetSocketQuaternion(TEXT("足D_L")))>.05f);
 A->StartSprint();const float Before=A->GetVelocity().Size2D();Step(1,true);
 TestTrue(TEXT("Sprint accelerates rather than teleports speed"),A->GetVelocity().Size2D()>Before&&A->GetVelocity().Size2D()<A->Settings->RunSpeed);
 Step(90,true);TestTrue(TEXT("Shift reaches configured run speed"),FMath::Abs(A->GetVelocity().Size2D()-A->Settings->RunSpeed)<1);
 TestTrue(TEXT("Run head stays above the pelvis after FBX coordinate conversion"),A->GetMesh()->GetSocketLocation(TEXT("頭")).Z>A->GetMesh()->GetSocketLocation(TEXT("腰")).Z+20);
 TestTrue(TEXT("Run sample selected"),Anim->Gait>1.98f);
 TestTrue(TEXT("Run playback matches source stride speed"),FMath::Abs(Anim->LocomotionPlayRate-A->Settings->RunSpeed/A->Settings->RunAnimationSpeed)<.02f);
 A->StopSprint();Step(90,true);TestTrue(TEXT("Releasing Shift returns to walk"),FMath::Abs(Anim->Gait-1)<.02f);
 Step(90,false);TestTrue(TEXT("Released movement stops capsule"),A->GetVelocity().Size2D()<.1f);TestTrue(TEXT("Stopped character blends back to idle"),Anim->Gait<.01f);
 const FVector Start=A->GetActorLocation();PC->SetControlRotation(FRotator(0,90,0));Step(60,true);
 TestTrue(TEXT("Movement follows camera yaw"),A->GetActorLocation().Y>Start.Y+80);
 Step(90,false);PC->SetControlRotation(FRotator(0,0,0));
 auto* WallActor=W->SpawnActor<AActor>();auto* Wall=NewObject<UBoxComponent>(WallActor);WallActor->SetRootComponent(Wall);Wall->SetBoxExtent(FVector(10,500,150));Wall->SetCollisionResponseToAllChannels(ECR_Block);Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Wall->RegisterComponent();WallActor->SetActorLocation(A->GetActorLocation()+FVector(100,0,65));
 Step(120,true);TestTrue(TEXT("Wall stops capsule penetration"),A->GetActorLocation().X<WallActor->GetActorLocation().X-30);
 TestTrue(TEXT("Blocked character animation follows actual velocity"),Anim->Gait<.05f);
 TArray<UObject*> Components;auto* Input=NewObject<UEnhancedInputComponent>(A);A->SetupPlayerInputComponent(Input);TestTrue(TEXT("Enhanced input bindings installed"),Input->GetActionEventBindings().Num()>=6);
 TestEqual(TEXT("Only gameplay GameMode spawns playable pawn"),GetDefault<AWardrobeExplorerGameMode>()->DefaultPawnClass.Get(),AWardrobeExplorer::StaticClass());
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeLocomotionPlantTest,"DreamBloom.Locomotion.PhysicsTranslationAndFootPlant",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeLocomotionPlantTest::RunTest(const FString&){
 auto* Class=LoadClass<AWardrobeExplorer>(nullptr,TEXT("/Game/BluePrint/ALS/Characters/BP_ALSCharacter.BP_ALSCharacter_C"));if(!TestNotNull(TEXT("Playable character"),Class))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeExplorer>(Class,FVector(0,0,87),FRotator::ZeroRotator);
 auto* PC=W->SpawnActor<AWardrobeExplorerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));PC->Possess(A);A->ApplySettings();PC->SetControlRotation(FRotator::ZeroRotator);
 auto* FloorActor=W->SpawnActor<AActor>();auto* Floor=NewObject<UBoxComponent>(FloorActor);FloorActor->SetRootComponent(Floor);Floor->SetBoxExtent(FVector(10000,10000,10));Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Floor->RegisterComponent();FloorActor->SetActorLocation(FVector(0,0,-10));
 auto Eval=[&](){A->GetMesh()->TickAnimation(1.f/60,false);A->GetMesh()->RefreshBoneTransforms();A->GetMesh()->HandleExistingParallelEvaluationTask(true,true);};Eval();
 FAnimNode_WardrobeFootIK* IK=nullptr;FAnimNode_KawaiiPhysics* K=nullptr;
 for(TFieldIterator<FStructProperty> P(A->GetMesh()->GetAnimInstance()->GetClass());P;++P){if(P->Struct==FAnimNode_WardrobeFootIK::StaticStruct())IK=P->ContainerPtrToValuePtr<FAnimNode_WardrobeFootIK>(A->GetMesh()->GetAnimInstance());if(P->Struct==FAnimNode_KawaiiPhysics::StaticStruct())K=P->ContainerPtrToValuePtr<FAnimNode_KawaiiPhysics>(A->GetMesh()->GetAnimInstance());}
 if(!TestNotNull(TEXT("Foot IK"),IK)||!TestNotNull(TEXT("Kawaii physics"),K)){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 TestTrue(TEXT("Locomotion enables foot locking"),IK->bLockPlantedFeet);
 TestTrue(TEXT("Local physics keeps rotation response"),K->SimulationSpace==EKawaiiPhysicsSimulationSpace::ComponentSpace&&K->PhysicsSettings.WorldDampingRotation<1.f);
 for(const auto& Bone:K->ModifyBones)TestEqual(TEXT("All simulated bones ignore world translation"),Bone.PhysicsSettings.WorldDampingLocation,1.f);
 auto PhysicsTrial=[&](bool Moving){
  A->SetActorLocation(FVector(0,0,87));A->GetCharacterMovement()->Velocity=FVector::ZeroVector;A->GetMesh()->InitAnim(true);
  for(TFieldIterator<FStructProperty> P(A->GetMesh()->GetAnimInstance()->GetClass());P;++P){if(P->Struct==FAnimNode_WardrobeFootIK::StaticStruct())IK=P->ContainerPtrToValuePtr<FAnimNode_WardrobeFootIK>(A->GetMesh()->GetAnimInstance());if(P->Struct==FAnimNode_KawaiiPhysics::StaticStruct())K=P->ContainerPtrToValuePtr<FAnimNode_KawaiiPhysics>(A->GetMesh()->GetAnimInstance());}
  // Compare the same animation phase, without ground adaptation or plant locking
  // moving the input pose. Only the component world translation changes.
  IK->bEnabled=false;
  for(int I=0;I<180;I++){if(Moving)A->AddActorWorldOffset(FVector(7,0,0));Eval();}
  TArray<FVector> Result;for(const auto& Bone:K->ModifyBones)Result.Add(Bone.Location);return Result;
 };
 const auto Stationary=PhysicsTrial(false),Translated=PhysicsTrial(true);float Worst=0;
 if(TestEqual(TEXT("Simulation cache count"),Translated.Num(),Stationary.Num()))for(int I=0;I<Stationary.Num();I++)Worst=FMath::Max(Worst,float(FVector::Distance(Stationary[I],Translated[I])));
 AddInfo(FString::Printf(TEXT("Physics after 1260 cm translation: max local difference %.4f cm"),Worst));TestTrue(TEXT("Translation does not drag secondary chains behind the character"),Worst<.5f);
 IK->bEnabled=true;
 auto SlipTrial=[&](bool Lock,bool Run){
  IK->bLockPlantedFeet=Lock;for(int I=0;I<2;I++){IK->bFootLocked[I]=IK->bLockReleasing[I]=false;IK->bLockCanAcquire[I]=true;}A->SetActorLocation(FVector(0,0,87));A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);if(Run)A->StartSprint();else A->StopSprint();
  auto Step=[&](){A->Move(FInputActionValue(FVector2D(0,1)));A->GetCharacterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);Eval();};for(int I=0;I<90;I++)Step();
  FVector Last[2],PreviousOffset[2];float Prev[2]={0,0};bool WasReleasing[2]={false,false};double Total=0;int Count=0,Releases=0;float WorstReleaseJump=0,WorstLiftFrame=0;
  for(int Frame=0;Frame<240;Frame++){Step();auto* Anim=A->GetMesh()->GetAnimInstance();for(int I=0;I<2;I++){
   const float Plant=Anim->GetCurveValue(I==0?IK->LeftPlantCurve:IK->RightPlantCurve);const FVector Pos=A->GetMesh()->GetSocketLocation(I==0?IK->LeftFoot.BoneName:IK->RightFoot.BoneName);
   if(Frame>0&&Prev[I]>.95f&&Plant>.95f){Total+=FVector::DistXY(Last[I],Pos)*60;Count++;}
   if(Frame>0&&Lock&&IK->bLockReleasing[I])WorstLiftFrame=FMath::Max(WorstLiftFrame,float(FVector::Distance(Last[I],Pos)));
   if(Frame>0&&Lock&&IK->bLockReleasing[I]&&!WasReleasing[I]){const FVector Diff=IK->SmoothedOffset[I]-PreviousOffset[I];WorstReleaseJump=FMath::Max(WorstReleaseJump,float(Diff.Size2D()));Releases++;}
   Last[I]=Pos;Prev[I]=Plant;PreviousOffset[I]=IK->SmoothedOffset[I];WasReleasing[I]=IK->bLockReleasing[I];
  }}
  if(Lock){AddInfo(FString::Printf(TEXT("%s actual lift-off: %d releases, correction jump %.4f cm, max foot displacement per 60 FPS frame %.4f cm"),Run?TEXT("Run"):TEXT("Walk"),Releases,WorstReleaseJump,WorstLiftFrame));TestTrue(TEXT("Real animation releases each planted foot"),Releases>5);TestTrue(TEXT("Actual lift-off retains the last applied XY correction"),WorstReleaseJump<.01f);TestTrue(TEXT("Actual lift-off has no large single-frame foot displacement"),WorstLiftFrame<12.f);}
  TestTrue(TEXT("Authored plant curves produce contact samples"),Count>20);return Count?float(Total/Count):BIG_NUMBER;
 };
 for(bool Run:{false,true}){const float Without=SlipTrial(false,Run),With=SlipTrial(true,Run);AddInfo(FString::Printf(TEXT("%s planted world speed: without lock %.3f with lock %.3f cm/s"),Run?TEXT("Run"):TEXT("Walk"),Without,With));TestTrue(TEXT("Foot locking reduces horizontal slip"),With<Without*.5f);}
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeFootLiftTest,"DreamBloom.Locomotion.FootLockLiftOffContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeFootLiftTest::RunTest(const FString&){
 for(int FPS:{30,60,120}){
  FAnimNode_WardrobeFootIK IK;IK.DeltaTime=1.f/FPS;IK.MeshWorld=FTransform::Identity;
  IK.UpdateFootLock(0,FVector::ZeroVector,1,true);
  IK.MeshWorld.SetTranslation(FVector(15,0,0));const FVector Locked=IK.UpdateFootLock(0,FVector(15,0,0),1,true);IK.SmoothedOffset[0]=Locked;
  const FVector First=IK.UpdateFootLock(0,FVector(15,0,0),0,true);
  TestTrue(TEXT("Sudden curve drop preserves last correction on the release frame"),First.Equals(Locked,.001f));
  FVector Previous=First;float Worst=0;
  for(int Frame=0;Frame<FPS;Frame++){
   const FVector Offset=IK.UpdateFootLock(0,FVector(15,0,0),0,true);Worst=FMath::Max(Worst,float(FVector::Distance(Offset,Previous)));
   TestTrue(TEXT("Release correction decreases without overshoot"),Offset.Size()<=Previous.Size()+.001f);Previous=Offset;IK.SmoothedOffset[0]=Offset;
  }
  TestTrue(TEXT("Release reaches the original animation"),Previous.IsNearlyZero(.001f));
  AddInfo(FString::Printf(TEXT("Lift-off %d FPS: release frame jump %.4f cm, max subsequent correction step %.4f cm"),FPS,FVector::Distance(First,Locked),Worst));
  TestTrue(TEXT("Release is spread across multiple frames"),Worst<8.f);
  IK.UpdateFootLock(0,FVector(15,0,0),1,true);TestTrue(TEXT("The next contact can lock again"),IK.bFootLocked[0]);
  IK.SmoothedOffset[0]=FVector(-8,0,0);IK.MeshWorld.SetRotation(FQuat(FVector::UpVector,FMath::DegreesToRadians(45.f)));
  TestTrue(TEXT("Turn safety release retains current correction"),IK.UpdateFootLock(0,FVector(15,0,0),1,true).Equals(FVector(-8,0,0),.001f));
  for(int Frame=0;Frame<FPS;Frame++)IK.UpdateFootLock(0,FVector(15,0,0),1,true);
  TestFalse(TEXT("Safety release cannot recapture in the same contact"),IK.bFootLocked[0]);
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCameraZoomTest,"DreamBloom.Locomotion.CameraWheelZoom",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCameraZoomTest::RunTest(const FString&){
 auto* Class=LoadClass<AWardrobeExplorer>(nullptr,TEXT("/Game/BluePrint/ALS/Characters/BP_ALSCharacter.BP_ALSCharacter_C"));if(!TestNotNull(TEXT("Playable character"),Class))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeExplorer>(Class);auto* PC=W->SpawnActor<AWardrobeExplorerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));PC->Possess(A);A->ApplySettings();
 if(!TestNotNull(TEXT("Camera data"),A->Settings.Get())){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 TestNotNull(TEXT("Zoom action configured"),A->Settings->ZoomAction.Get());bool Mapped=false;
 for(const auto& Mapping:A->Settings->Mapping->GetMappings())if(Mapping.Action==A->Settings->ZoomAction&&Mapping.Key==EKeys::MouseWheelAxis)Mapped=true;
 TestTrue(TEXT("Wheel axis maps to zoom"),Mapped);auto* Input=NewObject<UEnhancedInputComponent>(A);A->SetupPlayerInputComponent(Input);TestTrue(TEXT("Zoom event binding installed"),Input->GetActionEventBindings().Num()>=7);
 const float Start=A->CameraBoom->TargetArmLength;A->Zoom(FInputActionValue(1.f));
 TestTrue(TEXT("Wheel up selects a nearer target"),A->CameraZoomOffset<0);TestEqual(TEXT("Wheel does not snap the camera"),A->CameraBoom->TargetArmLength,Start);
 A->Tick(1.f/60);TestTrue(TEXT("Camera interpolates toward the nearer target"),A->CameraBoom->TargetArmLength<Start&&A->CameraBoom->TargetArmLength>Start+A->CameraZoomOffset);
 for(int I=0;I<60;I++)A->Tick(1.f/60);TestTrue(TEXT("Zoom is retained by repeated settings updates"),FMath::Abs(A->CameraBoom->TargetArmLength-(Start-A->Settings->CameraZoomStep))<.1f);
 A->Zoom(FInputActionValue(-2.f));for(int I=0;I<60;I++)A->Tick(1.f/60);TestTrue(TEXT("Wheel down moves the camera farther away"),A->CameraBoom->TargetArmLength>Start);
 A->Zoom(FInputActionValue(100.f));A->ApplySettings();TestEqual(TEXT("Near target clamps"),A->Settings->CameraDistance+A->CameraZoomOffset,A->Settings->CameraMinDistance);
 A->Zoom(FInputActionValue(-100.f));TestEqual(TEXT("Far target clamps"),A->Settings->CameraDistance+A->CameraZoomOffset,A->Settings->CameraMaxDistance);
 const float Offset=A->CameraZoomOffset;PC->SetIgnoreLookInput(true);A->Zoom(FInputActionValue(1.f));TestEqual(TEXT("Blocked/loading input cannot zoom"),A->CameraZoomOffset,Offset);PC->ResetIgnoreLookInput();PC->bPauseMenu=true;A->Zoom(FInputActionValue(1.f));TestEqual(TEXT("Pause menu cannot zoom"),A->CameraZoomOffset,Offset);
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeExpandedMovementTest,"DreamBloom.Locomotion.StartStopJumpIdle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeExpandedMovementTest::RunTest(const FString&){
 auto* Class=LoadClass<AWardrobeExplorer>(nullptr,TEXT("/Game/BluePrint/ALS/Characters/BP_ALSCharacter.BP_ALSCharacter_C"));if(!TestNotNull(TEXT("Playable blueprint"),Class))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeExplorer>(Class,FVector(0,0,85),FRotator::ZeroRotator);auto* PC=W->SpawnActor<AWardrobeExplorerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));PC->Possess(A);PC->SetControlRotation(FRotator::ZeroRotator);PC->ResetIgnoreMoveInput();PC->ResetIgnoreLookInput();
 A->Settings=DuplicateObject<UWardrobeLocomotionData>(A->Settings,A);A->ApplySettings();auto* D=A->Settings.Get();auto* Anim=Cast<UWardrobeLocomotionAnim>(A->GetMesh()->GetAnimInstance());
 if(!TestNotNull(TEXT("Animation instance"),Anim)){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 TestNotNull(TEXT("Walking stop"),D->WalkStopLeft.Get());TestNotNull(TEXT("Running stop"),D->RunStopRight.Get());TestNotNull(TEXT("Air loop"),D->AirLoop.Get());TestNotNull(TEXT("Landing"),D->Landing.Get());TestNotNull(TEXT("Jump input"),D->JumpAction.Get());
 bool Space=false;for(const auto& M:D->Mapping->GetMappings())if(M.Action==D->JumpAction&&M.Key==EKeys::SpaceBar)Space=true;TestTrue(TEXT("Space mapping"),Space);
 auto* FloorActor=W->SpawnActor<AActor>();auto* Floor=NewObject<UBoxComponent>(FloorActor);FloorActor->SetRootComponent(Floor);Floor->SetBoxExtent(FVector(10000,10000,10));Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Floor->RegisterComponent();FloorActor->SetActorLocation(FVector(0,0,-10));
 auto Eval=[&](){A->GetMesh()->TickAnimation(1.f/60,false);A->GetMesh()->RefreshBoneTransforms();A->GetMesh()->HandleExistingParallelEvaluationTask(true,true);};
 auto Step=[&](int Count,bool Input){for(int I=0;I<Count;I++){if(Input)A->Move(FInputActionValue(FVector2D(0,1)));A->Tick(1.f/60);A->GetCharacterMovement()->TickComponent(1.f/60,LEVELTICK_All,nullptr);Eval();}};
 A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);Eval();Step(1,true);TestEqual(TEXT("Idle enters walking start"),Anim->MovementAction,EWardrobeMovementAction::WalkStart);
 Step(65,true);TestTrue(TEXT("Start returns to moving gait"),Anim->MovementAction==EWardrobeMovementAction::None);
 Step(1,false);TestEqual(TEXT("Movement release selects walking stop"),Anim->MovementAction,EWardrobeMovementAction::WalkStop);TestTrue(TEXT("Stop montage is playing"),Anim->MovementMontage&&Anim->Montage_IsPlaying(Anim->MovementMontage));
 Step(3,true);TestFalse(TEXT("Renewed input interrupts stopping"),Anim->MovementAction==EWardrobeMovementAction::WalkStop);Step(70,true);A->StartSprint();Step(90,true);Step(1,false);TestEqual(TEXT("Running release selects running stop"),Anim->MovementAction,EWardrobeMovementAction::RunStop);
 const float StopSpeed=A->GetVelocity().Size2D();Step(6,false);TestTrue(TEXT("Authored stop retains a controlled braking step"),A->GetVelocity().Size2D()>1&&A->GetVelocity().Size2D()<=StopSpeed);
 float Prior=A->GetVelocity().Size2D(),WorstRotation=0;FQuat PriorLeg=A->GetMesh()->GetSocketQuaternion(TEXT("足D_L"));
 for(int I=0;I<210;I++){Step(1,false);const float Current=A->GetVelocity().Size2D();TestTrue(TEXT("Stopping cannot reaccelerate capsule"),Current<=Prior+.1f);Prior=Current;const auto Leg=A->GetMesh()->GetSocketQuaternion(TEXT("足D_L"));WorstRotation=FMath::Max(WorstRotation,float(Leg.AngularDistance(PriorLeg)));PriorLeg=Leg;}
 TestTrue(TEXT("Authored stop settles to zero travel"),Prior<.1f);TestTrue(TEXT("Evaluated stop has no abrupt leg rotation"),WorstRotation<1.f);
 AddInfo(FString::Printf(TEXT("BaseAnim3 stop max leg delta %.3f rad/frame; final speed %.3f"),WorstRotation,Prior));
 TestTrue(TEXT("Stationary animation is no longer frozen"),Anim->LocomotionPlayRate>.1f);
 A->StopSprint();const float BeforeJumpZ=A->GetActorLocation().Z;A->StartJump();Step(2,false);TestTrue(TEXT("Space causes physical lift"),A->GetActorLocation().Z>86&&A->GetCharacterMovement()->IsFalling());TestEqual(TEXT("Lift selects takeoff"),Anim->MovementAction,EWardrobeMovementAction::Takeoff);A->EndJump();
 FAnimNode_WardrobeFootIK* IK=nullptr;for(TFieldIterator<FStructProperty> P(Anim->GetClass());P;++P)if(P->Struct==FAnimNode_WardrobeFootIK::StaticStruct())IK=P->ContainerPtrToValuePtr<FAnimNode_WardrobeFootIK>(Anim);
 TestTrue(TEXT("Air disables foot ground adaptation"),IK&&IK->bRuntimeInAir&&!IK->Ground[0].bHit&&!IK->Ground[1].bHit);
 bool Air=false,Land=false;float MaxHeight=A->GetActorLocation().Z,SlotWeight=0,MaxLocalPelvis=0;FQuat AirLeg=FQuat::Identity;
 for(int I=0;I<100;I++){Step(1,false);Air|=Anim->MovementAction==EWardrobeMovementAction::Air;Land|=Anim->MovementAction==EWardrobeMovementAction::Land;MaxHeight=FMath::Max(MaxHeight,float(A->GetActorLocation().Z));const float LocalPelvis=A->GetMesh()->GetComponentTransform().InverseTransformPosition(A->GetMesh()->GetSocketLocation(TEXT("腰"))).Z;MaxLocalPelvis=FMath::Max(MaxLocalPelvis,LocalPelvis);if(Anim->MovementAction==EWardrobeMovementAction::Air){SlotWeight=FMath::Max(SlotWeight,Anim->GetSlotMontageGlobalWeight(TEXT("DefaultSlot")));AirLeg=A->GetMesh()->GetSocketQuaternion(TEXT("足D_L"));}}
 TestTrue(TEXT("Jump root height follows capsule movement only"),MaxLocalPelvis<115.f);
 const float ExpectedHeight=FMath::Square(D->JumpVelocity)/(2.f*FMath::Abs(A->GetCharacterMovement()->GetGravityZ()));
 TestTrue(TEXT("Capsule apex agrees with configured ballistic height"),FMath::Abs(MaxHeight-BeforeJumpZ-ExpectedHeight)<6.f);
 AddInfo(FString::Printf(TEXT("Jump capsule rise %.2f cm; expected %.2f; maximum mesh-local pelvis height %.2f cm"),MaxHeight-BeforeJumpZ,ExpectedHeight,MaxLocalPelvis));
 TestTrue(TEXT("Jump enters persistent airborne loop"),Air);TestTrue(TEXT("Touchdown plays landing"),Land);TestTrue(TEXT("Jump has actual height"),MaxHeight>130);TestTrue(TEXT("Landed foot IK fades back"),Anim->GroundIKAlpha>.99f&&IK&&!IK->bRuntimeInAir);
 TestTrue(TEXT("Air animation contributes to the evaluated slot"),SlotWeight>.95f);TestTrue(TEXT("Air animation changes the actual leg pose"),AirLeg.AngularDistance(A->GetMesh()->GetSocketQuaternion(TEXT("足D_L")))>.05f);AddInfo(FString::Printf(TEXT("Air slot weight %.3f; final leg difference %.3f radians"),SlotWeight,AirLeg.AngularDistance(A->GetMesh()->GetSocketQuaternion(TEXT("足D_L")))));
 Step(120,false);const float IdleYaw=A->GetActorRotation().Yaw;
 for(float CameraYaw:{95.f,-140.f,175.f}){PC->SetControlRotation(FRotator(0,CameraYaw,0));Step(120,false);TestTrue(TEXT("Orbiting the idle camera keeps character heading fixed"),FMath::Abs(FMath::FindDeltaAngleDegrees(IdleYaw,A->GetActorRotation().Yaw))<.01f);TestEqual(TEXT("Camera orbit does not start a movement montage"),Anim->MovementAction,EWardrobeMovementAction::None);}
 PC->SetControlRotation(FRotator(0,A->GetActorRotation().Yaw,0));
 A->Settings->IdleMinInterval=A->Settings->IdleMaxInterval=.1f;Anim->NextIdleTime=.1f;Step(15,false);TestEqual(TEXT("Idle interval triggers configured variation"),Anim->MovementAction,EWardrobeMovementAction::IdleVariation);Step(2,true);TestFalse(TEXT("Movement interrupts idle variation"),Anim->MovementAction==EWardrobeMovementAction::IdleVariation);
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobePauseKeyTest,"DreamBloom.Locomotion.EditorSafePauseKey",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobePauseKeyTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();
 W->SetGameMode(FURL(nullptr,TEXT("/Engine/Maps/Entry?game=/Script/DreamBloom.WardrobeExplorerGameMode"),TRAVEL_Absolute));
 auto* PC=W->SpawnActor<AWardrobeExplorerController>();PC->SetPlayer(NewObject<ULocalPlayer>(GEngine));PC->SetupInputComponent();
 auto* MenuData=NewObject<UWardrobeMenuData>();MenuData->AudioData=NewObject<UWardrobeAudioData>();
 MenuData->AudioData->Pause.Sound=MenuData->AudioData->Resume.Sound=LoadObject<USoundBase>(nullptr,TEXT("/Game/WardrobePinch21/Audio/Grab.Grab"));
 GI->GetSubsystem<UWardrobeSession>()->ExplorationMenuData=MenuData;
 auto* Audio=GI->GetSubsystem<UWardrobeAudioSubsystem>();const int32 BeforePause=Audio->PlayCount;
 auto* Binding=PC->InputComponent->KeyBindings.FindByPredicate([](const FInputKeyBinding& B){return B.Chord.Key==EKeys::P&&B.KeyEvent==IE_Pressed;});
 TestNotNull(TEXT("P works without editing IA_Pause or a DA"),Binding);
 if(Binding){
  TestTrue(TEXT("Controller consumes key to avoid duplicate pawn bindings"),Binding->bConsumeInput);
  Binding->KeyDelegate.Execute(EKeys::P);
  TestEqual(TEXT("Opening pause uses session WardrobeAudio once"),Audio->PlayCount,BeforePause+1);
  TestTrue(TEXT("P opens pause and guards movement/look"),PC->bPauseMenu&&PC->IsMoveInputIgnored()&&PC->IsLookInputIgnored());
  if(FSlateApplication::IsInitialized()){
   const auto Focus=PC->PauseWidget;TestTrue(TEXT("Pause menu can receive keyboard focus in UI-only mode"),Focus.IsValid()&&Focus->SupportsKeyboardFocus());
   if(Focus.IsValid())Focus->OnKeyDown(FGeometry(),FKeyEvent(EKeys::P,FModifierKeysState(),0,false,0,0));
   TestFalse(TEXT("P closes focused pause menu"),PC->bPauseMenu);
   TestEqual(TEXT("Resume uses session WardrobeAudio once"),Audio->PlayCount,BeforePause+2);
   TestTrue(TEXT("Resume restores move/look input"),!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored());
  }
 }
 if(PC->bPauseMenu)PC->TogglePauseMenu();GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
