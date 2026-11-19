#include "WardrobeAIServiceLifecycle.h"
#include "HAL/PlatformProcess.h"

namespace WardrobeAIServiceLifecycle
{
 struct FService
 {
  FProcHandle Process;
  TSet<const void*> Clients;
 };
 static TMap<FString,FService> Services;
 void RegisterClient(const FString& Key,const void* Client)
 {
  check(IsInGameThread());Services.FindOrAdd(Key).Clients.Add(Client);
 }
 bool IsRunning(const FString& Key)
 {
  check(IsInGameThread());auto* Entry=Services.Find(Key);
  if(!Entry||!Entry->Process.IsValid())return false;
  if(FPlatformProcess::IsProcRunning(Entry->Process))return true;
  FPlatformProcess::CloseProc(Entry->Process);Entry->Process.Reset();return false;
 }
 bool Launch(const FString& Key,const void* Client,const FString& Python,const FString& Arguments,const FString& Directory)
 {
  RegisterClient(Key,Client);if(IsRunning(Key))return true;
  auto& Entry=Services.FindChecked(Key);
  Entry.Process=FPlatformProcess::CreateProc(*Python,*Arguments,false,true,true,nullptr,0,*Directory,nullptr);
  return Entry.Process.IsValid();
 }
 void ReleaseClient(const void* Client)
 {
  check(IsInGameThread());
  for(auto It=Services.CreateIterator();It;++It){
   auto& Entry=It.Value();Entry.Clients.Remove(Client);
   if(Entry.Clients.IsEmpty()){
    if(Entry.Process.IsValid()){
     if(FPlatformProcess::IsProcRunning(Entry.Process))FPlatformProcess::TerminateProc(Entry.Process,true);
     FPlatformProcess::CloseProc(Entry.Process);
    }
    It.RemoveCurrent();
   }
  }
 }
 void StopAll()
 {
  for(auto& Pair:Services)if(Pair.Value.Process.IsValid()){
   FPlatformProcess::TerminateProc(Pair.Value.Process,true);FPlatformProcess::CloseProc(Pair.Value.Process);
  }
  Services.Reset();
 }
}
