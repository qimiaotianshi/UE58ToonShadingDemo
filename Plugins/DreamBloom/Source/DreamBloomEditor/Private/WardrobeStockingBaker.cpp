#include "WardrobeStockingBaker.h"
#include "WardrobeStockingEditorTools.h"
#include "WardrobeStocking.h"
#include "Engine/SkeletalMesh.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserMenuContexts.h"
#include "IContentBrowserSingleton.h"
#include "PropertyCustomizationHelpers.h"
#include "ToolMenus.h"
#include "Editor.h"
#include "Misc/PackageName.h"
#include "Misc/ScopedSlowTask.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/SWindow.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "DreamBloomStockingBaker"

namespace WardrobeStockingBaker {
static FDelegateHandle StartupHandle;
static TWeakPtr<SWindow> BakerWindow;
static const FName MenuOwner(TEXT("DreamBloomStockingBaker"));

bool Validate(USkeletalMesh* Stocking,USkeletalMesh* Body,const FString& Path,bool AllowOverwrite,FText& Error){
 Error=FText::GetEmpty();
 if(!Stocking||!Body){Error=LOCTEXT("MissingMeshes","请选择丝袜骨骼网格和角色身体。");return false;}
 if(Stocking==Body){Error=LOCTEXT("BodyIsStocking","丝袜必须是独立网格，不能选择角色身体本身。");return false;}
 FText Reason;
 if(!Path.StartsWith(TEXT("/Game/"))||!FPackageName::IsValidLongPackageName(Path,true,&Reason)){
  Error=LOCTEXT("BadOutput","输出应为 /Game/Wardrobe/Stockings/DA_MyStockingSurface，不要填写 .uasset。");return false;
 }
 const FString ObjectPath=Path+TEXT(".")+FPackageName::GetLongPackageAssetName(Path);
 if(FPackageName::DoesPackageExist(Path)||FindObject<UObject>(nullptr,*ObjectPath)){
  auto* Existing=LoadObject<UObject>(nullptr,*ObjectPath);
  if(!Cast<UWardrobeStockingSurface>(Existing)){Error=LOCTEXT("WrongOutputType","输出路径已有其他类型的资源，请更换路径。");return false;}
  if(!AllowOverwrite){Error=LOCTEXT("OverwriteDisabled","Surface 已存在；重新烘焙请勾选“覆盖已有 Surface”。");return false;}
 }
 if(GEditor&&GEditor->PlayWorld){Error=LOCTEXT("NoBakeInPIE","请先停止运行或模拟，再烘焙丝袜。");return false;}
 return true;
}

UWardrobeStockingSurface* Bake(USkeletalMesh* Stocking,USkeletalMesh* Body,const FString& Path,bool AllowOverwrite,FText& Error){
 if(!Validate(Stocking,Body,Path,AllowOverwrite,Error))return nullptr;
 FScopedSlowTask Progress(1,LOCTEXT("BakingProgress","正在贴合皮肤并烘焙丝袜蒙皮数据…"));
 if(!IsRunningCommandlet()&&FSlateApplication::IsInitialized())Progress.MakeDialog();
 FAssetCompilingManager::Get().FinishAllCompilation();
 auto* Surface=UWardrobeStockingEditorTools::BakeSurface(Stocking,Body,Path);
 if(!Surface)Error=LOCTEXT("BakeFailed","烘焙失败：请检查骨骼名称、绑定姿势及 Output Log 中的 STOCKING_BAKE 信息。");
 return Surface;
}

class SStockingBakerPanel:public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SStockingBakerPanel){} SLATE_ARGUMENT(USkeletalMesh*,InitialStocking) SLATE_END_ARGS()
 void Construct(const FArguments& Args){
  if(Args._InitialStocking)StockingPath=FSoftObjectPath(Args._InitialStocking);
  BodyPath=FSoftObjectPath(TEXT("/Game/Model/Daniya/Red/DaniyaToUe.DaniyaToUe"));
  OutputPath=DefaultOutput(Args._InitialStocking);
  ChildSlot[
   SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).Padding(20)[
    SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Text(LOCTEXT("Intro","选择独立丝袜网格，生成可配置到角色 DA 的 Surface 数据。"))]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("StockingLabel","丝袜骨骼网格"))]
    +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,12)[
     SNew(SObjectPropertyEntryBox).AllowedClass(USkeletalMesh::StaticClass())
     .ObjectPath_Lambda([this](){return StockingPath.ToString();})
     .OnObjectChanged_Lambda([this](const FAssetData& Asset){StockingPath=Asset.GetSoftObjectPath();OutputPath=DefaultOutput(Cast<USkeletalMesh>(Asset.GetAsset()));Result=FText::GetEmpty();})
    ]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("BodyLabel","角色身体（当前烘焙器适配达妮娅）"))]
    +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,12)[
     SNew(SObjectPropertyEntryBox).AllowedClass(USkeletalMesh::StaticClass())
     .ObjectPath_Lambda([this](){return BodyPath.ToString();})
     .OnObjectChanged_Lambda([this](const FAssetData& Asset){BodyPath=Asset.GetSoftObjectPath();Result=FText::GetEmpty();})
    ]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(LOCTEXT("OutputLabel","输出 Surface 路径"))]
    +SVerticalBox::Slot().AutoHeight().Padding(0,4,0,12)[
     SNew(SEditableTextBox).Text_Lambda([this](){return FText::FromString(OutputPath);})
     .OnTextChanged_Lambda([this](const FText& Text){OutputPath=Text.ToString().TrimStartAndEnd();Result=FText::GetEmpty();})
    ]
    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[
     SNew(SCheckBox).IsChecked_Lambda([this](){return AllowOverwrite?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
     .OnCheckStateChanged_Lambda([this](ECheckBoxState State){AllowOverwrite=State==ECheckBoxState::Checked;})
     [SNew(STextBlock).Text(LOCTEXT("OverwriteLabel","覆盖已有 Surface（用于重新烘焙）"))]
    ]
    +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,14)[
     SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("ModelScope","适用于紧身丝袜。烘焙会贴合达妮娅皮肤，并保存丝袜网格、骨架和 Surface；丝袜的 Physics Asset 引用会被清空。"))
    ]
    +SVerticalBox::Slot().AutoHeight()[
     SNew(SHorizontalBox)
     +SHorizontalBox::Slot().AutoWidth().Padding(0,0,12,0)[SNew(SButton).Text(LOCTEXT("BakeButton","烘焙 Surface")).OnClicked(this,&SStockingBakerPanel::OnBake)]
     +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(LOCTEXT("FindResult","定位生成资源")).IsEnabled_Lambda([this](){return !BakedPath.IsEmpty();}).OnClicked(this,&SStockingBakerPanel::OnLocate)]
    ]
    +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,0)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){return Result;})]
   ]
  ];
 }
