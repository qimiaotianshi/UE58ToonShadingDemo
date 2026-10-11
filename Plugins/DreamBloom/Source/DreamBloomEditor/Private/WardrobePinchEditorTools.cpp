#include "WardrobePinchEditorTools.h"
#include "WardrobePinchGraphNode.h"
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

