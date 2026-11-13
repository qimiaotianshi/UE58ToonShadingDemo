#include "WardrobeStartup.h"
#include "WardrobeStartupInternal.h"
#include "WardrobeShowcase.h"
#include "WardrobeMenu.h"
#include "WardrobeLoading.h"
#include "WardrobeAISpeechSubsystem.h"
#include "WardrobeAISpeech.h"
#include "WardrobeCharacters.h"
#include "MediaPlayer.h"
#include "MediaTexture.h"
#include "MediaSoundComponent.h"
#include "MoviePlayer.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Fonts/CompositeFont.h"
#include "Interfaces/IPluginManager.h"
#include "Rendering/DrawElements.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "EngineUtils.h"
#include "UnrealClient.h"

class SWardrobeStartupScreen : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SWardrobeStartupScreen) {}
  SLATE_ARGUMENT(TSharedPtr<FWardrobeStartupVisual>,Visual)
  SLATE_ARGUMENT(bool,NativeMovie)
  SLATE_EVENT(FSimpleDelegate,OnEnter)
 SLATE_END_ARGS()
 void Construct(const FArguments& Args)
 {
  Visual=Args._Visual;bNative=Args._NativeMovie;OnEnter=Args._OnEnter;
  ChildSlot[
   SNew(SOverlay)
   +SOverlay::Slot()[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor::Black)
    .Visibility_Lambda([this]{return !bNative||Visual->MovieDone.load()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})]
   +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SNew(SBox).WidthOverride_Lambda([this]{return FitSize.X;}).HeightOverride_Lambda([this]{return FitSize.Y;})
     [SNew(SImage).Image(&Visual->MovieBrush).ColorAndOpacity(FLinearColor::White)
      .Visibility_Lambda([this]{return bNative||Visual->MovieDone.load()?EVisibility::Collapsed:EVisibility::HitTestInvisible;})]]
   +SOverlay::Slot()[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
    .ColorAndOpacity_Lambda([this]{return FLinearColor(0,0,0,Visual->FadeBaked?0.f:Visual->Closing.load());})]
   +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(0,0,0,100)
    [SNew(STextBlock).Text_Lambda([this]{return FText::FromString(Visual->SceneReady.load()?TEXT("点击运行游戏"):TEXT("正在准备奇境…"));})
     .Font(FSlateFontInfo(Visual->Font,26))
     .ColorAndOpacity_Lambda([this]{
      const float Entrance=Visual->MovieDone.load()?1.f:Visual->Closing.load();
      const float Breath=.25f+.75f*float((FMath::Cos(FPlatformTime::Seconds()*2.5)+1.)*.5);
      return FLinearColor(1.f,.72f,.83f,Entrance*Breath);
     })
     .Visibility_Lambda([this]{return Visual->MovieDone.load()||(!Visual->FadeBaked&&!bNative&&Visual->Closing.load()>0)?EVisibility::HitTestInvisible:EVisibility::Collapsed;})]
  ];
 }
 virtual void Tick(const FGeometry& G,double Time,float Dt) override
 {
  SCompoundWidget::Tick(G,Time,Dt);FitSize=WardrobeStartup::FitVideo(G.GetLocalSize(),Visual->Aspect);
  if(bNative&&Visual->Duration>0&&Visual->FadeTime>0&&!Visual->MovieDone.load()){
   const double Remaining=Visual->Duration-(FPlatformTime::Seconds()-Visual->NativeStartedAt);
   const float X=FMath::Clamp(float(1.-Remaining/Visual->FadeTime),0.f,1.f);Visual->Closing.store(X*X*(3.f-2.f*X));
  }
 }
 virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override
 {
  const int32 Result=SCompoundWidget::OnPaint(Args,G,Clip,Out,Layer,Style,Enabled);
  // Native MoviePlayer draws below this overlay. Explicit bars enforce the authored frame ratio.
  if(bNative&&!Visual->MovieDone.load()){
   const FVector2D S=G.GetLocalSize(),F=WardrobeStartup::FitVideo(S,Visual->Aspect),Margin=(S-F)*.5;
   const auto* Solid=FCoreStyle::Get().GetBrush("WhiteBrush");
   auto Bar=[&](FVector2D P,FVector2D Size){if(Size.X>0&&Size.Y>0)FSlateDrawElement::MakeBox(Out,Result+1,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),Solid,ESlateDrawEffect::None,FLinearColor::Black);};
   Bar(FVector2D::ZeroVector,FVector2D(S.X,Margin.Y));Bar(FVector2D(0,S.Y-Margin.Y),FVector2D(S.X,Margin.Y));
   Bar(FVector2D::ZeroVector,FVector2D(Margin.X,S.Y));Bar(FVector2D(S.X-Margin.X,0),FVector2D(Margin.X,S.Y));
  }
  return Result+1;
 }
 virtual bool SupportsKeyboardFocus() const override {return true;}
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent& E) override
 {
  if(E.GetEffectingButton()==EKeys::LeftMouseButton&&Visual->CanEnter())OnEnter.ExecuteIfBound();
  return FReply::Handled();
 }
 virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override {return FReply::Handled();}
 virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override {return FReply::Handled();}
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent&) override {return FReply::Handled();}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override {return FReply::Handled();}
 virtual FReply OnKeyUp(const FGeometry&,const FKeyEvent&) override {return FReply::Handled();}
 virtual FReply OnKeyChar(const FGeometry&,const FCharacterEvent&) override {return FReply::Handled();}
