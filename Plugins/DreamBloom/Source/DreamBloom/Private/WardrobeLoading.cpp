#include "WardrobeLoading.h"
#include "WardrobeLocomotion.h"
#include "WardrobeShowcase.h"
#include "WardrobeMenu.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "MoviePlayer.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/LevelStreaming.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "WorldPartition/WorldPartitionSubsystem.h"
#include "ContentStreaming.h"
#include "RenderingThread.h"
#include "AssetCompilingManager.h"
#include "ShaderCompiler.h"
#include "ShaderPipelineCache.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Styling/CoreStyle.h"
#include "Fonts/CompositeFont.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include <atomic>

// MoviePlayer can paint on its Slate thread while OpenLevel blocks the game thread.
// The widgets read only this immutable presentation data and atomic status values, never UObjects.
struct FWardrobeLoadingVisual
{
 FSlateBrush Image;
 TSharedPtr<FCompositeFont> Font;
 FText Destination, Hint;
 std::atomic<int32> Phase{0}; // 0 package load (unknown total), 1 warmup, 2 complete
 std::atomic<float> Progress{0.65f};
 std::atomic<bool> Slow{false};
 bool HasImage=false;
};

class SWardrobeLoadingScreen : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SWardrobeLoadingScreen) {}
  SLATE_ARGUMENT(TSharedPtr<FWardrobeLoadingVisual>, Visual)
  SLATE_EVENT(FSimpleDelegate, OnContinue)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args)
 {
  Visual=Args._Visual;
  const FSlateBrush* Solid=FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));
  const FLinearColor Gold(.94f,.74f,.40f,1.f);
  ProgressStyle=FCoreStyle::Get().GetWidgetStyle<FProgressBarStyle>(TEXT("ProgressBar"));
  ProgressStyle.SetFillImage(*Solid);
  ChildSlot[
   SNew(SOverlay)
   +SOverlay::Slot()[SNew(SImage).Image(Solid).ColorAndOpacity(FLinearColor(.012f,.027f,.061f,1))]
   +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
    [SNew(SImage).Image(&Visual->Image).ColorAndOpacity(FLinearColor::White).Visibility(Visual->HasImage?EVisibility::HitTestInvisible:EVisibility::Collapsed)]]
   +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(64,48)
    [SNew(STextBlock).Text(FText::FromString(TEXT("奇境漫游   /   JOURNEY"))).Font(FSlateFontInfo(Visual->Font,17)).ColorAndOpacity(Gold)]
   +SOverlay::Slot().VAlign(VAlign_Bottom)
   [SNew(SBorder).BorderImage(Solid).BorderBackgroundColor(FLinearColor(.009f,.018f,.039f,.92f)).Padding(FMargin(64,28,64,38))
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)
      [SNew(STextBlock).Text(Visual->Destination).Font(FSlateFontInfo(Visual->Font,28)).ColorAndOpacity(FLinearColor(.96f,.96f,1))]
     +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)
      [SNew(STextBlock).Text(Visual->Hint).Font(FSlateFontInfo(Visual->Font,14)).ColorAndOpacity(FLinearColor(.68f,.77f,.89f)).AutoWrapText(true)]
     +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,10)
      [SNew(STextBlock).Text_Lambda([V=Visual](){
       if(V->Slow.load())return FText::FromString(TEXT("场景准备时间较长，正在继续加载…"));
       return FText::FromString(V->Phase.load()==0?TEXT("正在读取地图…"):V->Phase.load()==1?TEXT("正在准备画面…"):TEXT("准备就绪"));
      }).Font(FSlateFontInfo(Visual->Font,14)).ColorAndOpacity(FLinearColor(.90f,.94f,1))]
     +SVerticalBox::Slot().AutoHeight()
      [SNew(SBox).HeightOverride(4)[SNew(SProgressBar).Style(&ProgressStyle).Percent_Lambda([V=Visual](){return V->Phase.load()==0?TOptional<float>():TOptional<float>(V->Progress.load());}).FillColorAndOpacity(Gold)]]
     +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,14,0,0)
      [SNew(SButton).Visibility_Lambda([V=Visual](){return V->Slow.load()?EVisibility::Visible:EVisibility::Collapsed;})
       .OnClicked_Lambda([Fn=Args._OnContinue](){Fn.ExecuteIfBound();return FReply::Handled();})
       [SNew(STextBlock).Text(FText::FromString(TEXT("继续进入（部分资源可能仍在加载）"))).Font(FSlateFontInfo(Visual->Font,13))]]
    ]]
  ];
 }
 virtual bool SupportsKeyboardFocus() const override { return true; }
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override { return FReply::Handled(); }
 virtual FReply OnKeyUp(const FGeometry&,const FKeyEvent&) override { return FReply::Handled(); }
 virtual FReply OnKeyChar(const FGeometry&,const FCharacterEvent&) override { return FReply::Handled(); }
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
 virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent&) override { return FReply::Handled(); }
