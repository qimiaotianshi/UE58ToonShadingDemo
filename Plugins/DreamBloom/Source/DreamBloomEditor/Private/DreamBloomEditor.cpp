#include "Modules/ModuleManager.h"
#include "WardrobeEditorTickDriver.h"

/**
 * Editor-only module for DreamBloom. Besides graph nodes and asset tools it owns
 * FWardrobeEditorTickDriver, which gives AWardrobeEditorTickActor a reliable per-frame
 * tick inside the editor world.
 */
class FDreamBloomEditorModule:public IModuleInterface{
public:
 virtual void StartupModule() override{
  FWardrobeEditorTickDriver::Startup();
 }
 virtual void ShutdownModule() override{
  FWardrobeEditorTickDriver::Shutdown();
 }
};

IMPLEMENT_MODULE(FDreamBloomEditorModule,DreamBloomEditor)