private:
 TSharedPtr<FWardrobeStartupVisual> Visual;
 FSimpleDelegate OnEnter;
 FVector2D FitSize=FVector2D(1920,1080);
 bool bNative=false;
};

void UWardrobeStartupSubsystem::Initialize(FSubsystemCollectionBase& C)
{
 Super::Initialize(C);
 if(IsRunningDedicatedServer()||IsRunningCommandlet()||!FApp::CanEverRender())return;
 Settings=LoadObject<UWardrobeMenuData>(nullptr,TEXT("/Game/Wardrobe/DA_WardrobeMenu.DA_WardrobeMenu"));
 // In a standalone game this starts before the first synchronous map load and AI service setup.
 if(!GIsEditor)PrepareNativeMovie();
 TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&UWardrobeStartupSubsystem::Tick));
}
bool UWardrobeStartupSubsystem::PrepareNativeMovie()
{
 if(!Settings||!Settings->bPlayStartupMovie||!IsMoviePlayerEnabled())return false;
 MovieFile=WardrobeStartup::ResolveMovie(Settings->StartupMovie.FilePath,FPaths::ProjectContentDir());
 if(MovieFile.IsEmpty()||!FPaths::FileExists(MovieFile))return false;
 Visual=MakeShared<FWardrobeStartupVisual>();Visual->Aspect=Settings->StartupAspectRatio;
 Visual->FadeTime=Settings->StartupFadeTime;Visual->NativeStartedAt=FPlatformTime::Seconds();
 FString Meta;
 if(FFileHelper::LoadFileToString(Meta,*(MovieFile+TEXT(".startup.json")))){
  TSharedPtr<FJsonObject> Object;
  if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Meta),Object)&&Object){
   Object->TryGetNumberField(TEXT("duration"),Visual->Duration);Object->TryGetBoolField(TEXT("fade_baked"),Visual->FadeBaked);
  }
 }
 Visual->Font=MakeShared<FCompositeFont>(NAME_None,IPluginManager::Get().FindPlugin("DreamBloom")->GetBaseDir()/TEXT("Resources/NotoSansSC-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
 const auto V=Visual;
 ClipHandle=GetMoviePlayer()->OnMovieClipFinished().AddLambda([V](const FString&){V->MovieDone.store(true);});
 FinishedHandle=GetMoviePlayer()->OnMoviePlaybackFinished().AddUObject(this,&UWardrobeStartupSubsystem::NativeMovieFinished);
 FLoadingScreenAttributes Attr;
 FString Relative=MovieFile;FPaths::MakePathRelativeTo(Relative,*FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()/TEXT("Movies/")));
 Relative=FPaths::ChangeExtension(Relative,TEXT(""));Relative.RemoveFromEnd(TEXT("."));
 Attr.MoviePaths.Add(Relative);Attr.bAutoCompleteWhenLoadingCompletes=false;Attr.bMoviesAreSkippable=false;
 // Return to normal engine ticks after map + movie, then warm up under a separate opaque cover.
 // Never tick GEngine recursively inside LoadMap or wait for UObject readiness on MoviePlayer's Slate thread.
 Attr.WidgetLoadingScreen=SNew(SWardrobeStartupScreen).Visual(Visual).NativeMovie(true);
 GetMoviePlayer()->SetupLoadingScreen(Attr);
 bNativeMovie=GetMoviePlayer()->PlayMovie();bActive=bNativeMovie;
 if(!bNativeMovie){GetMoviePlayer()->OnMovieClipFinished().Remove(ClipHandle);GetMoviePlayer()->OnMoviePlaybackFinished().Remove(FinishedHandle);Visual.Reset();}
 if(bNativeMovie)UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_NATIVE_MOVIE_BEGIN"));
 return bNativeMovie;
}
void UWardrobeStartupSubsystem::NativeMovieFinished()
{
 if(Visual)Visual->MovieDone.store(true);
 bNativeMovie=false;
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_NATIVE_MOVIE_END"));
}
void UWardrobeStartupSubsystem::BeginForMenu(UWardrobeMenu* Menu)
{
 if(!Menu||!Menu->IsMenuMap()||bCompleted||MenuOwner.IsValid()||IsRunningCommandlet()||!FApp::CanEverRender())return;
 Settings=Menu->Data;MenuOwner=Menu;
 if(!Settings||!Settings->bPlayStartupMovie){bCompleted=true;return;}
 if(!Visual){
  Visual=MakeShared<FWardrobeStartupVisual>();Visual->Aspect=Settings->StartupAspectRatio;
  Visual->FadeTime=Settings->StartupFadeTime;
  Visual->Font=MakeShared<FCompositeFont>(NAME_None,IPluginManager::Get().FindPlugin("DreamBloom")->GetBaseDir()/TEXT("Resources/NotoSansSC-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  MovieFile=WardrobeStartup::ResolveMovie(Settings->StartupMovie.FilePath,FPaths::ProjectContentDir());
  FString Meta;
  if(FFileHelper::LoadFileToString(Meta,*(MovieFile+TEXT(".startup.json")))){
   TSharedPtr<FJsonObject> Object;
   if(FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Meta),Object)&&Object)Object->TryGetBoolField(TEXT("fade_baked"),Visual->FadeBaked);
  }
 }
 bActive=true;StartedAt=FPlatformTime::Seconds();ReadyAt=0;WarmupFrames=0;
 if(!bNativeMovie&&!Visual->MovieDone.load())BeginMediaMovie();
 Overlay=SNew(SWardrobeStartupScreen).Visual(Visual).NativeMovie(bNativeMovie).OnEnter(FSimpleDelegate::CreateWeakLambda(this,[this]{EnterGame();}));
 if(auto* Viewport=GetGameInstance()->GetGameViewportClient())Viewport->AddViewportWidgetContent(Overlay.ToSharedRef(),20000);
 GuardInput();
 bool HasAI=false;
 if(Settings->CharacterData)for(const auto& Character:Settings->CharacterData->Characters)if(Character.AISpeech&&Character.AISpeech->bEnabled){
  HasAI=true;const FString Endpoint=Character.AISpeech->TTSEndpoint;
  if(Endpoint.EndsWith(TEXT("/tts"))){HealthURL=Endpoint.LeftChop(4)+TEXT("/wardrobe/status");bTTSReady=false;}
  break;
 }
 if(HasAI)if(auto* AI=GetGameInstance()->GetSubsystem<UWardrobeAISpeechSubsystem>())AI->Prewarm();
 UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_BEGIN aspect=%.4f native=%d"),Visual->Aspect,bNativeMovie);
}
void UWardrobeStartupSubsystem::BeginMediaMovie()
{
 if(MovieFile.IsEmpty()||!FPaths::FileExists(MovieFile)){MediaFailed(TEXT(""));return;}
 MediaPlayer=NewObject<UMediaPlayer>(this);MediaPlayer->PlayOnOpen=true;MediaPlayer->SetLooping(false);
 MediaPlayer->OnMediaOpened.AddDynamic(this,&UWardrobeStartupSubsystem::MediaOpened);
 MediaPlayer->OnMediaOpenFailed.AddDynamic(this,&UWardrobeStartupSubsystem::MediaFailed);
 MediaPlayer->OnEndReached.AddDynamic(this,&UWardrobeStartupSubsystem::MediaEnded);
 MediaTexture=NewObject<UMediaTexture>(this);MediaTexture->AutoClear=false;MediaTexture->NewStyleOutput=false;MediaTexture->SetMediaPlayer(MediaPlayer);MediaTexture->UpdateResource();
 Visual->MovieBrush.SetResourceObject(MediaTexture);Visual->MovieBrush.ImageSize=FVector2D(1920,1080);Visual->MovieBrush.DrawAs=ESlateBrushDrawType::Image;
 if(auto* World=GetWorld()){
  MediaSound=NewObject<UMediaSoundComponent>(this);MediaSound->bIsUISound=true;MediaSound->SetMediaPlayer(MediaPlayer);MediaSound->RegisterComponentWithWorld(World);MediaSound->Start();
 }
 if(!MediaPlayer->OpenFile(MovieFile))MediaFailed(TEXT(""));
}
void UWardrobeStartupSubsystem::MediaOpened(FString URL){MediaOpenedAt=FPlatformTime::Seconds();UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_MEDIA_OPENED"));}
void UWardrobeStartupSubsystem::MediaFailed(FString URL){if(Visual){Visual->MovieFailed.store(true);Visual->MovieDone.store(true);}UE_LOG(LogTemp,Warning,TEXT("WARDROBE_STARTUP_MOVIE_UNAVAILABLE: configure an MP4 inside Content/Movies; keeping the enter-game gate."));}
void UWardrobeStartupSubsystem::MediaEnded(){if(Visual){Visual->MovieDone.store(true);Visual->Closing.store(1.f);}if(MediaSound)MediaSound->Stop();UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_MEDIA_END"));}
bool UWardrobeStartupSubsystem::CanEnterGame() const {return bActive&&Visual&&Visual->CanEnter();}
FString UWardrobeStartupSubsystem::GetStartupStatus() const
{
 if(!bActive)return TEXT("Inactive");
 if(CanEnterGame())return TEXT("Ready");
 return Visual&&Visual->MovieDone.load()?TEXT("Preparing"):TEXT("Playing");
}
void UWardrobeStartupSubsystem::GuardInput()
{
 if(!Overlay||!FSlateApplication::IsInitialized())return;
 auto* PC=GetGameInstance()->GetFirstLocalPlayerController();
 if(PC&&GuardedController!=PC){
  if(GuardedController.IsValid()){GuardedController->SetIgnoreMoveInput(false);GuardedController->SetIgnoreLookInput(false);GuardedController->bShowMouseCursor=bSavedCursor;}
  GuardedController=PC;bSavedCursor=PC->bShowMouseCursor;PC->SetIgnoreMoveInput(true);PC->SetIgnoreLookInput(true);PC->FlushPressedKeys();
 }
 if(PC)PC->bShowMouseCursor=Visual&&Visual->CanEnter();
 if(FSlateApplication::Get().GetKeyboardFocusedWidget()!=Overlay){FSlateApplication::Get().ReleaseAllPointerCapture();FSlateApplication::Get().SetAllUserFocus(Overlay,EFocusCause::SetDirectly);}
}
bool UWardrobeStartupSubsystem::Tick(float Dt)
{
#if WITH_DEV_AUTOMATION_TESTS
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeStartupQA"))&&QAEnteredAt>0&&FPlatformTime::Seconds()-QAEnteredAt>3){
  UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_QA_COMPLETE"));FPlatformMisc::RequestExit(false);return false;
 }
#endif
 if(!bActive||!Visual||!MenuOwner.IsValid())return true;
 GuardInput();const double Now=FPlatformTime::Seconds();++WarmupFrames;
 if(!bTTSReady&&!HealthRequest&&Now>=NextHealthCheck){
  HealthRequest=FHttpModule::Get().CreateRequest();HealthRequest->SetURL(HealthURL);HealthRequest->SetVerb(TEXT("GET"));HealthRequest->SetTimeout(2.f);
  HealthRequest->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
  HealthRequest->OnProcessRequestComplete().BindLambda([Weak=TWeakObjectPtr<UWardrobeStartupSubsystem>(this)](FHttpRequestPtr,FHttpResponsePtr Response,bool OK){
   auto* S=Weak.Get();if(!S||!S->bActive)return;S->HealthRequest.Reset();S->NextHealthCheck=FPlatformTime::Seconds()+1.;
   // A different GPT-SoVITS server can omit our status route; a live 404 also proves it has booted.
   if(OK&&Response&&(Response->GetResponseCode()==200||Response->GetResponseCode()==404)){
    S->bTTSReady=true;UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_TTS_READY"));
   }
  });
  if(!HealthRequest->ProcessRequest()){HealthRequest.Reset();NextHealthCheck=Now+1.;}
 }
 if(MediaSound)MediaSound->UpdatePlayer();
 if(MediaPlayer&&!Visual->MovieDone.load()){
  const double Duration=MediaPlayer->GetDuration().GetTotalSeconds(),Time=MediaPlayer->GetTime().GetTotalSeconds();
  if(Duration>0&&Visual->FadeTime>0){
   const float X=FMath::Clamp(float(1.-(Duration-Time)/Visual->FadeTime),0.f,1.f),Fade=X*X*(3.f-2.f*X);
   Visual->Closing.store(Fade);if(MediaSound&&!Visual->FadeBaked)MediaSound->SetVolumeMultiplier(1.f-Fade);
  }
  if(MediaPlayer->HasError()||(!MediaOpenedAt&&Now-StartedAt>15))MediaFailed(TEXT(""));
  if(MediaOpenedAt&&MediaPlayer->GetDuration().GetTotalSeconds()>0&&Now-MediaOpenedAt>MediaPlayer->GetDuration().GetTotalSeconds()+10)MediaEnded();
 }
 const auto* Loading=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>();
 const auto* AI=GetGameInstance()->GetSubsystem<UWardrobeAISpeechSubsystem>();
 const bool Ready=WarmupFrames>=12&&Now-StartedAt>=1.5&&Loading&&Loading->IsSceneReady(GetWorld())&&(!AI||!AI->IsPrewarming())&&bTTSReady;
 if(Ready){if(ReadyAt==0)ReadyAt=Now;}else ReadyAt=0;
 // A quiet half-second avoids showing the entry prompt between separate streaming/shader batches.
 const bool Finished=(ReadyAt>0&&Now-ReadyAt>=.5)||Now-StartedAt>=FMath::Max(5.f,Settings->StartupPrepareTimeout);
 if(Finished&&!Visual->SceneReady.exchange(true))UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_SCENE_READY timeout=%d"),!Ready);
#if WITH_DEV_AUTOMATION_TESTS
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeStartupQA"))){
  for(TActorIterator<AWardrobeShowcase> It(GetWorld());It;++It)if(It->AISpeech)It->AISpeech->SetAutomaticPaused(true);
  if(CanEnterGame()){
   if(QAReadyAt==0){QAReadyAt=Now;FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("WardrobeStartupQA/prompt.png"),true,false);}
   if(Now-QAReadyAt>1&&Now-StartedAt>12){
    const bool First=EnterGame(),Second=EnterGame();
    UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_QA_ENTER first=%d second=%d"),First,Second);QAEnteredAt=Now;
   }
  }
 }
