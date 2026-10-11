#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeCharacters.h"
#include "WardrobeShowcase.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/AudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/SkeletalMesh.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "MovieScene.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobePinchReleaseTest,"DreamBloom.Pinch.AudioAndReleaseSequence",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobePinchReleaseTest::RunTest(const FString&){
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeShowcase>();
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/SK_DaniyaFullBodyPinch.SK_DaniyaFullBodyPinch"));
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/AB_FullBodySequencePinch.AB_FullBodySequencePinch"));
 if(!TestNotNull(TEXT("Demo mesh"),Mesh)||!TestNotNull(TEXT("Demo AnimBP"),BP))return false;
 A->Character->SetSkeletalMesh(Mesh);A->Character->SetAnimInstanceClass(BP->GeneratedClass);
 auto* Data=NewObject<UWardrobeCharacterData>();Data->Characters.SetNum(1);auto& P=Data->Characters[0];P.PhysicsFadeTime=.2f;P.MaxInteractionDuration=1;
 FWardrobePinchSounds Pool;Pool.Sounds.Add(TSoftObjectPtr<USoundBase>());
 Pool.Sounds.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/TestA.TestA"))));const auto DuplicateSound=Pool.Sounds.Last();Pool.Sounds.Add(DuplicateSound);
 Pool.Sounds.Add(TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Audio/TestB.TestB"))));
 TSoftObjectPtr<USoundBase> Previous;
 for(int I=0;I<12;I++){auto Choice=A->ChoosePinchSound(Pool,TEXT("TestCheek"),TEXT("Grab"));TestFalse(TEXT("Random choice skips empty"),Choice.Sound.IsNull());if(I>0)TestTrue(TEXT("Avoid adjacent repeated clip"),Choice.Sound!=Previous);Previous=Choice.Sound;}
 FWardrobePinchSounds Single;Single.Sounds.Add(Pool.Sounds[1]);TestTrue(TEXT("One clip remains supported"),A->ChoosePinchSound(Single,TEXT("TestCheek"),TEXT("Move")).Sound==Single.Sounds[0]);
 FWardrobePinchSounds Empty;TestTrue(TEXT("Empty pool stays silent"),A->ChoosePinchSound(Empty,TEXT("TestCheek"),TEXT("Release")).Sound.IsNull());
 auto MakeSequence=[](){auto* S=NewObject<ULevelSequence>();S->Initialize();S->GetMovieScene()->SetTickResolutionDirectly(FFrameRate(24000,1));S->GetMovieScene()->SetPlaybackRange(0,72000);return S;};
 P.DefaultSequence=MakeSequence();P.PinchRegions.SetNum(1);auto& R=P.PinchRegions[0];R.Bone=TEXT("DB_Pinch_Cheek_L");R.ReleaseSequence=MakeSequence();
 A->InteractionData=Data;A->InteractionProfileIndex=0;A->SetInteractionEnabled(true);A->TickSequencePhysicsBlend(1);
 auto Eval=[&](){A->Character->TickAnimation(1.f/60,false);A->Character->RefreshBoneTransforms();A->Character->HandleExistingParallelEvaluationTask(true,true);};Eval();
 const FVector Base=A->Character->GetSocketLocation(R.Bone),Origin=Base+FVector(0,60,0),Ray(0,-1,0);
 TestTrue(TEXT("Grab"),A->BeginPinch(Origin,Ray));
 A->PinchGrabAudio=NewObject<UAudioComponent>(A);A->PinchMoveAudio=NewObject<UAudioComponent>(A);A->bPinchGrabFinished=false;
 A->UpdatePinch(Origin+FVector(2,0,0),Ray);A->TickPinch(1.f/60);TestEqual(TEXT("Move waits for grab completion"),A->PinchMoveVolume,0.f);TestFalse(TEXT("Move not started early"),A->bPinchMoveStarted);
 A->FinishPinchGrabSound();A->TickPinch(1.f/60);TestTrue(TEXT("Motion after grab starts move"),A->bPinchMoveStarted);
 for(int I=0;I<40;I++){A->UpdatePinch(Origin+FVector(2+.04f*I,0,0),Ray);A->TickPinch(1.f/60);}const float Weak=A->PinchMoveVolume;
 for(int I=0;I<60;I++){A->UpdatePinch(Origin+FVector(40+.04f*I,0,0),Ray);A->TickPinch(1.f/60);}
 TestTrue(TEXT("Stretch controls volume"),A->PinchMoveVolume>Weak+.3f&&A->PinchMoveVolume<=1.001f);
 for(int I=0;I<30;I++){A->UpdatePinch(Origin+FVector(42.36f,0,0),Ray);A->TickPinch(1.f/60);}
 TestTrue(TEXT("Stationary pointer pauses loop"),A->bPinchMovePaused&&A->PinchMoveVolume<.01f);
 A->UpdatePinch(Origin+FVector(43,0,0),Ray);A->TickPinch(1.f/60);TestFalse(TEXT("New motion resumes loop"),A->bPinchMovePaused);
 A->RepeatPinchMoveSound();TestTrue(TEXT("Finished sample repeats while moving"),A->bPinchMoveStarted);
 for(int I=0;I<30;I++)A->TickPinch(1.f/60);A->RepeatPinchMoveSound();TestFalse(TEXT("Sample does not repeat while still"),A->bPinchMoveStarted);
 A->ReleasePinch();TestNull(TEXT("Grab stopped on release"),A->PinchGrabAudio.Get());TestNull(TEXT("Move stopped on release"),A->PinchMoveAudio.Get());
 TestNotNull(TEXT("Release sequence queued"),A->PendingPinchReleaseSequence.Get());A->ReleasePinch();
 TestTrue(TEXT("Default remains during spring recovery"),A->IsDefaultSequencePlaying());
 for(int I=0;I<200;I++){A->TickPinch(1.f/60);if(A->IsInteracting())break;}
 TestTrue(TEXT("Aftercare queued"),A->IsInteracting()&&A->IsSequenceSwitching());A->TickSequencePhysicsBlend(.2f);
 TestFalse(TEXT("Aftercare interrupts default"),A->IsDefaultSequencePlaying());TestTrue(TEXT("Aftercare starts"),A->InteractionPlayer&&A->InteractionPlayer->IsPlaying());
 A->TickInteraction(1.2f);TestTrue(TEXT("Aftercare not cut by interaction timeout"),A->IsInteracting()&&!A->IsSequenceSwitching());
 A->InteractionPlayer->Pause();A->TickInteraction(.01f);TestTrue(TEXT("Aftercare return uses fade"),A->IsSequenceSwitching());A->TickSequencePhysicsBlend(.2f);
 TestFalse(TEXT("Aftercare finished"),A->IsInteracting());TestTrue(TEXT("Default resumes"),A->IsDefaultSequencePlaying());
 Eval();TestTrue(TEXT("Grab again"),A->BeginPinch(Origin,Ray));A->ReleasePinch();TestNotNull(TEXT("Queued again"),A->PendingPinchReleaseSequence.Get());
 TestTrue(TEXT("New grab cancels delayed aftercare"),A->BeginPinch(Origin,Ray));TestNull(TEXT("Delayed sequence canceled"),A->PendingPinchReleaseSequence.Get());
 A->ReleasePinch();A->SetInteractionEnabled(false);A->TickPinch(3);A->TickSequencePhysicsBlend(3);TestFalse(TEXT("Disable prevents queued sequence"),A->IsInteracting()||A->IsDefaultSequencePlaying());
 TestFalse(TEXT("Sound volume removed"),FWardrobeSoundEntry::StaticStruct()->FindPropertyByName(TEXT("Volume"))!=nullptr);
 TestFalse(TEXT("DA master volume removed"),UWardrobeAudioData::StaticClass()->FindPropertyByName(TEXT("MasterVolume"))!=nullptr);
 TestFalse(TEXT("Old movement distance setting removed"),FWardrobePinchRegion::StaticStruct()->FindPropertyByName(TEXT("MoveSoundDistance"))!=nullptr);
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif

