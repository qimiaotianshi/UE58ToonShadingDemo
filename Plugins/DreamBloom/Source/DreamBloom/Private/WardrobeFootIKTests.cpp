#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeShowcase.h"
#include "WardrobeFootIKNode.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "LevelSequence.h"
#include "LevelSequencePlayer.h"
#include "LevelSequenceActor.h"
#include "MovieScene.h"
#include "MovieSceneBindingOverrides.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeFootIKTest,"DreamBloom.Ground.FeetPelvisAndSlope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeFootIKTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/SK_DaniyaFullBodyPinch.SK_DaniyaFullBodyPinch"));
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/AB_FullBodySequencePinch.AB_FullBodySequencePinch"));
 if(!TestNotNull(TEXT("Demo mesh"),Mesh)||!TestNotNull(TEXT("Demo blueprint"),BP))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeShowcase>();A->Character->SetSkeletalMesh(Mesh);A->Character->SetAnimInstanceClass(BP->GeneratedClass);
 auto Eval=[&](){A->Character->TickAnimation(1.f/60,false);A->Character->RefreshBoneTransforms();A->Character->HandleExistingParallelEvaluationTask(true,true);};Eval();
 FAnimNode_WardrobeFootIK* IK=nullptr;
 for(TFieldIterator<FStructProperty> P(A->Character->GetAnimInstance()->GetClass());P;++P)if(P->Struct==FAnimNode_WardrobeFootIK::StaticStruct())IK=P->ContainerPtrToValuePtr<FAnimNode_WardrobeFootIK>(A->Character->GetAnimInstance());
 if(!TestNotNull(TEXT("Compiled foot IK node"),IK)){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 IK->bAutoSoleContact=false;
 auto* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/CheekCare49/A_Stand01Default.A_Stand01Default"));
 if(TestNotNull(TEXT("Authored default pose"),Idle)){
  auto* Montage=A->Character->GetAnimInstance()->PlaySlotAnimationAsDynamicMontage(Idle,TEXT("DefaultSlot"),0,0,1,100);
  TestNotNull(TEXT("Authored animation reaches source slot"),Montage);for(int I=0;I<30;I++)Eval();A->Character->GetAnimInstance()->Montage_Pause(Montage);
 }
 AddInfo(FString::Printf(TEXT("IK configuration: pelvis=%s feet=%s,%s soleHeight=%.3f cm"),*IK->Pelvis.BoneName.ToString(),*IK->LeftFoot.BoneName.ToString(),*IK->RightFoot.BoneName.ToString(),IK->SoleHeight));
 IK->BlendTime=.1f;IK->MaxPlantHeight=50.f;IK->MaxPelvisOffset=20.f;
 auto Pos=[&](bool Left){return A->Character->GetSocketLocation(Left?IK->LeftFoot.BoneName:IK->RightFoot.BoneName);};
 const FVector L=Pos(true),R=Pos(false);const FQuat StartRot=A->Character->GetSocketQuaternion(IK->LeftFoot.BoneName);
 const float Scale=A->Character->GetComponentScale().GetAbsMax(),Sole=IK->SoleHeight*Scale;
 const float BaseGround=FMath::Min(L.Z,R.Z)-Sole-8.f*Scale;
 auto Box=[&](FVector Center,FVector Extent){auto* Actor=W->SpawnActor<AActor>();auto* C=NewObject<UBoxComponent>(Actor);Actor->SetRootComponent(C);C->SetBoxExtent(Extent);C->SetCollisionObjectType(ECC_WorldStatic);C->SetCollisionResponseToAllChannels(ECR_Block);C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);C->RegisterComponent();Actor->SetActorLocation(Center);return C;};
 auto* Floor=Box(FVector((L.X+R.X)*.5,(L.Y+R.Y)*.5,BaseGround-2),FVector(1000,1000,2));
 for(int I=0;I<90;I++)Eval();
 TestTrue(TEXT("Traces detect collision floor"),IK->Ground[0].bHit&&IK->Ground[1].bHit);
 TestTrue(TEXT("Left sole reaches ground"),FMath::Abs(Pos(true).Z-BaseGround-Sole)<.25f);
 TestTrue(TEXT("Right sole reaches ground"),FMath::Abs(Pos(false).Z-BaseGround-Sole)<.25f);
 TestTrue(TEXT("Pelvis compensates downward"),IK->PelvisOffset<-5.f);
 {
  const auto& LOD=Mesh->GetResourceForRendering()->LODRenderData[0];auto* Weights=A->Character->GetSkinWeightBuffer(0);TArray<FMatrix44f> Matrices;A->Character->CacheRefToLocalMatrices(Matrices);
  float Min[2]={BIG_NUMBER,BIG_NUMBER};const FVector Feet[2]={Pos(true),Pos(false)};
  for(uint32 V=0;V<LOD.GetNumVertices();V++){
   const FVector P=A->Character->GetComponentTransform().TransformPosition(FVector(USkeletalMeshComponent::GetSkinnedVertexPosition(A->Character,V,LOD,*Weights,Matrices)));
   const int Side=FVector::DistSquaredXY(P,Feet[0])<FVector::DistSquaredXY(P,Feet[1])?0:1;
   Min[Side]=FMath::Min(Min[Side],float(P.Z));
  }
  AddInfo(FString::Printf(TEXT("SHOE_GEOMETRY: ankle=%.4f,%.4f bottom=%.4f,%.4f ground=%.4f requiredSole=%.4f,%.4f"),Feet[0].Z,Feet[1].Z,Min[0],Min[1],BaseGround,(Feet[0].Z-Min[0])/Scale,(Feet[1].Z-Min[1])/Scale));
  TestTrue(TEXT("Actual left shoe mesh stays above floor"),Min[0]>=BaseGround-.15f);
  TestTrue(TEXT("Actual right shoe mesh stays above floor"),Min[1]>=BaseGround-.15f);
 }
 auto* Step=Box(FVector(L.X,L.Y,BaseGround+2.5),FVector(4.f*Scale,4.f*Scale,2.5));
 for(int I=0;I<90;I++)Eval();
 TestTrue(TEXT("Feet adapt independently to step"),Pos(true).Z>Pos(false).Z+3.f);
 TestTrue(TEXT("Step foot stays over surface"),Pos(true).Z>BaseGround+5.f+Sole-.4f);
 Step->SetCollisionEnabled(ECollisionEnabled::NoCollision);Floor->SetWorldRotation(FRotator(10,0,0));
 for(int I=0;I<90;I++)Eval();
 TestTrue(TEXT("Slope produces non-flat normal"),IK->Ground[0].Normal.Z<.995f);
 TestTrue(TEXT("Foot rotates with slope"),StartRot.AngularDistance(A->Character->GetSocketQuaternion(IK->LeftFoot.BoneName))>.08f);
 const FVector N=Floor->GetUpVector(),Surface=Floor->GetComponentLocation()+N*2;
 const FVector Foot=Pos(true);const float Height=Surface.Z-(N.X*(Foot.X-Surface.X)+N.Y*(Foot.Y-Surface.Y))/N.Z;
 TestTrue(TEXT("Slope sole stays above plane"),Foot.Z>=Height+Sole-.5f);
 // Ground changes blend instead of snapping; missing ground/disable returns to the authored pose.
 const float Previous=IK->PelvisOffset;Floor->SetWorldLocation(Floor->GetComponentLocation()+FVector(0,0,4));Eval();
 TestTrue(TEXT("Ground height change interpolates"),FMath::Abs(IK->PelvisOffset-Previous)<3.f);
 IK->bEnabled=false;for(int I=0;I<120;I++)Eval();TestTrue(TEXT("Disable restores authored foot pose"),Pos(true).Equals(L,.1f)&&Pos(false).Equals(R,.1f));
 TestTrue(TEXT("Disable restores pelvis"),FMath::Abs(IK->PelvisOffset)<.01f);
 GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeFootSequenceTest,"DreamBloom.Ground.RealSequenceAndActorZ",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeFootSequenceTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/SK_DaniyaFullBodyPinch.SK_DaniyaFullBodyPinch"));
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/AB_FullBodySequencePinch.AB_FullBodySequencePinch"));
 auto* Seq=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/CheekCare49/LS_Stand01Default.LS_Stand01Default"));
 if(!TestNotNull(TEXT("Mesh"),Mesh)||!TestNotNull(TEXT("BP"),BP)||!TestNotNull(TEXT("Real default Sequence"),Seq))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeShowcase>();A->Character->SetSkeletalMesh(Mesh);A->Character->SetAnimInstanceClass(BP->GeneratedClass);
 auto Eval=[&](){A->Character->TickAnimation(1.f/60,false);A->Character->RefreshBoneTransforms();A->Character->HandleExistingParallelEvaluationTask(true,true);};Eval();
 FAnimNode_WardrobeFootIK* IK=nullptr;for(TFieldIterator<FStructProperty> P(A->Character->GetAnimInstance()->GetClass());P;++P)if(P->Struct==FAnimNode_WardrobeFootIK::StaticStruct())IK=P->ContainerPtrToValuePtr<FAnimNode_WardrobeFootIK>(A->Character->GetAnimInstance());
 if(!TestNotNull(TEXT("IK instance"),IK)){GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return false;}
 TestTrue(TEXT("Baked shoe contact data"),IK->LeftSolePoints.Num()>3&&IK->RightSolePoints.Num()>3);IK->MaxPelvisOffset=60;IK->MaxPlantHeight=18;
 auto* GroundActor=W->SpawnActor<AActor>();auto* Floor=NewObject<UBoxComponent>(GroundActor);GroundActor->SetRootComponent(Floor);Floor->SetBoxExtent(FVector(1000,1000,2));Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Floor->RegisterComponent();GroundActor->SetActorLocation(FVector(0,0,-2));
 FMovieSceneSequencePlaybackSettings Settings;Settings.LoopCount.Value=-1;ALevelSequenceActor* SequenceActor=nullptr;
 auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,Settings,SequenceActor);
 for(const auto& Binding:Seq->GetMovieScene()->GetBindings()){
  const FMovieSceneObjectBindingID ID(UE::MovieScene::FRelativeObjectBindingID(Binding.GetObjectGuid()));
  SequenceActor->BindingOverrides->SetBinding(ID,{Binding.GetName()==TEXT("Daniya")?static_cast<UObject*>(A->Character):static_cast<UObject*>(A)},false);
 }
 Player->Play();Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(.5,EUpdatePositionMethod::Jump));
 auto Check=[&](float Z){
  A->SetActorLocation(FVector(0,0,Z));for(int I=0;I<90;I++){Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(.5,EUpdatePositionMethod::Jump));Eval();}
  TestTrue(FString::Printf(TEXT("Z %.0f retains plant weight"),Z),IK->LastPlant[0]>.99f&&IK->LastPlant[1]>.99f);
  const auto& LOD=Mesh->GetResourceForRendering()->LODRenderData[0];auto* Weights=A->Character->GetSkinWeightBuffer(0);TArray<FMatrix44f> Matrices;A->Character->CacheRefToLocalMatrices(Matrices);
  const FVector Feet[2]={A->Character->GetSocketLocation(IK->LeftFoot.BoneName),A->Character->GetSocketLocation(IK->RightFoot.BoneName)};float Min[2]={BIG_NUMBER,BIG_NUMBER};
  const FVector Normal=Floor->GetUpVector(),Surface=Floor->GetComponentLocation()+Normal*2;
  for(uint32 V=0;V<LOD.GetNumVertices();V++){
   const FVector P=A->Character->GetComponentTransform().TransformPosition(FVector(USkeletalMeshComponent::GetSkinnedVertexPosition(A->Character,V,LOD,*Weights,Matrices)));
   const int Side=FVector::DistSquaredXY(P,Feet[0])<FVector::DistSquaredXY(P,Feet[1])?0:1;Min[Side]=FMath::Min(Min[Side],float(FVector::DotProduct(P-Surface,Normal)));
  }
  AddInfo(FString::Printf(TEXT("REAL_SEQUENCE_GROUND Z=%.1f soles=%.3f,%.3f pelvis=%.3f weights=%.2f,%.2f"),Z,Min[0],Min[1],IK->PelvisOffset,IK->LastPlant[0],IK->LastPlant[1]));
  for(int Side=0;Side<2;Side++)TestTrue(FString::Printf(TEXT("Real Sequence Z %.0f shoe %d contacts floor"),Z,Side),Min[Side]>=-.3f&&Min[Side]<1.f);
 };
 for(float Z:{0.f,15.f,30.f,-15.f,-30.f})Check(Z);
 Floor->SetWorldRotation(FRotator(12,0,0));Check(0.f);
 Player->Stop();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
