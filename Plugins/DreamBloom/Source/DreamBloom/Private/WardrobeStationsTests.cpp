#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeStations.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeMenu.h"
#include "WardrobeAudio.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/PlayerController.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStationTest,"DreamBloom.Stations.TransitionAndInteraction",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStationTest::RunTest(const FString& Parameters){
 UWorld* World=UWorld::CreateWorld(EWorldType::Game,false);
 if(!TestNotNull(TEXT("Test world"),World))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 auto* PC=World->SpawnActor<APlayerController>();
 auto* Director=NewObject<UWardrobeStationController>();
 auto* A=World->SpawnActor<AWardrobeShowcase>();auto* B=World->SpawnActor<AWardrobeShowcase>();
 auto* CamA=World->SpawnActor<ACameraActor>();auto* CamB=World->SpawnActor<ACameraActor>();
 CamA->SetActorLocationAndRotation(FVector(10,20,30),FRotator(0,170,0));
 CamB->SetActorLocationAndRotation(FVector(410,220,130),FRotator(20,-170,0));
 CamA->GetCameraComponent()->SetFieldOfView(35);CamB->GetCameraComponent()->SetFieldOfView(55);
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));
 if(!TestNotNull(TEXT("Engine test mesh"),Mesh)){GEngine->DestroyWorldContext(World);World->DestroyWorld(false);return false;}
 A->Character->SetSkeletalMesh(Mesh);B->Character->SetSkeletalMesh(Mesh);
 auto* Profiles=NewObject<UWardrobeCharacterData>();Profiles->Characters.SetNum(2);
 CamA->Tags.Add(TEXT("CamA"));CamB->Tags.Add(TEXT("CamB"));A->Tags.Add(TEXT("PersonA"));B->Tags.Add(TEXT("PersonB"));
 Profiles->Characters.SetNum(3);
 Profiles->Characters[0].CameraTag=TEXT("CamA");Profiles->Characters[0].CharacterTag=TEXT("PersonA");
 Profiles->Characters[1].CameraTag=TEXT("CamB");Profiles->Characters[1].CharacterTag=TEXT("PersonB");
 Profiles->Characters[1].Duration=2;Profiles->Characters[1].bAllowCharacterRotation=false;
 PC->SetViewTarget(CamA);
 TestTrue(TEXT("Initialize first pair"),Director->Initialize(PC,Profiles,0));Director->SetPreviewEnabled(true);
 TestTrue(TEXT("Only A enabled"),A->IsInteractionEnabled()&&!B->IsInteractionEnabled());
 TestTrue(TEXT("A profile retained"),A->GetInteractionProfile()==&Profiles->Characters[0]);
 TestNull(TEXT("Inactive profile not assigned prematurely"),B->GetInteractionProfile());
 const FTransform AuthoredA=CamA->GetActorTransform(),AuthoredB=CamB->GetActorTransform();
 Director->GetPreviewCamera()->AddActorWorldOffset(FVector(0,25,0));
 Director->ResetPreview();TestTrue(TEXT("Reset restores authored framing"),Director->GetPreviewCamera()->GetActorTransform().Equals(AuthoredA));
 AddExpectedMessage(TEXT("WARDROBE_TAG_BINDING"),EAutomationExpectedMessageFlags::Contains,5);
 TestFalse(TEXT("Empty tags rejected"),Director->SelectStation(2));
 auto* Duplicate=World->SpawnActor<ACameraActor>();Duplicate->Tags.Add(TEXT("CamB"));
 TestFalse(TEXT("Duplicate camera tag rejected"),Director->SelectStation(1));Duplicate->Tags.Empty();
 Duplicate->Tags.Add(TEXT("PersonB"));TestFalse(TEXT("Duplicate character tag rejected"),Director->SelectStation(1));Duplicate->Tags.Empty();
 Profiles->Characters[1].CameraTag=TEXT("MissingCamera");TestFalse(TEXT("Missing camera rejected"),Director->SelectStation(1));
 Profiles->Characters[1].CameraTag=TEXT("PersonB");TestFalse(TEXT("Wrong actor type rejected"),Director->SelectStation(1));Profiles->Characters[1].CameraTag=TEXT("CamB");
 TestTrue(TEXT("Invalid destination keeps current interaction"),A->IsInteractionEnabled());
 TestTrue(TEXT("Start two-second travel"),Director->SelectStation(1));
 TestFalse(TEXT("Repeated input during travel ignored"),Director->SelectStation(0));
 TestTrue(TEXT("Both interactions locked in flight"),!A->IsInteractionEnabled()&&!B->IsInteractionEnabled()&&!Director->GetActiveCharacter());
 Director->Tick(1);
 const FVector Mid=Director->GetPreviewCamera()->GetActorLocation();
 TestTrue(TEXT("Spatial curve deviates from straight chord"),!Mid.Equals((AuthoredA.GetLocation()+AuthoredB.GetLocation())*.5,1));
 TestTrue(TEXT("Still moving halfway"),Director->IsTransitioning());
 TestTrue(TEXT("Rotation uses short path across 180"),FMath::Abs(Director->GetPreviewCamera()->GetActorRotation().Yaw)>160);
 TestEqual(TEXT("FOV interpolated"),Director->GetPreviewCamera()->GetCameraComponent()->FieldOfView,45.f);
 Director->Tick(1);
 TestTrue(TEXT("Arrived at B"),Director->GetActiveCharacter()==B&&!Director->IsTransitioning());
 TestTrue(TEXT("B receives DA profile automatically"),B->GetInteractionProfile()==&Profiles->Characters[1]);
 TestTrue(TEXT("Only B enabled after arrival"),!A->IsInteractionEnabled()&&B->IsInteractionEnabled());
 TestFalse(TEXT("Seated character cannot rotate"),Director->CanRotateCharacter());
 TestTrue(TEXT("Exact endpoint transform"),Director->GetPreviewCamera()->GetActorTransform().Equals(AuthoredB,.001));
 TestTrue(TEXT("Authored cameras remain unchanged"),CamA->GetActorTransform().Equals(AuthoredA)&&CamB->GetActorTransform().Equals(AuthoredB));
 // Exercise the retained Sequence interaction against the currently enabled actor.
 auto* Seq=NewObject<ULevelSequence>();Seq->Initialize();Seq->GetMovieScene()->SetPlaybackRange(0,24000);
 const FGuid Binding=Seq->GetMovieScene()->AddPossessable(TEXT("Person"),AWardrobeShowcase::StaticClass());
 Seq->GetMovieScene()->TagBinding(TEXT("Person"),UE::MovieScene::FFixedObjectBindingID(Binding,MovieSceneSequenceID::Root));
 FWardrobeInteractionRegion Region;Region.BoneOrSocket=A->Character->GetBoneName(0);Region.Reactions.SetNum(1);Region.Reactions[0].Sequence=Seq;Region.Reactions[0].CharacterBindingTag=TEXT("Person");
 Profiles->Characters[0].PhysicsFadeTime=0;Profiles->Characters[1].PhysicsFadeTime=0;Profiles->Characters[0].Interactions.Add(Region);Profiles->Characters[1].Interactions.Add(Region);
 TestFalse(TEXT("Inactive actor cannot react"),A->TriggerInteraction(0));
 TestTrue(TEXT("Active actor can play interaction sequence"),B->TriggerInteraction(0));
 TestFalse(TEXT("Camera controls locked during interaction"),Director->CanManipulate());
 TestTrue(TEXT("Switch cancels old reaction"),Director->SelectStation(0));
 TestFalse(TEXT("Old sequence stopped"),B->IsInteracting());Director->Tick(2);
 TestTrue(TEXT("Returned to A"),Director->GetActiveCharacter()==A);
 Director->SetPreviewEnabled(false);TestFalse(TEXT("Loading/quit disables interaction"),A->TriggerInteraction(0));
 Director->SetPreviewEnabled(true);
 Profiles->Characters[1].Duration=0;TestTrue(TEXT("Zero duration accepted"),Director->SelectStation(1));TestTrue(TEXT("Zero duration snaps exactly"),Director->GetActiveCharacter()==B);
 Profiles->Characters[0].Duration=2;Director->SelectStation(0);CamA->Destroy();Director->Tick(.1f);
 TestTrue(TEXT("Destroyed destination aborts safely to B"),!Director->IsTransitioning()&&Director->GetActiveCharacter()==B&&B->IsInteractionEnabled());
 Director->Shutdown();GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeCurveTest,"DreamBloom.Stations.CurvesAndRemovedSettings",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeCurveTest::RunTest(const FString& Parameters){
 auto* Curve=NewObject<UCurveFloat>();Curve->FloatCurve.AddKey(0,10);Curve->FloatCurve.AddKey(1,20);
 TestEqual(TEXT("Normalize curve start"),UWardrobeStationController::EvaluateProgress(Curve,0),0.f);
 TestEqual(TEXT("Normalize curve midpoint"),UWardrobeStationController::EvaluateProgress(Curve,.5),.5f);
 TestEqual(TEXT("Normalize curve end"),UWardrobeStationController::EvaluateProgress(Curve,1),1.f);
 auto* Constant=NewObject<UCurveFloat>();Constant->FloatCurve.AddKey(0,1);Constant->FloatCurve.AddKey(1,1);
 TestEqual(TEXT("Constant curve falls back safely"),UWardrobeStationController::EvaluateProgress(Constant,.5),.5f);
 TestNull(TEXT("Voice settings removed"),FWardrobeCharacterOption::StaticStruct()->FindPropertyByName(TEXT("Voices")));
 TestNull(TEXT("DA mesh swapping removed"),FWardrobeCharacterOption::StaticStruct()->FindPropertyByName(TEXT("Mesh")));
 TestNull(TEXT("Destination sequence list removed"),UWardrobeMenuData::StaticClass()->FindPropertyByName(TEXT("Sequences")));
 TestNull(TEXT("Legacy destination sequence removed"),UWardrobeMenuData::StaticClass()->FindPropertyByName(TEXT("Sequence")));
 TestNull(TEXT("Sequence selection function removed"),UWardrobeMenu::StaticClass()->FindFunctionByName(TEXT("SetSequenceMode")));
 TestNull(TEXT("Sequence selection sound removed"),UWardrobeAudioData::StaticClass()->FindPropertyByName(TEXT("SequenceSelect")));
 TestNotNull(TEXT("Interaction sequences retained"),FWardrobeReaction::StaticStruct()->FindPropertyByName(TEXT("Sequence")));
 return true;
}
#endif
