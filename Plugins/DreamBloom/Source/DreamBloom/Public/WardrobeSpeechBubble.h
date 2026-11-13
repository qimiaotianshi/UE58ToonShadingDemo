#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "WardrobeSpeechBubble.generated.h"

class AWardrobeShowcase;
class SWardrobeSpeechCard;

/** One current caption, anchored to the speaking character. Never captures pointer input. */
UCLASS()
class DREAMBLOOM_API UWardrobeSpeechBubble : public UUserWidget {
 GENERATED_BODY()
public:
 void ShowCaption(AWardrobeShowcase* Character,const FString& Text);
 void FinishCaption(bool Immediately);
 virtual void ReleaseSlateResources(bool ReleaseChildren) override;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
 virtual void NativeTick(const FGeometry& Geometry,float Dt) override;
private:
 friend class FWardrobeSpeechPresentationTest;
 void ClearCaption();
 TWeakObjectPtr<AWardrobeShowcase> Speaker;
 FString Caption;
 TSharedPtr<SWardrobeSpeechCard> Card;
 float FadeRemaining=0;
 bool bFinishing=false;
};
