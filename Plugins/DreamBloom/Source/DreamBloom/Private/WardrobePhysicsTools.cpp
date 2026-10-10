#include "WardrobePhysicsTools.h"
#include "Animation/AnimBlueprint.h"
#if WITH_EDITOR
#include "Animation/AnimInstance.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/AnimBlueprintFactory.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_LocalRefPose.h"
#include "AnimGraphNode_SequencePlayer.h"
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
UAnimBlueprint* UWardrobePhysicsTools::CreateTouchTestBlueprint(USkeletalMesh* Mesh,UAnimSequence* Idle,const TArray<FName>& RootBones,const FString& PackagePath){
#if WITH_EDITOR
 if(!Mesh||!Mesh->GetSkeleton()||RootBones.IsEmpty()||!PackagePath.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(PackagePath)||FPackageName::DoesPackageExist(PackagePath)||FindPackage(nullptr,*PackagePath))return nullptr;
 if(Idle&&Idle->GetSkeleton()!=Mesh->GetSkeleton())return nullptr;
 for(FName Bone:RootBones)if(Mesh->GetRefSkeleton().FindBoneIndex(Bone)==INDEX_NONE)return nullptr;
 auto* Package=CreatePackage(*PackagePath);auto* Factory=NewObject<UAnimBlueprintFactory>();
 Factory->ParentClass=UAnimInstance::StaticClass();Factory->TargetSkeleton=Mesh->GetSkeleton();Factory->PreviewSkeletalMesh=Mesh;
 auto* BP=Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(),Package,FName(*FPackageName::GetLongPackageAssetName(PackagePath)),RF_Public|RF_Standalone,nullptr,GWarn));
 if(!BP)return nullptr;
 UEdGraph* Graph=nullptr;UAnimGraphNode_Root* Root=nullptr;
 for(const auto& GraphPtr:BP->FunctionGraphs)for(const auto& NodePtr:GraphPtr->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(NodePtr.Get())){Graph=GraphPtr.Get();Root=R;break;}
 if(!Graph||!Root)return nullptr;
 using namespace WardrobePhysicsTools;
 UEdGraphNode* Source=nullptr;
 if(Idle){auto* N=Add<UAnimGraphNode_SequencePlayer>(Graph,-800);N->Node.SetSequence(Idle);N->Node.SetLoopAnimation(true);Source=N;}
 else Source=Add<UAnimGraphNode_LocalRefPose>(Graph,-800);
 auto* ToComponent=Add<UAnimGraphNode_LocalToComponentSpace>(Graph,-600);
 auto* Kawaii=Add<UAnimGraphNode_KawaiiPhysics>(Graph,-400);
 Kawaii->Node.RootBone.BoneName=RootBones[0];
 for(int32 I=1;I<RootBones.Num();I++){FKawaiiPhysicsRootBoneSetting Setting;Setting.RootBone.BoneName=RootBones[I];Kawaii->Node.AdditionalRootBones.Add(Setting);}
 Kawaii->Node.DummyBoneLength=5;Kawaii->Node.PhysicsSettings.Radius=3;Kawaii->Node.Alpha=1;
 auto* ToLocal=Add<UAnimGraphNode_ComponentToLocalSpace>(Graph,-200);Root->NodePosX=0;
 for(auto* Pin:Root->Pins)if(Pin->Direction==EGPD_Input)Pin->BreakAllPinLinks();
 if(!Link(Source,ToComponent)||!Link(ToComponent,Kawaii)||!Link(Kawaii,ToLocal)||!Link(ToLocal,Root))return nullptr;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
 if(BP->Status==BS_Error||!BP->GeneratedClass)return nullptr;
 FAssetRegistryModule::AssetCreated(BP);Package->MarkPackageDirty();
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 const FString File=FPackageName::LongPackageNameToFilename(PackagePath,FPackageName::GetAssetPackageExtension());
 if(!UPackage::SavePackage(Package,BP,*File,Args))return nullptr;
 return BP;
#else
 return nullptr;
#endif
}


