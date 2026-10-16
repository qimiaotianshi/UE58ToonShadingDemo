#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeFootIKNode.h"
#include "WardrobeLimbNode.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeLimbTest,"DreamBloom.Limbs.RealSequenceDragReactionAndGround",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeLimbTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/SK_DaniyaFullBodyPinch.SK_DaniyaFullBodyPinch"));
 auto* BP=LoadObject<UAnimBlueprint>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/AB_FullBodySequencePinch.AB_FullBodySequencePinch"));
 auto* Original=LoadObject<UWardrobeCharacterData>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/DA_FullBodyCharacters.DA_FullBodyCharacters"));
 auto* Seq=LoadObject<ULevelSequence>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/CheekCare49/LS_Stand01Default.LS_Stand01Default"));
 if(!TestNotNull(TEXT("Mesh"),Mesh)||!TestNotNull(TEXT("BP"),BP)||!TestNotNull(TEXT("DA"),Original)||!TestNotNull(TEXT("Actual default Sequence"),Seq))return false;
 auto* Data=DuplicateObject<UWardrobeCharacterData>(Original,GetTransientPackage());Data->Characters[0].DefaultSequence=nullptr;Data->Characters[0].CursorLook.bEnabled=false;
 if(!TestEqual(TEXT("Four demo limbs"),Data->Characters[0].LimbDrags.Num(),4))return false;
 auto* W=UWorld::CreateWorld(EWorldType::Game,false);GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(W);
 auto* A=W->SpawnActor<AWardrobeShowcase>();A->Character->SetSkeletalMesh(Mesh);A->Character->SetAnimInstanceClass(BP->GeneratedClass);A->InteractionData=Data;A->InteractionProfileIndex=0;A->SetInteractionEnabled(true);
 auto* FloorActor=W->SpawnActor<AActor>();auto* Floor=NewObject<UBoxComponent>(FloorActor);FloorActor->SetRootComponent(Floor);Floor->SetBoxExtent(FVector(1000,1000,2));Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Floor->RegisterComponent();FloorActor->SetActorLocation(FVector(0,0,-2));
 FMovieSceneSequencePlaybackSettings Settings;Settings.LoopCount.Value=-1;ALevelSequenceActor* SeqActor=nullptr;auto* Player=ULevelSequencePlayer::CreateLevelSequencePlayer(W,Seq,Settings,SeqActor);
 for(const auto& Binding:static_cast<const UMovieScene*>(Seq->GetMovieScene())->GetBindings())SeqActor->BindingOverrides->SetBinding(FMovieSceneObjectBindingID(UE::MovieScene::FRelativeObjectBindingID(Binding.GetObjectGuid())),{Binding.GetName()==TEXT("Daniya")?static_cast<UObject*>(A->Character):static_cast<UObject*>(A)},false);
 Player->Play();
 auto Eval=[&](){Player->SetPlaybackPosition(FMovieSceneSequencePlaybackParams(.5,EUpdatePositionMethod::Jump));A->Character->TickAnimation(1.f/60,false);A->Character->RefreshBoneTransforms();A->Character->HandleExistingParallelEvaluationTask(true,true);};
 auto Steps=[&](int N){for(int I=0;I<N;I++){A->TickLimbDrag(1.f/60);Eval();}};Steps(90);
 FAnimNode_WardrobeFootIK* Ground=nullptr;FAnimNode_WardrobeLimbIK* IK=nullptr;
 for(TFieldIterator<FStructProperty> P(A->Character->GetAnimInstance()->GetClass());P;++P){if(P->Struct==FAnimNode_WardrobeFootIK::StaticStruct())Ground=P->ContainerPtrToValuePtr<FAnimNode_WardrobeFootIK>(A->Character->GetAnimInstance());if(P->Struct==FAnimNode_WardrobeLimbIK::StaticStruct())IK=P->ContainerPtrToValuePtr<FAnimNode_WardrobeLimbIK>(A->Character->GetAnimInstance());}
 TestNotNull(TEXT("Ground node"),Ground);TestNotNull(TEXT("Limb solver node"),IK);
 auto VertexCentroid=[&](FName Bone){
  const auto& Ref=Mesh->GetRefSkeleton();const int End=Ref.FindBoneIndex(Bone);const auto& LOD=Mesh->GetResourceForRendering()->LODRenderData[0];const auto& Weights=*A->Character->GetSkinWeightBuffer(0);TArray<FMatrix44f> Matrices;A->Character->CacheRefToLocalMatrices(Matrices);FVector Sum=FVector::ZeroVector;int Count=0;
  for(const auto& Section:LOD.RenderSections)for(uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;V++){
   uint32 Total=0;for(uint32 J=0;J<Weights.GetMaxBoneInfluences();J++){const int Map=Weights.GetBoneIndex(V,J);if(!Section.BoneMap.IsValidIndex(Map))continue;int B=Section.BoneMap[Map];while(B!=INDEX_NONE){if(B==End){Total+=Weights.GetBoneWeight(V,J);break;}B=Ref.GetParentIndex(B);}}
   if(Total>32767){Sum+=A->Character->GetComponentTransform().TransformPosition(FVector(USkeletalMeshComponent::GetSkinnedVertexPosition(A->Character,V,LOD,Weights,Matrices)));Count++;}
  }TestTrue(TEXT("Deform endpoint owns skinned vertices"),Count>10);return Count?Sum/Count:FVector::ZeroVector;
 };
 for(int I=0;I<4;I++){
  A->ClearLimbDrag();Steps(90);auto& R=Data->Characters[0].LimbDrags[I];const FString Label=R.Label.ToString();
  const auto& Ref=Mesh->GetRefSkeleton();for(FName N:{R.UpperBone,R.JointBone,R.EndBone}){const int B=Ref.FindBoneIndex(N);AddInfo(FString::Printf(TEXT("CHAIN %s parent=%s"),*N.ToString(),B==INDEX_NONE?TEXT("MISSING"):*Ref.GetBoneName(Ref.GetParentIndex(B)).ToString()));}
  const FVector Base=A->Character->GetSocketLocation(R.EndBone),Upper=A->Character->GetSocketLocation(R.UpperBone),Ray(0,-1,0),Origin=Base+FVector(0,100,0);
  const FVector MeshBefore=VertexCentroid(R.EndBone);
  TestFalse(Label+TEXT(" misses outside hit sphere"),A->BeginLimbDrag(Origin+FVector(100,0,0),Ray));
  TestTrue(Label+TEXT(" can grab"),A->BeginLimbDrag(Origin,Ray));Eval();
  TestTrue(Label+TEXT(" press does not snap endpoint"),FVector::Distance(A->Character->GetSocketLocation(R.EndBone),Base)<.2f);
  const FVector Pull=(Upper-Base)*.15f+FVector(0,0,10);
  A->UpdateLimbDrag(Origin+Pull/R.DragSensitivity,Ray);Steps(90);
  const auto S=A->GetLimbSnapshot();const FVector Actual=A->Character->GetSocketLocation(R.EndBone);const FVector MeshAfter=VertexCentroid(R.EndBone);
  AddInfo(FString::Printf(TEXT("DRAG %s start=%s target=%s actual=%s meshDelta=%s"),*Label,*Base.ToString(),*S.TargetWorld.ToString(),*Actual.ToString(),*(MeshAfter-MeshBefore).ToString()));
  TestTrue(Label+TEXT(" IK follows reachable cursor target"),FVector::Distance(Actual,S.TargetWorld)<1.f);
  TestTrue(Label+TEXT(" actual skin follows drag"),FVector::DotProduct(MeshAfter-MeshBefore,Actual-Base)>5.f);
  if(R.bFoot&&Ground){const int Side=I-2;TestTrue(Label+TEXT(" dragged foot ground suppressed"),Ground->DragSuppression[Side]>.99f);TestTrue(Label+TEXT(" opposite foot stays planted"),Ground->LastPlant[1-Side]>.99f);}
  A->UpdateLimbDrag(Origin+FVector(1000,0,1000),Ray);Steps(90);TestTrue(Label+TEXT(" maximum drag distance clamps"),(A->GetLimbSnapshot().TargetWorld-Base).Size()<=R.MaxDistance+.01f);
  const FVector BeforeRelease=A->Character->GetSocketLocation(R.EndBone);A->ReleaseLimbDrag();Eval();TestFalse(Label+TEXT(" release ends mouse gesture"),A->IsLimbDragging());TestTrue(Label+TEXT(" release retains last pose"),FVector::Distance(BeforeRelease,A->Character->GetSocketLocation(R.EndBone))<.05f);
  Steps(1);TestTrue(Label+TEXT(" first release frame remains smooth"),FVector::Distance(BeforeRelease,A->Character->GetSocketLocation(R.EndBone))<1.f);
  Steps(120);TestFalse(Label+TEXT(" recovery completes"),A->HasLimbPose());TestTrue(Label+TEXT(" endpoint returns to default Sequence"),FVector::Distance(A->Character->GetSocketLocation(R.EndBone),Base)<.3f);
  TestTrue(Label+TEXT(" can grab again after recovery"),A->BeginLimbDrag(Origin,Ray));A->ReleaseLimbDrag();Steps(4);const FVector Regrab=A->Character->GetSocketLocation(R.EndBone);
  TestTrue(Label+TEXT(" can regrab while recovering"),A->BeginLimbDrag(Regrab+FVector(0,100,0),Ray));Eval();TestTrue(Label+TEXT(" regrab preserves endpoint"),FVector::Distance(Regrab,A->Character->GetSocketLocation(R.EndBone))<.3f);A->ClearLimbDrag();Steps(90);
 }
 // Test left/right animation direction on the actual running source pose.
 auto& R=Data->Characters[0].LimbDrags[0];Steps(90);const FVector Hand=A->Character->GetSocketLocation(R.EndBone),Origin=Hand+FVector(0,100,0),Head=A->Character->GetSocketLocation(TEXT("頭"));
 for(float Direction:{-1.f,1.f}){A->ClearLimbDrag();Steps(90);TestTrue(TEXT("Direction grab"),A->BeginLimbDrag(Origin,FVector(0,-1,0)));A->UpdateLimbDrag(Origin+FVector(Direction*20,0,0),FVector(0,-1,0));Steps(90);const FVector Delta=A->Character->GetSocketLocation(TEXT("頭"))-Head;AddInfo(FString::Printf(TEXT("BODY direction=%.0f headDelta=%s"),Direction,*Delta.ToString()));TestTrue(TEXT("Body follows horizontal mouse force"),Delta.X*Direction>.05f);}
 FQuat VerticalPoses[2];int VI=0;
 for(float Direction:{-1.f,1.f}){A->ClearLimbDrag();Steps(90);TestTrue(TEXT("Vertical grab"),A->BeginLimbDrag(Origin,FVector(0,-1,0)));A->UpdateLimbDrag(Origin+FVector(0,0,Direction*20),FVector(0,-1,0));Steps(90);VerticalPoses[VI++]=A->Character->GetSocketQuaternion(TEXT("上半身"));}
 TestTrue(TEXT("Up and down use distinct authored body poses"),VerticalPoses[0].AngularDistance(VerticalPoses[1])>.01f);
 A->SetInteractionEnabled(false);TestFalse(TEXT("Switching station clears held limb and recovery"),A->HasLimbPose());
 A->SetInteractionEnabled(true);Data->Characters[0].LimbDrags[0].JointBone=NAME_None;TestFalse(TEXT("Invalid chain cannot start a drag"),A->BeginLimbDrag(Origin,FVector(0,-1,0)));
 Player->Stop();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
