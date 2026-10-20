#include "WardrobeEditorTickDriver.h"
#include "WardrobeEditorTickActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogWardrobeEditorTick,Log,All);

FTSTicker::FDelegateHandle FWardrobeEditorTickDriver::TickerHandle;

static TAutoConsoleVariable<int32> CVarWardrobeEditorTick(
 TEXT("Wardrobe.EditorTick"),1,
 TEXT("Drive AWardrobeEditorTickActor blueprint logic while the level is open in the editor.\n")
 TEXT("0 disables the driver, 1 enables it (default)."),
 ECVF_Default);

static TAutoConsoleVariable<int32> CVarWardrobeEditorTickLog(
 TEXT("Wardrobe.EditorTick.Log"),0,
 TEXT("Log how many editor-tick actors are driven, once every 300 frames."),
 ECVF_Default);

void FWardrobeEditorTickDriver::Startup(){
 if(TickerHandle.IsValid()) return;
 TickerHandle=FTSTicker::GetCoreTicker().AddTicker(
  FTickerDelegate::CreateStatic(&FWardrobeEditorTickDriver::Tick),0.f);
 UE_LOG(LogWardrobeEditorTick,Log,TEXT("Editor tick driver started (FTSTicker)"));
}

void FWardrobeEditorTickDriver::Shutdown(){
 if(!TickerHandle.IsValid()) return;
 FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
 TickerHandle.Reset();
 UE_LOG(LogWardrobeEditorTick,Log,TEXT("Editor tick driver stopped"));
}

bool FWardrobeEditorTickDriver::IsDriverActive(){
 return TickerHandle.IsValid();
}

bool FWardrobeEditorTickDriver::Tick(float DeltaSeconds){
 if(!GEngine||CVarWardrobeEditorTick.GetValueOnGameThread()<=0) return true;

 int32 DrivenCount=0;
 for(const FWorldContext& Context:GEngine->GetWorldContexts()){
  UWorld* World=Context.World();
  if(!World||Context.WorldType!=EWorldType::Editor) continue;

  // Collect first: a blueprint tick may spawn or destroy actors, which would invalidate the iterator.
  TArray<AWardrobeEditorTickActor*> Actors;
  for(TActorIterator<AWardrobeEditorTickActor> It(World);It;++It){
   if(IsValid(*It)) Actors.Add(*It);
  }
  for(AWardrobeEditorTickActor* Actor:Actors){
   if(!IsValid(Actor)||!Actor->ShouldDriveEditorTick(World)) continue;
   Actor->DriveEditorTick(DeltaSeconds);
   ++DrivenCount;
  }
 }

 if(CVarWardrobeEditorTickLog.GetValueOnGameThread()>0){
  static uint64 LastLogFrame=0;
  if(GFrameCounter-LastLogFrame>=300){
   LastLogFrame=GFrameCounter;
   UE_LOG(LogWardrobeEditorTick,Log,TEXT("Drove %d editor-tick actor(s), dt=%.4f"),DrivenCount,DeltaSeconds);
  }
 }
 return true;
}
