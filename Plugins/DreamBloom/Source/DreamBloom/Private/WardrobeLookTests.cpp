#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeLookTest,"DreamBloom.Look.IdleMouseHeadNeckEyesAndPause",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeLookTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobePinch23/SK_DaniyaPinch.SK_DaniyaPinch"));
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/WardrobePinch23/AB_DaniyaCursorLookV2.AB_DaniyaCursorLookV2"));
 auto* Data=LoadObject<UWardrobeCharacterData>(nullptr,TEXT("/Game/WardrobePinch23/DA_PinchCharacters.DA_PinchCharacters"));
 if(!TestNotNull(TEXT("Look blueprint"),BP)||!TestNotNull(TEXT("Look DA"),Data)||!TestNotNull(TEXT("Look mesh"),Mesh))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);auto* P=W->SpawnActor<AWardrobeShowcase>();
 P->Character->SetSkeletalMesh(Mesh);P->Character->SetAnimInstanceClass(BP->GeneratedClass);P->InteractionData=Data;P->InteractionProfileIndex=0;P->SetInteractionEnabled(true);
 auto Eval=[&](){P->Character->TickAnimation(1.f/60,false);P->Character->RefreshBoneTransforms();P->Character->HandleExistingParallelEvaluationTask(true,true);};Eval();
 const FQuat Head=P->Character->GetSocketQuaternion(TEXT("頭")),Neck=P->Character->GetSocketQuaternion(TEXT("首")),Eye=P->Character->GetSocketQuaternion(TEXT("目_L"));const FVector Anchor=P->Character->GetSocketLocation(TEXT("頭")),Origin=Anchor+FVector(0,100,0),RightRay=FVector(.3,-1,.15).GetSafeNormal();
 P->UpdateCursorLook(.016f,Origin,RightRay,true);TestTrue(TEXT("Idle delay avoids immediate twitch"),P->GetCursorLookRotation(TEXT("頭")).Equals(FQuat::Identity,.001f));
 for(int I=0;I<180;I++){P->UpdateCursorLook(1.f/60,Origin,RightRay,true);Eval();}
 AddInfo(FString::Printf(TEXT("Gaze head %.4f neck %.4f eye %.4f eyeLocal %.4f"),Head.AngularDistance(P->Character->GetSocketQuaternion(TEXT("頭"))),Neck.AngularDistance(P->Character->GetSocketQuaternion(TEXT("首"))),Eye.AngularDistance(P->Character->GetSocketQuaternion(TEXT("目_L"))),P->GetCursorLookRotation(TEXT("目_L")).GetAngle()));
 TestTrue(TEXT("Head looks at cursor"),Head.AngularDistance(P->Character->GetSocketQuaternion(TEXT("頭")))>.08f);TestTrue(TEXT("Neck participates"),Neck.AngularDistance(P->Character->GetSocketQuaternion(TEXT("首")))>.03f);TestTrue(TEXT("Eyes participate"),Eye.AngularDistance(P->Character->GetSocketQuaternion(TEXT("目_L")))>.08f);
 const FQuat Right=P->GetCursorLookRotation(TEXT("頭"));for(int I=0;I<180;I++){P->UpdateCursorLook(1.f/60,Origin,FVector(-.3,-1,-.15).GetSafeNormal(),true);Eval();}
 TestTrue(TEXT("Opposite cursor changes gaze"),Right.AngularDistance(P->GetCursorLookRotation(TEXT("頭")))>.2f);
 P->SetInteractionEnabled(false);for(int I=0;I<180;I++){P->UpdateCursorLook(1.f/60,Origin,RightRay,true);Eval();}TestTrue(TEXT("Station disabled restores gaze"),P->GetCursorLookRotation(TEXT("頭")).Equals(FQuat::Identity,.001f));
 P->SetInteractionEnabled(true);for(int I=0;I<180;I++)P->UpdateCursorLook(1.f/60,Origin,RightRay,true);const auto& R=Data->Characters[0].PinchRegions[0];const FVector Cheek=P->Character->GetSocketLocation(R.Bone);
 TestTrue(TEXT("Pinch starts"),P->BeginPinch(Cheek+FVector(0,50,0),FVector(0,-1,0)));for(int I=0;I<180;I++)P->UpdateCursorLook(1.f/60,Origin,RightRay,true);TestTrue(TEXT("Pinch pauses idle look"),P->GetCursorLookRotation(TEXT("頭")).Equals(FQuat::Identity,.001f));P->UpdatePinch(Cheek+FVector(20,50,0),FVector(0,-1,0));for(int I=0;I<90;I++)P->TickPinch(1.f/60);P->ReleasePinch();
 for(int I=0;I<36;I++){P->TickPinch(1.f/60);P->UpdateCursorLook(1.f/60,Origin,RightRay,true);}
 TestTrue(TEXT("Gaze blends in before body return ends without idle pause"),P->GetCursorLookRotation(TEXT("頭")).GetAngle()>.02f);
 for(int I=0;I<180;I++){P->TickPinch(1.f/60);P->UpdateCursorLook(1.f/60,Origin,RightRay,true);}TestTrue(TEXT("Idle resumes after release"),P->GetCursorLookRotation(TEXT("頭")).GetAngle()>.08f);
 for(int I=0;I<180;I++)P->UpdateCursorLook(1.f/60,Origin,RightRay,false);TestTrue(TEXT("Missing cursor restores gaze"),P->GetCursorLookRotation(TEXT("頭")).Equals(FQuat::Identity,.001f));
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
