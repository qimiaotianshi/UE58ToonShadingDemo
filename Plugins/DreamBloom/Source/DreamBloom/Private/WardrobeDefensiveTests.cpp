#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeFootIKNode.h"
#include "WardrobeLimbNode.h"
#include "Animation/AnimInstanceProxy.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/MemStack.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeInvalidFootPoseTest,"DreamBloom.Ground.InvalidCompactPoseIndices",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeInvalidFootPoseTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/SK_DaniyaFullBodyPinch.SK_DaniyaFullBodyPinch"));
 if(!TestNotNull(TEXT("Fixture mesh"),Mesh))return false;
 const auto& Ref=Mesh->GetRefSkeleton();int Child=INDEX_NONE;
 for(int I=1;I<Ref.GetNum();I++)if(Ref.GetParentIndex(I)==0){Child=I;break;}
 if(!TestTrue(TEXT("Fixture has a root child"),Child!=INDEX_NONE))return false;
 FMemMark Mark(FMemStack::Get());TArray<FBoneIndexType> Required;
 for(int I=0;I<Ref.GetNum();I++)Required.Add(I);
 FBoneContainer Bones(Required,UE::Anim::FCurveFilterSettings(),*Mesh);
 FAnimInstanceProxy Proxy;
 auto Check=[&](const TCHAR* Label,FName Pelvis,FName Left,FName Right,bool ExpectedPelvis){
  FComponentSpacePoseContext Context(&Proxy);Context.Pose.InitPose(&Bones);
  FAnimNode_WardrobeFootIK Node;Node.bEnabled=false;Node.Pelvis.BoneName=Pelvis;Node.LeftFoot.BoneName=Left;Node.RightFoot.BoneName=Right;Node.InitializeBoneReferences(Bones);
  TArray<FBoneTransform> Out;
  // Deliberately bypass IsValidToEvaluate: evaluation must guard its own reads.
  Node.EvaluateSkeletalControl_AnyThread(Context,Out);
  TestEqual(FString(Label)+TEXT(" writes only a valid pelvis or no bones"),Out.Num(),ExpectedPelvis?1:0);
  for(const auto& Bone:Out)TestTrue(FString(Label)+TEXT(" output index is valid"),Context.Pose.GetPose().IsValidIndex(Bone.BoneIndex));
 };
 const FName Root=Ref.GetBoneName(0),RootChild=Ref.GetBoneName(Child);
 Check(TEXT("Missing pelvis"),NAME_None,Root,Root,false);
 Check(TEXT("Both missing feet"),Root,NAME_None,NAME_None,true);
 Check(TEXT("Missing left, root right"),Root,NAME_None,Root,true);
 Check(TEXT("Root feet have no knee"),Root,Root,Root,true);
 Check(TEXT("Root-child feet have no hip"),Root,RootChild,RootChild,true);
 // Reproduce the distinction that caused the parent-index crash: valid mesh
 // references with invalid cached compact indices must fail before parent reads.
 FAnimNode_WardrobeFootIK Stale;
 Stale.Pelvis.BoneName=TEXT("腰");Stale.LeftFoot.BoneName=TEXT("足首D_L");Stale.RightFoot.BoneName=TEXT("足首D_R");
 Stale.InitializeBoneReferences(Bones);
 if(!TestTrue(TEXT("Fixture has a valid foot chain"),Stale.IsValidToEvaluate(nullptr,Bones)))return false;
 for(auto* Bone:{&Stale.Pelvis,&Stale.LeftFoot,&Stale.RightFoot}){
  const auto Saved=Bone->CachedCompactPoseIndex;
  const int BadIndices[]={INDEX_NONE,Bones.GetCompactPoseNumBones()};
  for(int Bad:BadIndices){
   Bone->CachedCompactPoseIndex=FCompactPoseBoneIndex(Bad);
   TestTrue(TEXT("Mesh reference still passes IsValidToEvaluate"),Bone->IsValidToEvaluate(Bones));
   TestFalse(TEXT("Foot node rejects invalid cached compact index"),Stale.IsValidToEvaluate(nullptr,Bones));
  }
  Bone->CachedCompactPoseIndex=Saved;
 }
 TestTrue(TEXT("Restoring compact indices restores the valid chain"),Stale.IsValidToEvaluate(nullptr,Bones));
 // RequiredBones can validate a reference while the current pose has fewer
 // transforms. Limb evaluation must check the pose itself before parent walks.
 for(bool Foot:{false,true}){
  FCompactPose Truncated;Truncated.SetBoneContainer(&Bones);Truncated.InitBones(1);
  Truncated[FCompactPoseBoneIndex(0)]=FTransform::Identity;
  FComponentSpacePoseContext Context(&Proxy);Context.Pose.InitPose(Truncated);
  FAnimNode_WardrobeLimbIK Limb;Limb.DragAlpha=1;
  Limb.Upper.BoneName=Foot?TEXT("足D_L"):TEXT("腕_L");
  Limb.Joint.BoneName=Foot?TEXT("ひざD_L"):TEXT("ひじ_L");
  Limb.End.BoneName=Foot?TEXT("足首D_L"):TEXT("手首_L");
  Limb.Upper.Initialize(Bones);Limb.Joint.Initialize(Bones);Limb.End.Initialize(Bones);
  TestTrue(TEXT("Truncated-pose fixture keeps valid limb references"),Limb.Upper.IsValidToEvaluate(Bones)&&Limb.Joint.IsValidToEvaluate(Bones)&&Limb.End.IsValidToEvaluate(Bones));
  TArray<FBoneTransform> Out;Limb.EvaluateSkeletalControl_AnyThread(Context,Out);
  TestEqual(TEXT("Limb rejects indices outside the current pose"),Out.Num(),0);
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeInvalidWorldTransformTest,"DreamBloom.Ground.InvalidWorldTransform",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeInvalidWorldTransformTest::RunTest(const FString&){
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/WardrobeFullBodyPinch42/SK_DaniyaFullBodyPinch.SK_DaniyaFullBodyPinch"));
 if(!TestNotNull(TEXT("Fixture mesh"),Mesh))return false;
 FMemMark Mark(FMemStack::Get());TArray<FBoneIndexType> Required;
 for(int I=0;I<Mesh->GetRefSkeleton().GetNum();I++)Required.Add(I);
 FBoneContainer Bones(Required,UE::Anim::FCurveFilterSettings(),*Mesh);
 FAnimInstanceProxy Proxy;
 FAnimNode_WardrobeFootIK Node;
 Node.Pelvis.BoneName=TEXT("腰");Node.LeftFoot.BoneName=TEXT("足首D_L");Node.RightFoot.BoneName=TEXT("足首D_R");
 Node.MeshWorld=FTransform(FQuat(0,0,0,0),FVector::ZeroVector);
 Node.PelvisOffset=12;
 Node.Initialize_AnyThread(FAnimationInitializeContext(&Proxy));
 TestTrue(TEXT("Reinitialization resets cached world transform"),Node.MeshWorld.Equals(FTransform::Identity));
 TestEqual(TEXT("Reinitialization resets pelvis adaptation"),Node.PelvisOffset,0.f);
 Node.InitializeBoneReferences(Bones);
 if(!TestTrue(TEXT("Fixture valid foot chain"),Node.IsValidToEvaluate(nullptr,Bones)))return false;
 auto Eval=[&](const TCHAR* Label){
  FComponentSpacePoseContext Context(&Proxy);Context.Pose.InitPose(&Bones);
  TArray<FBoneTransform> Out;Node.EvaluateSkeletalControl_AnyThread(Context,Out);
  TestTrue(FString(Label)+TEXT(" preserves valid pose output"),Out.Num()>0);
  for(const auto& Bone:Out){
   TestTrue(FString(Label)+TEXT(" has valid output index"),Context.Pose.GetPose().IsValidIndex(Bone.BoneIndex));
   TestFalse(FString(Label)+TEXT(" does not contaminate pose with nonfinite values"),Bone.Transform.ContainsNaN());
   TestTrue(FString(Label)+TEXT(" keeps output rotations normalized"),Bone.Transform.GetRotation().IsNormalized());
  }
 };
 // Force ground hits so the test executes the formerly crashing transform path.
 auto Seed=[&](){
  Node.bSnap=false;Node.DeltaTime=1.f/60;Node.BlendTime=.15f;Node.PelvisOffset=-2.f;
  for(int I=0;I<2;I++){
   Node.Ground[I].bHit=true;Node.Ground[I].Point=FVector::ZeroVector;Node.Ground[I].Normal=FVector::UpVector;
   Node.SmoothedOffset[I]=FVector(0,0,4);Node.SmoothedRotation[I]=FQuat::Identity;Node.LastPlant[I]=1;
  }
 };
 auto CheckInvalid=[&](const TCHAR* Label,const FTransform& Bad){
  Seed();Node.MeshWorld=Bad;Eval(Label);
  for(int I=0;I<2;I++){
   TestEqual(FString(Label)+TEXT(" skips ground planting"),Node.LastPlant[I],0.f);
   TestTrue(FString(Label)+TEXT(" smoothly releases previous finite offset"),Node.SmoothedOffset[I].Z>0&&Node.SmoothedOffset[I].Z<4);
  }
 };
 CheckInvalid(TEXT("Zero quaternion"),FTransform(FQuat(0,0,0,0),FVector::ZeroVector));
 CheckInvalid(TEXT("Non-unit quaternion"),FTransform(FQuat(0,0,0,2),FVector::ZeroVector));
#if !ENABLE_NAN_DIAGNOSTIC
 // Diagnostic builds repair NaNs in setters. Test raw nonfinite snapshots when
 // using the engine configuration that retains them, as in the reported crash.
 const double NaN=std::numeric_limits<double>::quiet_NaN(),Inf=std::numeric_limits<double>::infinity();
 CheckInvalid(TEXT("NaN quaternion"),FTransform(FQuat(NaN,0,0,1),FVector::ZeroVector));
 CheckInvalid(TEXT("NaN translation"),FTransform(FQuat::Identity,FVector(NaN,0,0)));
 CheckInvalid(TEXT("Infinite scale"),FTransform(FQuat::Identity,FVector::ZeroVector,FVector(1,Inf,1)));
#endif
 // A valid snapshot resumes ground adaptation on the next evaluation.
 Seed();Node.MeshWorld=FTransform::Identity;Node.bSnap=true;Eval(TEXT("Valid transform recovery"));
 TestTrue(TEXT("Valid world transform restores ground planting"),Node.LastPlant[0]>0&&Node.LastPlant[1]>0);
 Node.PreUpdate(nullptr);TestFalse(TEXT("Missing instance clears left ground hit"),Node.Ground[0].bHit);
 TestFalse(TEXT("Missing instance clears right ground hit"),Node.Ground[1].bHit);
 return true;
}
#endif
