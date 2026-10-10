#include "DreamBloomWidget.h"
#include "LevelSequence.h"
#include "LevelSequenceActor.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneSequencePlayer.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Fonts/CompositeFont.h"
#include "Rendering/DrawElements.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"

namespace DB {
static FLinearColor Ink(.035f,.075f,.095f,1.f);
static FLinearColor Muted(.11f,.17f,.18f,1.f);
static FLinearColor Gold(.35f,.23f,.085f,1.f);
static FLinearColor Cream(.96f,.95f,.89f,1.f);
static FText T(const FString& S) {return FText::FromString(S);}
}

class SDreamBloom : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SDreamBloom) {} SLATE_ARGUMENT(UDreamBloomWidget*,Owner) SLATE_END_ARGS()
 TWeakObjectPtr<UDreamBloomWidget> Owner;
 FString Resource;
 TSharedPtr<FSlateDynamicImageBrush> Garden;
 TMap<FString,TSharedPtr<FSlateVectorImageBrush>> Icons;
 TSharedPtr<FCompositeFont> ChineseFont,LatinFont;
 FButtonStyle Primary,Secondary,Dark;
 FEditableTextBoxStyle InputStyle;
 FSlateRoundedBoxBrush Field{FLinearColor(1,1,1,.60f),12.f,FLinearColor(.6f,.64f,.61f,.35f),1.f};
 FSlateRoundedBoxBrush Glass{FLinearColor(1,1,1,.24f),20.f,FLinearColor(1,1,1,.4f),1.f};
 FSlateRoundedBoxBrush HUD{FLinearColor(.03f,.065f,.08f,.88f),20.f,FLinearColor(1,1,1,.15f),1.f};
 TSharedPtr<SEditableTextBox> NameInput;
 TSharedPtr<SCanvas> Root;
 float Age=0;
 int32 LastPage=-1;

 FSlateFontInfo Font(int32 Size,bool English=false) const {
  FSlateFontInfo F(English?LatinFont:ChineseFont,Size);
  return F;
 }
 TSharedRef<STextBlock> Text(const FString& S,int32 Size,FLinearColor Color=DB::Ink,bool English=false) {
  return SNew(STextBlock).Text(DB::T(S)).Font(Font(Size,English)).ColorAndOpacity(Color);
 }
 TSharedRef<SWidget> Icon(const FString& Name,float Size,FLinearColor Color) {
  if(!Icons.Contains(Name)) Icons.Add(Name,MakeShared<FSlateVectorImageBrush>(Resource/(TEXT("lucide-")+Name+TEXT(".svg")),FVector2D(24,24)));
  return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SImage).Image(Icons[Name].Get()).ColorAndOpacity(Color)];
 }
 void Put(TSharedRef<SCanvas> C,float X,float Y,float W,float H,TSharedRef<SWidget> Widget) {
  C->AddSlot().Position(FVector2D(X,Y)).Size(FVector2D(W,H))[Widget];
 }
 TSharedRef<SWidget> Button(const FString& Label,const FString& IconName,TFunction<void()> Action,bool Main=true,bool OnDark=false) {
  const FLinearColor Color=Main||OnDark?DB::Cream:DB::Ink;
  return SNew(SButton).ButtonStyle(OnDark?&Dark:(Main?&Primary:&Secondary)).ContentPadding(FMargin(22,8)).HAlign(HAlign_Center).VAlign(VAlign_Center)
   .OnClicked_Lambda([Action](){Action();return FReply::Handled();})
   [SNew(SHorizontalBox)
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,12,0)[Icon(IconName,20,Color)]
    +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Text(Label,18,Color)]];
 }
 void Construct(const FArguments& Args) {
  Owner=Args._Owner;
  Resource=IPluginManager::Get().FindPlugin(TEXT("DreamBloom"))->GetBaseDir()/TEXT("Resources");
  ChineseFont=MakeShared<FCompositeFont>(NAME_None,Resource/TEXT("NotoSerifSC-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  LatinFont=MakeShared<FCompositeFont>(NAME_None,Resource/TEXT("CormorantGaramond-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  Garden=MakeShared<FSlateDynamicImageBrush>(FName(*(Resource/TEXT("Garden.png"))),FVector2D(1600,900));
  Primary.SetNormal(FSlateRoundedBoxBrush(FLinearColor(.075f,.17f,.19f),28.f));
  Primary.SetHovered(FSlateRoundedBoxBrush(FLinearColor(.15f,.29f,.30f),28.f,FLinearColor(.88f,.8f,.59f),1.f));
  Primary.SetPressed(FSlateRoundedBoxBrush(FLinearColor(.045f,.11f,.14f),28.f));
  Primary.SetNormalPadding(FMargin(0));Primary.SetPressedPadding(FMargin(0,1,0,-1));
  Secondary.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.32f),28.f,FLinearColor(.5f,.58f,.57f,.38f),1.f));
  Secondary.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.66f),28.f,FLinearColor(.5f,.58f,.57f,.6f),1.f));
  Secondary.SetPressed(FSlateRoundedBoxBrush(FLinearColor(.72f,.81f,.80f,.75f),28.f));
  Dark.SetNormal(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.08f),24.f));
  Dark.SetHovered(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.21f),24.f));
  Dark.SetPressed(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.30f),24.f));
  InputStyle=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));
  InputStyle.SetBackgroundImageNormal(Field).SetBackgroundImageHovered(Field)
   .SetBackgroundImageFocused(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.8f),12.f,DB::Gold,1.f))
   .SetFont(Font(18)).SetForegroundColor(DB::Ink).SetPadding(FMargin(18,12));

  auto Menu=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&Owner->Page!=2?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
  Put(Menu,0,0,1600,900,SNew(SImage).Image(Garden.Get()));
  Put(Menu,60,48,30,30,Icon(TEXT("flower-2"),30,DB::Ink));
  Put(Menu,104,43,340,34,Text(TEXT("BLOOMING LETTER"),23,DB::Ink,true));
  Put(Menu,106,79,310,26,Text(TEXT("花间来信  /  梦境序章"),12,DB::Muted));
  Put(Menu,1338,57,200,28,Text(TEXT("A LITTLE WONDER"),15,DB::Muted,true));
  Put(Menu,235,626,380,32,Text(TEXT("每一场相逢，都是花开的序章"),17,FLinearColor(.28f,.39f,.40f)));
  Put(Menu,245,665,310,24,Text(TEXT("WHERE EVERY WISH BLOOMS"),13,DB::Muted,true));
  Put(Menu,931,178,500,30,Text(TEXT("A LETTER FROM THE GARDEN"),17,DB::Gold,true));
  Put(Menu,922,222,530,94,Text(TEXT("花间来信"),58));
  Put(Menu,933,326,500,35,Text(TEXT("循着微光，赴一场温柔的奇遇。"),20,DB::Muted));
  Put(Menu,934,385,455,1,SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity(FLinearColor(.4f,.45f,.4f,.22f)));

  auto Login=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&Owner->Page==0?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
  Put(Login,0,0,440,28,Text(TEXT("以你的名字，写下故事的开始"),17));
  Put(Login,0,48,455,60,SAssignNew(NameInput,SEditableTextBox).Style(&InputStyle).HintText(DB::T(TEXT("旅人的名字（可选）")))
   .OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type How){if(How==ETextCommit::OnEnter&&Owner.IsValid())Owner->Login(Value.ToString());}));
  Put(Login,0,131,455,58,Button(TEXT("启  程"),TEXT("arrow-right"),[this](){if(Owner.IsValid())Owner->Login(NameInput->GetText().ToString());}));
  Put(Login,0,209,455,25,Text(TEXT("本地旅人入口  ·  无需账号与密码"),13,DB::Muted));
  Put(Menu,934,427,465,245,Login);

  auto Lobby=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&Owner->Page==1?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
  Put(Lobby,0,0,475,35,SNew(STextBlock).Text_Lambda([this](){return DB::T(TEXT("欢迎回来，")+(Owner.IsValid()?Owner->Traveler:FString()));}).Font(Font(19)).ColorAndOpacity(DB::Ink));
  Put(Lobby,0,54,455,114,SNew(SBorder).BorderImage(&Glass).Padding(20)
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Text(TEXT("回忆剧场"),23)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,9,0,0)[Text(TEXT("珍藏这一刻，让美好循环发生。"),15,DB::Muted)]]);
  Put(Lobby,0,189,455,58,Button(TEXT("开启回忆  ·  循环播放"),TEXT("play"),[this](){if(Owner.IsValid())Owner->StartMemory();}));
  Put(Lobby,0,264,455,48,Button(TEXT("切换旅人"),TEXT("user-round"),[this](){if(Owner.IsValid())Owner->Logout();},false));
  Put(Lobby,0,320,470,40,SNew(STextBlock).Text_Lambda([this](){return DB::T(Owner.IsValid()?Owner->Error:FString());}).Font(Font(14)).ColorAndOpacity(FLinearColor(.6f,.15f,.1f)).AutoWrapText(true));
  Put(Menu,934,426,480,365,Lobby);
  Put(Menu,60,830,600,25,Text(TEXT("愿所有微小的美好，都在此刻被看见。"),13,DB::Muted));
  Put(Menu,1315,833,220,22,Text(TEXT("DREAMBLOOM   /   01"),14,DB::Muted,true));

  auto Cinema=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&Owner->Page==2?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
  Put(Cinema,48,42,205,48,Button(TEXT("返回花园"),TEXT("arrow-left"),[this](){if(Owner.IsValid())Owner->ReturnToLobby();},false,true));
  Put(Cinema,1190,48,360,35,SNew(STextBlock).Text_Lambda([this](){return DB::T(Owner.IsValid()&&Owner->IsPaused()?TEXT("回忆剧场  /  已暂停"):TEXT("回忆剧场  /  循环放映中"));}).Font(Font(16)).ColorAndOpacity(DB::Cream).ShadowOffset(FVector2D(1,1)).ShadowColorAndOpacity(FLinearColor::Black));
  Put(Cinema,355,756,890,112,SNew(SBorder).BorderImage(&HUD));
  Put(Cinema,384,770,800,25,SNew(STextBlock).Text_Lambda([this](){return DB::T(Owner.IsValid()?Owner->Status():FString());}).Font(Font(14)).ColorAndOpacity(DB::Cream));
  Put(Cinema,384,805,248,45,SNew(SButton).ButtonStyle(&Dark).HAlign(HAlign_Center).VAlign(VAlign_Center)
   .OnClicked_Lambda([this](){if(Owner.IsValid())Owner->TogglePause();return FReply::Handled();})
   [SNew(STextBlock).Text_Lambda([this](){return DB::T(Owner.IsValid()&&Owner->IsPaused()?TEXT("继续播放"):TEXT("暂停播放"));}).Font(Font(17)).ColorAndOpacity(DB::Cream)]);
  Put(Cinema,654,805,248,45,Button(TEXT("从头播放"),TEXT("rotate-ccw"),[this](){if(Owner.IsValid())Owner->RestartMemory();},false,true));
  Put(Cinema,930,812,285,28,Text(TEXT("让这一刻，再一次盛放"),14,DB::Cream));
  Put(Cinema,380,863,840,3,SNew(SProgressBar).Percent_Lambda([this](){return TOptional<float>(Owner.IsValid()?Owner->Progress():0.f);}).FillColorAndOpacity(FLinearColor(.9f,.77f,.48f)).BorderPadding(FVector2D::ZeroVector));
  Root=SNew(SCanvas);Put(Root.ToSharedRef(),0,0,1600,900,Menu);Put(Root.ToSharedRef(),0,0,1600,900,Cinema);
  ChildSlot[SNew(SOverlay)
   +SOverlay::Slot()[SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush")).ColorAndOpacity_Lambda([this](){return Owner.IsValid()&&Owner->Page==2?FLinearColor::Transparent:FLinearColor(.55f,.65f,.68f);}).Visibility(EVisibility::HitTestInvisible)]
   +SOverlay::Slot()[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(1600).HeightOverride(900)[Root.ToSharedRef()]]]];
 }
 virtual void Tick(const FGeometry& G,double Time,float Delta) override {
  SCompoundWidget::Tick(G,Time,Delta);
  if(Owner.IsValid()&&Owner->Page!=LastPage){LastPage=Owner->Page;Age=0;}
  Age+=Delta;SetRenderOpacity(FMath::Clamp(Age/0.45f,0.f,1.f));
 }
 virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& Geometry,const FSlateRect& Culling,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override {
  int32 Last=SCompoundWidget::OnPaint(Args,Geometry,Culling,Elements,Layer,Style,Enabled);
  if(Owner.IsValid()&&Owner->Page!=2&&Root.IsValid()){
   const FGeometry G=Root->GetCachedGeometry();
   for(int i=0;i<20;i++){
    float X=65.f+FMath::Fmod(i*137.f+FMath::Sin(Age*.25f+i)*20.f,685.f);
    float Y=FMath::Fmod(i*97.f+900.f-Age*(9.f+i%4),900.f);
    if(Y<0)Y+=900;
    float A=.22f+.26f*(.5f+.5f*FMath::Sin(Age*1.2f+i));
    FVector2f P(X,Y);float R=i%3==0?3.f:1.6f;
    FSlateDrawElement::MakeBox(Elements,Last+1,G.ToPaintGeometry(FVector2f(R,R),FSlateLayoutTransform(P)),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,FLinearColor(1,.94f,.75f,A));
   }
   return Last+1;
  }return Last;
 }
};

