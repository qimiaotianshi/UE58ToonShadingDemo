#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeReactionTest,"DreamBloom.Pinch.ReactionDirectionStrengthAndRestore",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeReactionTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobePinch23/SK_DaniyaPinch.SK_DaniyaPinch"));
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/WardrobePinch23/AB_DaniyaPinch.AB_DaniyaPinch"));
 auto* Data=LoadObject<UWardrobeCharacterData>(nullptr,TEXT("/Game/WardrobePinch23/DA_PinchCharacters.DA_PinchCharacters"));
 if(!TestNotNull(TEXT("Reaction mesh"),Mesh)||!TestNotNull(TEXT("Reaction AnimBP"),BP)||!TestNotNull(TEXT("Reaction DA"),Data))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* P=W->SpawnActor<AWardrobeShowcase>();P->Character->SetSkeletalMesh(Mesh);P->Character->SetAnimInstanceClass(BP->GeneratedClass);P->InteractionData=Data;P->InteractionProfileIndex=0;P->SetInteractionEnabled(true);
 auto Eval=[&](){P->Character->TickAnimation(1.f/60,false);P->Character->RefreshBoneTransforms();P->Character->HandleExistingParallelEvaluationTask(true,true);};Eval();
 // Keep numerical assertions deterministic while preserving the user's live DA configuration.
 auto* TestData=DuplicateObject<UWardrobeCharacterData>(Data,GetTransientPackage());
 TestData->Characters[0].PinchRegions[0].DragSensitivity=.25f;P->InteractionData=TestData;
 const auto& R=TestData->Characters[0].PinchRegions[0];const FVector Base=P->Character->GetSocketLocation(R.Bone),Origin=Base+FVector(0,50,0),Ray(0,-1,0);
 const FQuat HeadBase=P->Character->GetSocketQuaternion(TEXT("頭")),BodyBase=P->Character->GetSocketQuaternion(TEXT("上半身"));
 const FVector HeadPosition=P->Character->GetSocketLocation(TEXT("頭"));
 auto Pull=[&](FVector Delta){TestTrue(TEXT("Pick cheek"),P->BeginPinch(Origin,Ray));P->UpdatePinch(Origin+Delta,Ray);for(int32 I=0;I<90;I++){P->TickPinch(1.f/60);Eval();}};
 auto Restore=[&](){const FQuat Before=P->GetPinchReactionRotation(TEXT("頭"));const FVector OffsetBefore=P->GetPinchOffset(R.Bone);const float EyeBefore=P->Character->GetMorphTarget(TEXT("DB_EyeClose_L"));P->ReleasePinch();P->ReleasePinch();
  TestTrue(TEXT("Mouse release does not instantly reset reaction"),Before.AngularDistance(P->GetPinchReactionRotation(TEXT("頭")))<.001f);
  P->TickPinch(1.f/60);Eval();
  if(EyeBefore>.4f){TestTrue(TEXT("Expression still present one frame after release"),P->Character->GetMorphTarget(TEXT("DB_EyeClose_L"))>.3f);TestTrue(TEXT("Cheek retains deformation one frame after release"),P->GetPinchOffset(R.Bone).Size()>OffsetBefore.Size()*.8f);}
  bool Crossed=false;for(int32 I=0;I<120;I++){P->TickPinch(1.f/60);Eval();Crossed|=FVector::DotProduct(OffsetBefore,P->GetPinchOffset(R.Bone))<-.001f;}
  if(OffsetBefore.Size()>1)TestTrue(TEXT("Released cheek has spring overshoot"),Crossed);
  for(int32 I=0;I<180;I++){P->TickPinch(1.f/60);Eval();}};
 Pull(FVector(1,0,0));TestTrue(TEXT("Small mouse displacement only creates small deformation"),P->GetPinchOffset(R.Bone).Size()>.2f&&P->GetPinchOffset(R.Bone).Size()<.3f);TestTrue(TEXT("Small mouse displacement has gentle expression"),P->Character->GetMorphTarget(TEXT("DB_EyeClose_L"))<.1f);Restore();
 Pull(FVector(2.4,0,0));const FQuat Weak=P->GetPinchReactionRotation(TEXT("頭"));const float WeakEye=P->Character->GetMorphTarget(FName(TEXT("DB_EyeClose_L")));Restore();
 Pull(FVector(40,0,0));const FQuat Strong=P->GetPinchReactionRotation(TEXT("頭"));
 TestTrue(TEXT("Stronger pull increases authored head reaction"),Strong.GetAngle()>Weak.GetAngle()+.05f);
 TestTrue(TEXT("Pinched eye closes by force"),P->Character->GetMorphTarget(FName(TEXT("DB_EyeClose_L")))>WeakEye+.4f);
 TestTrue(TEXT("Mouth curve from clip drives expression"),P->Character->GetMorphTarget(TEXT("DB_MouthPout"))>.5f);
 TestEqual(TEXT("Other eye remains neutral"),P->Character->GetMorphTarget(TEXT("DB_EyeClose_R")),0.f);
 TestTrue(TEXT("Head anim node evaluates reaction"),HeadBase.AngularDistance(P->Character->GetSocketQuaternion(TEXT("頭")))>.15f);
 TestTrue(TEXT("Head follows positive mouse direction"),P->Character->GetSocketLocation(TEXT("頭")).X>HeadPosition.X+.1);
 TestTrue(TEXT("Body anim node evaluates reaction"),BodyBase.AngularDistance(P->Character->GetSocketQuaternion(TEXT("上半身")))>.02f);Restore();
 TestTrue(TEXT("Head restores"),HeadBase.AngularDistance(P->Character->GetSocketQuaternion(TEXT("頭")))<.001f);
 TestEqual(TEXT("Eye restores"),P->Character->GetMorphTarget(FName(TEXT("DB_EyeClose_L"))),0.f);
 Pull(FVector(-40,0,0));TestTrue(TEXT("Opposite pull retains animation reaction"),P->GetPinchReactionRotation(TEXT("頭")).GetAngle()>.1f);TestTrue(TEXT("Opposite pull deforms cheek"),P->GetPinchOffset(R.Bone).Size()>1.f);TestTrue(TEXT("Head follows negative mouse direction"),P->Character->GetSocketLocation(TEXT("頭")).X<HeadPosition.X-.1);TestTrue(TEXT("Opposite drag switches body pose"),Strong.AngularDistance(P->GetPinchReactionRotation(TEXT("頭")))>.2f);Restore();
 Pull(FVector(0,0,40));const FQuat Up=P->GetPinchReactionRotation(TEXT("頭"));TestTrue(TEXT("Upward drag retains animation reaction"),Up.GetAngle()>.1f);TestTrue(TEXT("Upward drag tilts head upward"),P->Character->GetSocketQuaternion(TEXT("頭")).RotateVector(FVector(0,0,1)).Z>HeadBase.RotateVector(FVector(0,0,1)).Z+.1);Restore();
 Pull(FVector(0,0,-40));TestTrue(TEXT("Downward drag switches body pose"),Up.AngularDistance(P->GetPinchReactionRotation(TEXT("頭")))>.2f);TestTrue(TEXT("Downward drag tilts head downward"),P->Character->GetSocketQuaternion(TEXT("頭")).RotateVector(FVector(0,0,1)).Z<HeadBase.RotateVector(FVector(0,0,1)).Z-.1);Restore();
 Pull(FVector(40,0,0));P->UpdatePinch(Origin+FVector(.3,0,0),Ray);for(int32 I=0;I<90;I++){P->TickPinch(1.f/60);Eval();}TestTrue(TEXT("Dragging back lowers expression"),P->Character->GetMorphTarget(TEXT("DB_EyeClose_L"))<.2f);
 P->SetInteractionEnabled(false);Eval();TestTrue(TEXT("Disable resets reaction"),P->GetPinchReactionRotation(TEXT("頭")).Equals(FQuat::Identity,.001f));
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
