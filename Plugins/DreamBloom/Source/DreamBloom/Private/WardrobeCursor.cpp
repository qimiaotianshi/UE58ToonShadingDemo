#include "WardrobeCursor.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Interfaces/IPluginManager.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/SViewport.h"
#include "Brushes/SlateImageBrush.h"
#include "Rendering/DrawElements.h"

void FWardrobeCursorInputState::Down(FKey Button){
 if(Button==EKeys::LeftMouseButton||Button==EKeys::RightMouseButton||Button==EKeys::MiddleMouseButton){Buttons.Add(Button);ReleasedUntil=0;}
}
void FWardrobeCursorInputState::Up(FKey Button,double Now){if(Buttons.Remove(Button)&&Buttons.IsEmpty())ReleasedUntil=Now+FMath::Clamp(ReleaseTime,0.f,1.f);}
void FWardrobeCursorInputState::Reset(){Buttons.Reset();ReleasedUntil=0;}
EWardrobeCursorState FWardrobeCursorInputState::Get(double Now)const{return !Buttons.IsEmpty()?EWardrobeCursorState::Pressed:Now<ReleasedUntil?EWardrobeCursorState::Released:EWardrobeCursorState::Default;}

class FWardrobeCursorProcessor : public IInputProcessor {
public:
 TWeakObjectPtr<UGameViewportClient> Viewport;
 FWardrobeCursorInputState Input;
 float Size=40;FVector2D HotSpot;
 TSharedPtr<FSlateBrush> Brushes[3];
 bool OwnsPointer(FSlateApplication& App,const FPointerEvent& Event)const{
  auto* Client=Viewport.Get();auto Widget=Client?Client->GetGameViewportWidget():nullptr;
  return Widget&&Widget->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition())&&App.FindWidgetWindow(Widget.ToSharedRef())==App.GetActiveTopLevelWindow();
 }
 virtual void Tick(float,FSlateApplication& App,TSharedRef<ICursor>)override{
  if(!App.IsActive()){Input.Reset();return;}
  TArray<FKey> Lost;for(FKey Button:Input.Buttons)if(!App.GetPressedMouseButtons().Contains(Button))Lost.Add(Button);
  for(FKey Button:Lost)Input.Up(Button,FPlatformTime::Seconds());
 }
 virtual bool HandleMouseButtonDownEvent(FSlateApplication& App,const FPointerEvent& Event)override{if(OwnsPointer(App,Event))Input.Down(Event.GetEffectingButton());return false;}
 virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& App,const FPointerEvent& Event)override{return HandleMouseButtonDownEvent(App,Event);}
 virtual bool HandleMouseButtonUpEvent(FSlateApplication&,const FPointerEvent& Event)override{Input.Up(Event.GetEffectingButton(),FPlatformTime::Seconds());return false;}
 virtual const TCHAR* GetDebugName()const override{return TEXT("WardrobePinkCursor");}
};
class SWardrobeCursor : public SLeafWidget {
public:
 SLATE_BEGIN_ARGS(SWardrobeCursor){} SLATE_ARGUMENT(TSharedPtr<FWardrobeCursorProcessor>,Visual) SLATE_END_ARGS()
 TSharedPtr<FWardrobeCursorProcessor> Visual;
 void Construct(const FArguments& Args){Visual=Args._Visual;SetVisibility(EVisibility::HitTestInvisible);}
 virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(Visual->Size*2);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Elements,int32 Layer,const FWidgetStyle&,bool)const override{
  const auto State=Visual->Input.Get(FPlatformTime::Seconds());const auto& Brush=Visual->Brushes[int32(State)];
  if(Brush){const FVector2D Offset=FVector2D(Visual->Size)-Visual->HotSpot*Visual->Size;
   FSlateDrawElement::MakeBox(Elements,Layer,G.ToPaintGeometry(FVector2D(Visual->Size),FSlateLayoutTransform(Offset)),Brush.Get());}
  return Layer;
 }
};
void UWardrobeCursorSubsystem::Configure(const FWardrobeCursorSettings& Settings){
 Restore();if(!Settings.bEnabled||!FSlateApplication::IsInitialized()||IsRunningCommandlet())return;
 auto* Client=GetGameInstance()->GetGameViewportClient();auto Plugin=IPluginManager::Get().FindPlugin(TEXT("DreamBloom"));if(!Client||!Plugin)return;
 Viewport=Client;bPreviousSoftware=Client->GetIsUsingSoftwareCursorWidgets();bPreviousHideDuringCapture=Client->HideCursorDuringCapture();Client->SetHideCursorDuringCapture(false);
 Processor=MakeShared<FWardrobeCursorProcessor>();Processor->Viewport=Client;
 Processor->Size=FMath::IsFinite(Settings.Size)?FMath::Clamp(Settings.Size,16.f,128.f):40.f;
 Processor->HotSpot=Settings.HotSpot.ContainsNaN()?FVector2D(.0625,.0625):FVector2D(FMath::Clamp(Settings.HotSpot.X,0.,1.),FMath::Clamp(Settings.HotSpot.Y,0.,1.));
 Processor->Input.ReleaseTime=FMath::IsFinite(Settings.ReleaseTime)?FMath::Clamp(Settings.ReleaseTime,0.f,1.f):.16f;
 const TSoftObjectPtr<UTexture2D> Textures[]={Settings.DefaultTexture,Settings.PressedTexture,Settings.ReleasedTexture};
 const TCHAR* Files[]={TEXT("pink_default.png"),TEXT("pink_pressed.png"),TEXT("pink_released.png")};
 for(int32 I=0;I<3;++I){
  if(auto* Texture=Textures[I].LoadSynchronous()){LoadedTextures.AddUnique(Texture);auto Brush=MakeShared<FSlateBrush>();Brush->SetResourceObject(Texture);Brush->ImageSize=FVector2D(Processor->Size);Brush->DrawAs=ESlateBrushDrawType::Image;Processor->Brushes[I]=Brush;}
  else Processor->Brushes[I]=MakeShared<FSlateImageBrush>(Plugin->GetBaseDir()/TEXT("Resources/Cursor")/Files[I],FVector2D(Processor->Size));
 }
 auto Widget=SNew(SWardrobeCursor).Visual(Processor);
 for(auto Type:{EMouseCursor::Default,EMouseCursor::Hand,EMouseCursor::TextEditBeam,EMouseCursor::GrabHand,EMouseCursor::GrabHandClosed}){PreviousWidgets.Add(int32(Type),Client->GetSoftwareCursorWidget(Type));Client->SetSoftwareCursorWidget(Type,Widget);}
 Client->SetUseSoftwareCursorWidgets(true);FSlateApplication::Get().RegisterInputPreProcessor(Processor,0);
}
void UWardrobeCursorSubsystem::Restore(){
 if(Processor&&FSlateApplication::IsInitialized())FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
 if(auto* Client=Viewport.Get()){for(const auto& Entry:PreviousWidgets)Client->SetSoftwareCursorWidget(EMouseCursor::Type(Entry.Key),Entry.Value);Client->SetUseSoftwareCursorWidgets(bPreviousSoftware);Client->SetHideCursorDuringCapture(bPreviousHideDuringCapture);}
 Processor.Reset();PreviousWidgets.Reset();LoadedTextures.Reset();Viewport.Reset();
}
EWardrobeCursorState UWardrobeCursorSubsystem::GetCursorState()const{return Processor?Processor->Input.Get(FPlatformTime::Seconds()):EWardrobeCursorState::Default;}
void UWardrobeCursorSubsystem::Deinitialize(){Restore();Super::Deinitialize();}