#endif
 return true;
}
bool UWardrobeStartupSubsystem::EnterGame()
{
 if(!CanEnterGame())return false;
 if(auto* Audio=GetGameInstance()->GetSubsystem<UWardrobeAudioSubsystem>())Audio->PlayEvent(EWardrobeUISound::Confirm);
 Finish();return true;
}
void UWardrobeStartupSubsystem::Finish()
{
 if(!bActive&&bCompleted)return;
 if(Overlay)if(auto* Viewport=GetGameInstance()->GetGameViewportClient())Viewport->RemoveViewportWidgetContent(Overlay.ToSharedRef());
 if(GuardedController.IsValid()){GuardedController->SetIgnoreMoveInput(false);GuardedController->SetIgnoreLookInput(false);GuardedController->bShowMouseCursor=bSavedCursor;GuardedController->FlushPressedKeys();}
 GuardedController.Reset();
 if(HealthRequest){HealthRequest->OnProcessRequestComplete().Unbind();HealthRequest->CancelRequest();HealthRequest.Reset();}
 if(MediaSound){MediaSound->Stop();MediaSound->SetMediaPlayer(nullptr);MediaSound->DestroyComponent();MediaSound=nullptr;}
 if(MediaPlayer){MediaPlayer->OnMediaOpened.RemoveAll(this);MediaPlayer->OnMediaOpenFailed.RemoveAll(this);MediaPlayer->OnEndReached.RemoveAll(this);MediaPlayer->Close();MediaPlayer=nullptr;}
 if(MediaTexture){MediaTexture->SetMediaPlayer(nullptr);MediaTexture=nullptr;}
 if(Visual)Visual->MovieBrush.SetResourceObject(nullptr);
 bActive=false;bCompleted=true;Overlay.Reset();Visual.Reset();
 if(MenuOwner.IsValid()){MenuOwner->RestoreMenuFocus();UE_LOG(LogTemp,Display,TEXT("WARDROBE_STARTUP_ENTER_GAME"));}
}
void UWardrobeStartupSubsystem::Deinitialize()
{
 FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
 if(IsMoviePlayerEnabled()){
  GetMoviePlayer()->OnMovieClipFinished().Remove(ClipHandle);GetMoviePlayer()->OnMoviePlaybackFinished().Remove(FinishedHandle);
  if(bNativeMovie)GetMoviePlayer()->StopMovie();
 }
 Finish();MenuOwner.Reset();Settings=nullptr;Super::Deinitialize();
}
