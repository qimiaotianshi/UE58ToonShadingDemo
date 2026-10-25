#include "WardrobeMenu.h"
#include "WardrobeStory.h"
#include "WardrobeShowcase.h"
#include "WardrobeLoading.h"
#include "WardrobeCharacters.h"
#include "WardrobeStations.h"
#include "WardrobeFeedback.h"
#include "LevelSequencePlayer.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SCanvas.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Fonts/CompositeFont.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"
#include "Blueprint/SlateBlueprintLibrary.h"

namespace Wardrobe {
// sRGB palette: milky blush surfaces, raspberry accents, and readable plum text.
static FLinearColor Color(const TCHAR* Hex){return FLinearColor::FromSRGBColor(FColor::FromHex(Hex));}
static FLinearColor White=Color(TEXT("593345")), Soft=Color(TEXT("906476")), Pink=Color(TEXT("B93E72"));
static FLinearColor Milk=Color(TEXT("FFF5F9")), Blush=Color(TEXT("FCE1ED")), Rose=Color(TEXT("D9578C")), Border=Color(TEXT("E8A3BE"));
static FText T(const FString& S){return FText::FromString(S);}
static const FSlateBrush* Solid(){return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));}
}

class SWardrobeMenu : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SWardrobeMenu){} SLATE_ARGUMENT(UWardrobeMenu*,Owner) SLATE_END_ARGS()
 TWeakObjectPtr<UWardrobeMenu> Owner;
 TSharedPtr<SCanvas> Canvas;
 TSharedPtr<FCompositeFont> Face;
 TMap<FString,TSharedPtr<FSlateVectorImageBrush>> Icons;
 TMap<int32,TSharedPtr<FSlateBrush>> Thumbs;
 TSharedPtr<SEditableTextBox> Name;
 FString Resources;
 FButtonStyle Clear,Golden,Card;
 FEditableTextBoxStyle Input;
 FSlateRoundedBoxBrush Panel{Wardrobe::Milk.CopyWithNewOpacity(.94f),16.f,Wardrobe::Border.CopyWithNewOpacity(.80f),1.f};
 FSlateRoundedBoxBrush Selected{Wardrobe::Blush,12.f,Wardrobe::Pink,2.f};
 FSlateRoundedBoxBrush Toast{Wardrobe::Milk.CopyWithNewOpacity(.97f),14.f,Wardrobe::Border,1.f};
 FKey DragButton;
 FVector2D PressPosition;
 bool bDragged=false;
 bool bPhysicsDragging=false;
 void CancelPointerGesture(){if(Owner.IsValid())Owner->EndPhysicsDrag();bPhysicsDragging=false;DragButton=FKey();bDragged=true;FSlateApplication::Get().ReleaseAllPointerCapture();}
 TSharedPtr<SBox> ChoiceHost,SaveHost;
 TSharedPtr<SVerticalBox> SaveRows;
 TArray<FWardrobeStorySlot> Slots;
 bool bNewStory=false;
 int32 OverwriteSlot=INDEX_NONE;
 float Age=0;
 FSlateFontInfo Font(int32 Size)const{return FSlateFontInfo(Face,Size);}
 TSharedRef<SWidget> Label(const FString& S,int32 Size,FLinearColor C=Wardrobe::White){return SNew(STextBlock).Text(Wardrobe::T(S)).Font(Font(Size)).ColorAndOpacity(C);}
 void Put(TSharedRef<SCanvas> C,float X,float Y,float W,float H,TSharedRef<SWidget> V){C->AddSlot().Position(FVector2D(X,Y)).Size(FVector2D(W,H))[V];}
 TSharedRef<SWidget> Icon(const FString& Id,float Size,FLinearColor C=Wardrobe::White){if(!Icons.Contains(Id))Icons.Add(Id,MakeShared<FSlateVectorImageBrush>(Resources/(TEXT("lucide-")+Id+TEXT(".svg")),FVector2D(24)));return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SImage).Image(Icons[Id].Get()).ColorAndOpacity(C)];}
 TSharedRef<SWidget> Button(const FString& Text,const FString& Glyph,TFunction<void()> Fn,bool Gold=false){const FLinearColor TextColor=Gold?Wardrobe::Milk:Wardrobe::White;if(Text.IsEmpty())return SNew(SButton).ButtonStyle(&Clear).IsFocusable(false).ContentPadding(4).HAlign(HAlign_Center).VAlign(VAlign_Center).OnClicked_Lambda([Fn](){Fn();return FReply::Handled();})[Icon(Glyph,20)];return SNew(SButton).ButtonStyle(Gold?&Golden:&Clear).IsFocusable(false).ContentPadding(FMargin(15,8)).HAlign(HAlign_Center).VAlign(VAlign_Center).OnClicked_Lambda([Fn](){Fn();return FReply::Handled();})[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,10,0)[Icon(Glyph,19,TextColor)]+SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[Label(Text,17,TextColor)]];}
 EVisibility MainVisible()const{return Owner.IsValid()&&Owner->IsMenuMap()?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;}
 TSharedRef<SWidget> BuildChoices(){
  auto Tiles=SNew(SCanvas);Thumbs.Empty();
  if(!Owner.IsValid())return Tiles;
  Owner->LoadedThumbnails.Empty();
  const int32 Count=Owner->ChoiceCount();
  for(int32 i=0;i<Count;i++){
   auto Tile=SNew(SCanvas);const bool Ready=Owner->ChoiceReady(i);
   Put(Tile,10,8,100,20,Label(FString::Printf(TEXT("%02d"),i+1),11,Ready?Wardrobe::Pink:Wardrobe::Soft));
   if(UTexture2D* Tex=Owner->ChoiceThumbnail(i).LoadSynchronous()){
    Owner->LoadedThumbnails.AddUnique(Tex);auto Brush=MakeShared<FSlateBrush>();Brush->SetResourceObject(Tex);Brush->ImageSize=FVector2D(256,144);Brush->DrawAs=ESlateBrushDrawType::Image;Thumbs.Add(i,Brush);
    Put(Tile,6,33,118,70,SNew(SImage).Image(Thumbs[i].Get()));
   }else Put(Tile,34,34,62,62,Icon(Ready?TEXT("mountain"):TEXT("lock-keyhole"),50,Ready?Wardrobe::White:Wardrobe::Soft));
   Put(Tile,7,111,118,40,SNew(STextBlock).Text(Owner->ChoiceLabel(i)).Font(Font(11)).ColorAndOpacity(Ready?Wardrobe::White:Wardrobe::Soft).Justification(ETextJustify::Center).AutoWrapText(true));
   auto Contents=SNew(SOverlay)
    +SOverlay::Slot()[SNew(SBorder).BorderImage(&Selected).Visibility_Lambda([this,i](){return Owner.IsValid()&&Owner->SelectedChoice()==i?EVisibility::HitTestInvisible:EVisibility::Collapsed;})]
    +SOverlay::Slot()[Tile];
   Put(Tiles,(i%3)*145,(i/3)*161,132,150,SNew(SButton).ButtonStyle(&Card).IsFocusable(false).ContentPadding(0).OnClicked_Lambda([this,i](){if(Owner.IsValid())Owner->SelectChoice(i);return FReply::Handled();})[Contents]);
  }
  if(Count==0)return SNew(SBox).VAlign(VAlign_Center).HAlign(HAlign_Center)[Label(TEXT("请先在 DA 中添加可选项目"),15,Wardrobe::Soft)];
  return SNew(SScrollBox).ScrollBarVisibility(EVisibility::Collapsed)+SScrollBox::Slot()[SNew(SBox).HeightOverride(FMath::DivideAndRoundUp(Count,3)*161.f)[Tiles]];
 }
 void RefreshChoices(){if(ChoiceHost){ChoiceHost->SetContent(SNullWidget::NullWidget);ChoiceHost->SetContent(BuildChoices());}}
 void RefreshStoryRows(){
  SaveRows->ClearChildren();
  for(const auto& Slot:Slots){
   const FString Detail=Slot.bExists?FString::Printf(TEXT("%s  ·  %s\n%s"),*Slot.PlayerName,*Slot.Progress.ToString(),*(Slot.SavedAt+(FDateTime::Now()-FDateTime::UtcNow())).ToString(TEXT("%Y-%m-%d %H:%M"))):TEXT("空存档");
   const FString Title=FString::Printf(TEXT("存档 %d  %s"),Slot.Index+1,OverwriteSlot==Slot.Index?TEXT("再次点击确认覆盖"):TEXT(""));
   SaveRows->AddSlot().AutoHeight().Padding(0,4)[SNew(SButton).ButtonStyle(&Card).IsFocusable(false).ContentPadding(16).IsEnabled(bNewStory||Slot.bValid)
    .OnClicked_Lambda([this,Slot](){
     if(!Owner.IsValid())return FReply::Handled();
     if(bNewStory&&Slot.bExists&&OverwriteSlot!=Slot.Index){Owner->PlayUISound(EWardrobeUISound::Confirm);OverwriteSlot=Slot.Index;RefreshStoryRows();return FReply::Handled();}
     Owner->ChooseStorySlot(Slot.Index,bNewStory&&Slot.bExists);return FReply::Handled();
    })[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Label(Title,18,OverwriteSlot==Slot.Index?Wardrobe::Pink:Wardrobe::White)]
     +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)[Label(Detail,13,Wardrobe::Soft)]]];
  }
 }
 void OpenStorySlots(bool NewGame){
  CancelPointerGesture();bNewStory=NewGame;OverwriteSlot=INDEX_NONE;Slots.Empty();
  if(Owner.IsValid())if(auto* Story=Owner->GetGameInstance()->GetSubsystem<UWardrobeStorySubsystem>())Slots=Story->GetSaveSlots();
  RefreshStoryRows();
 }
 void Construct(const FArguments& A){
  Owner=A._Owner;Resources=IPluginManager::Get().FindPlugin(TEXT("DreamBloom"))->GetBaseDir()/TEXT("Resources");
  Face=MakeShared<FCompositeFont>(NAME_None,Resources/TEXT("NotoSansSC-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  Clear.SetNormal(FSlateRoundedBoxBrush(Wardrobe::Milk.CopyWithNewOpacity(.88f),12.f,Wardrobe::Border.CopyWithNewOpacity(.65f),1.f)).SetHovered(FSlateRoundedBoxBrush(Wardrobe::Blush,12.f,Wardrobe::Rose,1.5f)).SetPressed(FSlateRoundedBoxBrush(Wardrobe::Border,12.f));
  Golden.SetNormal(FSlateRoundedBoxBrush(Wardrobe::Rose,14.f,Wardrobe::Pink,1.f)).SetHovered(FSlateRoundedBoxBrush(Wardrobe::Color(TEXT("E76C9E")),14.f,Wardrobe::Milk,1.5f)).SetPressed(FSlateRoundedBoxBrush(Wardrobe::Pink,14.f));
  Card.SetNormal(FSlateRoundedBoxBrush(Wardrobe::Milk.CopyWithNewOpacity(.96f),12.f,Wardrobe::Border.CopyWithNewOpacity(.70f),1.f)).SetHovered(FSlateRoundedBoxBrush(Wardrobe::Blush,12.f,Wardrobe::Rose,2.f)).SetPressed(Selected);
  Input=FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(TEXT("NormalEditableTextBox"));Input.SetFont(Font(18)).SetForegroundColor(Wardrobe::White).SetPadding(FMargin(15,13)).SetBackgroundImageNormal(Panel).SetBackgroundImageHovered(Panel).SetBackgroundImageFocused(Selected);
  Canvas=SNew(SCanvas);
  auto Main=SNew(SCanvas).Visibility_Lambda([this](){return MainVisible();});
  Put(Main,48,36,200,35,Label(TEXT("奇境漫游"),25));
  Put(Main,50,78,240,23,Label(TEXT("WONDER  /  COLLECTION"),10,Wardrobe::Soft));
  // Narrow mode bar follows the reference's four highlighted tabs.
  Put(Main,452,33,34,34,SNew(SBorder).BorderImage(&Panel).HAlign(HAlign_Center).VAlign(VAlign_Center)[Label(TEXT("Q"),15)]);
  Put(Main,1162,33,34,34,SNew(SBorder).BorderImage(&Panel).HAlign(HAlign_Center).VAlign(VAlign_Center)[Label(TEXT("E"),15)]);
  for(int32 i=0;i<4;i++){
   auto Tab=SNew(SOverlay).Visibility_Lambda([this,i](){return Owner.IsValid()&&(Owner->CurrentMode()/4)*4+i<Owner->CharacterCount()?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
   Tab->AddSlot()[SNew(SBorder).BorderImage(&Selected).Visibility_Lambda([this,i](){return Owner.IsValid()&&Owner->CurrentMode()%4==i?EVisibility::HitTestInvisible:EVisibility::Hidden;})];
   Tab->AddSlot()[SNew(SButton).ButtonStyle(&Clear).IsFocusable(false).HAlign(HAlign_Center).VAlign(VAlign_Center).OnClicked_Lambda([this,i](){if(Owner.IsValid())Owner->ChangeMode((Owner->CurrentMode()/4)*4+i);return FReply::Handled();})[SNew(STextBlock).Text_Lambda([this,i](){return Wardrobe::T(Owner.IsValid()?Owner->ModeLabel((Owner->CurrentMode()/4)*4+i):TEXT("人物"));}).Font(Font(14)).OverflowPolicy(ETextOverflowPolicy::Ellipsis).ColorAndOpacity_Lambda([this,i](){return Owner.IsValid()&&Owner->CurrentMode()%4==i?Wardrobe::Pink:Wardrobe::White;})]];
   Put(Main,499+i*160,25,150,52,Tab);
  }
  Put(Main,54,138,245,26,Label(TEXT("角色预览"),19));
  Put(Main,54,171,280,26,SNew(STextBlock).Text_Lambda([this](){return Wardrobe::T(Owner.IsValid()?Owner->ModeLabel(Owner->CurrentMode()):TEXT(""));}).Font(Font(13)).ColorAndOpacity(Wardrobe::Soft));
  Put(Main,64,363,38,40,Button(TEXT(""),TEXT("zoom-in"),[this](){if(Owner.IsValid())Owner->ZoomCharacter(-3);}));
  Put(Main,64,421,38,40,Button(TEXT(""),TEXT("zoom-out"),[this](){if(Owner.IsValid())Owner->ZoomCharacter(3);}));
  Put(Main,64,479,38,40,Button(TEXT(""),TEXT("rotate-ccw"),[this](){if(Owner.IsValid())Owner->ResetCharacter();}));
  Put(Main,62,802,214,52,Button(TEXT("退出游戏"),TEXT("door-open"),[this](){if(Owner.IsValid())Owner->SetQuitPrompt(true);},true));
  Put(Main,306,831,728,42,Label(TEXT("左键 · 手脚拖拽 / 揉捏 / 物理    右键横拖 · 转角色    中键 · 平移    滚轮 · 缩放"),11,Wardrobe::Soft));
  Put(Main,1060,106,488,671,SNew(SBorder).BorderImage(&Panel));
  auto Lobby=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&Owner->IsLoggedIn()?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
  Put(Lobby,1090,125,148,38,Label(TEXT("目的地"),23));
  Put(Lobby,1380,134,130,30,Label(TEXT("选择地图"),14,Wardrobe::Soft));
  Put(Lobby,1090,181,425,1,SNew(SImage).Image(Wardrobe::Solid()).ColorAndOpacity(Wardrobe::Border.CopyWithNewOpacity(.70f)));
  Put(Lobby,1090,200,435,390,SAssignNew(ChoiceHost,SBox)[BuildChoices()]);
  Put(Lobby,1090,609,427,48,SNew(STextBlock).Text_Lambda([this](){return Wardrobe::T(Owner.IsValid()?Owner->SelectionDescription():TEXT(""));}).Font(Font(12)).ColorAndOpacity(Wardrobe::Soft).AutoWrapText(true));
  Put(Lobby,1090,683,428,54,Button(TEXT("返回登录"),TEXT("arrow-left"),[this](){if(Owner.IsValid())Owner->ReturnToLogin();}));
  Put(Lobby,1060,801,488,53,Button(TEXT("开始游戏"),TEXT("play"),[this](){if(Owner.IsValid())Owner->StartGame();},true));
  Put(Main,0,0,1600,900,Lobby);
  auto LoginPanel=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&!Owner->IsLoggedIn()?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
  Put(LoginPanel,1110,184,360,55,Label(TEXT("欢迎来到奇境"),30));
  Put(LoginPanel,1112,254,350,46,Label(TEXT("每一次相遇，都有新的风景。"),16,Wardrobe::Soft));
  Put(LoginPanel,1112,346,300,27,Label(TEXT("旅人昵称"),14));
  Put(LoginPanel,1112,385,380,60,SAssignNew(Name,SEditableTextBox).Style(&Input).HintText(Wardrobe::T(TEXT("输入名字，或直接进入"))).OnTextCommitted_Lambda([this](const FText& Text,ETextCommit::Type Kind){if(Kind==ETextCommit::OnEnter&&Owner.IsValid())Owner->Login(Text.ToString());}));
  Put(LoginPanel,1112,482,380,58,Button(Owner.IsValid()&&Owner->HasStory()?TEXT("新游戏"):TEXT("进入奇境"),TEXT("arrow-right"),[this](){if(Owner.IsValid())Owner->Login(Name->GetText().ToString());},true));
  Put(LoginPanel,1112,566,380,30,Label(TEXT("本地体验  ·  无需账号密码"),12,Wardrobe::Soft));
  auto LoginStory=SNew(SHorizontalBox).Visibility_Lambda([this](){return Owner.IsValid()&&Owner->HasStory()?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
  LoginStory->AddSlot().FillWidth(1).Padding(3)[Button(TEXT("继续游戏"),TEXT("arrow-right"),[this](){if(Owner.IsValid())Owner->ContinueGame();})];
  LoginStory->AddSlot().FillWidth(1).Padding(3)[Button(TEXT("读取存档"),TEXT("mountain"),[this](){if(Owner.IsValid())Owner->OpenStorySlots(false);})];
  Put(LoginPanel,1112,627,380,54,LoginStory);
  Put(Main,0,0,1600,900,LoginPanel);
  Put(Canvas.ToSharedRef(),0,0,1600,900,Main);
  auto PlayHUD=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&(!Owner->IsMenuMap())?EVisibility::SelfHitTestInvisible:EVisibility::Collapsed;});
  Put(PlayHUD,48,35,240,48,Button(TEXT("返回角色界面"),TEXT("arrow-left"),[this](){if(Owner.IsValid())Owner->ReturnToMenu();}));
  Put(PlayHUD,1210,35,340,48,SNew(SBorder).BorderImage(&Panel).HAlign(HAlign_Center).VAlign(VAlign_Center)[Label(TEXT("奇境漫游  /  场景预览"),16)]);
  Put(Canvas.ToSharedRef(),0,0,1600,900,PlayHUD);
  Put(Canvas.ToSharedRef(),475,116,550,48,SNew(SBorder).BorderImage(&Toast).HAlign(HAlign_Center).VAlign(VAlign_Center).Visibility_Lambda([this](){return Owner.IsValid()&&!Owner->Message.IsEmpty()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})[SNew(STextBlock).Text_Lambda([this](){return Wardrobe::T(Owner.IsValid()?Owner->Message:TEXT(""));}).Font(Font(15)).ColorAndOpacity(Wardrobe::White)]);
  auto Exit=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&Owner->QuitPrompt?EVisibility::Visible:EVisibility::Collapsed;});
  Put(Exit,0,0,1600,900,SNew(SImage).Image(Wardrobe::Solid()).ColorAndOpacity(Wardrobe::Color(TEXT("5E3048")).CopyWithNewOpacity(.58f)));
  Put(Exit,540,305,520,260,SNew(SBorder).BorderImage(&Panel));Put(Exit,685,344,280,40,Label(TEXT("结束本次旅程？"),25));
  Put(Exit,584,463,196,51,Button(TEXT("继续漫游"),TEXT("arrow-left"),[this](){if(Owner.IsValid())Owner->SetQuitPrompt(false);}));
  Put(Exit,814,463,196,51,Button(TEXT("退出游戏"),TEXT("door-open"),[this](){if(Owner.IsValid())Owner->Quit();},true));Put(Canvas.ToSharedRef(),0,0,1600,900,Exit);
  auto Saves=SNew(SCanvas).Visibility_Lambda([this](){return Owner.IsValid()&&Owner->StorySlotsOpen?EVisibility::Visible:EVisibility::Collapsed;});
  Put(Saves,0,0,1600,900,SNew(SBorder).BorderImage(Wardrobe::Solid()).BorderBackgroundColor(Wardrobe::Color(TEXT("5E3048")).CopyWithNewOpacity(.58f)));
  Put(Saves,440,180,720,560,SNew(SBorder).BorderImage(&Panel));
  Put(Saves,474,212,620,44,SNew(STextBlock).Font(Font(26)).ColorAndOpacity(Wardrobe::White).Text_Lambda([this](){return Wardrobe::T(bNewStory?TEXT("新游戏 · 选择存档位置"):TEXT("读取存档"));}));
  Put(Saves,474,276,652,356,SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(SaveRows,SVerticalBox)]);
  Put(Saves,474,662,410,40,Label(TEXT("每场演出结束后自动保存进度"),14,Wardrobe::Soft));
  Put(Saves,918,654,208,48,Button(TEXT("返回"),TEXT("arrow-left"),[this](){if(Owner.IsValid())Owner->CloseStorySlots();}));
  Put(Canvas.ToSharedRef(),0,0,1600,900,Saves);
  ChildSlot[SNew(SScaleBox).Stretch(EStretch::ScaleToFit).HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).WidthOverride(1600).HeightOverride(900)[Canvas.ToSharedRef()]]];
 }
 virtual FReply OnPreviewMouseButtonDown(const FGeometry&,const FPointerEvent& Event)override{
  if(Owner.IsValid()&&Event.GetEffectingButton()==EKeys::LeftMouseButton)Owner->PlayClickEffect(Event.GetScreenSpacePosition());
  return FReply::Unhandled();
 }
 virtual bool SupportsKeyboardFocus()const override{return true;}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& Key)override{
  if(!Owner.IsValid())return FReply::Unhandled();
  if(Owner->StorySlotsOpen){if(Key.GetKey()==EKeys::Escape&&!Key.IsRepeat())Owner->CloseStorySlots();return FReply::Handled();}
  if(!Owner->QuitPrompt&&Key.GetKey()==EKeys::Q){Owner->ChangeMode(Owner->CurrentMode()-1);return FReply::Handled();}
  if(!Owner->QuitPrompt&&Key.GetKey()==EKeys::E){Owner->ChangeMode(Owner->CurrentMode()+1);return FReply::Handled();}
  if(Key.GetKey()==EKeys::Escape){if(Owner->QuitPrompt)Owner->SetQuitPrompt(false);else if(!Owner->IsMenuMap())Owner->ReturnToMenu();else Owner->SetQuitPrompt(true);return FReply::Handled();}
  return FReply::Unhandled();
 }
 bool OnCharacter(const FPointerEvent& Event)const{if(!Owner.IsValid()||!Owner->CanPreviewInput()||Owner->QuitPrompt)return false;auto P=Canvas->GetCachedGeometry().AbsoluteToLocal(Event.GetScreenSpacePosition());return P.X>135&&P.X<1020&&P.Y>100&&P.Y<800;}
 virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent& Event)override{
  if(DragButton.IsValid())return FReply::Handled();
  if(OnCharacter(Event)&&(Event.GetEffectingButton()==EKeys::LeftMouseButton||Event.GetEffectingButton()==EKeys::RightMouseButton||Event.GetEffectingButton()==EKeys::MiddleMouseButton)){
   DragButton=Event.GetEffectingButton();
   PressPosition=Event.GetScreenSpacePosition();bDragged=false;
   bPhysicsDragging=DragButton==EKeys::LeftMouseButton&&Owner->BeginPhysicsDrag(PressPosition);
   return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
  }return FReply::Unhandled();
 }
 virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent& Event)override{
  if(DragButton.IsValid()&&Event.GetEffectingButton()==DragButton){
   const bool bConsumedByDrag=bPhysicsDragging&&((Owner.IsValid()&&Owner->HasLimbDragGesture())||bDragged||FVector2D::Distance(PressPosition,Event.GetScreenSpacePosition())>1.f);
   if(Owner.IsValid())Owner->EndPhysicsDrag();bPhysicsDragging=false;
   const float Threshold=Owner.IsValid()&&Owner->Data?Owner->Data->ClickDragThreshold:6.f;
   if(DragButton==EKeys::LeftMouseButton&&!bConsumedByDrag&&!bDragged&&FVector2D::Distance(PressPosition,Event.GetScreenSpacePosition())<Threshold&&OnCharacter(Event))Owner->ClickCharacter(Event.GetScreenSpacePosition());
   DragButton=FKey();return FReply::Handled().ReleaseMouseCapture();
  }return FReply::Unhandled();
 }
 virtual void OnMouseCaptureLost(const FCaptureLostEvent&)override{if(Owner.IsValid())Owner->EndPhysicsDrag();bPhysicsDragging=false;DragButton=FKey();}
 virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent& Event)override{
  if(Owner.IsValid()&&DragButton.IsValid()){
   if(!Event.IsMouseButtonDown(DragButton)){Owner->EndPhysicsDrag();bPhysicsDragging=false;DragButton=FKey();return FReply::Handled().ReleaseMouseCapture();}
   const FVector2D Delta=Event.GetCursorDelta();
   const float Threshold=Owner->Data?FMath::Max(1.f,Owner->Data->ClickDragThreshold):6.f;
   bDragged|=FVector2D::Distance(PressPosition,Event.GetScreenSpacePosition())>=Threshold;
   if(bPhysicsDragging)Owner->UpdatePhysicsDrag(Event.GetScreenSpacePosition());
   else if(DragButton==EKeys::MiddleMouseButton)Owner->PanCamera(Delta);
   else if(DragButton==EKeys::RightMouseButton&&bDragged)Owner->TurnCharacter(Delta.X*.35f);
   return FReply::Handled();
  }return FReply::Unhandled();
 }
 virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent& Event)override{if(OnCharacter(Event)){Owner->ZoomCharacter(-Event.GetWheelDelta()*2);return FReply::Handled();}return FReply::Unhandled();}
 virtual void Tick(const FGeometry& G,double Time,float Delta)override{SCompoundWidget::Tick(G,Time,Delta);Age+=Delta;}
 virtual int32 OnPaint(const FPaintArgs& A,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& E,int32 L,const FWidgetStyle& Style,bool Enabled)const override{
  int32 Last=SCompoundWidget::OnPaint(A,G,Clip,E,L,Style,Enabled);
  if(MainVisible()==EVisibility::Collapsed)return Last;
  auto C=Canvas->GetCachedGeometry();
  auto Line=[&](TArray<FVector2f> P,FLinearColor Col,float Thick=1.f){FSlateDrawElement::MakeLines(E,Last+1,C.ToPaintGeometry(),P,ESlateDrawEffect::None,Col,true,Thick);};
  Line({{32,25},{32,873},{1568,873},{1568,25}},Wardrobe::Border.CopyWithNewOpacity(.50f));
  Line({{32,107},{1007,107}},Wardrobe::Border.CopyWithNewOpacity(.38f));
  Line({{46,25},{32,25},{32,39}},Wardrobe::Pink);Line({{1554,25},{1568,25},{1568,39}},Wardrobe::Pink);
  int Mode=Owner.IsValid()?Owner->CurrentMode():0;float X=574+(Mode%4)*160;
  Line({{X-7,81},{X,87},{X+7,81},{X,75},{X-7,81}},Wardrobe::Pink,1.3f);
  for(int i=0;i<23;i++){float Xp=175+FMath::Fmod(i*151.f,820.f);float Yp=145+FMath::Fmod(i*73.f+Age*(2+i%3),615.f);float Op=.1f+.20f*(.5f+.5f*FMath::Sin(Age+i));Line({{Xp-2,Yp},{Xp+2,Yp}},Wardrobe::Blush.CopyWithNewOpacity(Op));Line({{Xp,Yp-2},{Xp,Yp+2}},Wardrobe::Blush.CopyWithNewOpacity(Op));}
  return Last+1;
 }
};

