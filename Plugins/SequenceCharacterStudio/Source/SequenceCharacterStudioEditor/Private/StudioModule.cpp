#include "Modules/ModuleManager.h"
#include "SequenceCharacterTools.h"
#include "SequenceCharacterRig.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/MorphTarget.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "ToolMenus.h"
#include "Framework/Docking/TabManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "PropertyCustomizationHelpers.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/StrongObjectPtr.h"
namespace {const FName StudioTab(TEXT("SequenceCharacterStudio"));}
class SCharacterStudio:public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SCharacterStudio){}SLATE_END_ARGS()
 void Construct(const FArguments&){
  ChildSlot[SNew(SVerticalBox)
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(STextBlock).Text(FText::FromString(TEXT("选择人物并打开 Sequence，然后添加表情轨道。\n表情：下面的数值会在当前时间自动打关键帧。\n身体动画请使用 UE 官方 Control Rig。"))).AutoWrapText(true)]
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SButton).Text(FText::FromString(TEXT("为选中人物添加 / 打开表情轨道"))).OnClicked(this,&SCharacterStudio::Attach)]
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(STextBlock).Text_Lambda([this](){return Status;}).AutoWrapText(true)]
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(STextBlock).Text(FText::FromString(TEXT("导入音频生成的表情动画（AnimSequence）")))]
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SObjectPropertyEntryBox).AllowedClass(UAnimSequence::StaticClass())
    .ObjectPath_Lambda([this](){return SourceAnimation.IsValid()?SourceAnimation->GetPathName():FString();})
    .OnObjectChanged_Lambda([this](const FAssetData& Asset){SourceAnimation.Reset(Cast<UAnimSequence>(Asset.GetAsset()));})]
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("起始帧 ")))]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SSpinBox<int32>).Value_Lambda([this](){return ImportStart;}).OnValueChanged_Lambda([this](int32 V){ImportStart=V;})]
    +SHorizontalBox::Slot().AutoWidth().Padding(8,0)[SNew(SButton).Text(FText::FromString(TEXT("使用当前帧"))).OnClicked_Lambda([this](){ImportStart=ULevelSequenceEditorBlueprintLibrary::GetLocalPosition().Frame.GetFrame().Value;return FReply::Handled();})]
    +SHorizontalBox::Slot().AutoWidth()[SNew(STextBlock).Text(FText::FromString(TEXT("强度 ")))]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SSpinBox<float>).MinValue(0.f).MaxValue(3.f).Delta(.05f).Value_Lambda([this](){return ImportGain;}).OnValueChanged_Lambda([this](float V){ImportGain=V;})]]
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SCheckBox).IsChecked_Lambda([this](){return bReplaceImport?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState V){bReplaceImport=V==ECheckBoxState::Checked;})[SNew(STextBlock).Text(FText::FromString(TEXT("替换导入区间内的同名表情关键帧（可撤销）")))]]
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SButton).Text(FText::FromString(TEXT("导入为可编辑的 BlendShape 关键帧"))).OnClicked_Lambda([this](){
    if(!Sequence.IsValid()||!Track.IsValid()||ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence()!=Sequence.Get()){Status=FText::FromString(TEXT("请先为当前 Sequence 的人物添加 / 打开表情轨道。"));return FReply::Handled();}
    const auto Result=USequenceCharacterTools::ImportAnimationMorphCurves(Sequence.Get(),Track.Get(),SourceAnimation.Get(),ImportStart,ImportGain,bReplaceImport);
    Status=FText::FromString(Result.Message);if(Result.bSuccess)ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();return FReply::Handled();})]
   +SVerticalBox::Slot().AutoHeight().Padding(6)[SNew(SSearchBox).HintText(FText::FromString(TEXT("搜索 BlendShape 名称"))).OnTextChanged_Lambda([this](const FText& T){Filter=T.ToString();Rebuild();})]
   +SVerticalBox::Slot().FillHeight(1)[SAssignNew(List,SScrollBox)]
  ];
 }
