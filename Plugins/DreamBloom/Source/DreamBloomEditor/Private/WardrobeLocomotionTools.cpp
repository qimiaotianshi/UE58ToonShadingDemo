#include "WardrobeLocomotionTools.h"
#include "WardrobeLocomotion.h"
#include "WardrobeShowcase.h"
#include "WardrobeFootIKGraphNode.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Factories/AnimBlueprintFactory.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_LocalToComponentSpace.h"
#include "AnimGraphNode_ComponentToLocalSpace.h"
#include "AnimGraphNode_KawaiiPhysics.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Components/SkeletalMeshComponent.h"
namespace ExplorerTools {
template<class T> T* Asset(const FString& Path){auto* P=CreatePackage(*Path);auto* A=NewObject<T>(P,FName(*FPackageName::GetLongPackageAssetName(Path)),RF_Public|RF_Standalone);FAssetRegistryModule::AssetCreated(A);return A;}
bool Save(UObject* A){FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;return UPackage::SavePackage(A->GetOutermost(),A,*FPackageName::LongPackageNameToFilename(A->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);}
template<class T> T* Node(UEdGraph* G,int X){auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->PostPlacedNewNode();N->AllocateDefaultPins();N->NodePosX=X;return N;}
bool Link(UEdGraphNode* A,UEdGraphNode* B){for(auto* Out:A->Pins)if(Out->Direction==EGPD_Output&&Out->PinType.PinCategory==TEXT("struct"))for(auto* In:B->Pins)if(In->Direction==EGPD_Input&&A->GetSchema()->TryCreateConnection(Out,In))return true;return false;}
UBlueprint* BP(UClass* Parent,const FString& Path){auto* P=CreatePackage(*Path);auto* B=FKismetEditorUtilities::CreateBlueprint(Parent,P,FName(*FPackageName::GetLongPackageAssetName(Path)),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());FAssetRegistryModule::AssetCreated(B);return B;}
}
bool UWardrobeLocomotionTools::BuildLocomotionAssets(USkeletalMesh* Mesh,UAnimSequence* Idle,UAnimSequence* Walk,UAnimSequence* Run){
 if(!Mesh||!Idle||!Walk||!Run||!Mesh->GetSkeleton())return false;
 using namespace ExplorerTools;const FString Root=TEXT("/Game/BluePrint/ALS/");
 auto* Blend=Asset<UBlendSpace1D>(Root+TEXT("AnimationBP/BS_ALSLocomotion"));Blend->SetSkeleton(Mesh->GetSkeleton());
 auto& Axis=const_cast<FBlendParameter&>(Blend->GetBlendParameter(0));Axis.DisplayName=TEXT("Gait");Axis.Min=0;Axis.Max=2;Axis.GridNum=8;
 Blend->AddSample(Idle,FVector(0,0,0));Blend->AddSample(Walk,FVector(1,0,0));Blend->AddSample(Run,FVector(2,0,0));Blend->ValidateSampleData();Blend->ResampleData();if(!Save(Blend))return false;
 auto* Factory=NewObject<UAnimBlueprintFactory>();Factory->ParentClass=UWardrobeLocomotionAnim::StaticClass();Factory->TargetSkeleton=Mesh->GetSkeleton();Factory->PreviewSkeletalMesh=Mesh;
 const FString BPPath=Root+TEXT("AnimationBP/ABP_ALSLocomotion");auto* Package=CreatePackage(*BPPath);
 auto* Anim=Cast<UAnimBlueprint>(Factory->FactoryCreateNew(UAnimBlueprint::StaticClass(),Package,FName(TEXT("ABP_ALSLocomotion")),RF_Public|RF_Standalone,nullptr,GWarn));if(!Anim)return false;
 UEdGraph* Graph=nullptr;UAnimGraphNode_Root* Result=nullptr;
 for(const auto& G:Anim->FunctionGraphs)for(const auto& N:G->Nodes)if(auto* R=Cast<UAnimGraphNode_Root>(N.Get())){Graph=G.Get();Result=R;}
 if(!Graph)return false;
 auto* Player=Node<UAnimGraphNode_BlendSpacePlayer>(Graph,-1000);Player->SetAnimationAsset(Blend);
 for(auto& P:Player->ShowPinForProperties)if(P.PropertyName==TEXT("X"))P.bShowPin=true;
 Player->ReconstructNode();
 auto* Gait=Node<UK2Node_VariableGet>(Graph,-1200);Gait->VariableReference.SetSelfMember(TEXT("Gait"));Gait->ReconstructNode();
 auto* X=Player->FindPin(TEXT("X"));auto* Value=Gait->FindPin(TEXT("Gait"));
 if(!X||!Value||!Graph->GetSchema()->TryCreateConnection(Value,X))return false;
 auto* CS=Node<UAnimGraphNode_LocalToComponentSpace>(Graph,-650);auto* Foot=Node<UAnimGraphNode_WardrobeFootIK>(Graph,-400);
 Foot->Node.Pelvis.BoneName=TEXT("腰");Foot->Node.LeftFoot.BoneName=TEXT("足首D_L");Foot->Node.RightFoot.BoneName=TEXT("足首D_R");Foot->Node.LeftToe.BoneName=TEXT("足先EX_L");Foot->Node.RightToe.BoneName=TEXT("足先EX_R");
 // UE sanitizes Blender dots to underscores. Fall back to the preserved FK chain if needed.
 const auto& Ref=Mesh->GetRefSkeleton();if(Ref.FindBoneIndex(Foot->Node.LeftFoot.BoneName)==INDEX_NONE){Foot->Node.LeftFoot.BoneName=TEXT("足首D.L");Foot->Node.RightFoot.BoneName=TEXT("足首D.R");Foot->Node.LeftToe.BoneName=TEXT("足先EX.L");Foot->Node.RightToe.BoneName=TEXT("足先EX.R");}
 if(!Foot->Node.BuildContactData(Mesh))return false;
 UEdGraphNode* Previous=Foot;
 // Secondary chains are kept modest; body locomotion and skinning remain the source pose.
 TArray<FName> Roots;
 for(const auto& B:Ref.GetRawRefBoneInfo()){
  const FString Name=B.Name.ToString();
  if((Name.StartsWith(TEXT("发"))||Name.StartsWith(TEXT("髪"))||Name.StartsWith(TEXT("裙")))&&B.ParentIndex>=0){
   const FString Parent=Ref.GetBoneName(B.ParentIndex).ToString();
   if(!Parent.StartsWith(TEXT("发"))&&!Parent.StartsWith(TEXT("髪"))&&!Parent.StartsWith(TEXT("裙")))Roots.Add(B.Name);
  }
 }
 if(Roots.Num()>0){
  auto* Physics=Node<UAnimGraphNode_KawaiiPhysics>(Graph,-120);Physics->Node.RootBone.BoneName=Roots[0];
  for(int I=1;I<Roots.Num();I++){FKawaiiPhysicsRootBoneSetting S;S.RootBone.BoneName=Roots[I];Physics->Node.AdditionalRootBones.Add(S);}
  Physics->Node.PhysicsSettings.Damping=.3f;Physics->Node.PhysicsSettings.Stiffness=.15f;Physics->Node.PhysicsSettings.WorldDampingLocation=1.f;Physics->Node.PhysicsSettings.WorldDampingRotation=.9f;
  if(!Link(Previous,Physics))return false;Previous=Physics;
 }
 auto* Local=Node<UAnimGraphNode_ComponentToLocalSpace>(Graph,150);Result->NodePosX=350;
 if(!Link(Player,CS)||!Link(CS,Foot)||!Link(Previous,Local)||!Link(Local,Result))return false;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Anim);FKismetEditorUtilities::CompileBlueprint(Anim);if(Anim->Status==BS_Error)return false;FAssetRegistryModule::AssetCreated(Anim);if(!Save(Anim))return false;
 auto* Move=Asset<UInputAction>(Root+TEXT("Input/IA_Move"));Move->ValueType=EInputActionValueType::Axis2D;
 auto* Look=Asset<UInputAction>(Root+TEXT("Input/IA_Look"));Look->ValueType=EInputActionValueType::Axis2D;
 auto* Sprint=Asset<UInputAction>(Root+TEXT("Input/IA_Sprint"));auto* Pause=Asset<UInputAction>(Root+TEXT("Input/IA_Pause"));
 auto* Mapping=Asset<UInputMappingContext>(Root+TEXT("Input/IMC_ALS"));
 auto Map=[&](UInputAction* Action,FKey Key,bool Y,bool Negative){auto& M=Mapping->MapKey(Action,Key);if(Negative)M.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));if(Y)M.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(Mapping));};
 Map(Move,EKeys::W,true,false);Map(Move,EKeys::S,true,true);Map(Move,EKeys::D,false,false);Map(Move,EKeys::A,false,true);
 Map(Look,EKeys::MouseX,false,false);Map(Look,EKeys::MouseY,true,false);Map(Sprint,EKeys::LeftShift,false,false);Map(Sprint,EKeys::RightShift,false,false);Map(Pause,EKeys::Escape,false,false);
 const TArray<UObject*> InputAssets={Move,Look,Sprint,Pause,Mapping};for(auto* A:InputAssets)if(!Save(A))return false;
 auto* Data=Asset<UWardrobeLocomotionData>(Root+TEXT("Data/DA_ALSLocomotion"));Data->Mapping=Mapping;Data->MoveAction=Move;Data->LookAction=Look;Data->SprintAction=Sprint;Data->PauseAction=Pause;if(!Save(Data))return false;
 auto* Character=BP(AWardrobeExplorer::StaticClass(),Root+TEXT("Characters/BP_ALSCharacter"));
 auto* C=Cast<AWardrobeExplorer>(Character->GeneratedClass->GetDefaultObject());C->Settings=Data;C->GetMesh()->SetSkeletalMeshAsset(Mesh);C->GetMesh()->SetAnimInstanceClass(Anim->GeneratedClass);if(!Save(Character))return false;
 auto* Controller=BP(AWardrobeExplorerController::StaticClass(),Root+TEXT("Game/BP_ALSPlayerController"));if(!Save(Controller))return false;
 auto* Mode=BP(AWardrobeExplorerGameMode::StaticClass(),Root+TEXT("Game/BP_ALSGameMode"));
 auto* M=Cast<AWardrobeExplorerGameMode>(Mode->GeneratedClass->GetDefaultObject());M->DefaultPawnClass=Character->GeneratedClass;M->PlayerControllerClass=Controller->GeneratedClass;if(!Save(Mode))return false;
 UE_LOG(LogTemp,Display,TEXT("ALS68_ASSETS_READY physicsRoots=%d"),Roots.Num());return true;
}