private:
 TSharedPtr<FWardrobeLoadingVisual> Visual;
 FProgressBarStyle ProgressStyle;
};

void UWardrobeLoadingSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);
 if(IsRunningDedicatedServer()||IsRunningCommandlet())return;
 PostLoadHandle=FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this,&UWardrobeLoadingSubsystem::OnPostLoadMap);
 FailureHandle=GEngine->OnTravelFailure().AddUObject(this,&UWardrobeLoadingSubsystem::OnTravelFailure);
 if(IsMoviePlayerEnabled())GetMoviePlayer()->OnMoviePlaybackStarted().AddWeakLambda(this,[this](){if(bActive)UE_LOG(LogTemp,Display,TEXT("WARDROBE_LOADING_MOVIE_STARTED"));});
 TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UWardrobeLoadingSubsystem::Tick));
}

void UWardrobeLoadingSubsystem::Deinitialize()
{
 Finish(true);
 FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
 FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadHandle);
 if(GEngine)GEngine->OnTravelFailure().Remove(FailureHandle);
 if(IsMoviePlayerEnabled())GetMoviePlayer()->OnMoviePlaybackStarted().RemoveAll(this);
 Super::Deinitialize();
}

bool UWardrobeLoadingSubsystem::TravelToLevel(TSoftObjectPtr<UWorld> Level,UWardrobeMenuData* Settings,
 TSoftObjectPtr<UTexture2D> ImageOverride,FText Destination,FString Options)
{
 if(bActive||!GetWorld()||!GetGameInstance()->GetGameViewportClient()||Level.IsNull())return false;
 const FString Target=Level.ToSoftObjectPath().GetLongPackageName();
 if(!FPackageName::DoesPackageExist(Target))return false;
 if(!Settings)Settings=LoadObject<UWardrobeMenuData>(nullptr,TEXT("/Game/Wardrobe/DA_WardrobeMenu.DA_WardrobeMenu"));
 MinDisplay=Settings?FMath::Max(0.f,Settings->MinimumLoadingScreenTime):1.5f;
 Warmup=Settings?FMath::Max(.5f,Settings->SceneWarmupTime):2.f;
 SlowWarning=Settings?FMath::Max(5.f,Settings->SlowLoadingWarningTime):45.f;
 SettleTime=Settings?FMath::Max(.1f,Settings->TravelSettleTime):.35f;
 SettleFramesRequired=Settings?FMath::Clamp(Settings->TravelSettleFrames,1,30):3;
 if(ImageOverride.IsNull()&&Settings)ImageOverride=Settings->LoadingBackground;
 LoadingTexture=ImageOverride.LoadSynchronous();
 if(LoadingTexture){
  LoadingTexture->SetForceMipLevelsToBeResident(30.f);
  LoadingTexture->WaitForStreaming();
 }
 Visual=MakeShared<FWardrobeLoadingVisual>();
 Visual->HasImage=LoadingTexture!=nullptr;
 Visual->Image.SetResourceObject(LoadingTexture);
 Visual->Image.ImageSize=LoadingTexture?FVector2D(LoadingTexture->GetSizeX(),LoadingTexture->GetSizeY()):FVector2D(1920,1080);
 Visual->Image.DrawAs=ESlateBrushDrawType::Image;
 Visual->Image.TintColor=FSlateColor(FLinearColor::White);
 Visual->Font=MakeShared<FCompositeFont>(NAME_None,IPluginManager::Get().FindPlugin(TEXT("DreamBloom"))->GetBaseDir()/TEXT("Resources/NotoSansSC-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
 Visual->Destination=Destination.IsEmpty()?FText::FromString(TEXT("下一段旅程")):Destination;
 Visual->Hint=Settings?Settings->LoadingHint:FText::FromString(TEXT("新的风景，即将展开。"));
 PackageName=Target;TravelOptions=Options;
 bActive=true;bTravelIssued=false;bSettled=false;bMapLoaded=false;bAdditionalLoading=false;bManualContinue=false;
 WarmupFrames=0;SettleFrames=0;ReadyAt=0;CompleteAt=0;StartedAt=FPlatformTime::Seconds();bQAScreenshot=false;
 PreviousFocus=FSlateApplication::Get().GetKeyboardFocusedWidget();
 Overlay=SNew(SWardrobeLoadingScreen).Visual(Visual).OnContinue(FSimpleDelegate::CreateWeakLambda(this,[this](){bManualContinue=true;}));
 ShowOverlay();GuardInput();
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_LOADING_BEGIN target=%s image=%s settle=%.2fs/%d"),*PackageName,LoadingTexture?*LoadingTexture->GetPathName():TEXT("fallback"),SettleTime,SettleFramesRequired);
 return true;
}

void UWardrobeLoadingSubsystem::ShowOverlay()
{
 if(auto* Viewport=GetGameInstance()->GetGameViewportClient()){
  Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
  Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(),10000);
 }
}

void UWardrobeLoadingSubsystem::GuardInput()
{
 if(!FSlateApplication::IsInitialized())return;
 // Reacquire focus after the destination's BeginPlay creates its own HUD.
 if(FSlateApplication::Get().GetKeyboardFocusedWidget()!=Overlay){
  PreviousFocus=FSlateApplication::Get().GetKeyboardFocusedWidget();
  FSlateApplication::Get().ReleaseAllPointerCapture();
  FSlateApplication::Get().SetAllUserFocus(Overlay,EFocusCause::SetDirectly);
 }
 APlayerController* PC=GetGameInstance()->GetFirstLocalPlayerController();
 if(PC&&GuardedController!=PC){
  if(GuardedController.IsValid()){
   GuardedController->SetIgnoreMoveInput(false);GuardedController->SetIgnoreLookInput(false);
   GuardedController->bShowMouseCursor=bSavedCursor;
  }
  GuardedController=PC;bSavedCursor=PC->bShowMouseCursor;
  PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);PC->FlushPressedKeys();
 }
 if(PC)PC->bShowMouseCursor=Visual->Slow.load();
}

