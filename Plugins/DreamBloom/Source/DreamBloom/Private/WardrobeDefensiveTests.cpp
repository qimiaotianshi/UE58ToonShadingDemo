#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeFootIKNode.h"
#include "WardrobeLimbNode.h"
#include "Animation/AnimInstanceProxy.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/MemStack.h"

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
#endif
