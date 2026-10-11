#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeCharacters.h"
#include "WardrobeShowcase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "MovieSceneObjectBindingID.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
static ULevelSequence* PreviewSequence(const TCHAR* Tag,double X){
 auto* Seq=NewObject<ULevelSequence>();Seq->Initialize();auto* Scene=Seq->GetMovieScene();Scene->SetDisplayRate(FFrameRate(30,1));Scene->SetTickResolutionDirectly(FFrameRate(24000,1));Scene->SetPlaybackRange(0,24000);
 auto Id=Scene->AddPossessable(TEXT("PreviewCharacter"),AWardrobeShowcase::StaticClass());Scene->TagBinding(FName(Tag),UE::MovieScene::FFixedObjectBindingID(Id,MovieSceneSequenceID::Root));
 auto* Track=Scene->AddTrack<UMovieScene3DTransformTrack>(Id);auto* S=Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());S->SetRange(TRange<FFrameNumber>::All());S->EvalOptions.CompletionMode=EMovieSceneCompletionMode::KeepState;Track->AddSection(*S);
 auto Channels=S->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
 for(int32 I=0;I<9;I++)Channels[I]->SetDefault(I>=6?1.:0.);
 Channels[0]->AddLinearKey(0,X);Channels[0]->AddLinearKey(24000,X+10.);return Seq;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefaultSequenceTest,"DreamBloom.DefaultSequence.LoopInterruptAndReturn",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDefaultSequenceTest::RunTest(const FString&){
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* PC=W->SpawnActor<APlayerController>();auto* Cam=W->SpawnActor<ACameraActor>();PC->SetViewTarget(Cam);
 auto* A=W->SpawnActor<AWardrobeShowcase>();auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Engine/EngineMeshes/SkeletalCube.SkeletalCube"));if(!TestNotNull(TEXT("Test mesh"),Mesh))return false;A->Character->SetSkeletalMesh(Mesh);A->SetActorLocation(FVector(12,0,0));
 auto* Data=NewObject<UWardrobeCharacterData>();Data->Characters.SetNum(1);auto& P=Data->Characters[0];P.DefaultSequence=PreviewSequence(TEXT("Person"),100);
 P.DefaultSequence.Get()->BindPossessableObject(P.DefaultSequence.Get()->GetMovieScene()->GetPossessable(0).GetGuid(),*A,W);P.PhysicsFadeTime=0;P.InteractionCooldown=0;P.MaxInteractionDuration=1;
 P.Interactions.SetNum(1);auto& R=P.Interactions[0];R.Cooldown=0;R.Reactions.SetNum(1);R.Reactions[0].Sequence=PreviewSequence(TEXT("Person"),300);R.Reactions[0].CharacterBindingTag=TEXT("Person");A->InteractionData=Data;A->InteractionProfileIndex=0;
 A->SetInteractionEnabled(true);TestTrue(TEXT("Default starts"),A->IsDefaultSequencePlaying());auto* Idle=A->GetDefaultSequencePlayer();TestEqual(TEXT("Play rate"),Idle->GetPlayRate(),1.f);TestTrue(TEXT("Default camera cuts disabled"),Idle->GetDisableCameraCuts());
 Idle->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(.5f,EUpdatePositionMethod::Jump));TestTrue(TEXT("Default authored actor binding animated"),FMath::IsNearlyEqual(A->GetActorLocation().X,105.,.01));
 A->SetInteractionEnabled(true);TestTrue(TEXT("Repeated enabling does not restart"),Idle==A->GetDefaultSequencePlayer());
 // Invalid interaction leaves default uninterrupted.
 R.Reactions[0].CharacterBindingTag=TEXT("Missing");TestFalse(TEXT("Missing reaction binding rejected"),A->TriggerInteraction(0));TestTrue(TEXT("Invalid reaction preserves idle"),A->GetDefaultSequencePlayer()==Idle&&A->IsDefaultSequencePlaying());R.Reactions[0].CharacterBindingTag=TEXT("Person");
 TestTrue(TEXT("Reaction starts"),A->TriggerInteraction(0));TestFalse(TEXT("Default stopped before reaction"),A->IsDefaultSequencePlaying());TestTrue(TEXT("Reaction owns pose"),FMath::IsNearlyEqual(A->GetActorLocation().X,300.,.01));
 A->TickInteraction(1.1f);TestFalse(TEXT("Timeout finishes reaction"),A->IsInteracting());TestTrue(TEXT("Default restarted"),A->IsDefaultSequencePlaying());TestTrue(TEXT("Default restarts from beginning"),FMath::Abs(A->GetDefaultSequencePlayer()->GetCurrentTime().AsSeconds())<.001);TestTrue(TEXT("Default drives after reaction"),FMath::IsNearlyEqual(A->GetActorLocation().X,100.,.01));
 P.MaxInteractionDuration=30;TestTrue(TEXT("Natural completion reaction starts"),A->TriggerInteraction(0));
 for(int I=0;I<60;I++)W->Tick(LEVELTICK_All,.02f);A->TickInteraction(.02f);
 TestFalse(TEXT("Natural sequence completion detected"),A->IsInteracting());TestTrue(TEXT("Natural completion returns to default"),A->IsDefaultSequencePlaying());
 TestTrue(TEXT("Second reaction"),A->TriggerInteraction(0));A->CancelInteraction(true);TestTrue(TEXT("Explicit cancellation returns to default"),A->IsDefaultSequencePlaying());
 TestTrue(TEXT("Third reaction"),A->TriggerInteraction(0));A->SetInteractionEnabled(false);TestFalse(TEXT("Station disable stops reaction"),A->IsInteracting());TestFalse(TEXT("Station disable does not restart default"),A->IsDefaultSequencePlaying());TestTrue(TEXT("KeepState overridden on disable"),FMath::IsNearlyEqual(A->GetActorLocation().X,12.,.01));
 A->SetInteractionEnabled(true);Idle=A->GetDefaultSequencePlayer();
 // The player's real update advances through several loops, without restoring the baseline between loops.
 Idle->SetPlayRate(1);for(int I=0;I<150;I++)W->Tick(LEVELTICK_All,.02f);TestTrue(TEXT("Still playing after three cycles"),A->IsDefaultSequencePlaying());TestTrue(TEXT("Loop retains keyed transform"),A->GetActorLocation().X>=99.9&&A->GetActorLocation().X<=110.1);
 P.PhysicsFadeTime=.2f;
 A->TickSequencePhysicsBlend(1);TestEqual(TEXT("Physics fully visible"),A->GetSequencePhysicsWeight(),1.f);
 TestTrue(TEXT("Faded interaction queued"),A->TriggerInteraction(0));TestTrue(TEXT("Default continues during fade out"),A->IsDefaultSequencePlaying());TestTrue(TEXT("Input locked during transition"),A->IsSequenceSwitching());
 A->TickSequencePhysicsBlend(.1f);TestTrue(TEXT("Half fade out"),FMath::IsNearlyEqual(A->GetSequencePhysicsWeight(),.5f,.01));TestTrue(TEXT("Default not switched early"),A->IsDefaultSequencePlaying());
 A->TickSequencePhysicsBlend(.1f);TestFalse(TEXT("Default stopped at zero"),A->IsDefaultSequencePlaying());TestEqual(TEXT("Incoming physics starts at zero"),A->GetSequencePhysicsWeight(),0.f);
 A->TickSequencePhysicsBlend(.1f);TestTrue(TEXT("Half fade in"),FMath::IsNearlyEqual(A->GetSequencePhysicsWeight(),.5f,.01));A->TickSequencePhysicsBlend(.1f);
 A->CancelInteraction(true);TestTrue(TEXT("Return waits for fade"),A->IsInteracting()&&!A->IsDefaultSequencePlaying());A->TickSequencePhysicsBlend(.1f);TestTrue(TEXT("Return fading"),FMath::IsNearlyEqual(A->GetSequencePhysicsWeight(),.5f,.01));A->TickSequencePhysicsBlend(.1f);TestTrue(TEXT("Default returns at zero"),A->IsDefaultSequencePlaying());TestEqual(TEXT("Default physics fades from zero"),A->GetSequencePhysicsWeight(),0.f);A->TickSequencePhysicsBlend(.4f);TestEqual(TEXT("Default physics recovered"),A->GetSequencePhysicsWeight(),1.f);
 TestTrue(TEXT("Natural faded reaction"),A->TriggerInteraction(0));A->TickSequencePhysicsBlend(.2f);A->TickSequencePhysicsBlend(.4f);
 for(int I=0;I<60;I++)W->Tick(LEVELTICK_All,.02f);A->TickInteraction(.02f);
 TestTrue(TEXT("Natural end waits at final pose"),A->IsSequenceSwitching()&&A->IsInteracting());A->TickSequencePhysicsBlend(.2f);TestTrue(TEXT("Natural end resumes default after fade"),A->IsDefaultSequencePlaying());
 TestTrue(TEXT("Queue before disabling"),A->TriggerInteraction(0));A->SetInteractionEnabled(false);A->TickSequencePhysicsBlend(1);TestFalse(TEXT("No queued restart after disable"),A->IsInteracting()||A->IsDefaultSequencePlaying());
 A->SetInteractionEnabled(false);P.DefaultSequence=nullptr;A->SetInteractionEnabled(true);TestFalse(TEXT("Empty default keeps existing blueprint setup"),A->IsDefaultSequencePlaying());TestTrue(TEXT("No sequence does not force animation mode"),A->Character->GetAnimationMode()==EAnimationMode::AnimationBlueprint);
 P.DefaultSequence=PreviewSequence(TEXT("Person"),100);P.DefaultSequence.Get()->BindPossessableObject(P.DefaultSequence.Get()->GetMovieScene()->GetPossessable(0).GetGuid(),*A,W);
 TestFalse(TEXT("Default binding setting removed"),FWardrobeCharacterOption::StaticStruct()->FindPropertyByName(TEXT("DefaultCharacterBindingTag"))!=nullptr);
 TestFalse(TEXT("Default rate setting removed"),FWardrobeCharacterOption::StaticStruct()->FindPropertyByName(TEXT("DefaultSequencePlayRate"))!=nullptr);
 A->SetInteractionEnabled(true);A->EndPlay(EEndPlayReason::RemovedFromWorld);TestFalse(TEXT("EndPlay stops default"),A->IsDefaultSequencePlaying());TestFalse(TEXT("EndPlay cannot restart"),A->StartDefaultSequence());
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
