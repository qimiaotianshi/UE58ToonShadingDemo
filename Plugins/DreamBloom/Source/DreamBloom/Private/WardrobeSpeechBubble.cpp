#include "WardrobeSpeechBubble.h"
#include "WardrobeShowcase.h"
#include "WardrobeCharacters.h"
#include "WardrobeMenu.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/IPluginManager.h"
#include "Fonts/CompositeFont.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "UObject/UObjectIterator.h"

namespace WardrobeCaption {
 constexpr float Width=360.f,FadeTime=.18f;
 const FLinearColor Ink(.28f,.085f,.17f,1.f),Pink(1.f,.20f,.43f,1.f);
}

class SWardrobeSpeechCard : public SCompoundWidget {
public:
 SLATE_BEGIN_ARGS(SWardrobeSpeechCard){} SLATE_END_ARGS()
 void Construct(const FArguments&){
  const auto Plugin=IPluginManager::Get().FindPlugin(TEXT("DreamBloom"));
  Font=MakeShared<FCompositeFont>(NAME_None,Plugin->GetBaseDir()/TEXT("Resources/NotoSansSC-Regular.ttf"),EFontHinting::Default,EFontLoadingPolicy::LazyLoad);
  ChildSlot[SNew(SBox).WidthOverride(WardrobeCaption::Width).MinDesiredHeight(126.f)
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight().Padding(19,5,0,5)
     [SNew(STextBlock).Text(FText::FromString(TEXT("悄悄话"))).Font(FSlateFontInfo(Font,14)).ColorAndOpacity(FLinearColor::White)]
    +SVerticalBox::Slot().AutoHeight().Padding(22,17,22,23)
     [SAssignNew(Line,STextBlock).Font(FSlateFontInfo(Font,19)).ColorAndOpacity(WardrobeCaption::Ink).WrapTextAt(316.f).LineHeightPercentage(1.25f)]
   ]];
 }
 void SetCaption(const FString& Text){Line->SetText(FText::FromString(Text));}
 virtual int32 OnPaint(const FPaintArgs& Args,const FGeometry& G,const FSlateRect& Clip,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle& Style,bool Enabled) const override {
  const FVector2f Size=G.GetLocalSize();const FLinearColor Tint=Style.GetColorAndOpacityTint();
  auto Box=[&](const FSlateBrush& Brush,FVector2f P,FVector2f S,FLinearColor C,int32 L){FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(S,FSlateLayoutTransform(P)),&Brush,ESlateDrawEffect::None,C*Tint*Brush.GetTint(Style));};
  // Only the milk-white body is translucent. Header, text and pink ornament are opaque at rest.
  Box(Body,{0,20},{Size.X,Size.Y-20},FLinearColor(1.f,.98f,.99f,.65f),Layer);
  Box(Outline,{0,20},{Size.X,Size.Y-20},FLinearColor::White,Layer+1);
  Box(Header,{0,0},{172,34},WardrobeCaption::Pink,Layer+1);
  for(int32 Row=0;Row<3;++Row)for(int32 Col=0;Col<5;++Col){
   Box(Dot,{99.f+Col*10.f+(Row%2)*3.f,5.f+Row*10.f},{3,3},FLinearColor(1,.66f,.77f,1),Layer+2);
  }
  for(int32 Row=0;Row<4;++Row)for(int32 Col=0;Col<=Row+1;++Col){
   Box(Dot,{Size.X-16.f-Col*9.f,Size.Y-15.f-Row*9.f},{2.5f,2.5f},FLinearColor(1,.72f,.82f,1),Layer+1);
  }
  TArray<FVector2f> Star;const FVector2f Center(161,26);
  for(int32 I=0;I<=10;++I){const float A=-UE_PI/2+I*UE_PI/5,R=I%2?4.5f:10.f;Star.Add(Center+FVector2f(FMath::Cos(A),FMath::Sin(A))*R);}
  FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Star,ESlateDrawEffect::None,FLinearColor::White*Tint,true,1.4f);
  for(int32 I=0;I<3;++I){TArray<FVector2f> Trail={{165.f+I*3.f,16.f-I*3.f},{179.f+I*3.f,2.f}};FSlateDrawElement::MakeLines(Out,Layer+2,G.ToPaintGeometry(),Trail,ESlateDrawEffect::None,FLinearColor::White*Tint,true,1.2f);}
  return SCompoundWidget::OnPaint(Args,G,Clip,Out,Layer+3,Style,Enabled);
 }
private:
 TSharedPtr<FCompositeFont> Font;
 TSharedPtr<STextBlock> Line;
 FSlateRoundedBoxBrush Body{FLinearColor::White,16.f};
 FSlateRoundedBoxBrush Outline{FLinearColor::Transparent,16.f,FLinearColor(1.f,.76f,.84f,1.f),1.f};
 FSlateRoundedBoxBrush Header{FLinearColor::White,FVector4(17,4,4,17)};
 FSlateRoundedBoxBrush Dot{FLinearColor::White,2.f};
};

