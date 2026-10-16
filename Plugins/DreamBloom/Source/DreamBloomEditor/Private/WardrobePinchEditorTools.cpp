#include "WardrobePinchEditorTools.h"
#include "WardrobePinchGraphNode.h"
#include "WardrobeReleaseGraphNode.h"
#include "WardrobeFootIKGraphNode.h"
#include "WardrobeLimbGraphNode.h"
#include "Animation/AnimBlueprint.h"
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_KawaiiPhysics.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
namespace WardrobePhysicsTools {
template<class T> T* Add(UEdGraph* Graph,int32 X){auto* N=NewObject<T>(Graph);Graph->AddNode(N,false,false);N->CreateNewGuid();N->PostPlacedNewNode();N->AllocateDefaultPins();N->NodePosX=X;return N;}
bool Link(UEdGraphNode* A,UEdGraphNode* B){
 for(auto* Out:A->Pins)if(Out->Direction==EGPD_Output)for(auto* In:B->Pins)if(In->Direction==EGPD_Input&&A->GetSchema()->TryCreateConnection(Out,In))return true;
 return false;
}
}
#endif

bool UWardrobePinchEditorTools::AddReleaseBlendNode(UAnimBlueprint* BP){
#if WITH_EDITOR
 if(!BP)return false;
 for(const auto& G:BP->FunctionGraphs)for(const auto& N:G->Nodes)if(Cast<UAnimGraphNode_WardrobeRelease>(N.Get()))return true;
 for(const auto& G:BP->FunctionGraphs)for(const auto& N:G->Nodes)if(auto* Local=Cast<UAnimGraphNode_ComponentToLocalSpace>(N.Get())){
  auto* Blend=WardrobePhysicsTools::Add<UAnimGraphNode_WardrobeRelease>(G.Get(),Local->NodePosX+150);
  for(auto* Out:Local->Pins)if(Out->Direction==EGPD_Output){auto Links=Out->LinkedTo;for(auto* In:Links){Out->BreakLinkTo(In);for(auto* P:Blend->Pins)if(P->Direction==EGPD_Output)Blend->GetSchema()->TryCreateConnection(P,In);}}
  if(!WardrobePhysicsTools::Link(Local,Blend))return false;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
 }
#endif
 return false;
}

bool UWardrobePinchEditorTools::AddLimbDragNodes(UAnimBlueprint* BP){
#if WITH_EDITOR
 if(!BP)return false;
 UAnimGraphNode_WardrobeLimbReaction* Body=nullptr;UAnimGraphNode_WardrobeLimbIK* ExistingIK=nullptr;
 UAnimGraphNode_WardrobeRelease* Release=nullptr;UAnimGraphNode_WardrobeFootIK* Foot=nullptr;UEdGraph* Graph=nullptr;
 for(const auto& G:BP->FunctionGraphs)for(const auto& N:G->Nodes){
  if(auto* V=Cast<UAnimGraphNode_WardrobeLimbReaction>(N.Get()))Body=V;
  if(auto* V=Cast<UAnimGraphNode_WardrobeLimbIK>(N.Get()))ExistingIK=V;
  if(auto* V=Cast<UAnimGraphNode_WardrobeRelease>(N.Get())){Release=V;Graph=G.Get();}
  if(auto* V=Cast<UAnimGraphNode_WardrobeFootIK>(N.Get()))Foot=V;
 }
 if(Body&&ExistingIK)return true;
 if(!Release||!Foot||Foot->GetGraph()!=Graph)return false;
 auto Insert=[&](UEdGraphNode* Before,UEdGraphNode* Added){
  for(auto* Out:Before->Pins)if(Out->Direction==EGPD_Output){auto Links=Out->LinkedTo;for(auto* In:Links){Out->BreakLinkTo(In);for(auto* P:Added->Pins)if(P->Direction==EGPD_Output)if(!Added->GetSchema()->TryCreateConnection(P,In))return false;}}
  return WardrobePhysicsTools::Link(Before,Added);
 };
 if(!Body){Body=WardrobePhysicsTools::Add<UAnimGraphNode_WardrobeLimbReaction>(Graph,Release->NodePosX+100);Body->NodePosY=250;if(!Insert(Release,Body))return false;}
 if(!ExistingIK){ExistingIK=WardrobePhysicsTools::Add<UAnimGraphNode_WardrobeLimbIK>(Graph,Foot->NodePosX+100);ExistingIK->NodePosY=250;if(!Insert(Foot,ExistingIK))return false;}
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
#else
 return false;
#endif
}