private:
 TWeakObjectPtr<ULevelSequence> Sequence;TWeakObjectPtr<UMovieSceneControlRigParameterTrack> Track;TWeakObjectPtr<USkeletalMeshComponent> Mesh;
 TSharedPtr<SScrollBox> List;FText Status;FString Filter;
 TStrongObjectPtr<UAnimSequence> SourceAnimation;int32 ImportStart=0;float ImportGain=1.f;bool bReplaceImport=true;
 FReply Attach(){
  auto* Seq=ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence();if(!Seq){Status=FText::FromString(TEXT("请先打开要编辑的 Level Sequence。"));return FReply::Handled();}
  AActor* Selected=nullptr;for(FSelectionIterator I(*GEditor->GetSelectedActors());I;++I)if(auto* A=Cast<AActor>(*I)){Selected=A;break;}
  if(!Selected){Status=FText::FromString(TEXT("请在关卡中选中人物 Actor。"));return FReply::Handled();}
  TArray<USkeletalMeshComponent*> Components;Selected->GetComponents(Components);USkeletalMeshComponent* Comp=nullptr;
  for(auto* C:Components)if(C->GetSkeletalMeshAsset()){if(Comp){Status=FText::FromString(TEXT("该 Actor 有多个 SkeletalMesh，请用蓝图/Python接口指定 Component。"));return FReply::Handled();}Comp=C;}
  auto* T=USequenceCharacterTools::AddMorphTrack(Seq,Comp);if(!T){Status=FText::FromString(TEXT("添加失败：检查模型 / 只读状态；旧混合轨道需先转移身体动画后重建表情轨道。"));return FReply::Handled();}
  Sequence=Seq;Track=T;Mesh=Comp;ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
  Status=FText::FromString(FString::Printf(TEXT("%s · %s\n保存 Sequence 即可保存动作和表情关键帧。"),*Selected->GetActorLabel(),*Seq->GetName()));Rebuild();return FReply::Handled();
 }
 void Rebuild(){
  if(!List)return;List->ClearChildren();if(!Mesh.IsValid()||!Mesh->GetSkeletalMeshAsset())return;
  for(const auto& Morph:Mesh->GetSkeletalMeshAsset()->GetMorphTargets()){
   const FName Name=Morph->GetFName();if(!Filter.IsEmpty()&&!Name.ToString().Contains(Filter))continue;
   List->AddSlot().Padding(6,2)[SNew(SHorizontalBox)
    +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromName(Name))]
    +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(110)[SNew(SSpinBox<float>).MinValue(-1.f).MaxValue(1.f).MinSliderValue(TOptional<float>()).MaxSliderValue(TOptional<float>()).Delta(0.f).MinFractionalDigits(3).MaxFractionalDigits(6)
     .Value_Lambda([this,Name](){if(auto* T=Track.Get())if(auto* R=T->GetControlRig())return R->GetControlValue(UFKControlRig::GetControlName(Name,ERigElementType::Curve)).Get<float>();return 0.f;})
     .OnValueChanged_Lambda([this,Name](float V){if(Sequence.IsValid()&&Track.IsValid()&&ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence()==Sequence.Get()){
      const int32 Frame=ULevelSequenceEditorBlueprintLibrary::GetLocalPosition().Frame.GetFrame().Value;
      if(USequenceCharacterTools::KeyMorph(Sequence.Get(),Track.Get(),Name,Frame,V))ULevelSequenceEditorBlueprintLibrary::RefreshCurrentLevelSequence();
     }})]]
   ];
  }
 }
};
class FStudioEditorModule:public IModuleInterface {
public:
 void StartupModule()override{
  FGlobalTabmanager::Get()->RegisterNomadTabSpawner(StudioTab,FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&){return SNew(SDockTab).TabRole(ETabRole::NomadTab)[SNew(SCharacterStudio)];})).SetDisplayName(FText::FromString(TEXT("Sequence Character Studio"))).SetMenuType(ETabSpawnerMenuType::Hidden);
  UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this,&FStudioEditorModule::Menus));
 }
 void ShutdownModule()override{UToolMenus::UnRegisterStartupCallback(this);UToolMenus::UnregisterOwner(this);FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(StudioTab);}
 void Menus(){FToolMenuOwnerScoped Owner(this);auto* Menu=UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));Menu->FindOrAddSection(TEXT("CharacterAnimation")).AddMenuEntry(TEXT("SequenceCharacterStudio"),FText::FromString(TEXT("Sequence Character Studio")),FText::FromString(TEXT("直接在 Sequencer 编辑骨骼与 BlendShape")),FSlateIcon(),FUIAction(FExecuteAction::CreateLambda([](){FGlobalTabmanager::Get()->TryInvokeTab(StudioTab);})));}
};
IMPLEMENT_MODULE(FStudioEditorModule,SequenceCharacterStudioEditor)