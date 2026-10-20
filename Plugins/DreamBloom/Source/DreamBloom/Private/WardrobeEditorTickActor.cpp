#include "WardrobeEditorTickActor.h"
#include "Engine/World.h"
#include "UObject/Script.h"
#if WITH_EDITOR
#include "Editor.h"
#endif

AWardrobeEditorTickActor::AWardrobeEditorTickActor(){
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.bStartWithTickEnabled=true;
 PrimaryActorTick.bAllowTickOnDedicatedServer=false;
 bReplicates=false;
}

bool AWardrobeEditorTickActor::ShouldDriveEditorTick(const UWorld* World) const{
 if(!World||!bEnableEditorTick) return false;
 // Only the editor world is driven. PIE worlds tick their own actors through the normal pipeline.
 if(World->WorldType!=EWorldType::Editor) return false;
#if WITH_EDITOR
 if(!bTickDuringPIE&&GEditor&&GEditor->PlayWorld) return false;
#endif
 return true;
}

void AWardrobeEditorTickActor::DriveEditorTick(float DeltaSeconds){
 const uint64 Frame=GFrameCounter;
 if(Frame==LastEditorTickFrame) return;
 LastEditorTickFrame=Frame;
 if(!bEnableEditorTick) return;

 float Dt=DeltaSeconds;
 if(EditorTickInterval>0.f){
  EditorTickAccumulator+=DeltaSeconds;
  if(EditorTickAccumulator<EditorTickInterval) return;
  Dt=EditorTickAccumulator;
  EditorTickAccumulator=0.f;
 }else{
  EditorTickAccumulator=0.f;
 }

 // AActor::Tick dispatches the blueprint ReceiveTick (Event Tick) for blueprint-derived classes.
 if(bDriveRegularTickEvent){
  Super::Tick(Dt);
  return;
 }

 UFunction* Func=GetClass()->FindFunctionByName(TEXT("ReceiveEditorTick"));
 if(!Func) return;

 // ProcessEvent reads and writes Func->ParmsSize bytes, which does not necessarily match
 // sizeof(float). Passing a 4 byte stack struct lets it stomp the caller frame and the
 // blueprint script then silently never runs. Always hand it a buffer that is big enough.
 enum { EditorTickParmsBytes = 128 };
 if(Func->ParmsSize>EditorTickParmsBytes){
  UE_LOG(LogTemp,Warning,TEXT("[EditorTick] %s ParmsSize=%d exceeds buffer, skipped"),*GetClass()->GetName(),Func->ParmsSize);
  return;
 }
 uint8 Parms[EditorTickParmsBytes];
 FMemory::Memzero(Parms,EditorTickParmsBytes);
 if(FProperty* Prop=Func->FindPropertyByName(TEXT("DeltaSeconds"))){
  *Prop->ContainerPtrToValuePtr<float>(Parms)=Dt;
 }
 // Blueprint script execution is disabled in the editor outside of PIE unless this guard is
 // active. Without it ProcessEvent silently does nothing, even though the tick itself runs.
#if WITH_EDITOR
 FEditorScriptExecutionGuard ScriptGuard;
#endif
 ProcessEvent(Func,Parms);
}