void UWardrobeLoadingSubsystem::OnPostLoadMap(UWorld* World)
{
 if(!bActive||!World||World->GetGameInstance()!=GetGameInstance())return;
 DestinationWorld=World;bMapLoaded=true;LoadedAt=FPlatformTime::Seconds();
 Visual->Phase.store(1);ShowOverlay();GuardInput();
 if(CompletedTravels==0&&FParse::Param(FCommandLine::Get(),TEXT("WardrobeLoadingEdgeQA"))){
  bAdditionalLoading=true;SlowWarning=5.f;
 }
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_LOADING_MAP_READY target=%s movie=%d"),*PackageName,IsMoviePlayerEnabled()&&GetMoviePlayer()->IsMovieCurrentlyPlaying());
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeAudioQA")))GetGameInstance()->GetSubsystem<UWardrobeAudioSubsystem>()->AuditTravelAudio();
}

bool UWardrobeLoadingSubsystem::IsSceneReady(UWorld* World) const
{
 if(!World||!World->HasBegunPlay()||bAdditionalLoading||IsAsyncLoading()||World->IsVisibilityRequestPending())return false;
 for(const ULevelStreaming* Level:World->GetStreamingLevels()){
  if(!Level)continue;
  if(Level->HasLoadRequestPending()||(Level->ShouldBeLoaded()&&(!Level->GetLoadedLevel()||(Level->ShouldBeVisible()&&!Level->IsLevelVisible()))))return false;
 }
 if(auto* Partition=World->GetSubsystem<UWorldPartitionSubsystem>())if(!Partition->IsStreamingCompleted())return false;
 if(IStreamingManager::Get().GetNumWantingResources()>0)return false;
 if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())return false;
 if(FAssetCompilingManager::Get().GetNumRemainingAssets()>0)return false;
 if(FShaderPipelineCache::NumPrecompilesRemaining()>0)return false;
 return true;
}

