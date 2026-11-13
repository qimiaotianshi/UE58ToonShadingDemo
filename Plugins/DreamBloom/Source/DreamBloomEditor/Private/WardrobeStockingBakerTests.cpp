#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "WardrobeStockingBaker.h"
#include "WardrobeStocking.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "ToolMenus.h"
#include "ContentBrowserMenuContexts.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStockingEditorValidation,"DreamBloom.Stockings.Editor.ValidationAndBake",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStockingEditorValidation::RunTest(const FString&){
 auto* Body=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Model/Daniya/Red/DaniyaToUe.DaniyaToUe"));
 auto* Original=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Model/Daniya/Red/Stockings/SK_Daniya_Stockings.SK_Daniya_Stockings"));
 if(!TestNotNull(TEXT("Body exists"),Body)||!TestNotNull(TEXT("Independent stocking exists"),Original))return false;
 FText Error;const FString Base=TEXT("/Game/StockingNativeEditorTests/T_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
 TestFalse(TEXT("Missing mesh is rejected"),WardrobeStockingBaker::Validate(nullptr,Body,Base+TEXT("/Surface"),false,Error));
 TestFalse(TEXT("Body cannot be used as a stocking"),WardrobeStockingBaker::Validate(Body,Body,Base+TEXT("/Surface"),false,Error));
 TestFalse(TEXT("Disk paths are rejected"),WardrobeStockingBaker::Validate(Original,Body,TEXT("C:/Stockings/Test.uasset"),false,Error));
 TestFalse(TEXT("Other asset types cannot be overwritten"),WardrobeStockingBaker::Validate(Original,Body,TEXT("/Game/Model/Daniya/Red/DaniyaToUe"),true,Error));
 TestTrue(TEXT("Fresh Surface path is accepted"),WardrobeStockingBaker::Validate(Original,Body,Base+TEXT("/Surface"),false,Error));
 auto* Skeleton=DuplicateObject<USkeleton>(Original->GetSkeleton(),CreatePackage(*(Base+TEXT("/Skeleton"))),TEXT("Skeleton"));
 auto* Mesh=DuplicateObject<USkeletalMesh>(Original,CreatePackage(*(Base+TEXT("/Stocking"))),TEXT("Stocking"));
 Mesh->SetSkeleton(Skeleton);
 auto* Surface=WardrobeStockingBaker::Bake(Mesh,Body,Base+TEXT("/Surface"),false,Error);
 if(!TestNotNull(TEXT("Native tool bakes real character data"),Surface))return false;
 TestTrue(TEXT("Dense geometry survives baking"),Surface->Vertices.Num()>4000&&Surface->Triangles.Num()>12000);
 TestEqual(TEXT("Surface matches chosen body"),Surface->BodyMesh.Get(),Body);
 TestEqual(TEXT("Surface stores chosen textile"),Surface->StockingMesh.Get(),Mesh);
 TestFalse(TEXT("Existing Surface needs explicit overwrite"),WardrobeStockingBaker::Validate(Mesh,Body,Base+TEXT("/Surface"),false,Error));
 TestTrue(TEXT("Overwrite checkbox permits rebaking"),WardrobeStockingBaker::Validate(Mesh,Body,Base+TEXT("/Surface"),true,Error));
 return true;
}

class FWardrobeStockingWindowShot:public IAutomationLatentCommand {
public:
 FWardrobeStockingWindowShot(TSharedPtr<SWindow> InWindow,FAutomationTestBase* InTest):Window(InWindow),Test(InTest),Started(FPlatformTime::Seconds()){}
 bool Update() override{
  if(FPlatformTime::Seconds()-Started<1)return false;
  TArray<FColor> Pixels;FIntVector Size=FIntVector::ZeroValue;
  const bool Captured=Window.IsValid()&&FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size);
  Test->TestTrue(TEXT("Native Slate baker window renders"),Captured);
  if(Captured){
   TArray64<uint8> PNG;FImageUtils::PNGCompressImageArray(Size.X,Size.Y,TArrayView64<const FColor>(Pixels.GetData(),Pixels.Num()),PNG);
   const FString Path=FPaths::ProjectSavedDir()/TEXT("StockingPreview/NativeStockingBaker.png");
   IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
   Test->TestTrue(TEXT("Native tool screenshot is saved"),FFileHelper::SaveArrayToFile(PNG,*Path));
  }
  if(Window)Window->RequestDestroyWindow();Window.Reset();return true;
 }
private:
 TSharedPtr<SWindow> Window;FAutomationTestBase* Test;double Started;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeStockingEditorMenu,"DreamBloom.Stockings.Editor.NativeMenuAndWindow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWardrobeStockingEditorMenu::RunTest(const FString&){
 auto HasEntry=[](UToolMenu* Menu,FName SectionName,FName Entry){auto* Section=Menu?Menu->FindSection(SectionName):nullptr;return Section&&Section->FindEntry(Entry);};
 WardrobeStockingBaker::RegisterMenus();
 auto* Tools=UToolMenus::Get()->FindMenu(TEXT("LevelEditor.MainMenu.Tools"));
 TestTrue(TEXT("Tools menu exposes native stocking baker"),HasEntry(Tools,TEXT("DreamBloom"),TEXT("DreamBloomStockingBaker")));
 auto* Menu=UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(USkeletalMesh::StaticClass());
 auto* Selection=NewObject<UContentBrowserAssetContextMenuContext>();
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Model/Daniya/Red/Stockings/SK_Daniya_Stockings.SK_Daniya_Stockings"));
 Selection->SelectedAssets.Add(FAssetData(Mesh));FToolMenuContext Context;Context.AddObject(Selection);
 auto* Generated=UToolMenus::Get()->GenerateMenu(Menu->MenuName,Context);
 TestTrue(TEXT("Single Skeletal Mesh context exposes baking action"),HasEntry(Generated,TEXT("GetAssetActions"),TEXT("DreamBloomBakeStocking")));
 Selection->SelectedAssets.Add(FAssetData(Mesh));
 auto* Multiple=UToolMenus::Get()->GenerateMenu(Menu->MenuName,Context);
 TestTrue(TEXT("Multiple selection does not offer an ambiguous bake"),Multiple&&!HasEntry(Multiple,TEXT("GetAssetActions"),TEXT("DreamBloomBakeStocking")));
 if(!IsRunningCommandlet()&&FSlateApplication::IsInitialized()){
  auto Window=WardrobeStockingBaker::OpenWindow(Mesh);
  TestTrue(TEXT("Native window opens without Python"),Window.IsValid());
  if(Window)ADD_LATENT_AUTOMATION_COMMAND(FWardrobeStockingWindowShot(Window,this));
 }
 return true;
}
#endif