private:
 FSoftObjectPath StockingPath,BodyPath;
 FString OutputPath,BakedPath;
 FText Result;
 bool AllowOverwrite=false;
 static FString DefaultOutput(USkeletalMesh* Mesh){return TEXT("/Game/Wardrobe/Stockings/DA_")+FString(Mesh?*Mesh->GetName():TEXT("MyStocking"))+TEXT("_Surface");}
 FReply OnBake(){
  BakedPath.Reset();
  auto* Surface=Bake(Cast<USkeletalMesh>(StockingPath.TryLoad()),Cast<USkeletalMesh>(BodyPath.TryLoad()),OutputPath,AllowOverwrite,Result);
  if(Surface){
   BakedPath=Surface->GetPathName();
   Result=FText::Format(LOCTEXT("BakedResult","烘焙完成：{0}\n顶点 {1}，三角面 {2}\n在角色 DA 的 Stockings 中选择这个 Surface，开启 Enabled，并选择丝袜材质或材质实例。"),FText::FromString(OutputPath),FText::AsNumber(Surface->Vertices.Num()),FText::AsNumber(Surface->Triangles.Num()/3));
  }
  return FReply::Handled();
 }
 FReply OnLocate(){
  if(auto* Asset=LoadObject<UObject>(nullptr,*BakedPath)){TArray<UObject*> Assets{Asset};GEditor->SyncBrowserToObjects(Assets);}
  return FReply::Handled();
 }
};

TSharedPtr<SWindow> OpenWindow(USkeletalMesh* InitialStocking){
 if(!FSlateApplication::IsInitialized()||IsRunningCommandlet())return nullptr;
 if(auto Existing=BakerWindow.Pin()){Existing->BringToFront();return Existing;}
 auto Window=SNew(SWindow).Title(LOCTEXT("WindowTitle","DreamBloom · 丝袜烘焙"))
  .ClientSize(FVector2D(680,600)).SupportsMaximize(false).SupportsMinimize(false)
  [SNew(SStockingBakerPanel).InitialStocking(InitialStocking)];
 BakerWindow=Window;FSlateApplication::Get().AddWindow(Window);return Window;
}

static void OpenFromSelection(){
 TArray<FAssetData> Selected;FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
 OpenWindow(Selected.Num()==1?Cast<USkeletalMesh>(Selected[0].GetAsset()):nullptr);
}
static void OpenFromContext(const FToolMenuContext& Context){
 const auto* Selection=Context.FindContext<UContentBrowserAssetContextMenuContext>();
 if(Selection&&Selection->SelectedAssets.Num()==1)OpenWindow(Cast<USkeletalMesh>(Selection->SelectedAssets[0].GetAsset()));
}
void RegisterMenus(){
 FToolMenuOwnerScoped Owner(MenuOwner);
 auto* Tools=UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
 Tools->FindOrAddSection(TEXT("DreamBloom")).AddMenuEntry(TEXT("DreamBloomStockingBaker"),LOCTEXT("ToolMenuLabel","丝袜烘焙…"),LOCTEXT("ToolMenuTip","打开 DreamBloom 的原生 C++ 丝袜烘焙工具。"),FSlateIcon(),FUIAction(FExecuteAction::CreateStatic(&OpenFromSelection)));
 auto* Assets=UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(USkeletalMesh::StaticClass());
 Assets->FindOrAddSection(TEXT("GetAssetActions")).AddDynamicEntry(TEXT("DreamBloomStockingEntry"),FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& Section){
  const auto* Selection=Section.FindContext<UContentBrowserAssetContextMenuContext>();
  if(Selection&&Selection->SelectedAssets.Num()==1)Section.AddMenuEntry(TEXT("DreamBloomBakeStocking"),LOCTEXT("ContextLabel","烘焙丝袜 Surface…"),LOCTEXT("ContextTip","将这个独立骨骼网格烘焙成角色丝袜数据。"),FSlateIcon(),FToolMenuExecuteAction::CreateStatic(&OpenFromContext));
 }));
}
void Startup(){
 if(!IsRunningCommandlet())StartupHandle=UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&RegisterMenus));
}
void Shutdown(){
 if(StartupHandle.IsValid()){UToolMenus::UnRegisterStartupCallback(StartupHandle);StartupHandle.Reset();}
 if(auto Window=BakerWindow.Pin())Window->RequestDestroyWindow();BakerWindow.Reset();
 if(UToolMenus::IsToolMenuUIEnabled())UToolMenus::Get()->UnregisterOwnerByName(MenuOwner);
}
}

#undef LOCTEXT_NAMESPACE