UWardrobeSession* UWardrobeMenu::Session()const{return GetGameInstance()?GetGameInstance()->GetSubsystem<UWardrobeSession>():nullptr;}
void UWardrobeMenu::RestoreMenuFocus(){if(SlateMenu)FSlateApplication::Get().SetAllUserFocus(SlateMenu,EFocusCause::SetDirectly);}
bool UWardrobeMenu::IsLoggedIn()const{return Session()&&Session()->LoggedIn;}
bool UWardrobeMenu::IsMenuMap()const{return GetWorld()&&((Data&&!Data->MenuLevel.IsNull()&&UGameplayStatics::GetCurrentLevelName(this,true)==Data->MenuLevel.ToSoftObjectPath().GetAssetName())||GetWorld()->GetMapName().Contains(TEXT("L_Wardrobe")));}
int32 UWardrobeMenu::CurrentMode()const{return IsValid(StationController)?(StationController->IsTransitioning()?StationController->TargetStation:FMath::Max(0,StationController->CurrentStation)):0;}
int32 UWardrobeMenu::CharacterCount()const{return Data&&Data->CharacterData?Data->CharacterData->Characters.Num():0;}
FString UWardrobeMenu::ModeLabel(int32 I)const{return Data&&Data->CharacterData&&Data->CharacterData->Characters.IsValidIndex(I)?Data->CharacterData->Characters[I].Label.ToString():TEXT("未配置人物");}
int32 UWardrobeMenu::ChoiceCount()const{return Data?Data->Levels.Num():0;}
int32 UWardrobeMenu::SelectedChoice()const{return Session()?Session()->SelectedMap:INDEX_NONE;}
bool UWardrobeMenu::ChoiceReady(int32 I)const{return Data&&Data->Levels.IsValidIndex(I)&&!Data->Levels[I].Level.IsNull();}
FText UWardrobeMenu::ChoiceLabel(int32 I)const{return Data&&Data->Levels.IsValidIndex(I)?Data->Levels[I].Label:FText::GetEmpty();}
TSoftObjectPtr<UTexture2D> UWardrobeMenu::ChoiceThumbnail(int32 I)const{return Data&&Data->Levels.IsValidIndex(I)?Data->Levels[I].Thumbnail:TSoftObjectPtr<UTexture2D>();}
FString UWardrobeMenu::SelectionDescription()const{
 const int32 I=SelectedChoice();if(I<0||I>=ChoiceCount())return TEXT("选择一张地图，再点击开始游戏");
 if(!ChoiceReady(I))return TEXT("该地图尚未配置，敬请期待");
 return FString::Printf(TEXT("已选：%s\n点击开始游戏进入地图"),*ChoiceLabel(I).ToString());
}
void UWardrobeMenu::SelectChoice(int32 I){
 if(!IsMenuMap()||!IsLoggedIn()||QuitPrompt||StorySlotsOpen||I<0||I>=ChoiceCount())return;
 PlayUISound(ChoiceReady(I)?EWardrobeUISound::MapSelect:EWardrobeUISound::Unavailable);
 if(auto* S=Session())S->SelectedMap=I;RestoreMenuFocus();
}
void UWardrobeMenu::StartGame(){
 if(!IsMenuMap()||QuitPrompt||StorySlotsOpen)return;
 if(!IsLoggedIn()){Message=TEXT("请先进入奇境");NoticeTime=3;PlayUISound(EWardrobeUISound::Unavailable);return;}
 auto* Loading=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>();
 if(!Loading||Loading->IsLoading())return;
 const int32 I=SelectedChoice();
 if(!ChoiceReady(I)){Message=TEXT("请先选择已配置的地图");NoticeTime=3;PlayUISound(EWardrobeUISound::Unavailable);return;}
 const auto& Item=Data->Levels[I];
 if(!FPackageName::DoesPackageExist(Item.Level.ToSoftObjectPath().GetLongPackageName())){Message=TEXT("目标地图不存在，或没有加入打包");NoticeTime=3;PlayUISound(EWardrobeUISound::Unavailable);return;}
 if(Loading->TravelToLevel(Item.Level,Data,Item.LoadingBackground,Item.Label,TEXT("game=/Game/BluePrint/ALS/Game/BP_ALSGameMode.BP_ALSGameMode_C"))){
  if(auto* S=Session()){S->ExplorationMenuData=Data;const FString Package=UWorld::RemovePIEPrefix(GetWorld()->GetOutermost()->GetName());S->ExplorationReturnLevel=TSoftObjectPtr<UWorld>(FSoftObjectPath(Package+TEXT(".")+FPackageName::GetLongPackageAssetName(Package)));}
  if(SlateMenu)SlateMenu->CancelPointerGesture();
  if(IsValid(StationController))StationController->SetPreviewEnabled(false);
  PlayUISound(EWardrobeUISound::StartGame);
  UE_LOG(LogTemp,Display,TEXT("WARDROBE_START map=%s"),*Item.Level.ToString());
 }
}
bool UWardrobeMenu::HasStory() const{return Data&&Data->StoryFlow;}
void UWardrobeMenu::OpenStorySlots(bool NewGame){
 if(!HasStory()||!IsMenuMap()||IsLoggedIn()||QuitPrompt||StorySlotsOpen)return;
 if(auto* Loading=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>())if(Loading->IsLoading())return;
 auto* Story=GetGameInstance()->GetSubsystem<UWardrobeStorySubsystem>();
 if(!Story->Configure(Data->StoryFlow)){Message=Story->LastError.ToString();NoticeTime=8;PlayUISound(EWardrobeUISound::Unavailable);return;}
 PlayUISound(NewGame?EWardrobeUISound::NewGame:EWardrobeUISound::LoadGame);
 StorySlotsOpen=true;if(SlateMenu)SlateMenu->OpenStorySlots(NewGame);RestoreMenuFocus();
}
void UWardrobeMenu::CloseStorySlots(){
 if(!StorySlotsOpen)return;
 StorySlotsOpen=false;PlayUISound(EWardrobeUISound::Back);RestoreMenuFocus();
}
void UWardrobeMenu::PrepareStoryTravel(){
 StorySlotsOpen=false;if(SlateMenu)SlateMenu->CancelPointerGesture();
 if(IsValid(StationController))StationController->SetPreviewEnabled(false);
}
void UWardrobeMenu::ChooseStorySlot(int32 Index,bool Overwrite){
 if(!HasStory()||!StorySlotsOpen||!SlateMenu)return;
 auto* Story=GetGameInstance()->GetSubsystem<UWardrobeStorySubsystem>();
 const bool NewGame=SlateMenu->bNewStory;
 const bool Success=NewGame?Story->StartNewGame(Index,PendingNewGameName,Data,Overwrite):Story->ContinueGame(Index,Data);
 if(Success){
  PlayUISound(EWardrobeUISound::SaveSlotSelect);
  if(NewGame){StorySlotsOpen=false;Message=TEXT("欢迎来到奇境，")+(Session()?Session()->Name:PendingNewGameName);NoticeTime=3;RestoreMenuFocus();}
  else PrepareStoryTravel();
 }
 else {Message=Story->LastError.ToString();NoticeTime=8;PlayUISound(EWardrobeUISound::Unavailable);}
}
void UWardrobeMenu::ContinueGame(){
 if(!HasStory()||!IsMenuMap()||IsLoggedIn()||QuitPrompt||StorySlotsOpen)return;
 auto* Story=GetGameInstance()->GetSubsystem<UWardrobeStorySubsystem>();
 if(Story->Configure(Data->StoryFlow)&&Story->ContinueLatest(Data)){PlayUISound(EWardrobeUISound::ContinueGame);PrepareStoryTravel();}
 else {Message=Story->LastError.ToString();NoticeTime=6;PlayUISound(EWardrobeUISound::Unavailable);}
}
TSharedRef<SWidget> UWardrobeMenu::RebuildWidget(){if(!Data)Data=LoadObject<UWardrobeMenuData>(nullptr,TEXT("/Game/Wardrobe/DA_WardrobeMenu.DA_WardrobeMenu"));SlateMenu=SNew(SWardrobeMenu).Owner(this);return SlateMenu.ToSharedRef();}
void UWardrobeMenu::ReleaseSlateResources(bool B){EndPhysicsDrag();Super::ReleaseSlateResources(B);SlateMenu.Reset();}
void UWardrobeMenu::NativeConstruct(){
 Super::NativeConstruct();
 if(HasStory())GetGameInstance()->GetSubsystem<UWardrobeStorySubsystem>()->Configure(Data->StoryFlow);
 if(auto* Audio=GetGameInstance()->GetSubsystem<UWardrobeAudioSubsystem>())Audio->Configure(Data?Data->AudioData.Get():nullptr);
 if(IsMenuMap()){
  StationController=NewObject<UWardrobeStationController>(this);
  if(!StationController->Initialize(GetOwningPlayer(),Data?Data->CharacterData.Get():nullptr,Session()?Session()->Mode:INDEX_NONE)){
   Message=StationController->LastError.ToString();NoticeTime=15;
  }
  StationController->SetPreviewEnabled(true);Showcase=StationController->GetActiveCharacter();
 }
 CapturePreviewCamera();
 if(auto* PC=GetOwningPlayer()){PC->bShowMouseCursor=true;FInputModeUIOnly Input;Input.SetWidgetToFocus(SlateMenu);Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);PC->SetInputMode(Input);}
 RestoreMenuFocus();
}
void UWardrobeMenu::Login(const FString& Value){
 if(!IsMenuMap()||IsLoggedIn()||QuitPrompt||StorySlotsOpen)return;
 if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return;
 PendingNewGameName=Value.TrimStartAndEnd().Left(16);if(PendingNewGameName.IsEmpty())PendingNewGameName=TEXT("旅人");
 if(HasStory()){OpenStorySlots(true);return;}
 PlayUISound(EWardrobeUISound::Login);if(auto* S=Session()){S->LoggedIn=true;S->Name=PendingNewGameName;Message=TEXT("欢迎回来，")+S->Name;}
 RestoreMenuFocus();NoticeTime=3;
}
void UWardrobeMenu::ReturnToLogin(){
 if(!IsMenuMap()||!IsLoggedIn()||QuitPrompt||StorySlotsOpen)return;
 if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return;
 if(SlateMenu)SlateMenu->CancelPointerGesture();
 if(auto* S=Session()){S->LoggedIn=false;if(SlateMenu&&SlateMenu->Name)SlateMenu->Name->SetText(FText::FromString(S->Name));}
 PlayUISound(EWardrobeUISound::Back);RestoreMenuFocus();Message.Empty();
}
void UWardrobeMenu::ChangeMode(int32 I){
 if(!IsMenuMap()||QuitPrompt||StorySlotsOpen||CharacterCount()==0||!IsValid(StationController)||StationController->IsTransitioning())return;
 if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return;
 const int32 Mode=((I%CharacterCount())+CharacterCount())%CharacterCount();
 if(Mode==CurrentMode())return;
 if(!StationController->SelectStation(Mode)){Message=StationController->LastError.ToString();NoticeTime=3;PlayUISound(EWardrobeUISound::Unavailable);return;}
 if(SlateMenu)SlateMenu->CancelPointerGesture();
 Showcase=StationController->GetActiveCharacter();
 const auto& Sound=Data->CharacterData->Characters[Mode].TransitionSound;
 if(Sound.Sound.IsNull())PlayUISound(EWardrobeUISound::ActionSwitch);
 else if(auto* Audio=GetGameInstance()->GetSubsystem<UWardrobeAudioSubsystem>()){FWardrobeSoundEntry Entry;Entry.Sound=Sound.Sound;Audio->PlayEntry(EWardrobeUISound::ActionSwitch,Entry);}
 Message=ModeLabel(Mode);NoticeTime=2.5f;RestoreMenuFocus();
}
void UWardrobeMenu::OpenChoice(int32 I){SelectChoice(I);if(SelectedChoice()==I)StartGame();}
void UWardrobeMenu::ReturnToMenu(){
 if(IsMenuMap())return;
 if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>()){
  if(!L->IsLoading())PlayUISound(EWardrobeUISound::Back);
  const auto Map=Data?Data->MenuLevel:TSoftObjectPtr<UWorld>();
  L->TravelToLevel(Map,Data,{},FText::FromString(TEXT("返回奇境")),TEXT("game=/Script/DreamBloom.WardrobeMenuGameMode"));
 }
}
void UWardrobeMenu::PlayUISound(EWardrobeUISound Event){if(GetGameInstance())if(auto* Audio=GetGameInstance()->GetSubsystem<UWardrobeAudioSubsystem>())Audio->PlayEvent(Event);}
void UWardrobeMenu::SetQuitPrompt(bool Enabled){if(QuitPrompt==Enabled)return;QuitPrompt=Enabled;if(Enabled&&SlateMenu)SlateMenu->CancelPointerGesture();if(IsValid(StationController))StationController->SetPreviewEnabled(!Enabled);PlayUISound(Enabled?EWardrobeUISound::Confirm:EWardrobeUISound::Cancel);}
void UWardrobeMenu::Quit(){if(auto* Story=GetGameInstance()->GetSubsystem<UWardrobeStorySubsystem>())Story->SaveCheckpoint();UKismetSystemLibrary::QuitGame(this,GetOwningPlayer(),EQuitPreference::Quit,false);}
bool UWardrobeMenu::CanPreviewInput()const{
 if(!IsMenuMap()||QuitPrompt||StorySlotsOpen||!IsValid(StationController)||!StationController->CanManipulate())return false;
 if(auto* L=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>())if(L->IsLoading())return false;
 return true;
}
void UWardrobeMenu::CapturePreviewCamera(){PreviewCamera=IsValid(StationController)?StationController->GetPreviewCamera():nullptr;}
void UWardrobeMenu::TurnCharacter(float Degrees){if(!DragCharacter.IsValid()&&CanPreviewInput()&&StationController->CanRotateCharacter())if(auto* C=StationController->GetActiveCharacter())C->AddActorWorldRotation(FRotator(0,Degrees,0));}
void UWardrobeMenu::ZoomCharacter(float Amount){if(DragCharacter.IsValid()||!CanPreviewInput())return;CapturePreviewCamera();if(PreviewCamera.IsValid())PreviewCamera->GetCameraComponent()->SetFieldOfView(FMath::Clamp(PreviewCamera->GetCameraComponent()->FieldOfView+Amount,10.f,90.f));}
void UWardrobeMenu::PanCamera(FVector2D Delta){
 if(DragCharacter.IsValid()||!CanPreviewInput())return;CapturePreviewCamera();if(!PreviewCamera.IsValid())return;
 auto* C=PreviewCamera.Get();auto* Person=StationController->GetActiveCharacter();int32 W=1600,H=900;GetOwningPlayer()->GetViewportSize(W,H);
 const float Distance=FVector::Distance(C->GetActorLocation(),Person->GetActorLocation());
 const float UnitsPerPixel=2.f*Distance*FMath::Tan(FMath::DegreesToRadians(C->GetCameraComponent()->FieldOfView*.5f))/FMath::Max(1,W);
 C->SetActorLocation(C->GetActorLocation()+(-C->GetActorRightVector()*Delta.X+C->GetActorUpVector()*Delta.Y)*UnitsPerPixel);
}
void UWardrobeMenu::ClickCharacter(FVector2D ScreenPosition){
 if(!CanPreviewInput()||!GetOwningPlayer())return;
 Showcase=StationController->GetActiveCharacter();if(!IsValid(Showcase))return;
 FVector2D Pixel,Viewport;USlateBlueprintLibrary::AbsoluteToViewport(this,ScreenPosition,Pixel,Viewport);
 FVector Origin,Direction;if(GetOwningPlayer()->DeprojectScreenPositionToWorld(Pixel.X,Pixel.Y,Origin,Direction)){
  Showcase->CharacterMessage=FText::GetEmpty();
  FHitResult TouchHit;
  if(Showcase->TraceTouch(Origin,Direction,TouchHit))Showcase->ApplyPhysicsTouch(TouchHit.ImpactPoint,Direction);
  Showcase->ClickInteraction(Origin,Direction);
  if(!Showcase->CharacterMessage.IsEmpty()){Message=Showcase->CharacterMessage.ToString();NoticeTime=2.5f;}
 }
}
void UWardrobeMenu::ResetCharacter(){if(!CanPreviewInput())return;PlayUISound(EWardrobeUISound::ResetCamera);StationController->ResetPreview();}
void UWardrobeMenu::NativeDestruct(){EndPhysicsDrag();StopClickEffect();if(IsValid(StationController))StationController->Shutdown();StationController=nullptr;Super::NativeDestruct();}
void UWardrobeMenu::NativeTick(const FGeometry& G,float Dt){
 Super::NativeTick(G,Dt);auto* Loading=GetGameInstance()->GetSubsystem<UWardrobeLoadingSubsystem>();
 const bool LoadingActive=Loading&&Loading->IsLoading();
 if(ClickEffectPlayer){ClickEffectAge+=Dt;if(LoadingActive||!ClickEffectPlayer->IsPlaying()||ClickEffectAge>=(Data&&Data->ClickEffect?FMath::Max(.1f,Data->ClickEffect->MaxDuration):5.f))StopClickEffect();}
 if(IsValid(StationController)){
  // A save-slot overlay blocks gestures through CanPreviewInput, not Sequence playback.
  StationController->SetPreviewEnabled(!LoadingActive&&!QuitPrompt);
  StationController->Tick(Dt);
  Showcase=StationController->GetActiveCharacter();
  if(!StationController->IsTransitioning()&&StationController->CurrentStation!=INDEX_NONE)if(auto* S=Session())S->Mode=StationController->CurrentStation;
 }
#if WITH_EDITOR
 if(FParse::Param(FCommandLine::Get(),TEXT("WardrobeLocomotionMenuQA")))if(auto* S=Session()){
  static int QAVisits=0;static double QATime=0;
  if(QATime==0){QATime=FPlatformTime::Seconds();++QAVisits;UE_LOG(LogTemp,Display,TEXT("ALS68_MENU_QA visit=%d menuMap=%d controller=%s pawn=%s"),QAVisits,IsMenuMap(),*GetOwningPlayer()->GetClass()->GetName(),*GetNameSafe(GetOwningPlayer()->GetPawn()));}
  if(LoadingActive&&QAVisits>1)QATime=FPlatformTime::Seconds();
  if(QAVisits==1&&FPlatformTime::Seconds()-QATime>5){
   Login(TEXT("旅人"));for(int I=0;Data&&I<Data->Levels.Num();I++)if(Data->Levels[I].Level.ToSoftObjectPath().GetLongPackageName()==TEXT("/Game/BluePrint/ALS/Maps/L_ALSFirstDemo")){SelectChoice(I);StartGame();break;}
   QATime=0;QAVisits=1;
  }else if(QAVisits>1&&FPlatformTime::Seconds()-QATime>5){UE_LOG(LogTemp,Display,TEXT("ALS68_MENU_RETURN_OK loggedIn=%d menuMap=%d"),S->LoggedIn,IsMenuMap());FPlatformMisc::RequestExit(false);}
 }
#endif
 if(!LoadingActive&&NoticeTime>0){NoticeTime-=Dt;if(NoticeTime<=0)Message.Empty();}
}
AWardrobeMenuGameMode::AWardrobeMenuGameMode(){DefaultPawnClass=nullptr;MenuClass=UWardrobeMenu::StaticClass();}
void AWardrobeMenuGameMode::BeginPlay(){Super::BeginPlay();if(auto* PC=UGameplayStatics::GetPlayerController(this,0)){Menu=CreateWidget<UWardrobeMenu>(PC,MenuClass);if(Menu){if(MenuData)Menu->Data=MenuData;else if(auto* S=GetGameInstance()->GetSubsystem<UWardrobeSession>())if(S->ExplorationMenuData)Menu->Data=S->ExplorationMenuData;Menu->AddToViewport(80);}}UE_LOG(LogTemp,Display,TEXT("WARDROBE_UI_READY"));}