bool UWardrobeLocomotionTools::UpdateLocomotionAssets(){
 using namespace ExplorerTools;const FString Root=TEXT("/Game/BluePrint/ALS/");
 auto* Data=LoadObject<UWardrobeLocomotionData>(nullptr,*(Root+TEXT("Data/DA_ALSLocomotion")));
 auto* Blend=LoadObject<UBlendSpace1D>(nullptr,*(Root+TEXT("AnimationBP/BS_ALSLocomotion")));
 auto* Anim=LoadObject<UAnimBlueprint>(nullptr,*(Root+TEXT("AnimationBP/ABP_ALSLocomotion")));
 auto* Character=LoadObject<UBlueprint>(nullptr,*(Root+TEXT("Characters/BP_ALSCharacter")));
 if(!Data||!Blend||!Anim||!Character)return false;
 Data->LocomotionBlendSpace=Blend;if(!Save(Data))return false;
 for(auto& G:Anim->FunctionGraphs)for(auto& N:G->Nodes)if(auto* Player=Cast<UAnimGraphNode_BlendSpacePlayer>(N.Get())){
  for(auto& P:Player->ShowPinForProperties)if(P.PropertyName==TEXT("BlendSpace")||P.PropertyName==TEXT("PlayRate"))P.bShowPin=true;
  Player->ReconstructNode();
  auto* In=Player->FindPin(TEXT("BlendSpace"));if(!In)return false;
  if(In->LinkedTo.Num()==0){auto* Value=Node<UK2Node_VariableGet>(G,-1200);Value->NodePosY=180;Value->VariableReference.SetSelfMember(TEXT("LocomotionBlendSpace"));Value->ReconstructNode();
   auto* Out=Value->FindPin(TEXT("LocomotionBlendSpace"));if(!Out||!G->GetSchema()->TryCreateConnection(Out,In))return false;}
  auto* RateIn=Player->FindPin(TEXT("PlayRate"));if(!RateIn)return false;
  if(RateIn->LinkedTo.Num()==0){auto* Rate=Node<UK2Node_VariableGet>(G,-1200);Rate->NodePosY=320;Rate->VariableReference.SetSelfMember(TEXT("LocomotionPlayRate"));Rate->ReconstructNode();
   auto* RateOut=Rate->FindPin(TEXT("LocomotionPlayRate"));if(!RateOut||!G->GetSchema()->TryCreateConnection(RateOut,RateIn))return false;}
  break;
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Anim);FKismetEditorUtilities::CompileBlueprint(Anim);if(Anim->Status==BS_Error||!Save(Anim))return false;
 // Match left/right foot contact between the two authored cycles.
 auto Mark=[&](const TCHAR* Name,float Left,float Right){
  auto* A=LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("Animations/A_ALS_")+Name));if(!A)return false;
  A->AuthoredSyncMarkers.Reset();FAnimSyncMarker L;L.MarkerName=TEXT("LeftPlant");L.Time=Left;A->AuthoredSyncMarkers.Add(L);
  FAnimSyncMarker R;R.MarkerName=TEXT("RightPlant");R.Time=Right;A->AuthoredSyncMarkers.Add(R);
  A->SortSyncMarkers();A->RefreshSyncMarkerDataFromAuthored();return Save(A);
 };
 if(!Mark(TEXT("Walk"),18.f/30,2.f/30)||!Mark(TEXT("Run"),20.f/30,8.f/30))return false;
 Blend->ValidateSampleData();Blend->ResampleData();if(!Save(Blend))return false;
 // Model and clips share X-forward/Z-up FBX import settings. Do not add a second yaw conversion.
 auto* C=Cast<AWardrobeExplorer>(Character->GeneratedClass->GetDefaultObject());if(!C||!C->GetMesh()->GetSkeletalMeshAsset())return false;
 const auto& Ref=C->GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton();TArray<FTransform> CS=Ref.GetRefBonePose();
 for(int I=0;I<CS.Num();I++){const int P=Ref.GetParentIndex(I);if(P>=0&&CS.IsValidIndex(P))CS[I]=CS[I]*CS[P];}
 C->GetMesh()->SetRelativeRotation(FRotator::ZeroRotator);
 for(auto& G:Anim->FunctionGraphs)for(auto& N:G->Nodes)if(auto* Foot=Cast<UAnimGraphNode_WardrobeFootIK>(N.Get())){
  Foot->Node.bLockPlantedFeet=true;
  if(!Foot->Node.BuildContactData(C->GetMesh()->GetSkeletalMeshAsset()))return false;
 }
 C->Settings=Data;if(!Save(Character))return false;
 auto Secondary=[](FName Bone){const FString S=Bone.ToString();return S.StartsWith(TEXT("发"))||S.StartsWith(TEXT("髪"))||S.StartsWith(TEXT("スカート"));};
 TArray<FName> Roots,SimBones;
 for(int I=0;I<Ref.GetNum();I++)if(Secondary(Ref.GetBoneName(I))){SimBones.Add(Ref.GetBoneName(I));const int P=Ref.GetParentIndex(I);if(P<0||!Secondary(Ref.GetBoneName(P)))Roots.Add(Ref.GetBoneName(I));}
 for(auto& G:Anim->FunctionGraphs)for(auto& N:G->Nodes)if(auto* K=Cast<UAnimGraphNode_KawaiiPhysics>(N.Get())){
  // Component-space simulation retains pose/rotation response but ignores actor
  // translation. A curve below 1 would reintroduce translation on child bones.
  K->Node.SimulationSpace=EKawaiiPhysicsSimulationSpace::ComponentSpace;
  K->Node.PhysicsSettings.WorldDampingLocation=1.f;
  K->Node.WorldDampingLocationCurveData.GetRichCurve()->Reset();
  K->Node.AdditionalRootBones.Reset();if(Roots.Num()){K->Node.RootBone.BoneName=Roots[0];for(int I=1;I<Roots.Num();I++){FKawaiiPhysicsRootBoneSetting S;S.RootBone.BoneName=Roots[I];K->Node.AdditionalRootBones.Add(S);}}
  K->Node.CapsuleLimits.Reset();
  for(const FName Bone:{FName(TEXT("足D_L")),FName(TEXT("足D_R"))}){const int I=Ref.FindBoneIndex(Bone);if(I==INDEX_NONE)continue;FCapsuleLimit L;L.DrivingBone.BoneName=Bone;L.Radius=7;L.Length=27;L.OffsetLocation=FVector(0,0,18);K->Node.CapsuleLimits.Add(L);}
 }
 // Secondary physics owns these chains; avoid layering baked VMD hair/skirt motion onto simulation.
 for(const TCHAR* Name:{TEXT("Idle"),TEXT("Walk"),TEXT("Run")}){auto* A=LoadObject<UAnimSequence>(nullptr,*(Root+TEXT("Animations/A_ALS_")+Name));auto& Controller=A->GetController();Controller.OpenBracket(FText::FromString(TEXT("Secondary physics tracks")),false);for(FName Bone:SimBones)Controller.RemoveBoneTrack(Bone,false);Controller.CloseBracket(false);if(!Save(A))return false;}
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Anim);FKismetEditorUtilities::CompileBlueprint(Anim);if(Anim->Status==BS_Error||!Save(Anim))return false;
 UE_LOG(LogTemp,Display,TEXT("ALS68_UPDATED meshYaw=%.3f"),C->GetMesh()->GetRelativeRotation().Yaw);return true;
}
bool UWardrobeLocomotionTools::AddLocomotionDestination(UWardrobeMenuData* Menu){
 if(!Menu)return false;const FSoftObjectPath Path(TEXT("/Game/BluePrint/ALS/Maps/L_ALSFirstDemo.L_ALSFirstDemo"));
 bool Found=false;for(const auto& Item:Menu->Levels)if(Item.Level.ToSoftObjectPath()==Path)Found=true;
 if(!Found){FWardrobeLevelOption Item;Item.Label=FText::FromString(TEXT("跑图试练"));Item.Level=TSoftObjectPtr<UWorld>(Path);Menu->Levels.Add(Item);}
 UE_LOG(LogTemp,Display,TEXT("ALS68_MENU_CONFIG menu=%s return=%s"),*Menu->GetPathName(),*Menu->MenuLevel.ToString());
 return ExplorerTools::Save(Menu);
}
bool UWardrobeLocomotionTools::UpdateLocomotionZoomInput(){
 using namespace ExplorerTools;const FString Root=TEXT("/Game/BluePrint/ALS/");
 auto* Data=LoadObject<UWardrobeLocomotionData>(nullptr,*(Root+TEXT("Data/DA_ALSLocomotion")));if(!Data||!Data->Mapping)return false;
 auto* Zoom=LoadObject<UInputAction>(nullptr,*(Root+TEXT("Input/IA_Zoom")));if(!Zoom)Zoom=Asset<UInputAction>(Root+TEXT("Input/IA_Zoom"));
 Zoom->ValueType=EInputActionValueType::Axis1D;Data->ZoomAction=Zoom;
 bool Found=false;for(const auto& Mapping:Data->Mapping->GetMappings())if(Mapping.Action==Zoom&&Mapping.Key==EKeys::MouseWheelAxis)Found=true;
 if(!Found)Data->Mapping->MapKey(Zoom,EKeys::MouseWheelAxis);
 return Save(Zoom)&&Save(Data->Mapping)&&Save(Data);
}