bool UWardrobePinchEditorTools::AddFootIKNode(UAnimBlueprint* BP){
#if WITH_EDITOR
 if(!BP||!BP->TargetSkeleton)return false;
 for(const auto& G:BP->FunctionGraphs)for(const auto& N:G->Nodes)if(auto* IK=Cast<UAnimGraphNode_WardrobeFootIK>(N.Get())){
  const auto& Rig=BP->TargetSkeleton->GetReferenceSkeleton();
  if(Rig.FindBoneIndex(TEXT("足首D_L"))!=INDEX_NONE&&Rig.FindBoneIndex(TEXT("足首D_R"))!=INDEX_NONE){
   IK->Node.LeftFoot.BoneName=TEXT("足首D_L");IK->Node.RightFoot.BoneName=TEXT("足首D_R");
   IK->Node.LeftToe.BoneName=TEXT("足先EX_L");IK->Node.RightToe.BoneName=TEXT("足先EX_R");
  }
  IK->Node.bAutoSoleContact=true;IK->Node.TraceDistance=100;IK->Node.MaxPelvisOffset=60;
  auto* Preview=BP->GetPreviewMesh();if(!Preview&&BP->TargetSkeleton)Preview=BP->TargetSkeleton->GetPreviewMesh(true);
  if(!IK->Node.BuildContactData(Preview)){UE_LOG(LogTemp,Display,TEXT("CONTACT_BAKE_FAILED mesh=%s left=%s right=%s points=%d,%d"),*GetNameSafe(Preview),*IK->Node.LeftFoot.BoneName.ToString(),*IK->Node.RightFoot.BoneName.ToString(),IK->Node.LeftSolePoints.Num(),IK->Node.RightSolePoints.Num());return false;}
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
 }
 const auto& Ref=BP->TargetSkeleton->GetReferenceSkeleton();
 auto Choose=[&](std::initializer_list<const TCHAR*> Names)->FName{for(auto* Name:Names)if(Ref.FindBoneIndex(Name)!=INDEX_NONE)return FName(Name);return NAME_None;};
 const FName L=Choose({TEXT("足首D_L"),TEXT("足首_L"),TEXT("足首.L"),TEXT("LeftFoot"),TEXT("foot_l")}),R=Choose({TEXT("足首D_R"),TEXT("足首_R"),TEXT("足首.R"),TEXT("RightFoot"),TEXT("foot_r")});
 const FName Pelvis=Choose({TEXT("腰"),TEXT("pelvis"),TEXT("Hips"),TEXT("下半身")});
 if(L.IsNone()||R.IsNone()||Pelvis.IsNone())return false;
 for(const auto& G:BP->FunctionGraphs)for(const auto& N:G->Nodes)if(auto* Release=Cast<UAnimGraphNode_WardrobeRelease>(N.Get())){
  auto* ToCS=WardrobePhysicsTools::Add<UAnimGraphNode_LocalToComponentSpace>(G.Get(),Release->NodePosX+200);
  auto* IK=WardrobePhysicsTools::Add<UAnimGraphNode_WardrobeFootIK>(G.Get(),Release->NodePosX+400);
  auto* ToLocal=WardrobePhysicsTools::Add<UAnimGraphNode_ComponentToLocalSpace>(G.Get(),Release->NodePosX+600);
  IK->Node.Pelvis.BoneName=Pelvis;IK->Node.LeftFoot.BoneName=L;IK->Node.RightFoot.BoneName=R;
  IK->Node.LeftToe.BoneName=Choose({TEXT("足先EX_L"),TEXT("つま先_L"),TEXT("つま先.L"),TEXT("ball_l"),TEXT("LeftToeBase")});
  IK->Node.RightToe.BoneName=Choose({TEXT("足先EX_R"),TEXT("つま先_R"),TEXT("つま先.R"),TEXT("ball_r"),TEXT("RightToeBase")});
  // Estimate ankle height from reference mesh bounds; shoes may need fine tuning.
  if(auto* Mesh=BP->GetPreviewMesh()){
   const auto& MR=Mesh->GetRefSkeleton();auto ComponentRef=[&](FName Name){int32 Index=MR.FindBoneIndex(Name);FTransform T=MR.GetRefBonePose()[Index];while((Index=MR.GetParentIndex(Index))!=INDEX_NONE)T=T*MR.GetRefBonePose()[Index];return T;};
   const auto Bounds=Mesh->GetImportedBounds();
   IK->Node.SoleHeight=FMath::Clamp(float((ComponentRef(L).GetLocation().Z+ComponentRef(R).GetLocation().Z)*.5-(Bounds.Origin.Z-Bounds.BoxExtent.Z)),0.f,25.f);
   IK->Node.BuildContactData(Mesh);
  }
  for(auto* Out:Release->Pins)if(Out->Direction==EGPD_Output){auto Links=Out->LinkedTo;for(auto* In:Links){Out->BreakLinkTo(In);for(auto* P:ToLocal->Pins)if(P->Direction==EGPD_Output)ToLocal->GetSchema()->TryCreateConnection(P,In);}}
  if(!WardrobePhysicsTools::Link(Release,ToCS)||!WardrobePhysicsTools::Link(ToCS,IK)||!WardrobePhysicsTools::Link(IK,ToLocal))return false;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
 }
#endif
 return false;
}