bool UWardrobeLoadingSubsystem::Tick(float DeltaSeconds)
{
 if(!bActive)return true;
 const double Now=FPlatformTime::Seconds();
 GuardInput();
 if(!bTravelIssued){
  // Let the viewport present the opaque cover before entering blocking engine travel.
  if(Now-StartedAt<CoverHold)return true;
  // [Settle] Hold the cover for a few more frames and then flush before travelling. OpenLevel tears
  // the source world down synchronously (CleanupWorld -> RemovePrimitiveSceneInfo ->
  // RemoveCachedMeshDrawCommands), and that teardown indexes Scene->CachedMeshDrawCommandStateBuckets
  // by ids recorded when the commands were cached. If the render thread still has queued primitive
  // updates from the outgoing world, a bucket can already be gone and the lookup returns garbage.
  // Draining both streaming and the render command queue first makes the teardown start from a
  // quiet scene instead of racing the tail of the previous frame.
  if(!bSettled){
   ++SettleFrames;
   if(SettleFrames<SettleFramesRequired||Now-StartedAt<SettleTime)return true;
   bSettled=true;
  }
  bTravelIssued=true;
  // PIE does not support MoviePlayer. Its viewport overlay still covers warmup.
  if(GetWorld()->WorldType!=EWorldType::PIE&&IsMoviePlayerEnabled()){
   FLoadingScreenAttributes Attr;
   Attr.bAutoCompleteWhenLoadingCompletes=true;Attr.bMoviesAreSkippable=false;
   Attr.MinimumLoadingScreenDisplayTime=-1;
   Attr.WidgetLoadingScreen=SNew(SWardrobeLoadingScreen).Visual(Visual);
   GetMoviePlayer()->SetupLoadingScreen(Attr);
   UE_LOG(LogTemp,Display,TEXT("WARDROBE_LOADING_MOVIE_PREPARED"));
  }
  // Last thing before the blocking travel: the loading screen above is already up, so a short
  // game thread stall here is invisible to the player.
  SettleBeforeTravel();
  UGameplayStatics::OpenLevel(GetGameInstance(),FName(*PackageName),true,TravelOptions);
  return true;
 }
 if(!bMapLoaded)return true;
 ++WarmupFrames;
 const double Elapsed=Now-LoadedAt;
 if(CompletedTravels==0&&FParse::Param(FCommandLine::Get(),TEXT("WardrobeLoadingEdgeQA"))&&Elapsed>6&&bAdditionalLoading){
  check(CompleteAt==0&&Visual->Slow.load());
  bAdditionalLoading=false;
  UE_LOG(LogTemp,Display,TEXT("WARDROBE_LOADING_HOLD_AND_SLOW_QA_PASSED"));
 }
 const bool bReady=WarmupFrames>=12&&Elapsed>=Warmup&&IsSceneReady(DestinationWorld.Get());
 if(bReady){if(ReadyAt==0)ReadyAt=Now;}else ReadyAt=0;
 // A continuously quiet period avoids uncovering between separate streaming batches.
 const bool bCanFinish=(ReadyAt>0&&Now-ReadyAt>=.5)||bManualContinue;
 if(bCanFinish&&Now-StartedAt>=MinDisplay){
  if(CompleteAt==0){CompleteAt=Now;Visual->Phase.store(2);Visual->Progress.store(1.f);Visual->Slow.store(false);}
  if(Now-CompleteAt>=.25){Finish();return true;}
 }else{
  CompleteAt=0;Visual->Phase.store(1);
  const float PhaseProgress=.65f+.29f*FMath::Clamp(float(Elapsed/(Warmup+1.f)),0.f,1.f);
  Visual->Progress.store(FMath::Max(Visual->Progress.load(),PhaseProgress));
  Visual->Slow.store(Elapsed>SlowWarning);
 }
 if(!bQAScreenshot&&Elapsed>.35&&FParse::Param(FCommandLine::Get(),TEXT("WardrobeLoadingQA"))){
  const FString Dir=FPaths::ProjectSavedDir()/TEXT("WardrobeLoadingQA");IFileManager::Get().MakeDirectory(*Dir,true);
  FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("loading-%02d.png"),CompletedTravels+1),true,false);
  bQAScreenshot=true;
 }
 return true;
}

