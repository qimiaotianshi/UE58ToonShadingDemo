#include "Modules/ModuleManager.h"
#include "WardrobeAIServiceLifecycle.h"
#include "Misc/CoreDelegates.h"

class FDreamBloomModule : public FDefaultModuleImpl
{
 FDelegateHandle ExitHandle;
public:
 virtual void StartupModule() override
 {
  ExitHandle=FCoreDelegates::OnPreExit.AddStatic(&WardrobeAIServiceLifecycle::StopAll);
 }
 virtual void ShutdownModule() override
 {
  FCoreDelegates::OnPreExit.Remove(ExitHandle);WardrobeAIServiceLifecycle::StopAll();
 }
};
IMPLEMENT_MODULE(FDreamBloomModule, DreamBloom)