UAnimBlueprint* UWardrobePinchEditorTools::CreatePinchTestBlueprint(USkeletalMesh* Mesh,const TArray<FName>& Bones,const FString& PackagePath){
#if WITH_EDITOR
 if(Mesh&&!Mesh->GetSkeleton()){
  const FString SkeletonPath=PackagePath+TEXT("_Skeleton");auto* SkeletonPackage=CreatePackage(*SkeletonPath);
  auto* Skeleton=NewObject<USkeleton>(SkeletonPackage,FName(*FPackageName::GetLongPackageAssetName(SkeletonPath)),RF_Public|RF_Standalone);
  Skeleton->MergeAllBonesToBoneTree(Mesh);Mesh->SetSkeleton(Skeleton);FAssetRegistryModule::AssetCreated(Skeleton);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(SkeletonPackage,Skeleton,*FPackageName::LongPackageNameToFilename(SkeletonPath,FPackageName::GetAssetPackageExtension()),Args);
 }
 if(!Mesh||!Mesh->GetSkeleton()||Bones.IsEmpty()||FPackageName::DoesPackageExist(PackagePath))return nullptr;
 auto* Package=CreatePackage(*PackagePath);auto* Factory=NewObject<UAnimBlueprintFactory>();Factory->ParentClass=UAnimInstance::StaticClass();Factory->TargetSkeleton=Mesh->GetSkeleton();Factory->PreviewSkeletalMesh=Mesh;
 auto* BP=Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(),Package,FName(*FPackageName::GetLongPackageAssetName(PackagePath)),RF_Public|RF_Standalone,nullptr,GWarn));if(!BP)return nullptr;
 UEdGraph* Graph=nullptr;UAnimGraphNode_Root* Root=nullptr;
 for(const auto& G:BP->FunctionGraphs)for(const auto& N:G->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(N.Get())){Graph=G.Get();Root=R;}
 if(!Graph||!Root)return nullptr;
 using namespace WardrobePhysicsTools;
 auto* Ref=Add<UAnimGraphNode_LocalRefPose>(Graph,-1000);auto* Slot=Add<UAnimGraphNode_Slot>(Graph,-800);Slot->Node.SlotName=TEXT("DefaultSlot");auto* ToComponent=Add<UAnimGraphNode_LocalToComponentSpace>(Graph,-600);if(!Link(Ref,Slot)||!Link(Slot,ToComponent))return nullptr;
 UEdGraphNode* Previous=ToComponent;
 for(int32 I=0;I<Bones.Num();I++){auto* N=Add<UAnimGraphNode_WardrobePinch>(Graph,-400+I*200);N->Node.BoneToModify.BoneName=Bones[I];N->Node.bReactionBone=!Bones[I].ToString().StartsWith(TEXT("DB_Pinch_"));if(!Link(Previous,N))return nullptr;Previous=N;}
 auto* ToLocal=Add<UAnimGraphNode_ComponentToLocalSpace>(Graph,Bones.Num()*200);Root->NodePosX=Bones.Num()*200+200;
 for(auto* Pin:Root->Pins)if(Pin->Direction==EGPD_Input)Pin->BreakAllPinLinks();
 if(!Link(Previous,ToLocal)||!Link(ToLocal,Root))return nullptr;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);if(BP->Status==BS_Error)return nullptr;
 FAssetRegistryModule::AssetCreated(BP);FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 if(!UPackage::SavePackage(Package,BP,*FPackageName::LongPackageNameToFilename(PackagePath,FPackageName::GetAssetPackageExtension()),Args))return nullptr;
 return BP;
#else
 return nullptr;
#endif
}
bool UWardrobePinchEditorTools::SetPinchBaseAnimation(UAnimBlueprint* BP,UAnimSequence* Animation){
#if WITH_EDITOR
 if(!BP||!Animation||BP->TargetSkeleton!=Animation->GetSkeleton())return false;
 for(const auto& Graph:BP->FunctionGraphs)for(int32 I=0;I<Graph->Nodes.Num();I++)if(auto* Ref=Cast<UAnimGraphNode_LocalRefPose>(Graph->Nodes[I].Get())){
  auto* Player=WardrobePhysicsTools::Add<UAnimGraphNode_SequencePlayer>(Graph.Get(),Ref->NodePosX);Player->Node.SetSequence(Animation);
  for(auto* Out:Ref->Pins)if(Out->Direction==EGPD_Output){auto Links=Out->LinkedTo;for(auto* In:Links){Out->BreakLinkTo(In);for(auto* P:Player->Pins)if(P->Direction==EGPD_Output)Player->GetSchema()->TryCreateConnection(P,In);}}
  Graph->RemoveNode(Ref);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
 }
#endif
 return false;
}