void UWardrobeLoadingSubsystem::SettleBeforeTravel()
{
 const double Begin=FPlatformTime::Seconds();
 const int32 StreamingBefore=IStreamingManager::Get().GetNumWantingResources();
 // Level streaming creates and destroys primitives on the render thread. Finish the outstanding
 // requests now instead of letting them land in the middle of the travel teardown.
 if(UWorld* World=GetWorld())World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
 // Drain everything already queued for the render thread (transform updates, primitive add/remove,
 // cached mesh draw command removal) so the world teardown does not index a bucket that is gone.
 FlushRenderingCommands();
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_LOADING_SETTLED target=%s frames=%d streaming=%d flush=%.2fms"),
  *PackageName,SettleFrames,StreamingBefore,(FPlatformTime::Seconds()-Begin)*1000.0);
}

void UWardrobeLoadingSubsystem::Finish(bool bFailed)
{
 if(!bActive)return;
 if(Overlay)if(auto* Viewport=GetGameInstance()->GetGameViewportClient())Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
 if(GuardedController.IsValid()){
  GuardedController->SetIgnoreMoveInput(false);GuardedController->SetIgnoreLookInput(false);
  GuardedController->bShowMouseCursor=bSavedCursor;
 }
 GuardedController.Reset();
 if(FSlateApplication::IsInitialized()){
  TArray<UUserWidget*> Menus;
  UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetGameInstance(),Menus,UWardrobeMenu::StaticClass(),true);
  if(Menus.Num()>0)CastChecked<UWardrobeMenu>(Menus[0])->RestoreMenuFocus();
  else if(auto Focus=PreviousFocus.Pin()){
   FWidgetPath Path;FSlateApplication::Get().GeneratePathToWidgetUnchecked(Focus.ToSharedRef(),Path);
   if(Path.IsValid())FSlateApplication::Get().SetAllUserFocus(Focus,EFocusCause::SetDirectly);
   else FSlateApplication::Get().SetAllUserFocusToGameViewport();
  }else FSlateApplication::Get().SetAllUserFocusToGameViewport();
 }
 bActive=false;
 if(!bFailed)if(auto* PC=Cast<AWardrobeExplorerController>(GetGameInstance()->GetFirstLocalPlayerController()))PC->SetExplorerInput();
 if(!bFailed)++CompletedTravels;
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_LOADING_END target=%s failed=%d manual=%d frames=%u settleFrames=%d seconds=%.2f completed=%d"),*PackageName,bFailed,bManualContinue,WarmupFrames,SettleFrames,FPlatformTime::Seconds()-StartedAt,CompletedTravels);
 Overlay.Reset();DestinationWorld.Reset();PreviousFocus.Reset();
}

void UWardrobeLoadingSubsystem::OnTravelFailure(UWorld* World,ETravelFailure::Type Type,const FString& Error)
{
 if(!bActive||(World&&World->GetGameInstance()!=GetGameInstance()))return;
 UE_LOG(LogTemp,Error,TEXT("WARDROBE_LOADING_FAILED %s"),*Error);
 if(IsMoviePlayerEnabled()){
  GetMoviePlayer()->StopMovie();
  if(!GetMoviePlayer()->IsMovieCurrentlyPlaying())GetMoviePlayer()->SetupLoadingScreen(FLoadingScreenAttributes());
 }
 Finish(true);
}
