#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeStations.h"
#include "WardrobeShowcase.h"
#include "WardrobeKawaiiForce.h"
#include "WardrobePhysicsTools.h"
#include "Animation/AnimBlueprint.h"
#include "KawaiiPhysicsLibrary.h"
#include "Camera/CameraComponent.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstanceProxy.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneMaterialParameterCollectionTrack.h"
#include "Sections/MovieSceneParameterSection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeDofTest,"DreamBloom.Feedback.CameraComponentDOF",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeDofTest::RunTest(const FString&){
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* PC=W->SpawnActor<APlayerController>();auto* Camera=W->SpawnActor<ACineCameraActor>();auto* Person=W->SpawnActor<AWardrobeShowcase>();
 Camera->Tags.Add(TEXT("DofCamera"));Person->Tags.Add(TEXT("Person"));
 Person->Character->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube")));
 auto* C=Camera->GetCineCameraComponent();C->FocusSettings.FocusMethod=ECameraFocusMethod::Manual;C->FocusSettings.bSmoothFocusChanges=false;C->FocusSettings.ManualFocusDistance=350;C->CurrentAperture=1.8f;C->PostProcessBlendWeight=1;
 auto* Data=NewObject<UWardrobeCharacterData>();Data->Characters.SetNum(1);Data->Characters[0].CameraTag=TEXT("DofCamera");Data->Characters[0].CharacterTag=TEXT("Person");
 auto* Controller=NewObject<UWardrobeStationController>();TestTrue(TEXT("Initialize CineCamera by tag"),Controller->Initialize(PC,Data));Controller->SetPreviewEnabled(true);
 FMinimalViewInfo View;Controller->GetPreviewCamera()->GetCameraComponent()->GetCameraView(.016f,View);
 TestTrue(TEXT("Engine camera component path carries DOF override"),View.PostProcessSettings.bOverride_DepthOfFieldFocalDistance);
 TestEqual(TEXT("Manual focus copied"),View.PostProcessSettings.DepthOfFieldFocalDistance,350.f);
 TestEqual(TEXT("Aperture copied"),View.PostProcessSettings.DepthOfFieldFstop,1.8f);
 C->FocusSettings.ManualFocusDistance=700;C->CurrentAperture=4;Controller->Tick(.016f);
 Controller->GetPreviewCamera()->GetCameraComponent()->GetCameraView(.016f,View);
 TestEqual(TEXT("Focus updates at runtime"),View.PostProcessSettings.DepthOfFieldFocalDistance,700.f);
 TestEqual(TEXT("Aperture updates at runtime"),View.PostProcessSettings.DepthOfFieldFstop,4.f);
 Controller->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeForceTest,"DreamBloom.Feedback.LocalPhysicsForce",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeForceTest::RunTest(const FString&){
 FAnimInstanceProxy Proxy;FComponentSpacePoseContext Context(&Proxy);
 FAnimNode_KawaiiPhysics Node;Node.SimulationSpace=EKawaiiPhysicsSimulationSpace::WorldSpace;Node.DeltaTime=.016f;
 FWardrobeKawaiiClickForce Force;Force.RadialBlend=0;Force.Radius=30;Force.Duration=.1f;
 FKawaiiPhysicsModifyBone Near,Far;Near.ParentIndex=0;Far.ParentIndex=0;Near.Location=FVector(0,1,0);Far.Location=FVector(100,0,0);
 Force.PreApply(Node,Context);Force.Apply(Near,Node,Context,FTransform::Identity);Force.Apply(Far,Node,Context,FTransform::Identity);
 TestTrue(TEXT("Bone near click is pushed"),Near.Location.X>0);
 TestEqual(TEXT("Bone outside radius untouched"),Far.Location.X,100.);
 for(int32 I=0;I<10;I++)Force.PreApply(Node,Context);
 const FVector Before=Near.Location;Force.Apply(Near,Node,Context,FTransform::Identity);
 TestTrue(TEXT("Pulse expires"),Near.Location.Equals(Before));
 TestEqual(TEXT("Boundary has zero force"),FWardrobeKawaiiClickForce::SpatialWeight(30,30,2),0.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeMpcTest,"DreamBloom.Feedback.MPCAndSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeMpcTest::RunTest(const FString&){
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Settings=NewObject<UWardrobeClickEffectData>();auto* Collection=NewObject<UMaterialParameterCollection>();Settings->Collection=Collection;
 for(FName Name:{Settings->WorldPositionParameter,Settings->ScreenUVParameter}){FCollectionVectorParameter P;P.ParameterName=Name;Collection->VectorParameters.Add(P);}
 for(FName Name:{Settings->TimeParameter,FName(TEXT("ClickIntensity"))}){FCollectionScalarParameter P;P.ParameterName=Name;Collection->ScalarParameters.Add(P);}
 Collection->PostEditChange();
 TestTrue(TEXT("Write click parameters"),Settings->WriteParameters(W,FVector(20,30,40),FVector2D(.25,.75),true));
 auto* Instance=W->GetParameterCollectionInstance(Collection);FLinearColor Value;
 Instance->GetVectorParameterValue(Settings->WorldPositionParameter,Value);TestTrue(TEXT("World XYZ preserved"),Value.Equals(FLinearColor(20,30,40,1)));
 Instance->GetVectorParameterValue(Settings->ScreenUVParameter,Value);TestTrue(TEXT("Screen UV and hit flag"),Value.Equals(FLinearColor(.25,.75,1,1)));
 auto* Sequence=NewObject<ULevelSequence>();Sequence->Initialize();auto* Movie=Sequence->GetMovieScene();Movie->SetPlaybackRange(0,24000);
 auto* Track=Movie->AddTrack<UMovieSceneMaterialParameterCollectionTrack>();Track->MPC=Collection;
 auto* Section=CastChecked<UMovieSceneParameterSection>(Track->CreateNewSection());Section->SetRange(TRange<FFrameNumber>(0,24000));Section->AddScalarParameterKey(TEXT("ClickIntensity"),0,1);Track->AddSection(*Section);
 FMovieSceneSequencePlaybackSettings Playback;Playback.bDisableCameraCuts=true;Playback.FinishCompletionStateOverride=EMovieSceneCompletionModeOverride::ForceRestoreState;
 ALevelSequenceActor* Actor=nullptr;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Sequence,Playback,Actor);
 TestNotNull(TEXT("Effect player"),Player);
 if(Player){Player->Play();Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(1),EUpdatePositionMethod::Jump));float Intensity=0;Instance->GetScalarParameterValue(TEXT("ClickIntensity"),Intensity);TestEqual(TEXT("Sequence animates independent MPC property"),Intensity,1.f);Player->Stop();Player->RestoreState();Actor->Destroy();}
 Settings->WorldPositionParameter=TEXT("Missing");TestFalse(TEXT("Missing parameter rejected"),Settings->WriteParameters(W,FVector::ZeroVector,FVector2D::ZeroVector,false));
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeDragTest,"DreamBloom.Feedback.PhysicsPickingAndDrag",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeDragTest::RunTest(const FString&){
 float Distance=0;
 TestTrue(TEXT("Ray hits existing body radius"),FWardrobeKawaiiClickForce::RaySphere(FVector(-10,0,0),FVector::ForwardVector,FVector::ZeroVector,2,Distance));
 TestEqual(TEXT("Front surface distance"),Distance,8.f);
 TestFalse(TEXT("Miss does not capture pointer"),FWardrobeKawaiiClickForce::RaySphere(FVector(-10,5,0),FVector::ForwardVector,FVector::ZeroVector,2,Distance));
 TestFalse(TEXT("Behind camera rejected"),FWardrobeKawaiiClickForce::RaySphere(FVector(10,0,0),FVector::ForwardVector,FVector::ZeroVector,2,Distance));
 FAnimNode_KawaiiPhysics Node;Node.SimulationSpace=EKawaiiPhysicsSimulationSpace::WorldSpace;Node.DeltaTime=.016f;
 TestTrue(TEXT("World simulation unchanged"),FWardrobeKawaiiClickForce::BoneWorldPosition(Node,FTransform(FVector(100,0,0)),FVector(2,0,0)).Equals(FVector(2,0,0)));
 Node.SimulationSpace=EKawaiiPhysicsSimulationSpace::ComponentSpace;
 TestTrue(TEXT("Component simulation transformed"),FWardrobeKawaiiClickForce::BoneWorldPosition(Node,FTransform(FVector(100,0,0)),FVector(2,0,0)).Equals(FVector(102,0,0)));
 Node.SimulationSpace=EKawaiiPhysicsSimulationSpace::WorldSpace;Node.ModifyBones.SetNum(3);Node.ModifyBones[0].ParentIndex=-1;Node.ModifyBones[1].ParentIndex=0;Node.ModifyBones[2].ParentIndex=0;
 FWardrobeKawaiiClickForce Force;Force.bDragging=true;Force.DragOrigins.Init(FVector::ZeroVector,3);Force.DragWeights={1,1,0};Force.DragDelta=FVector(100,0,0);Force.MaxDragSpeed=20;
 FAnimInstanceProxy Proxy;FComponentSpacePoseContext Context(&Proxy);
 for(auto& Bone:Node.ModifyBones)Force.Apply(Bone,Node,Context,FTransform::Identity);
 TestTrue(TEXT("Root fixed"),Node.ModifyBones[0].Location.IsNearlyZero());
 TestTrue(TEXT("Selected bone follows target with speed limit"),Node.ModifyBones[1].Location.X>0&&Node.ModifyBones[1].Location.X<=.321);
 TestTrue(TEXT("Outside brush unaffected"),Node.ModifyBones[2].Location.IsNearlyZero());
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeTouchGraphTest,"DreamBloom.Feedback.GeneratedAnimBPInteraction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeTouchGraphTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
 if(!TestNotNull(TEXT("Fixture mesh"),Mesh))return false;
 FString ExistingBP;FParse::Value(FCommandLine::Get(),TEXT("WardrobeTestBP="),ExistingBP);
 const FString Package=TEXT("/Game/WardrobeTouchTests/AB_Touch_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 auto* BP=ExistingBP.IsEmpty()?UWardrobePhysicsTools::CreateTouchTestBlueprint(Mesh,nullptr,{Mesh->GetRefSkeleton().GetBoneName(0)},Package):LoadObject<UAnimBlueprint>(nullptr,*ExistingBP);
 if(!ExistingBP.IsEmpty())Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Model/Daniya/Red/DaniyaToUe.DaniyaToUe"));
 if(!TestNotNull(TEXT("Generate and compile AnimBP with Kawaii node"),BP))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Person=W->SpawnActor<AWardrobeShowcase>();Person->Character->SetSkeletalMesh(Mesh);Person->Character->SetAnimInstanceClass(BP->GeneratedClass);
 auto* Data=NewObject<UWardrobeCharacterData>();Data->Characters.SetNum(1);Data->Characters[0].PhysicsTouch.bEnabled=true;
 Person->InteractionData=Data;Person->InteractionProfileIndex=0;Person->SetInteractionEnabled(true);
 Person->Character->TickAnimation(.016f,false);Person->Character->RefreshBoneTransforms();Person->Character->HandleExistingParallelEvaluationTask(true,true);
 TArray<FKawaiiPhysicsReference> Nodes;FGameplayTagContainer Filter;
 UKawaiiPhysicsLibrary::CollectKawaiiPhysicsNodes(Nodes,Person->Character,Filter,false);
 TestTrue(TEXT("Collect node without tag"),Nodes.Num()>0);
 if(Nodes.Num()){
  auto& Node=Nodes[0].GetAnimNode<FAnimNode_KawaiiPhysics>();
  TestTrue(TEXT("Simulation bodies initialized"),Node.ModifyBones.Num()>1);
  if(Node.ModifyBones.Num()>1){
   const FVector Center=FWardrobeKawaiiClickForce::BoneWorldPosition(Node,Person->Character->GetComponentTransform(),Node.ModifyBones[1].Location);
   const FVector Origin=Center-FVector(100,0,0);FHitResult Hit;
   TestTrue(TEXT("Hit simulated bone without PhysicsAsset"),Person->TraceTouch(Origin,FVector::ForwardVector,Hit));
   TestTrue(TEXT("Begin drag installs force"),Person->BeginPhysicsDrag(Origin,FVector::ForwardVector));
   Person->UpdatePhysicsDrag(Origin+FVector(0,200,0),FVector::ForwardVector);
   bool Bounded=false;for(auto& Value:Node.ExternalForces)if(auto* Force=Value.GetPtr<FWardrobeKawaiiClickForce>())Bounded=Force->DragDelta.Size()<=40.01&&Force->DragDelta.Size()>0;
   TestTrue(TEXT("World target clamps maximum drag distance"),Bounded);
   Person->SetInteractionEnabled(false);TestFalse(TEXT("Station disable releases drag"),Person->IsPhysicsDragging());
   TestEqual(TEXT("Owned forces removed on release"),Node.ExternalForces.Num(),0);
  }
 }
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
