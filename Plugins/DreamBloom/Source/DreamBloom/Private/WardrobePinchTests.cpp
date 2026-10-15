#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobePinchTest,"DreamBloom.Pinch.ImportedRigDragAndRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobePinchTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobePinch21/SK_DaniyaPinch.SK_DaniyaPinch"));
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/WardrobePinch21/AB_DaniyaPinch.AB_DaniyaPinch"));
 auto* Data=LoadObject<UWardrobeCharacterData>(nullptr,TEXT("/Game/WardrobePinch21/DA_PinchCharacters.DA_PinchCharacters"));
 if(!TestNotNull(TEXT("Imported weighted mesh"),Mesh)||!TestNotNull(TEXT("Compiled pinch AnimBP"),BP)||!TestNotNull(TEXT("Demo DA"),Data))return false;
 TestNull(TEXT("Pull Morph configuration removed"),FWardrobePinchRegion::StaticStruct()->FindPropertyByName(TEXT("PullMorph")));
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* Person=W->SpawnActor<AWardrobeShowcase>();Person->Character->SetSkeletalMesh(Mesh);Person->Character->SetAnimInstanceClass(BP->GeneratedClass);
 Person->InteractionData=Data;Person->InteractionProfileIndex=0;Person->SetInteractionEnabled(true);Person->Character->SetMorphTarget(TEXT("DB_Cheek_L_Bulge"),.37f);
 auto Evaluate=[&](){Person->Character->TickAnimation(1.f/60,false);Person->Character->RefreshBoneTransforms();Person->Character->HandleExistingParallelEvaluationTask(true,true);};Evaluate();
 const auto& Region=Data->Characters[0].PinchRegions[0];
 const FVector Base=Person->Character->GetSocketLocation(Region.Bone);const FVector Origin=Base+FVector(0,50,0),Ray=FVector(0,-1,0);
 TestFalse(TEXT("Off-face click misses"),Person->BeginPinch(Origin+FVector(30,0,0),Ray));
 TestTrue(TEXT("Cheek starts pinch"),Person->BeginPinch(Origin,Ray));
 Person->UpdatePinch(Origin+FVector(20,0,0),Ray);
 for(int32 I=0;I<90;I++){Person->TickPinch(1.f/60);Evaluate();}
 const FVector Offset=Person->GetPinchOffset(Region.Bone),Moved=Person->Character->GetSocketLocation(Region.Bone);
 TestTrue(TEXT("Offset clamps to configured cm range"),Offset.Size()>1&&Offset.Size()<2.51);
 TestTrue(TEXT("Animation node moves weighted bone by centimeters despite FBX root scale"),FVector::Distance(Base,Moved)>1&&FVector::Distance(Base,Moved)<2.6);
 TestTrue(TEXT("Pinch leaves separately authored morph untouched"),FMath::IsNearlyEqual(Person->Character->GetMorphTarget(TEXT("DB_Cheek_L_Bulge")),.37f));
 TestTrue(TEXT("Other cheek stays unchanged"),Person->GetPinchOffset(TEXT("DB_Pinch_Cheek_R")).IsNearlyZero());
 Person->ReleasePinch();TestFalse(TEXT("Release ends gesture"),Person->IsPinching());
 for(int32 I=0;I<180;I++){Person->TickPinch(1.f/60);Evaluate();}
 TestTrue(TEXT("Spring returns neutral bone"),Person->Character->GetSocketLocation(Region.Bone).Equals(Base,.02));
 TestTrue(TEXT("Release leaves separately authored morph untouched"),FMath::IsNearlyEqual(Person->Character->GetMorphTarget(TEXT("DB_Cheek_L_Bulge")),.37f));
 TestTrue(TEXT("Second pinch"),Person->BeginPinch(Origin,Ray));Person->SetInteractionEnabled(false);
 TestFalse(TEXT("Station change clears held gesture"),Person->IsPinching());TestTrue(TEXT("Station change clears offsets"),Person->GetPinchOffset(Region.Bone).IsNearlyZero());
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
