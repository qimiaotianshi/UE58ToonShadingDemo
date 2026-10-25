#include "WardrobeStory.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "WardrobeMenu.h"
#include "Misc/PackageName.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStoryRuntimeTest,"DreamBloom.Story.TriggerPauseRestoreAndLinearProgress",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStoryRuntimeTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone(FName(*FString::Printf(TEXT("StoryTest_%s"),*FGuid::NewGuid().ToString())));
 auto* W=GI->GetWorld();auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>();
 auto* PC=W->SpawnActor<APlayerController>();auto* Local=NewObject<ULocalPlayer>(GEngine);GI->AddLocalPlayer(Local,FPlatformUserId::CreateFromInternalId(0));PC->SetPlayer(Local);auto* Pawn=W->SpawnActor<ACharacter>();PC->Possess(Pawn);
 auto* Camera=W->SpawnActor<ACameraActor>();PC->SetViewTarget(Camera);const auto* ExpectedView=PC->GetViewTarget();PC->SetControlRotation(FRotator(10,20,0));Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
 auto* Flow=NewObject<UWardrobeStoryFlow>();Story->Flow=Flow;Flow->Steps.SetNum(2);
 const FString Package=W->GetOutermost()->GetName();
 for(int32 I=0;I<2;++I){auto& Step=Flow->Steps[I];Step.StoryId=I==0?TEXT("First"):TEXT("Second");Step.TriggerId=I==0?TEXT("AreaA"):TEXT("AreaB");Step.Level=TSoftObjectPtr<UWorld>(FSoftObjectPath(Package+TEXT(".")+FPackageName::GetLongPackageAssetName(Package)));Step.Sequence=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/BluePrint/Story/LS_StoryDemo01.LS_StoryDemo01"));}
 if(!TestNotNull(TEXT("Authored demo Sequence with wait events"),Flow->Steps[0].Sequence.Get()))return false;
 Pawn->GetCapsuleComponent()->SetGenerateOverlapEvents(true);Pawn->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Pawn->GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
 auto* Trigger=W->SpawnActor<AWardrobeStoryTrigger>();Trigger->TriggerId=TEXT("AreaB");Pawn->DispatchBeginPlay();Trigger->DispatchBeginPlay();Trigger->Box->UpdateOverlaps();Pawn->GetCapsuleComponent()->UpdateOverlaps();
 AddInfo(FString::Printf(TEXT("Pawn=%s capsule=%s box=%s begun=%d registered=%d/%d physics=%d/%d local=%d controller=%s"),*Pawn->GetActorLocation().ToString(),*Pawn->GetCapsuleComponent()->GetComponentLocation().ToString(),*Trigger->Box->GetComponentLocation().ToString(),Pawn->HasActorBegunPlay(),Pawn->GetCapsuleComponent()->IsRegistered(),Trigger->Box->IsRegistered(),Pawn->GetCapsuleComponent()->IsPhysicsStateCreated(),Trigger->Box->IsPhysicsStateCreated(),Pawn->IsLocallyControlled(),*GetNameSafe(UGameplayStatics::GetPlayerController(W,0))));
 TestTrue(TEXT("Player actually overlaps box"),Trigger->ContainsPlayer(Pawn));
 TestFalse(TEXT("Future step does not trigger"),Story->TryTrigger(Trigger));
 TestEqual(TEXT("Preview starts at stable first ID"),Story->GetCurrentStoryId(),FName(TEXT("First")));
 Trigger->TriggerId=TEXT("AreaA");const FTransform Before=Pawn->GetActorTransform();
 if(!TestTrue(TEXT("Current box starts performance"),Story->TryTrigger(Trigger))){GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 TestTrue(TEXT("Player is hidden and collision disabled"),Pawn->IsHidden()&&!Pawn->GetActorEnableCollision());
 TestTrue(TEXT("Movement and look disabled"),PC->IsMoveInputIgnored()&&PC->IsLookInputIgnored());
 TestEqual(TEXT("Hidden player does not fall through floor"),Pawn->GetCharacterMovement()->MovementMode.GetValue(),MOVE_None);
 Story->StartLoadedSequence();if(!TestNotNull(TEXT("Player created"),Story->Player.Get())){GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 Story->WaitForClick(nullptr);TestFalse(TEXT("An unrelated player cannot pause"),Story->IsWaitingForClick());
 Story->Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(FFrameTime(100),EUpdatePositionMethod::Play));
 TestTrue(TEXT("Authored Event Track pauses the actual root player at frame 100"),Story->IsWaitingForClick());
 Story->WaitForClick(Story->Player);Story->AdvanceRuntime(.016f);TestTrue(TEXT("Wait pauses Sequence"),Story->IsWaitingForClick()&&Story->Player->IsPaused());
 Story->bClickArmed=false;TestFalse(TEXT("Held entry click cannot resume"),Story->ClickContinue());Story->ArmContinueClick();TestTrue(TEXT("Released then new click resumes"),Story->ClickContinue());
 Story->CancelCinematic();TestFalse(TEXT("Abort keeps current story"),Story->IsCinematicActive());TestEqual(TEXT("Abort never advances"),Story->GetCurrentStoryId(),FName(TEXT("First")));
 TestTrue(TEXT("Visibility, collision and input restored"),!Pawn->IsHidden()&&Pawn->GetActorEnableCollision()&&!PC->IsMoveInputIgnored()&&!PC->IsLookInputIgnored());
 TestTrue(TEXT("View and original transform restored"),PC->GetViewTarget()==ExpectedView&&Pawn->GetActorTransform().Equals(Before));
 TestFalse(TEXT("Abort is not instantly retriggered inside the same box"),Story->TryTrigger(Trigger));Story->BlockedTrigger.Reset();
 TestTrue(TEXT("Can re-enter current scene"),Story->TryTrigger(Trigger));Story->StartLoadedSequence();
 Story->OnSequenceFinished();Story->OnSequenceStopped();TestEqual(TEXT("Finish callback defers mutation outside evaluation"),Story->GetCurrentStoryId(),FName(TEXT("First")));
 Story->AdvanceRuntime(.016f);TestEqual(TEXT("Natural completion advances despite stop notification"),Story->GetCurrentStoryId(),FName(TEXT("Second")));
 TestTrue(TEXT("First recorded once"),Story->Save->CompletedStories.Num()==1&&Story->Save->CompletedStories[0]==TEXT("First"));
 Trigger->TriggerId=TEXT("AreaB");Story->BlockedTrigger.Reset();Story->TryTrigger(Trigger);Story->StartLoadedSequence();Story->OnSequenceFinished();Story->AdvanceRuntime(.016f);
 TestTrue(TEXT("Final scene completes campaign"),Story->Save->bStoryComplete&&Story->GetCurrentStoryId().IsNone());
 GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStorySaveTest,"DreamBloom.Story.SlotsValidationAndStableIDs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStorySaveTest::RunTest(const FString&){
 auto* GI=NewObject<UGameInstance>(GEngine);GI->InitializeStandalone();auto* W=GI->GetWorld();auto* Story=GI->GetSubsystem<UWardrobeStorySubsystem>();
 auto* Source=LoadObject<UWardrobeStoryFlow>(nullptr,TEXT("/Game/BluePrint/Story/DA_StoryFlow.DA_StoryFlow"));if(!TestNotNull(TEXT("Saved narrative DA"),Source))return false;
 auto* Flow=DuplicateObject<UWardrobeStoryFlow>(Source,GI);Flow->SaveNamespace=TEXT("StoryAutomation_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);TestTrue(TEXT("Demo references exist"),Flow->ValidateConfiguration().IsEmpty());TestTrue(TEXT("Configure"),Story->Configure(Flow));
 const auto Empty=Story->GetSaveSlots();TestEqual(TEXT("Default maximum three slots"),Empty.Num(),3);TestFalse(TEXT("No existing save in isolated namespace"),Empty[0].bExists);
 auto* Saved=NewObject<UWardrobeStorySave>(Story);Saved->FlowPath=Flow->GetPathName();Saved->NextStoryId=Flow->Steps[1].StoryId;Saved->CompletedStories.Add(Flow->Steps[0].StoryId);Saved->Level=Flow->Steps[0].Level;Saved->PlayerName=TEXT("测试旅人");Saved->SavedAt=FDateTime::UtcNow();Saved->bHasPlayerTransform=true;Saved->PlayerTransform=FTransform(FRotator(0,45,0),FVector(1,2,100));
 TestTrue(TEXT("Valid checkpoint"),Story->IsSaveUsable(Saved));TestTrue(TEXT("Write isolated slot"),UGameplayStatics::SaveGameToSlot(Saved,Story->SlotName(1),0));
 auto Slots=Story->GetSaveSlots();TestTrue(TEXT("Slot listing exposes valid name and progress"),Slots[1].bValid&&Slots[1].PlayerName==TEXT("测试旅人"));
 auto* Loaded=Cast<UWardrobeStorySave>(UGameplayStatics::LoadGameFromSlot(Story->SlotName(1),0));TestTrue(TEXT("Save restores name, position and pending scene"),Loaded&&Loaded->PlayerName==Saved->PlayerName&&Loaded->PlayerTransform.Equals(Saved->PlayerTransform)&&Loaded->NextStoryId==Saved->NextStoryId);
 Story->Save=Loaded;Flow->Steps.Swap(0,1);TestEqual(TEXT("Reordering resolves progress by ID instead of index"),Story->CurrentStepIndex(),0);
 TestFalse(TEXT("New game cannot overwrite without confirmation"),Story->StartNewGame(1,TEXT("Other"),nullptr,false));TestFalse(TEXT("Out-of-range slot rejected"),Story->StartNewGame(3,TEXT("Other"),nullptr,false));
 Loaded->NextStoryId=TEXT("Removed");TestFalse(TEXT("Removed pending story rejected"),Story->IsSaveUsable(Loaded));Loaded->NextStoryId=Saved->NextStoryId;
 Loaded->PlayerTransform.SetRotation(FQuat(0,0,0,0));TestFalse(TEXT("Invalid saved rotation rejected"),Story->IsSaveUsable(Loaded));
 Flow->Steps[1].StoryId=Flow->Steps[0].StoryId;TestFalse(TEXT("Duplicate stable IDs caught"),Flow->ValidateConfiguration().IsEmpty());
 UGameplayStatics::DeleteGameInSlot(Story->SlotName(1),0);Story->Save=nullptr;GI->Shutdown();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
