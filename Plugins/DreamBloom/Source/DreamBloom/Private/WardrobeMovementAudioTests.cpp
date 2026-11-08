#include "WardrobeMovementAudio.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeLocomotion.h"
#include "WardrobeFootstepSurfaceRegion.h"
#include "Sound/SoundWave.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "AudioDevice.h"
#include "AudioDeviceManager.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeSurfaceAudioTest,"DreamBloom.MovementAudio.SurfaceSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeSurfaceAudioTest::RunTest(const FString&){
 auto* Data=NewObject<UWardrobeMovementAudioData>();auto* Default=NewObject<USoundWave>();auto* Grass=NewObject<USoundWave>();auto* Run=NewObject<USoundWave>();
 Data->Walk.Add(Default);FWardrobeSurfaceFootsteps Row;Row.Surface=SurfaceType2;Row.Walk.Add(Grass);Data->Surfaces.Add(Row);
 TestEqual(TEXT("Grass chooses its own walk group"),Data->GetSounds(EWardrobeMovementSound::Walk,SurfaceType2)[0].Get(),static_cast<USoundBase*>(Grass));
 TestEqual(TEXT("Missing grass run uses grass walk"),Data->GetSounds(EWardrobeMovementSound::Run,SurfaceType2)[0].Get(),static_cast<USoundBase*>(Grass));
 Data->Surfaces[0].Run.Add(Run);
 TestEqual(TEXT("Grass run selects its own variations"),Data->GetSounds(EWardrobeMovementSound::Run,SurfaceType2)[0].Get(),static_cast<USoundBase*>(Run));
 TestEqual(TEXT("Unknown surface uses defaults"),Data->GetSounds(EWardrobeMovementSound::Walk,SurfaceType3)[0].Get(),static_cast<USoundBase*>(Default));
 Data->Surfaces[0].Walk={nullptr};
 TestEqual(TEXT("Invalid group falls back"),Data->GetSounds(EWardrobeMovementSound::Walk,SurfaceType2)[0].Get(),static_cast<USoundBase*>(Default));
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeExplorer>();A->SetActorLocation(FVector(0,0,A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
 auto* FloorActor=W->SpawnActor<AActor>();auto* Floor=NewObject<UBoxComponent>(FloorActor);FloorActor->SetRootComponent(Floor);
 Floor->SetBoxExtent(FVector(500,500,10));Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);FloorActor->SetActorLocation(FVector(0,0,-10));
 auto* Phys=NewObject<UPhysicalMaterial>();Phys->SurfaceType=SurfaceType2;Floor->SetPhysMaterialOverride(Phys);Floor->RegisterComponent();
 FHitResult Probe;FCollisionQueryParams Params;Params.bReturnPhysicalMaterial=true;Params.AddIgnoredActor(A);
 const bool bHit=W->LineTraceSingleByChannel(Probe,FVector(0,0,40),FVector(0,0,-100),ECC_Visibility,Params);
 AddInfo(FString::Printf(TEXT("Surface trace: hit=%d physicalMaterial=%s surface=%d"),bHit,*GetNameSafe(Probe.PhysMaterial.Get()),int(UPhysicalMaterial::DetermineSurfaceType(Probe.PhysMaterial.Get()))));
 FVector Contact;TestEqual(TEXT("Foot trace reads grass physical material"),A->DetectFootstepSurface(true,Contact).GetValue(),SurfaceType2);
 TestTrue(TEXT("Contact is on floor, not capsule center"),FMath::Abs(Contact.Z)<.01);
 auto* Water=W->SpawnActor<AWardrobeFootstepSurfaceRegion>();Water->SetActorLocation(FVector(0,0,0));Water->Bounds->SetBoxExtent(FVector(100,100,30));Water->Surface=SurfaceType1;
 TestEqual(TEXT("Shallow water overrides its submerged ground"),A->DetectFootstepSurface(false,Contact).GetValue(),SurfaceType1);
 TestFalse(TEXT("Bridge above water stays dry"),Water->ContainsContact(FVector(0,0,100)));
 TestFalse(TEXT("Deep contact is outside shallow region"),Water->ContainsContact(FVector(0,0,-100)));
 TestFalse(TEXT("Outside XY footprint stays dry"),Water->ContainsContact(FVector(200,0,0)));
 auto* Smaller=W->SpawnActor<AWardrobeFootstepSurfaceRegion>();Smaller->Bounds->SetBoxExtent(FVector(50,50,30));Smaller->Surface=SurfaceType3;
 TestEqual(TEXT("Equal priority selects smaller region"),A->DetectFootstepSurface(true,Contact).GetValue(),SurfaceType3);
 Water->Priority=1;TestEqual(TEXT("Explicit priority wins"),A->DetectFootstepSurface(true,Contact).GetValue(),SurfaceType1);
 Water->Destroy();Smaller->Destroy();Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 TestEqual(TEXT("Missing ground safely uses default"),A->DetectFootstepSurface(true,Contact).GetValue(),SurfaceType_Default);
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeSurfacePlaybackTest,"DreamBloom.MovementAudio.NotifyPlaybackAndVariation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeSurfacePlaybackTest::RunTest(const FString&){
 auto* Audio=LoadObject<UWardrobeMovementAudioData>(nullptr,TEXT("/Game/BluePrint/ALS/Audio/DA_ALSMovementAudio.DA_ALSMovementAudio"));
 if(!TestNotNull(TEXT("Configured movement audio DA"),Audio))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 if(auto* Manager=FAudioDeviceManager::Get()){auto Params=Manager->GetDefaultParamsForNewWorld();Params.AssociatedWorld=W;W->SetAudioDevice(Manager->RequestAudioDevice(Params));}
 auto* A=W->SpawnActor<AWardrobeExplorer>();A->Settings=NewObject<UWardrobeLocomotionData>(A);A->Settings->MovementAudio=Audio;
 A->SetActorLocation(FVector(0,0,A->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
 auto* PC=W->SpawnActor<APlayerController>();PC->Possess(A);A->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
 auto* FloorActor=W->SpawnActor<AActor>();auto* Floor=NewObject<UBoxComponent>(FloorActor);FloorActor->SetRootComponent(Floor);FloorActor->SetActorLocation(FVector(0,0,-10));
 Floor->SetBoxExtent(FVector(500,500,10));Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Floor->RegisterComponent();
 auto* Notify=NewObject<UAnimNotify_WardrobeFootstep>();
 for(EPhysicalSurface Surface:{SurfaceType1,SurfaceType2,SurfaceType3}){
  auto* Phys=NewObject<UPhysicalMaterial>(FloorActor);Phys->SurfaceType=Surface;Floor->SetPhysMaterialOverride(Phys);
  for(bool Running:{false,true}){
   const auto Event=Running?EWardrobeMovementSound::Run:EWardrobeMovementSound::Walk;const auto& Choices=Audio->GetSounds(Event,Surface);
   TestTrue(TEXT("Surface has at least five variants"),Choices.Num()>=5);USoundBase* Previous=nullptr;
   A->GetCharacterMovement()->Velocity=FVector(Running?420.f:160.f,0,0);
   for(int32 I=0;I<4;++I){
    A->LastAnyFootstepTime=A->LastFootstepTime[0]=A->LastFootstepTime[1]=-1000;
    Notify->bLeftFoot=(I%2)==0;const int32 Before=A->MovementSoundPlayCount;
    Notify->Notify(A->GetMesh(),nullptr,FAnimNotifyEventReference());
    TestEqual(TEXT("A contact notify spawns one sound"),A->MovementSoundPlayCount,Before+1);
    if(A->MovementSounds.IsEmpty()){AddError(TEXT("Audio spawn failed; audio device is required for this test"));continue;}
    auto* Sound=A->MovementSounds.Last()->Sound.Get();
    TestTrue(TEXT("Spawned sound belongs to the surface and gait"),Choices.Contains(Sound));
    TestTrue(TEXT("Consecutive contacts do not repeat an asset"),Sound!=Previous);Previous=Sound;
   }
  }
 }
 const int32 Count=A->MovementSoundPlayCount;A->GetCharacterMovement()->Velocity=FVector::ZeroVector;
 A->LastAnyFootstepTime=A->LastFootstepTime[0]=-1000;Notify->Notify(A->GetMesh(),nullptr,FAnimNotifyEventReference());
 TestEqual(TEXT("Idle notify stays silent"),A->MovementSoundPlayCount,Count);
 A->StopMovementAudio();TestTrue(TEXT("Cleanup releases movement sounds"),A->MovementSounds.IsEmpty());
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
