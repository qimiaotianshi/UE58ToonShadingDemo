#pragma once

#include "CoreMinimal.h"

class FJsonObject;

namespace WardrobeAIDialogueLog
{
bool IsEnabled();
FString ReadUserText(const FJsonObject& Request);
void Write(const FGuid& RequestId, const FString& EventId, const TCHAR* Kind, const TCHAR* Stage,
           const FString& Text, FName Emotion = NAME_None);
void WriteExchange(const FGuid& RequestId, const FString& EventId, const TCHAR* Kind, const TCHAR* Stage,
                   const TCHAR* Peer, FString Body, const FString& ApiKey, int32 HttpStatus = INDEX_NONE,
                   bool bIncomplete = false);
}
