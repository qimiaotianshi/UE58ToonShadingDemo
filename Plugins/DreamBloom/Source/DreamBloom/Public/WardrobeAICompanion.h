#pragma once
#include "CoreMinimal.h"
#include "WardrobeAICompanion.generated.h"

/** Only heard dialogue is retained; generated but unplayed branches never enter history. */
USTRUCT()
struct DREAMBLOOM_API FWardrobeCompanionTurn {
 GENERATED_BODY()
 UPROPERTY(SaveGame) bool bPlayer=false;
 UPROPERTY(SaveGame) FString Text;
 UPROPERTY(SaveGame) FName Emotion;
};
USTRUCT()
struct DREAMBLOOM_API FWardrobeCompanionMemory {
 GENERATED_BODY()
 UPROPERTY(SaveGame) TArray<FWardrobeCompanionTurn> Turns;
 /** Exact excerpts from a player's message, selected by the model and verified locally. */
 UPROPERTY(SaveGame) TArray<FString> Notes;
 UPROPERTY(SaveGame) FDateTime LastSpokenAt;
};
namespace WardrobeAICompanion {
 DREAMBLOOM_API void BoundMemory(FWardrobeCompanionMemory& Memory,int32 MaxTurns,int32 MaxNotes);
 DREAMBLOOM_API TArray<FString> SelectNotes(const FWardrobeCompanionMemory& Memory,const FString& Query);
 DREAMBLOOM_API FString DescribeRecent(const FWardrobeCompanionMemory& Memory);
 DREAMBLOOM_API FString NormalizeLine(const FString& Line);
}