TSharedRef<SWidget> UWardrobeSpeechBubble::RebuildWidget(){SAssignNew(Card,SWardrobeSpeechCard);Card->SetCaption(Caption);return Card.ToSharedRef();}
void UWardrobeSpeechBubble::ReleaseSlateResources(bool Children){Super::ReleaseSlateResources(Children);Card.Reset();}
void UWardrobeSpeechBubble::ShowCaption(AWardrobeShowcase* Person,const FString& Text){
 Speaker=Person;Caption=Text.Left(120);bFinishing=false;FadeRemaining=0;
 // AddToPlayerScreen defaults to a stretched, full-screen slot. Prepare the card
 // before attachment so its translucent body can never paint over the whole view.
 SetVisibility(ESlateVisibility::HitTestInvisible);SetRenderOpacity(1.f);
 SetDesiredSizeInViewport(FVector2D(WardrobeCaption::Width,126.f));
 SetAlignmentInViewport(FVector2D::ZeroVector);SetPositionInViewport(FVector2D::ZeroVector,false);
 TakeWidget();if(Card)Card->SetCaption(Caption);
 UpdateCaptionLayout();
}
void UWardrobeSpeechBubble::ClearCaption(){
 Caption.Reset();Speaker.Reset();bFinishing=false;FadeRemaining=0;
 if(Card)Card->SetCaption(TEXT(""));SetVisibility(ESlateVisibility::Collapsed);RemoveFromParent();
}
void UWardrobeSpeechBubble::FinishCaption(bool Immediately){if(Immediately||Caption.IsEmpty())ClearCaption();else{bFinishing=true;FadeRemaining=WardrobeCaption::FadeTime;}}
void UWardrobeSpeechBubble::NativeTick(const FGeometry& Geometry,float Dt){
 Super::NativeTick(Geometry,Dt);
 auto* Person=Speaker.Get();auto* PC=GetOwningPlayer();auto* Mesh=Person?Person->Character.Get():nullptr;
 if(!Person||Person->IsHidden()||!Person->IsInteractionEnabled()||!Mesh||!PC||Caption.IsEmpty()){ClearCaption();return;}
 if(bFinishing){FadeRemaining-=FMath::Max(0.f,Dt);if(FadeRemaining<=0){ClearCaption();return;}SetRenderOpacity(FadeRemaining/WardrobeCaption::FadeTime);}
 UpdateCaptionLayout();
}
void UWardrobeSpeechBubble::UpdateCaptionLayout(){
 auto* Person=Speaker.Get();auto* PC=GetOwningPlayer();auto* Mesh=Person?Person->Character.Get():nullptr;
 if(!Person||!Mesh||!PC||Caption.IsEmpty()){SetVisibility(ESlateVisibility::Hidden);return;}
 FName Head;const auto* Profile=Person->GetInteractionProfile();
 if(Profile&&Mesh->DoesSocketExist(Profile->CursorLook.AnchorBone))Head=Profile->CursorLook.AnchorBone;
 if(Head.IsNone())for(const FName Candidate:{FName(TEXT("head")),FName(TEXT("Head")),FName(TEXT("頭"))})if(Mesh->DoesSocketExist(Candidate)){Head=Candidate;break;}
 const FVector Anchor=Head.IsNone()?Mesh->Bounds.Origin+FVector(0,0,Mesh->Bounds.BoxExtent.Z*.65):Mesh->GetSocketLocation(Head);
 FVector2D Point;const FVector2D Screen=UWidgetLayoutLibrary::GetPlayerScreenWidgetGeometry(PC).GetLocalSize();
 if(!UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(PC,Anchor,Point,true)||Screen.X<=0||Screen.Y<=0||Point.X<0||Point.Y<0||Point.X>Screen.X||Point.Y>Screen.Y){SetVisibility(ESlateVisibility::Hidden);return;}
 ForceLayoutPrepass();
 const FVector2D CardSize=Card?FVector2D(Card->GetDesiredSize()):FVector2D(WardrobeCaption::Width,126);
 FVector2D Position=Point+FVector2D(32,-CardSize.Y-12);
 // The existing menu uses a 1600x900 centered canvas. Keep captions clear of its destination panel.
 for(TObjectIterator<UWardrobeMenu> It;It;++It)if(It->GetWorld()==GetWorld()&&It->IsInViewport()){
  const double Scale=FMath::Min(Screen.X/1600.,Screen.Y/900.);
  const double PanelLeft=(Screen.X-1600*Scale)*.5+1040*Scale;
  Position.X=FMath::Min(Position.X,PanelLeft-CardSize.X);break;
 }
 Position.X=FMath::Clamp(Position.X,12.,FMath::Max(12.,Screen.X-CardSize.X-12));
 Position.Y=FMath::Clamp(Position.Y,12.,FMath::Max(12.,Screen.Y-CardSize.Y-12));
 SetDesiredSizeInViewport(CardSize);SetPositionInViewport(Position,false);
 SetVisibility(ESlateVisibility::HitTestInvisible);
}
