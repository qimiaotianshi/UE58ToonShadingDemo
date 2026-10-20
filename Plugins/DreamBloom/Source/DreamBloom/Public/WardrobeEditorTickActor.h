#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WardrobeEditorTickActor.generated.h"

/**
 * Base actor whose blueprint logic runs every frame in the editor world, not just during PIE.
 *
 * The editor ticks the level with LEVELTICK_ViewportsOnly, and FActorTickFunction::ExecuteTick only
 * reaches TickActor when ShouldTickIfViewportsOnly() returns true. That path also depends on viewport
 * realtime state, editor focus and background CPU throttling, so it silently stops working.
 *
 * This class is instead driven by FWardrobeEditorTickDriver (DreamBloomEditor module), a core
 * ticker that runs at the end of every editor frame, independent of viewport realtime state,
 * editor focus and background CPU throttling. In PIE and in packaged builds the class behaves
 * like a plain AActor and fires the regular Event Tick; Editor Tick is editor-only.
 *
 * The driver also opens an FEditorScriptExecutionGuard around the dispatch: blueprint script
 * execution is disabled in the editor outside of PIE, so ProcessEvent would otherwise silently
 * do nothing no matter how reliable the tick itself is.
 */
UCLASS(Blueprintable)
class DREAMBLOOM_API AWardrobeEditorTickActor : public AActor {
 GENERATED_BODY()
public:
 AWardrobeEditorTickActor();
 /** Editor-only event, driven by the editor tick driver. Never fires in a packaged build. */
 UFUNCTION(BlueprintImplementableEvent,Category="Editor Tick",meta=(DisplayName="Editor Tick"))
 void ReceiveEditorTick(float DeltaSeconds);
 /** Master switch for the editor-only tick. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Editor Tick") bool bEnableEditorTick=true;
 /** Keep ticking the editor instance while a PIE session runs. Off by default so only the PIE copy does work. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Editor Tick") bool bTickDuringPIE=false;
 /** Drive the regular Event Tick in the editor instead of Editor Tick, for sharing one graph. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Editor Tick") bool bDriveRegularTickEvent=false;
 /** Minimum seconds between editor ticks. 0 ticks once per frame. */
 UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Editor Tick",meta=(ClampMin="0.0",Units="s")) float EditorTickInterval=0.f;

 /** Called by the editor tick driver. Safe to call every frame; a second call in the same frame is ignored. */
 void DriveEditorTick(float DeltaSeconds);
 /** True when the driver should drive this actor inside the given world. */
 bool ShouldDriveEditorTick(const UWorld* World) const;
private:
 float EditorTickAccumulator=0.f;
 uint64 LastEditorTickFrame=MAX_uint64;
};