TSharedRef<SWidget> UDreamBloomWidget::RebuildWidget(){View=SNew(SDreamBloom).Owner(this);return View.ToSharedRef();}
void UDreamBloomWidget::ReleaseSlateResources(bool b){Super::ReleaseSlateResources(b);View.Reset();}
void UDreamBloomWidget::Login(const FString& Name){Traveler=Name.TrimStartAndEnd().Left(16);if(Traveler.IsEmpty())Traveler=TEXT("旅人");Page=1;Error.Empty();UE_LOG(LogTemp,Display,TEXT("DreamBloom: LOGIN local-only"));}
void UDreamBloomWidget::Logout(){ReturnToLobby();Page=0;Traveler=TEXT("旅人");}
void UDreamBloomWidget::StartMemory(){
 if(!Player){
  ULevelSequence* Asset=Sequence.LoadSynchronous();
  if(!Asset){Error=TEXT("回忆尚未就绪，请检查 Sequence 资源。 ");UE_LOG(LogTemp,Error,TEXT("DreamBloom: sequence missing"));return;}
  if(APlayerController* PC=GetOwningPlayer())PreviousCamera=PC->GetViewTarget();
  FMovieSceneSequencePlaybackSettings Settings;Settings.bAutoPlay=false;Settings.LoopCount.Value=-1;Settings.bHideHud=false;Settings.bDisableMovementInput=true;Settings.bDisableLookAtInput=true;
  ALevelSequenceActor* Actor=nullptr;Player=ULevelSequencePlayer::CreateLevelSequencePlayer(this,Asset,Settings,Actor);SequenceActor=Actor;
 }
 if(!Player||Player->GetDuration().AsSeconds()<=0){Error=TEXT("这段回忆没有可播放的内容。 ");return;}
 Player->Stop();LoopCount=0;LastPosition=-1;Player->PlayLooping(-1);Page=2;Error.Empty();
 UE_LOG(LogTemp,Display,TEXT("DreamBloom: PLAY looping=-1 duration=%.3f"),Player->GetDuration().AsSeconds());
}
bool UDreamBloomWidget::IsPaused()const{return Player&&Player->IsPaused();}
float UDreamBloomWidget::Progress()const{if(!Player)return 0;double D=Player->GetDuration().AsSeconds();return D>0?FMath::Clamp(float((Player->GetCurrentTime().AsSeconds()-Player->GetStartTime().AsSeconds())/D),0.f,1.f):0.f;}
FString UDreamBloomWidget::Status()const{if(!Player)return TEXT("回忆准备中");return FString::Printf(TEXT("%s  ·  第 %d 次放映                                      %.1f / %.1f 秒"),IsPaused()?TEXT("已暂停"):TEXT("循环播放中"),LoopCount+1,Player->GetCurrentTime().AsSeconds()-Player->GetStartTime().AsSeconds(),Player->GetDuration().AsSeconds());}
void UDreamBloomWidget::TogglePause(){if(!Player||Page!=2)return;if(Player->IsPaused()){Player->Play();UE_LOG(LogTemp,Display,TEXT("DreamBloom: RESUME"));}else{Player->Pause();UE_LOG(LogTemp,Display,TEXT("DreamBloom: PAUSE"));}}
void UDreamBloomWidget::RestartMemory(){if(Player&&Page==2){Player->Stop();LastPosition=-1;LoopCount=0;Player->PlayLooping(-1);UE_LOG(LogTemp,Display,TEXT("DreamBloom: RESTART"));}}
void UDreamBloomWidget::ReturnToLobby(){if(Player)Player->Stop();if(APlayerController* PC=GetOwningPlayer()){if(PreviousCamera)PC->SetViewTarget(PreviousCamera);PC->SetIgnoreMoveInput(false);PC->SetIgnoreLookInput(false);}Page=1;LastPosition=-1;UE_LOG(LogTemp,Display,TEXT("DreamBloom: RETURN stopped"));}
void UDreamBloomWidget::NativeDestruct(){if(Player)Player->Stop();if(SequenceActor)SequenceActor->Destroy();Player=nullptr;SequenceActor=nullptr;Super::NativeDestruct();}
void UDreamBloomWidget::Screenshot(const FString& Name){FString Dir=FPaths::ProjectSavedDir()/TEXT("DreamBloomQA");IFileManager::Get().MakeDirectory(*Dir,true);FScreenshotRequest::RequestScreenshot(Dir/Name,true,false);}
void UDreamBloomWidget::NativeTick(const FGeometry& G,float D){
 Super::NativeTick(G,D);
 if(Page==2&&Player&&Player->IsPlaying()){
  double P=Player->GetCurrentTime().AsSeconds();if(LastPosition>=0&&P+0.1<LastPosition){LoopCount++;NaturalLoops++;UE_LOG(LogTemp,Display,TEXT("DreamBloom: LOOP %d"),LoopCount);}LastPosition=P;
 }
 if(!FParse::Param(FCommandLine::Get(),TEXT("DreamBloomTest")))return;
 TestTime+=D;
 if(TestStep==0&&TestTime>2){Screenshot(TEXT("01-login.png"));TestStep++;}
 else if(TestStep==1&&TestTime>4){Login(TEXT("花间旅人"));TestStep++;}
 else if(TestStep==2&&TestTime>5){Screenshot(TEXT("02-lobby.png"));TestStep++;}
 else if(TestStep==3&&TestTime>7){StartMemory();TestStep++;}
 else if(TestStep==4&&TestTime>19){Screenshot(TEXT("03-looping.png"));CameraVerified=Player&&Player->GetActiveCameraComponent()!=nullptr;TestStep++;}
 else if(TestStep==5&&TestTime>20){TogglePause();PausedAt=Player?Player->GetCurrentTime().AsSeconds():-1;TestStep++;}
 else if(TestStep==6&&TestTime>21){Screenshot(TEXT("04-paused.png"));TestStep++;}
 else if(TestStep==7&&TestTime>22){PauseVerified=IsPaused()&&Player&&FMath::Abs(Player->GetCurrentTime().AsSeconds()-PausedAt)<.05;TogglePause();TestStep++;}
 else if(TestStep==8&&TestTime>24){RestartMemory();TestStep++;}
 else if(TestStep==9&&TestTime>26){ReturnToLobby();TestStep++;}
 else if(TestStep==10&&TestTime>27){Screenshot(TEXT("05-return.png"));TestStep++;}
 else if(TestStep==11&&TestTime>29){
  FString Result=FString::Printf(TEXT("naturalLoops=%d\nplayerStopped=%s\npage=%d\npauseTimeStable=%s\nsequenceCameraActive=%s\n"),NaturalLoops,Player&&!Player->IsPlaying()?TEXT("true"):TEXT("false"),Page,PauseVerified?TEXT("true"):TEXT("false"),CameraVerified?TEXT("true"):TEXT("false"));
  FFileHelper::SaveStringToFile(Result,*(FPaths::ProjectSavedDir()/TEXT("DreamBloomQA/result.txt")));
  UE_LOG(LogTemp,Display,TEXT("DreamBloom: TEST_COMPLETE %s"),*Result);Logout();TestStep++;
 }
}
ADreamBloomGameMode::ADreamBloomGameMode(){DefaultPawnClass=nullptr;LoginWidgetClass=UDreamBloomWidget::StaticClass();MemorySequence=TSoftObjectPtr<ULevelSequence>(FSoftObjectPath(TEXT("/Game/NewLevelSequence.NewLevelSequence")));}
void ADreamBloomGameMode::BeginPlay(){Super::BeginPlay();APlayerController* PC=UGameplayStatics::GetPlayerController(this,0);if(!PC)return;PC->ConsoleCommand(TEXT("DisableAllScreenMessages"),false);LoginWidget=CreateWidget<UDreamBloomWidget>(PC,LoginWidgetClass);if(!LoginWidget)return;LoginWidget->Sequence=MemorySequence;LoginWidget->AddToViewport(50);PC->bShowMouseCursor=true;FInputModeUIOnly Mode;Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Mode);UE_LOG(LogTemp,Display,TEXT("DreamBloom: UI_READY"));}
