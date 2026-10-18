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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeLimbTest,"DreamBloom.Limbs.RealSequenceDragAndGround",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
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
 // Dragging affects the IK chain, not authored torso feedback clips.
 auto& R=Data->Characters[0].LimbDrags[0];Steps(90);
 const FVector Hand=A->Character->GetSocketLocation(R.EndBone),Origin=Hand+FVector(0,100,0);
 const FQuat Torso=A->Character->GetSocketQuaternion(TEXT("上半身"));
 for(float Direction:{-1.f,1.f}){
  A->ClearLimbDrag();Steps(90);TestTrue(TEXT("IK-only grab"),A->BeginLimbDrag(Origin,FVector(0,-1,0)));
  A->UpdateLimbDrag(Origin+FVector(Direction*20,0,0),FVector(0,-1,0));Steps(90);
  TestTrue(TEXT("Limb dragging leaves source torso animation intact"),Torso.AngularDistance(A->Character->GetSocketQuaternion(TEXT("上半身")))<.001f);
 }
 for(const TCHAR* Name:{TEXT("BodyLeft"),TEXT("BodyRight"),TEXT("BodyUp"),TEXT("BodyDown"),TEXT("ReactionStrength")})
  TestNull(FString(TEXT("Removed limb DA property "))+Name,FWardrobeLimbDrag::StaticStruct()->FindPropertyByName(Name));
 // Gaze remains active during IK dragging and recovery.
 auto& Look=Data->Characters[0].CursorLook;Look.bEnabled=true;Look.AnchorBone=TEXT("頭");Look.ForwardAxis=FVector(0,1,0);Look.UpAxis=FVector::UpVector;Look.IdleDelay=.8f;Look.FollowSpeed=4.f;
 Look.Bones.Reset();for(auto Name:{FName(TEXT("頭")),FName(TEXT("首")),FName(TEXT("目_L")),FName(TEXT("目_R"))}){FWardrobeLookBone Bone;Bone.Bone=Name;Bone.Weight=Name==TEXT("首")?.2f:.45f;Look.Bones.Add(Bone);}
 for(int I=0;I<4;I++){
  A->ClearLimbDrag();Steps(90);
  const FVector WristOrAnkle=A->Character->GetSocketLocation(Data->Characters[0].LimbDrags[I].EndBone);
  const FVector PickOrigin=WristOrAnkle+FVector(0,100,0),PickRay(0,-1,0);
  const FVector GazeOrigin=A->Character->GetSocketLocation(TEXT("頭"))+FVector(0,100,0),GazeRight=FVector(.3,-1,.15).GetSafeNormal(),GazeLeft=FVector(-.3,-1,-.15).GetSafeNormal();
  for(int N=0;N<180;N++)A->UpdateCursorLook(1.f/60,GazeOrigin,GazeRight,false);
  TestTrue(TEXT("Limb gaze grab starts"),A->BeginLimbDrag(PickOrigin,PickRay));
  A->UpdateLimbDrag(PickOrigin+FVector(10,0,8),PickRay);Steps(90);
  const FQuat BaseHead=A->Character->GetSocketQuaternion(TEXT("頭")),BaseNeck=A->Character->GetSocketQuaternion(TEXT("首")),BaseEye=A->Character->GetSocketQuaternion(TEXT("目_L"));
  A->UpdateCursorLook(1.f/60,GazeOrigin,GazeRight,true);
  TestTrue(TEXT("Held limb bypasses idle gaze delay"),A->GetCursorLookRotation(TEXT("頭")).GetAngle()>.001f);
  for(int N=0;N<180;N++){A->UpdateCursorLook(1.f/60,GazeOrigin,GazeRight,true);Steps(1);}
  TestTrue(TEXT("Head follows mouse during limb drag"),BaseHead.AngularDistance(A->Character->GetSocketQuaternion(TEXT("頭")))>.03f);
  TestTrue(TEXT("Neck follows mouse during limb drag"),BaseNeck.AngularDistance(A->Character->GetSocketQuaternion(TEXT("首")))>.01f);
  TestTrue(TEXT("Eye follows mouse during limb drag"),BaseEye.AngularDistance(A->Character->GetSocketQuaternion(TEXT("目_L")))>.03f);
  const FQuat RightLook=A->GetCursorLookRotation(TEXT("頭"));
  for(int N=0;N<180;N++){A->UpdateCursorLook(1.f/60,GazeOrigin,GazeLeft,true);Steps(1);}
  TestTrue(TEXT("Dragged limb allows gaze direction changes"),RightLook.AngularDistance(A->GetCursorLookRotation(TEXT("頭")))>.1f);
  TestTrue(TEXT("Gaze keeps limb IK active"),A->GetLimbSnapshot().Alpha>.99f);
  A->ReleaseLimbDrag();A->TickLimbDrag(1.f/60);A->UpdateCursorLook(1.f/60,GazeOrigin,GazeLeft,true);Eval();
  TestTrue(TEXT("Limb recovery continues gaze"),A->GetCursorLookRotation(TEXT("頭")).GetAngle()>.02f);
  for(int N=0;N<180;N++){A->TickLimbDrag(1.f/60);A->UpdateCursorLook(1.f/60,GazeOrigin,GazeLeft,true);Eval();}
  TestTrue(TEXT("After limb recovery gaze stays engaged"),A->GetCursorLookRotation(TEXT("頭")).GetAngle()>.02f);
  for(int N=0;N<180;N++)A->UpdateCursorLook(1.f/60,GazeOrigin,GazeLeft,false);
  TestTrue(TEXT("Missing mouse still fades limb gaze out"),A->GetCursorLookRotation(TEXT("頭")).Equals(FQuat::Identity,.001f));
 }
 // Exercise the production Tick path with no viewport mouse updates. Slate's
 // deprojected movement rays must replace the original press ray on every move.
 for(int I=0;I<4;I++){
  A->ClearLimbDrag();Steps(90);
  for(int N=0;N<180;N++)A->UpdateCursorLook(1.f/60,FVector::ZeroVector,FVector::ForwardVector,false);
  const FVector End=A->Character->GetSocketLocation(Data->Characters[0].LimbDrags[I].EndBone);
  const FVector Camera=A->Character->GetSocketLocation(TEXT("頭"))+FVector(0,100,0);
  TestTrue(TEXT("Live cursor ray grab"),A->BeginLimbDrag(Camera,(End-Camera).GetSafeNormal()));
  A->TickCursorLook(1.f/60);TestTrue(TEXT("Mouse-down ray initializes gaze"),A->GetCursorLookRotation(TEXT("頭")).GetAngle()>.001f);
  FQuat Previous=FQuat::Identity;bool First=true;
  for(float X:{-28.f,-12.f,12.f,28.f}){
   const FVector CurrentRay=FVector(X,-100,0).GetSafeNormal();
   A->UpdateLimbDrag(Camera,CurrentRay);
   for(int N=0;N<120;N++){A->TickCursorLook(1.f/60);Steps(1);}
   const FQuat Now=A->GetCursorLookRotation(TEXT("頭"));
   if(!First)TestTrue(TEXT("Live mouse movement changes gaze through TickCursorLook"),Previous.AngularDistance(Now)>.025f);
   Previous=Now;First=false;
  }
  const FQuat Still=A->GetCursorLookRotation(TEXT("頭"));
  for(int N=0;N<60;N++){A->TickCursorLook(1.f/60);Steps(1);}
  TestTrue(TEXT("Stationary captured pointer keeps the latest gaze target"),Still.AngularDistance(A->GetCursorLookRotation(TEXT("頭")))<.005f);
  // Hand/foot reach limits must not freeze the pointer used by the head.
  auto& DragSettings=Data->Characters[0].LimbDrags[I];const float SavedMax=DragSettings.MaxDistance;DragSettings.MaxDistance=1.f;
  A->UpdateLimbDrag(Camera,FVector(-25,-100,12).GetSafeNormal());
  for(int N=0;N<120;N++){A->TickCursorLook(1.f/60);Steps(1);}
  const FQuat Limited=A->GetCursorLookRotation(TEXT("頭"));
  A->UpdateLimbDrag(Camera,FVector(25,-100,-12).GetSafeNormal());
  for(int N=0;N<120;N++){A->TickCursorLook(1.f/60);Steps(1);}
  TestTrue(TEXT("Cursor gaze changes even when limb distance is clamped"),Limited.AngularDistance(A->GetCursorLookRotation(TEXT("頭")))>.1f);
  DragSettings.MaxDistance=SavedMax;A->ClearLimbDrag();
 }
 Look.bEnabled=false;
 // A component can disappear during teardown; none of the drag paths may dereference it.
 auto* SavedCharacter=A->Character.Get();A->Character=nullptr;
 TestEqual(TEXT("Missing character produces an inactive snapshot"),A->GetLimbSnapshot().Alpha,0.f);
 A->UpdateLimbDrag(Origin,FVector(0,-1,0));TestFalse(TEXT("Missing character releases held limb"),A->IsLimbDragging());
 A->TickLimbDrag(1.f/60);TestFalse(TEXT("Missing character clears recovery"),A->HasLimbPose());
 Data->Characters[0].bDrawLimbDrags=true;A->TickLimbDrag(1.f/60);Data->Characters[0].bDrawLimbDrags=false;
 A->Character=SavedCharacter;
 A->SetInteractionEnabled(false);TestFalse(TEXT("Switching station clears held limb and recovery"),A->HasLimbPose());
 A->SetInteractionEnabled(true);Data->Characters[0].LimbDrags[0].JointBone=NAME_None;TestFalse(TEXT("Invalid chain cannot start a drag"),A->BeginLimbDrag(Origin,FVector(0,-1,0)));
 Player->Stop();GEngine->DestroyWorldContext(W);W->DestroyWorld(false);return true;
}
#endif
