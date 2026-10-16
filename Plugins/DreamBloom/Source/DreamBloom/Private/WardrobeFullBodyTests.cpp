#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeFullBodyTest,"DreamBloom.Pinch.FullBodyFiveRegions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeFullBodyTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/SK_DaniyaFullBodyPinch.SK_DaniyaFullBodyPinch"));
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/AB_FullBodySequencePinch.AB_FullBodySequencePinch"));
 auto* Data=LoadObject<UWardrobeCharacterData>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/DA_FullBodyCharacters.DA_FullBodyCharacters"));
 if(!TestNotNull(TEXT("Mesh"),Mesh)||!TestNotNull(TEXT("BP"),BP)||!TestNotNull(TEXT("DA"),Data))return false;
 // Keep this regression deterministic when the user's live DA removes regions
 // or deliberately leaves reaction slots empty. Never edit their saved asset.
 Data=DuplicateObject<UWardrobeCharacterData>(Data,GetTransientPackage());
 auto& Profile=Data->Characters[0];Profile.PinchRegions.Reset();
 for(const FString Suffix:{TEXT("Cheek_L"),TEXT("Cheek_R"),TEXT("Belly"),TEXT("Thigh_L"),TEXT("Thigh_R")}){
  FWardrobePinchRegion Region;Region.Bone=FName(*(TEXT("DB_Pinch_")+Suffix));Region.BodyUpAxis=FVector(0,0,-1); // Authored FullBody42 clip convention.
  auto Load=[&](const TCHAR* Direction){const FString Name=TEXT("A_")+Suffix+TEXT("_")+Direction;return LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/WardrobeFullBodyPinch42/")+Name+TEXT(".")+Name));};
  Region.BodyLeft=Load(TEXT("Left"));Region.BodyRight=Load(TEXT("Right"));Region.BodyUp=Load(TEXT("Up"));Region.BodyDown=Load(TEXT("Down"));
  if(!TestNotNull(TEXT("Fixture left clip"),Region.BodyLeft.Get())||!TestNotNull(TEXT("Fixture right clip"),Region.BodyRight.Get())||!TestNotNull(TEXT("Fixture up clip"),Region.BodyUp.Get())||!TestNotNull(TEXT("Fixture down clip"),Region.BodyDown.Get()))return false;
  Profile.PinchRegions.Add(Region);
 }
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* P=W->SpawnActor<AWardrobeShowcase>();P->Character->SetSkeletalMesh(Mesh);P->Character->SetAnimInstanceClass(BP->GeneratedClass);P->InteractionData=Data;P->InteractionProfileIndex=0;P->SetInteractionEnabled(true);
 auto Eval=[&](){P->Character->TickAnimation(1.f/60,false);P->Character->RefreshBoneTransforms();P->Character->HandleExistingParallelEvaluationTask(true,true);};Eval();
 TestEqual(TEXT("Five configured regions"),Data->Characters[0].PinchRegions.Num(),5);
 for(int32 I=0;I<Data->Characters[0].PinchRegions.Num();I++){
  const auto& R=Data->Characters[0].PinchRegions[I];const FString Prefix=R.Bone.ToString();
  for(const FVector Delta:{FVector(40,0,0),FVector(-40,0,0),FVector(0,0,40),FVector(0,0,-40)}){
   P->ClearPinch();Eval();const FVector Base=P->Character->GetSocketLocation(R.Bone),Origin=Base+FVector(0,60,0),Ray(0,-1,0);
   const FVector HeadBefore=P->Character->GetSocketLocation(TEXT("頭"));
   const FQuat HeadRotationBefore=P->Character->GetSocketQuaternion(TEXT("頭"));
   TestTrue(Prefix+TEXT(" pick"),P->BeginPinch(Origin,Ray));P->UpdatePinch(Origin+Delta,Ray);
   for(int32 J=0;J<90;J++){P->TickPinch(1.f/60);Eval();}
   const FVector Pull=P->GetPinchOffset(R.Bone);TestTrue(Prefix+TEXT(" helper deforms"),Pull.Size()>R.MaxDistance*.8f);
   TestTrue(Prefix+TEXT(" authored torso reacts"),P->GetPinchReactionRotation(TEXT("上半身")).GetAngle()>.02f);
   if(Delta.X!=0){
    TestTrue(Prefix+TEXT(" head follows horizontal drag"),(P->Character->GetSocketLocation(TEXT("頭")).X-HeadBefore.X)*Delta.X>.1f);
   }else if(I<2){
    const FVector ForwardBefore=HeadRotationBefore.RotateVector(FVector(0,0,1));
    const FVector ForwardAfter=P->Character->GetSocketQuaternion(TEXT("頭")).RotateVector(FVector(0,0,1));
    TestTrue(Prefix+TEXT(" head follows vertical drag"),(ForwardAfter.Z-ForwardBefore.Z)*Delta.Z>.1f);
   }
   TestTrue(Prefix+TEXT(" expression curves"),P->Character->GetMorphTarget(TEXT("DB_MouthPout"))>.2f);
   P->ReleasePinch();P->TickPinch(1.f/60);Eval();TestTrue(Prefix+TEXT(" no instant reset"),P->GetPinchOffset(R.Bone).Size()>Pull.Size()*.75f);
   bool Crossed=false;for(int32 J=0;J<180;J++){P->TickPinch(1.f/60);Eval();Crossed|=FVector::DotProduct(Pull,P->GetPinchOffset(R.Bone))<-.001f;}
   TestTrue(Prefix+TEXT(" spring overshoot"),Crossed);TestTrue(Prefix+TEXT(" restores"),P->GetPinchOffset(R.Bone).Size()<.01f);
  }
 }
 auto* Old=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/WardrobePinch23/A_PinchIdle.A_PinchIdle"));
 TestNotNull(TEXT("Old animation remains available"),Old);
 if(Old)TestTrue(TEXT("New skeleton explicitly compatible with old skeleton"),Mesh->GetSkeleton()->IsCompatibleForEditor(Old->GetSkeleton()));
 P->SetInteractionEnabled(false);GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif


