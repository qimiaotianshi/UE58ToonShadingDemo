#pragma once
#include "CoreMinimal.h"

namespace WardrobeAIServiceLifecycle
{
 void RegisterClient(const FString& Service,const void* Client);
 bool IsRunning(const FString& Service);
 bool Launch(const FString& Service,const void* Client,const FString& Python,const FString& Arguments,const FString& Directory);
 void ReleaseClient(const void* Client);
 void StopAll();
}
